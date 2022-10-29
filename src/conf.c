#define _XOPEN_SOURCE 500 // strtoul (C89)
#include <features.h>

#include <stdint.h>
struct config_;
#define config_access_s struct config_
#include "conf.h"

//extern FILE *errfp;
#define errfp stderr
#include "lock.h"
typedef struct lock lock_s;
#include "vt100.h"
#include "assert.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <alloca.h>
#include <fcntl.h>
#include <pwd.h>
#include <unistd.h> // close write access getuid
#include <sys/stat.h>
typedef uint32_t u32;
typedef  uint8_t u8;
typedef uint16_t u16;

#define APPNAME     "gsar"
#define APPNAME_LEN 4
///////////////////////////////////////////////////////////////////////////////
// utility

static void memrep (void *dst_, unsigned len, int oldchar, int newchar)
{
u8 *p, *tail;
	p = (u8 *)dst_, tail = p +len;
	while (p < tail) {
u8 *found;
		if (! (found = (u8 *)memchr (p, oldchar, tail -p)))
			break;
		p = found;
		*p++ = (u8)newchar;
	}
}

static bool mkdir_p (const char *dirpath, mode_t mode)
{
bool retval;
	retval = true;
char *dup;
	dup = strdup (dirpath);
ASSERTE(dup)
unsigned pos;
	pos = ('/' == *dirpath) ? 1 : 0;
	while (pos < strlen (dirpath)) {
		pos += strcspn (dirpath +pos, "/");
		dup[pos] = '\0';
		if (! (0 == mkdir (dup, mode) || EEXIST == errno)) {
fprintf (errfp, "%s: " VTRR "directory cannot create" VTO "." "\n", dup);
			retval = false;
			break;
		}
		dup[pos++] = '/';
	}
	free (dup);
	return retval;
}

///////////////////////////////////////////////////////////////////////////////
// non-state

static bool config_create_file (const char *path, mode_t mode)
{
int fd;
	if (-1 == (fd = open (path, O_WRONLY | O_CREAT | O_EXCL, mode))) {
fprintf (errfp, "%s: " VTRR "file cannot create" VTO ". '%s'(%d)" "\n", path, strerror (errno), errno);
			return false;
	}
	close (fd);
	// safety
	if (!(0 == access (path, F_OK))) {
fprintf (errfp, "%s: " VTRR "file cannot read" VTO " (unexpected). '%s'(%d)" "\n", path, strerror (errno), errno);
		return false;
	}
	return true;
}

///////////////////////////////////////////////////////////////////////////////
// private

typedef struct config_ {
	char *basedir;
	char *rawdata;
	unsigned cbmax;
	unsigned cbdata;
	bool is_dirty;
} config_s;

static const char *config_get_basedir (config_s *m_)
{
BUG(m_)
	if (m_->basedir)
		return m_->basedir;
	do {
char *env;
		if (env = getenv("XDG_CONFIG_HOME")) {
			m_->basedir = (char *)malloc (strlen(env) +1 +APPNAME_LEN +1);
			sprintf (m_->basedir, "%s" "/" APPNAME, env);
			break;
		}
const char *home;
		if (NULL == (home = getenv("HOME"))) {
struct passwd *pwd;
			pwd = getpwuid (getuid ());
			if (! (pwd && pwd->pw_dir))
				return NULL;
			home = pwd->pw_dir;
		}
		m_->basedir = (char *)malloc (strlen(home) +1 +7 +1 +APPNAME_LEN +1);
		sprintf (m_->basedir, "%s/.config/" APPNAME, home);
	} while (0);
struct stat st;
	if (!(0 == stat (m_->basedir, &st) && S_ISDIR(st.st_mode)) && !mkdir_p (m_->basedir, 0700))
		return NULL;
	return m_->basedir;
}

