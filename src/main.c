#define _XOPEN_SOURCE 500 // usleep (>= 500)
#include <features.h>
#include <gtk/gtk.h>
#include <stdint.h>

// TODO: move to XDG configuration ($HOME/.config/gsar/)
#define MAX_CPU 16
#define REDRAW_INTERVAL_MS 500 // [ms]
#define SCROLL_PER_REDRAW  2   // [px]
#define DX     6
#define DY     6
#define WIDTH  276
#define HEIGHT 48

#include "common/pixel.h"
struct gtk2_helper;
#define gtk_helper_s struct gtk2_helper
#include "gtk2_helper.h"
typedef struct gtk2_helper gtk2_helper_s;
#define DRAWING_AREA(root) gtk_widget_get_child (root, GTK_TYPE_DRAWING_AREA)

#include "bmp_grid.h"
typedef struct bmp_grid bmp_grid_s;
#include "bmp_helper.h"

#include "lock.h"
typedef struct lock lock_s;
#if 0
#define _lock_cleanup() lock_dtor(&m_->rawdata_lock);
#define _lock_warmup()  lock_ctor(&m_->rawdata_lock, sizeof(m_->rawdata_lock));
#define _lock()         lock_lock(&m_->rawdata_lock);
#define _unlock()       lock_unlock(&m_->rawdata_lock);
#else
#define _lock_cleanup()
#define _lock_warmup()
#define _lock()
#define _unlock()
#endif

#include "vt100.h"
#include "assert.h"

#include <stdbool.h>
#include <stdlib.h> // fprintf
#include <ctype.h> // isdigit

typedef uint32_t u32;
typedef  uint8_t u8;
typedef uint16_t u16;

#define max(a, b) ((a) > (b) ? (a) : (b))
///////////////////////////////////////////////////////////////
// CPU data layout

typedef struct cpu_load_ {
	int num;
	unsigned long long user;
	unsigned long long nice;
	unsigned long long sys;
	unsigned long long idle;
	unsigned long long iowait;
	unsigned long long steal;
	unsigned long long hardirq;
	unsigned long long softirq;
	unsigned long long guest;
	unsigned long long guest_nice;
	unsigned long long total;
} cpu_load_s;

///////////////////////////////////////////////////////////////
// CPU load wrapper

#define STAT "/proc/stat"

static void cpu_load_delta (cpu_load_s *delta, const cpu_load_s *curr, const cpu_load_s *prev)
{
	delta->user        = curr->user       - prev->user      ;
	delta->nice        = curr->nice       - prev->nice      ;
	delta->sys         = curr->sys        - prev->sys       ;
	delta->idle        = curr->idle       - prev->idle      ;
	delta->iowait      = curr->iowait     - prev->iowait    ;
	delta->steal       = curr->steal      - prev->steal     ;
	delta->hardirq     = curr->hardirq    - prev->hardirq   ;
	delta->softirq     = curr->softirq    - prev->softirq   ;
	delta->guest       = curr->guest      - prev->guest     ;
	delta->guest_nice  = curr->guest_nice - prev->guest_nice;
	delta->total       = curr->total      - prev->total     ;
}

static u8 cpu_load_get (u8 max_cpu, cpu_load_s *got)
{
u8 retval;
	retval = 0;
FILE *fp;
	if (! (fp = fopen(STAT, "r"))) {
fprintf (stderr, "%s: cannot open." "\n", STAT);
		return 0;
	}
char line[8192];
cpu_load_s cpu;
	while (fgets (line, sizeof(line), fp)) {
		if (! (0 == strncmp (line, "cpu", 3) && isdigit (line[3])))
			continue;
		memset (&cpu, 0, sizeof(cpu));
		sscanf (line + 3, "%d %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
		       &cpu.num,
		       &cpu.user,
		       &cpu.nice,
		       &cpu.sys,
		       &cpu.idle,
		       &cpu.iowait,
		       &cpu.hardirq,
		       &cpu.softirq,
		       &cpu.steal,
		       &cpu.guest,
		       &cpu.guest_nice);
		++cpu.num;
// ref) sysstat/rd_stats.c::get_per_cpu_interval()
		cpu.total = cpu.user
			       + cpu.nice
			       + cpu.sys
			       + cpu.idle
			       + cpu.iowait
			       + cpu.steal
			       + cpu.hardirq
			       + cpu.softirq
//			       + cpu.guest
//			       + cpu.guest_nice
		          ;
u8 found;
		for (found = 0; found < max_cpu; ++found)
			if (cpu.num == got[found].num || 0 == got[found].num)
				break;
		if (! (found < max_cpu)) {
fprintf (stderr, "%s: error: unexpected 'cpu%d'" "\n", STAT, cpu.num -1);
			continue;
		}
		memcpy (&got[found], &cpu, sizeof(got[found]));
		retval = max(retval, found +1);
	}
	fclose (fp);
	return retval;
}