static char *config_get_rawdata (config_s *m_)
{
BUG(m_)
// still read
	if (m_->rawdata)
		return m_->rawdata;

// make path
const char *confdir;
	if (! (confdir = config_get_basedir (m_)))
		return NULL;
char *path;
	path = (char *)malloc (strlen (confdir) +1 +APPNAME_LEN +5 +1);
	sprintf (path, "%s/" APPNAME ".conf", confdir);
struct stat st;
	if (!(0 == access (path, F_OK)) && !config_create_file (path, 0700))
		return NULL;
FILE *fp;
	if (NULL == (fp = fopen (path, "rb"))) {
fprintf (errfp, "%s: " VTRR "file cannot open" VTO "." "\n", path);
		return NULL;
	}

// heap allocate
	fseek (fp, 0, SEEK_END);
unsigned fsize;
	fsize = (unsigned)ftell (fp);
	fseek (fp, 0, SEEK_SET);
	for (m_->cbmax = 4096; m_->cbmax < fsize +2;)
		m_->cbmax *= 2;
	m_->rawdata = (char *)malloc (m_->cbmax);
ASSERTE(m_->rawdata)

// file reading
unsigned got;
	got = (unsigned)fread (m_->rawdata, 1, fsize, fp);
	fclose (fp);
	if (! (fsize == got)) {
fprintf (errfp, "%s: " VTRR "reading not enough" VTO ". (req %d but res %d bytes)." "\n", path, fsize, got);
		return NULL;
	}

// remove CR/LF from end of data
	while (0 < fsize && strchr ("\r" "\n", m_->rawdata[fsize -1]))
		--fsize;

// easier to operate
	memrep (m_->rawdata, fsize, '\n', '\0');
	(m_->rawdata +fsize)[0] = '\0';
	(m_->rawdata +fsize)[1] = '\0';

	m_->cbdata = fsize;
	return m_->rawdata;
}

static char *config_seek (config_s *m_, const char *key)
{
BUG(m_)
	if (NULL == key || '\0' == *key)
		return NULL;
char *text, *scan;
	if (! (text = config_get_rawdata (m_)))
		return NULL;
	while (! ('\0' == *text)) {
		scan = text + strspn (text, " \t");
		text = strchr (text, '\0') +1;
		if (! (0 == memcmp (scan, key, strlen(key))))
			continue;
		scan += strlen(key);
		scan += strspn (scan, " \t");
		if (! ('=' == *scan))
			continue;
		++scan;
		scan += strspn (scan, " \t");
		return scan;
	}
	return NULL;
}

static void config_expand (config_s *m_, unsigned len)
{
unsigned expand;
	if (! (m_->cbmax < m_->cbdata +len +2))
		return;
	while (m_->cbmax < m_->cbdata +len +2)
		m_->cbmax *= 2;
	m_->rawdata = (u8 *)realloc (m_->rawdata, m_->cbmax);
}

///////////////////////////////////////////////////////////////////////////////
// interface

static unsigned config_get_u (config_s *m_, const char *key, unsigned defval)
{
BUG(m_)
const char *val;
	if (NULL == (val = config_seek (m_, key)) || !isdigit(*val))
		return defval;
	return atoi(val);
}

#if 1
static void config_set_u (config_s *m_, const char *key, unsigned val)
{
BUG(m_)
char valstr[10 +1 +5]; // 10=strlen('4294967296')
	sprintf (valstr, "%d", val);
unsigned len;
	len = (unsigned)strlen (valstr);

char *dst, *tail;
// append to last line
	if (! (dst = config_seek (m_, key))) {
unsigned preLF, expand;
		preLF = (0 < m_->cbdata) ? 1 : 0; // not need LF if file is empty.
		config_expand (m_, expand = preLF + strlen (key) +3 +len);
		if (preLF)
			m_->rawdata[m_->cbdata] = '\0';
		sprintf (m_->rawdata +m_->cbdata +preLF, "%s = ", key);
		m_->cbdata += expand;
		dst = (tail = m_->rawdata + m_->cbdata) -len;
	}
// shoten target line
	else if (dst +len < (tail = strchr (dst, '\0'))) {
		memmove (dst +len, tail, m_->rawdata + m_->cbdata +2 - tail);
		m_->cbdata -= tail - (dst +len);
	}
// expand target line
	else if (tail < dst +len) {
char *old;
		old = m_->rawdata;
unsigned expand;
		config_expand (m_, expand = (dst +len) - tail);
		dst = m_->rawdata + (dst - old);
		tail = m_->rawdata + (tail - old);
		memmove (dst +len, tail, m_->rawdata + m_->cbdata +2 - tail);
		m_->cbdata += expand;
	}
	strcpy (dst, valstr);
	m_->is_dirty = true;
}

static bool config_commit (config_s *m_)
{
BUG(m_ && m_->rawdata)
//	if (!m_->is_dirty)
//		return;

const char *confdir;
	if (! (confdir = config_get_basedir (m_)))
		return false;
char *path;
	path = (char *)malloc (strlen (confdir) +1 +APPNAME_LEN +5 +1);
	sprintf (path, "%s/" APPNAME ".conf", confdir);
FILE *fp;
	if (NULL == (fp = fopen (path, "wb"))) {
fprintf (errfp, "%s: " VTRR "file cannot open" VTO "." "\n", path);
		return false;
	}

const unsigned lastLF
	= 1; // 1: last line has LF,  0: doesn't have
	memrep (m_->rawdata, m_->cbdata +lastLF, '\0', '\n');
	fwrite (m_->rawdata, 1, m_->cbdata +lastLF, fp); // TODO: error handling
	memrep (m_->rawdata, m_->cbdata +lastLF, '\n', '\0');
	fclose (fp);
	m_->is_dirty = false;

	return true;
}
#endif

///////////////////////////////////////////////////////////////////////////////
// dtor/ctor

static void config_reset (config_s *m_)
{
BUG(m_)
#ifndef __cplusplus
	memset (m_, 0, sizeof(config_s));
#else //def __cplusplus
	m_->basedir  = NULL;
	m_->rawdata  = NULL;
	m_->cbmax    = NULL;
	m_->cbdata   = NULL;
	m_->is_dirty = false;
#endif // __cplusplus
}

static void config_dtor (struct config *this_)
{
config_s *m_;
	m_ = (config_s *)this_;
BUG(m_)
	if (m_->rawdata)
		free (m_->rawdata);
	if (m_->basedir)
		free (m_->basedir);
	config_reset (m_);
}

static void config_ctor (struct config *this_, unsigned cb)
{
#ifndef __cplusplus
ASSERT2(sizeof(config_s) <= cb, " %d <= " VTRR "%d" VTO, (unsigned)sizeof(config_s), cb)
#endif // __cplusplus
config_s *m_;
	m_ = (config_s *)this_;
BUG(m_)
	config_reset (m_);
}

///////////////////////////////////////////////////////////////////////////////
// singlton

static lock_s s_thread_lock;
#define _lock_cleanup() lock_dtor(&s_thread_lock);
#define _lock_warmup() lock_ctor(&s_thread_lock, sizeof(s_thread_lock));
#define _lock() lock_lock(&s_thread_lock);
#define _unlock() lock_unlock(&s_thread_lock);

static struct config s_obj, *s_ptr = NULL;
struct config *config_get_singlton ()
{
_lock()
	if (NULL == s_ptr) {
		config_ctor (&s_obj, sizeof(s_obj));
		s_ptr = &s_obj;
	}
_unlock()
	return s_ptr;
}

// PENDING: call get_singlton() between _lock() .. _unlock() (*)define as invalid client code.
void config_cleanup()
{
_lock()
	if (s_ptr) {
		s_ptr = NULL;
		config_dtor (&s_obj);
	}
_unlock()
_lock_cleanup()
}

void config_warmup() { _lock_warmup() }

///////////////////////////////////////////////////////////////////////////////

static struct config_access_i o_config_access_i = {
	.get_u  = config_get_u,
	.set_u  = config_set_u,
	.commit = config_commit,
};
struct config_access_i *config_query_config_access_i (struct config *this_) { return &o_config_access_i; }