///////////////////////////////////////////////////////////////
// data layout

typedef struct gsar_ {
	cpu_load_s *prev;
	cpu_load_s *delta_prev;
	bmp_grid_s *grid;
	u8 **magdata;
#if 0
	lock_s rawdata_lock;
#endif
	u8 max_cpu;
	u8 mag;
	bool is_got_prev;
} gsar_s;

///////////////////////////////////////////////////////////////
// setter / getter

static GQuark quark_app_model = 0;

static void _set_control (gsar_s *m_, GtkWidget *da)
{
BUG(0 == quark_app_model)
	if (0 == quark_app_model)
		quark_app_model = g_quark_from_static_string ("app-model");
ASSERTE(0 < quark_app_model)
BUG(GTK_IS_DRAWING_AREA(da))
	g_object_set_qdata (G_OBJECT(da), quark_app_model, (gpointer)m_);
}

static gsar_s *_get_control (GtkWidget *da)
{
BUG(GTK_IS_DRAWING_AREA(da))
gsar_s *m_;
	m_ = (gsar_s *)g_object_get_qdata (G_OBJECT(da), quark_app_model);
ASSERTE(m_)
	return m_;
}

///////////////////////////////////////////////////////////////
// ctor / dtor

static void gsar_dtor (gsar_s *m_)
{
unsigned n;
	for (n = 0; n < m_->max_cpu; ++n)
		bmp_grid_dtor (&m_->grid[n]);
	if (m_->prev) {
		free (m_->prev);
		m_->prev = NULL;
	}
	if (m_->delta_prev) {
		free (m_->delta_prev);
		m_->delta_prev = NULL;
	}
	if (m_->grid) {
		free (m_->grid);
		m_->grid = NULL;
	}
	if (m_->magdata) {
unsigned n;
		for (n = 0; n < m_->max_cpu; ++n)
			if (m_->magdata[n]) {
				free (m_->magdata[n]);
				m_->magdata[n] = NULL;
			}
		free (m_->magdata);
		m_->magdata = NULL;
	}
_lock_cleanup()
}

///////////////////////////////////////////////////////////////
// graph running

static void gsar_upload_data (gsar_s *m_, GtkDrawingArea *da)
{
unsigned cb_mag;
	cb_mag = bmp_magnify (NULL, 24, WIDTH, HEIGHT, m_->mag, NULL, 0);

unsigned n;
	for (n = 0; n < m_->max_cpu; ++n) {
const u8 *rawdata;
		rawdata = bmp_grid_get (&m_->grid[n], PF_RGB888);
		bmp_magnify (rawdata, 24, WIDTH, HEIGHT, m_->mag, m_->magdata[n], cb_mag);
	}

gtk2_helper_s gh_this_;
	gtk2_helper_ctor (&gh_this_, sizeof(gh_this_), GTK_WIDGET(da));
struct gtk_helper_i *gh;
	gh = gtk2_helper_query_gtk_helper_i (&gh_this_);
_lock()
u16 y;
	for (y = DY, n = 0; n < m_->max_cpu; ++n) {
		gh->paint_data (&gh_this_, DX, y, WIDTH * m_->mag, HEIGHT * m_->mag, PF_RGB888, m_->magdata[n]);
		y += DY + HEIGHT * m_->mag;
	}
_unlock()
	gtk2_helper_release (&gh_this_);
}

static bool gsar_next_data (gsar_s *m_)
{
cpu_load_s delta[MAX_CPU], now[MAX_CPU];
unsigned max_cpu, i;
	memset (now, 0, sizeof(now));
	max_cpu = cpu_load_get (MAX_CPU, now);

	if (m_->is_got_prev)
		for (i = 0; i < max_cpu; ++i)
			cpu_load_delta (&delta[i], &now[i], &m_->prev[i]);

	for (i = 0; i < max_cpu; ++i)
		memcpy (&m_->prev[i], &now[i], sizeof(m_->prev[i]));

	if (!m_->is_got_prev) {
		m_->is_got_prev = true;
		return false;
	}

_lock()
	for (i = 0; i < max_cpu; ++i)
		bmp_grid_scroll (&m_->grid[i], LEFT, SCROLL_PER_REDRAW);
_unlock()

	for (i = 0; i < max_cpu; ++i) {
		if (! (0 < delta[i].total))
			continue;
cpu_load_s *p, *q; int green, yellow, red;
		p = &delta[i], q = &m_->delta_prev[i];
		yellow = HEIGHT * (int)(p->total - (p->iowait + p->sys)) / p->total;
		red    = HEIGHT * (int)(p->total - p->sys) / p->total;
		green  = HEIGHT * (int)p->idle / p->total;
u16 dx, sx;
		dx = WIDTH -1, sx = WIDTH -1 - SCROLL_PER_REDRAW;
		if (! (0 < m_->delta_prev[i].total)) {
_lock()
			bmp_grid_pset (&m_->grid[i], dx, yellow, 0xffff00); // RGB(255,255,0)
			bmp_grid_pset (&m_->grid[i], dx, red   , 0xff0000); // RGB(255,0,0)
			bmp_grid_pset (&m_->grid[i], dx, green , 0x00ff00); // RGB(0,255,0)
_unlock()
		}
		else {
int green0, yellow0, red0;
			yellow0 = HEIGHT * (int)(q->total - (q->iowait + q->sys)) / q->total;
			red0    = HEIGHT * (int)(q->total - q->sys) / q->total;
			green0  = HEIGHT * (int)q->idle / q->total;
_lock()
			bmp_grid_line (&m_->grid[i], sx, yellow0, dx, yellow, 0xffff00); // RGB(255,255,0)
			bmp_grid_line (&m_->grid[i], sx, red0   , dx, red   , 0xff0000); // RGB(255,0,0)
			bmp_grid_line (&m_->grid[i], sx, green0 , dx, green , 0x00ff00); // RGB(0,255,0)
_unlock()
		}
	}
	for (i = 0; i < max_cpu; ++i)
		memcpy (&m_->delta_prev[i], &delta[i], sizeof(m_->delta_prev[i]));
	return true;
}

static void gsar_kick_redraw (gsar_s *m_, GtkDrawingArea *da)
{
unsigned y, n;
	for (y = DY, n = 0; n < m_->max_cpu; ++n) {
		gtk_widget_queue_draw_area (GTK_WIDGET(da), DX, y, WIDTH * m_->mag, HEIGHT * m_->mag);
		y += DY + HEIGHT * m_->mag;
	}
}

///////////////////////////////////////////////////////////////
// GTK+ event handler

// GDK_DESTROY(1)
static void _on_destroy (GtkObject *object, gpointer data)
{
BUG(GTK_IS_DRAWING_AREA(data))
gsar_s *m_;
	m_ = _get_control (GTK_WIDGET(data));
	gsar_dtor (m_);

	gtk_main_quit ();
}

// GDK_EXPOSE(2)
static gint _on_expose (GtkWidget *da, GdkEventExpose *event, gpointer data)
{
BUG(GTK_IS_DRAWING_AREA(da))
gsar_s *m_;
	m_ = _get_control (da);
	gsar_upload_data (m_, GTK_DRAWING_AREA(da));
	return TRUE;
}

static gboolean _on_timer (gpointer da)
{
BUG(GTK_IS_DRAWING_AREA(da))
gsar_s *m_;
	m_ = _get_control (GTK_WIDGET(da));

	if (gsar_next_data (m_))
		gsar_kick_redraw (m_, da); // cause GDK_EXPOSE event.

	return G_SOURCE_CONTINUE;
}

///////////////////////////////////////////////////////////////

static void print_usage ()
{
fprintf (stderr, "usage:" "\n");
fprintf (stderr, "   -m, --magnify <n>  %s" "\n", "graph expanding");
}

typedef struct options_ {
	u8 mag;
} options_s;
static int parse_cmdline (options_s *m_, int argc, char **argv, char **cmdv)
{
	m_->mag = 1;
int cmdc = 0;
	cmdv[cmdc++] = argv[0];
unsigned i;
	for (i = 1; i < argc; ++i) {
		if (0 == strcmp ("--help", argv[i])) {
			print_usage ();
			exit (0);
		}
		if (i +1 < argc && (0 == strcmp ("-m", argv[i]) || 0 == strcmp ("--magnify", argv[i])))
			m_->mag = atoi (argv[++i]);
		else
			cmdv[cmdc++] = argv[i];
	}
	return cmdc;
}

int main (int argc, char *argv[])
{
unsigned max_cpu; cpu_load_s dummy[MAX_CPU];
	memset (dummy, 0, sizeof(dummy));
	max_cpu = cpu_load_get (MAX_CPU, dummy);

int cmdc; char **cmdv;
	cmdv = (char **)alloca (argc * sizeof(char *));
options_s opts;
	cmdc = parse_cmdline (&opts, argc, argv, cmdv);

gsar_s this_, *m_ = &this_;
	memset (m_, 0, sizeof(*m_));
	m_->max_cpu = max_cpu;
	m_->prev       = (cpu_load_s *)malloc (sizeof(cpu_load_s) * max_cpu);
	m_->delta_prev = (cpu_load_s *)malloc (sizeof(cpu_load_s) * max_cpu);
	m_->grid       = (bmp_grid_s *)malloc (sizeof(bmp_grid_s) * max_cpu);
	m_->magdata    = (u8 **)malloc (sizeof(u8 *) * max_cpu);
unsigned cb_mag;
	cb_mag = bmp_magnify (NULL, 24, WIDTH, HEIGHT, opts.mag, NULL, 0);
unsigned n;
	for (n = 0; n < max_cpu; ++n) {
		bmp_grid_ctor (&m_->grid[n], sizeof(m_->grid[n]), WIDTH, HEIGHT);
		m_->magdata[n] = (u8 *)malloc (cb_mag);
	}
	m_->mag = opts.mag;
_lock_warmup()

	gtk_init (&cmdc, &cmdv);
GtkWindow *appframe;
	if (! (appframe = GTK_WINDOW(gtk_window_new (GTK_WINDOW_TOPLEVEL))))
		return -1;

GtkWidget *da;
	da = gtk_drawing_area_new ();
ASSERTE(GTK_IS_DRAWING_AREA(da))
	g_signal_connect (appframe, "destroy", G_CALLBACK(_on_destroy), da);
	g_signal_connect (da, "expose_event", G_CALLBACK(_on_expose), NULL);
	gtk_widget_show (da);
	gtk_container_add (GTK_CONTAINER(appframe), da);

GtkWidget *da_;
	da_ = DRAWING_AREA(GTK_WIDGET(appframe));
	gtk_widget_set_size_request (GTK_WIDGET(da_)
		, DX * 2 +           WIDTH  * m_->mag
		, DY * 3 + max_cpu * HEIGHT * m_->mag 
		);

	_set_control (m_, da);
	gdk_threads_add_timeout (REDRAW_INTERVAL_MS, _on_timer, (gpointer)da);

	gtk_widget_show_all (GTK_WIDGET(appframe));
	gtk_main ();

	return 0;
}
