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
#define GRID_X 12
#define GRID_Y 12
#define  BKGND_RGB RGB(0,0,0)
#define   GRID_RGB RGB(0,128,64)
#define IOWAIT_RGB YELLOW
#define SYSTEM_RGB RED
#define  TOTAL_RGB GREEN

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
#include "conf.h"
#define CFG_LEFT "left"
#define CFG_TOP  "top"
#define DEFAULT_LEFT 0
#define DEFAULT_TOP  0

#include "vt100.h"
#include "assert.h"

#include <stdbool.h>
#include <stdlib.h> // fprintf
#include <ctype.h> // isdigit

typedef uint32_t u32, u24;
typedef  uint8_t u8;
typedef uint16_t u16;

#define max(a, b) ((a) > (b) ? (a) : (b))
#define RGB(r, g, b) (u24)(r << 16 | g << 8 | b)
#define RED    RGB(255,0,0)
#define GREEN  RGB(0,255,0)
#define YELLOW RGB(255,255,0)
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
// control layout

typedef struct gsar_ {
	cpu_load_s *prev;
	cpu_load_s *delta_prev;
	bmp_grid_s *grid;
	u8 **magdata;
#if 0
	lock_s rawdata_lock;
#endif
	u24 total_cl;
	u24 system_cl;
	u24 iowait_cl;
	u16 width;
	u16 height;
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
gpointer old;
	old = g_object_get_qdata (G_OBJECT(da), quark_app_model);
	g_object_set_qdata (G_OBJECT(da), quark_app_model, (gpointer)m_);
	if (old == (gpointer)m_)
		return;
	gtk_widget_set_size_request (GTK_WIDGET(da)
		, DX * 2 +               m_->width  * m_->mag
		, DY * 3 + m_->max_cpu * m_->height * m_->mag 
		);
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

static void gsar_ctor (gsar_s *m_, unsigned cb, u16 width, u16 height, u8 max_cpu, u8 mag)
{
ASSERT2(sizeof(gsar_s) <= cb, " %d <= " VTRR "%d" VTO, (unsigned)sizeof(gsar_s), cb)
	memset (m_, 0, sizeof(*m_));
	m_->width   = width;
	m_->height  = height;
	m_->max_cpu = max_cpu;
	m_->mag     = mag;
	m_->prev       = (cpu_load_s *)malloc (sizeof(cpu_load_s) * max_cpu);
	m_->delta_prev = (cpu_load_s *)malloc (sizeof(cpu_load_s) * max_cpu);
	m_->grid       = (bmp_grid_s *)malloc (sizeof(bmp_grid_s) * max_cpu);
	m_->magdata    = (u8 **)malloc (sizeof(u8 *) * max_cpu);
unsigned cb_mag;
	cb_mag = bmp_magnify (NULL, 24, width, height, mag, NULL, 0);
unsigned n;
	for (n = 0; n < max_cpu; ++n) {
		bmp_grid_ctor (&m_->grid[n], sizeof(m_->grid[n]), width, height);
		bmp_grid_set_grid (&m_->grid[n], GRID_X, GRID_Y);
		bmp_grid_set_color (&m_->grid[n], GRID_RGB, BKGND_RGB);
		bmp_grid_erase (&m_->grid[n], 0, 0, 0, 0);
		m_->magdata[n] = (u8 *)malloc (cb_mag);
	}
_lock_warmup()
}

///////////////////////////////////////////////////////////////
// graph running

static void gsar_upload_data (gsar_s *m_, GtkDrawingArea *da)
{
unsigned cb_mag;
	cb_mag = bmp_magnify (NULL, 24, m_->width, m_->height, m_->mag, NULL, 0);

unsigned n;
	for (n = 0; n < m_->max_cpu; ++n) {
const u8 *rawdata;
		rawdata = bmp_grid_get (&m_->grid[n], PF_RGB888);
		bmp_magnify (rawdata, 24, m_->width, m_->height, m_->mag, m_->magdata[n], cb_mag);
	}

gtk2_helper_s gh_this_;
	gtk2_helper_ctor (&gh_this_, sizeof(gh_this_), GTK_WIDGET(da));
struct gtk_helper_i *gh;
	gh = gtk2_helper_query_gtk_helper_i (&gh_this_);
_lock()
u16 y;
	for (y = DY, n = 0; n < m_->max_cpu; ++n) {
		gh->paint_data (&gh_this_, DX, y, m_->width * m_->mag, m_->height * m_->mag, PF_RGB888, m_->magdata[n]);
		y += DY + m_->height * m_->mag;
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
cpu_load_s *p, *q; int y_total, y_iowait, y_system;
		p = &delta[i], q = &m_->delta_prev[i];
		y_iowait = m_->height * (int)(p->total - (p->iowait + p->sys)) / p->total;
		y_system = m_->height * (int)(p->total - p->sys) / p->total;
		y_total  = m_->height * (int)p->idle / p->total;
u16 dx, sx;
		dx = m_->width -1, sx = m_->width -1 - SCROLL_PER_REDRAW;
		if (! (0 < m_->delta_prev[i].total)) {
_lock()
			bmp_grid_pset (&m_->grid[i], dx, y_iowait, IOWAIT_RGB);
			bmp_grid_pset (&m_->grid[i], dx, y_system, SYSTEM_RGB);
			bmp_grid_pset (&m_->grid[i], dx, y_total ,  TOTAL_RGB);
_unlock()
		}
		else {
int y_total0, y_iowait0, y_system0;
			y_iowait0 = m_->height * (int)(q->total - (q->iowait + q->sys)) / q->total;
			y_system0 = m_->height * (int)(q->total - q->sys) / q->total;
			y_total0  = m_->height * (int)q->idle / q->total;
_lock()
			bmp_grid_line (&m_->grid[i], sx, y_iowait0, dx, y_iowait, IOWAIT_RGB);
			bmp_grid_line (&m_->grid[i], sx, y_system0, dx, y_system, SYSTEM_RGB);
			bmp_grid_line (&m_->grid[i], sx, y_total0 , dx, y_total ,  TOTAL_RGB);
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
		gtk_widget_queue_draw_area (GTK_WIDGET(da), DX, y, m_->width * m_->mag, m_->height * m_->mag);
		y += DY + m_->height * m_->mag;
	}
}

///////////////////////////////////////////////////////////////
// application (data)

typedef struct app_ {
	struct config *cfg_this_;
	void (*on_finally)(struct app_ *m_, void *aux);
	void *on_finally_aux;
	int x, y;
} app_s;

static app_s *app_get_instance ()
{
static app_s s_app = {0};
	return &s_app;
}
static void app_set_config (app_s *m_, struct config *this_)
{
BUG(m_)
	m_->cfg_this_ = this_;
}

static struct config *app_get_config (app_s *m_)
{
BUG(m_)
	return m_->cfg_this_;
}

static void app_set_position (app_s *m_, int x, int y)
{
BUG(m_)
	m_->x = x;
	m_->y = y;
}

static void app_get_position (app_s *m_, int *x, int *y)
{
BUG(m_)
	if (x) *x = m_->x;
	if (y) *y = m_->y;
}

static void app_set_finally (app_s *m_, void (*callback)(app_s *m_, void *aux), void *aux)
{
BUG(m_)
	m_->on_finally     = callback;
	m_->on_finally_aux = aux;
}

static void app_call_finally (app_s *m_)
{
	if (m_->on_finally)
		m_->on_finally (m_, m_->on_finally_aux);
}

///////////////////////////////////////////////////////////////
// application (GTK+ event handler)

// GDK_CONFIGURE(13)
static void _on_configure (GtkWidget *appframe, GdkEventConfigure *event, gpointer user_data)
{
BUG(GTK_IS_WINDOW(appframe))
gint x, y;
	x = y = 0; // safety
	gtk_window_get_position (GTK_WINDOW(appframe), &x, &y);
app_s *m_
	= app_get_instance ();
	app_set_position (m_, (int)x, (int)y);
}

// GDK_DESTROY(1)
static void _on_destroy (GtkWidget *appframe, gpointer user_data)
{
BUG(GTK_IS_DRAWING_AREA(user_data))
GtkWidget *da;
	da = GTK_WIDGET(user_data);

app_s *m_
	= app_get_instance ();
	app_call_finally (m_);

gsar_s *gsar;
	gsar = _get_control (da);
	gsar_dtor (gsar);

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
// application (primary thread)

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

static void _on_exit (app_s *m_, void *user_data)
{
int x, y;
	app_get_position (m_, &x, &y);

struct config_access_i *cfg; struct config *cfg_this_;
	cfg = config_query_config_access_i (cfg_this_ = config_get_singlton ());
	cfg->set_u (cfg_this_, CFG_LEFT, x);
	cfg->set_u (cfg_this_, CFG_TOP , y);
	cfg->commit (cfg_this_);
}

int main (int argc, char *argv[])
{
unsigned cpu_count; cpu_load_s dummy[MAX_CPU];
	memset (dummy, 0, sizeof(dummy));
	cpu_count = cpu_load_get (MAX_CPU, dummy);

int cmdc; char **cmdv;
	cmdv = (char **)alloca (argc * sizeof(char *));
options_s opts;
	cmdc = parse_cmdline (&opts, argc, argv, cmdv);

	config_warmup ();
struct config_access_i *cfg; struct config *cfg_this_;
	cfg = config_query_config_access_i (cfg_this_ = config_get_singlton ());
u16 conf_left, conf_top;
	conf_left = (u16)cfg->get_u (cfg_this_, CFG_LEFT, DEFAULT_LEFT);
	conf_top  = (u16)cfg->get_u (cfg_this_, CFG_TOP , DEFAULT_TOP );

app_s *m_
	= app_get_instance ();
	app_set_config (m_, cfg_this_);
	app_set_position (m_, conf_left, conf_top);
	app_set_finally (m_, _on_exit, NULL);

gsar_s gsar;
	gsar_ctor (&gsar, sizeof(gsar), WIDTH, HEIGHT, cpu_count, opts.mag);

	gtk_init (&cmdc, &cmdv);
GtkWindow *appframe;
	if (! (appframe = GTK_WINDOW(gtk_window_new (GTK_WINDOW_TOPLEVEL))))
		return -1;
	gtk_window_move (appframe, conf_left, conf_top);

GtkWidget *da;
	da = gtk_drawing_area_new ();
ASSERTE(GTK_IS_DRAWING_AREA(da))
	g_signal_connect (appframe, "destroy"        , G_CALLBACK(_on_destroy)  , da);
	g_signal_connect (appframe, "configure_event", G_CALLBACK(_on_configure), da);
	g_signal_connect (da      , "expose_event"   , G_CALLBACK(_on_expose)   , NULL);
	gtk_widget_show (da);
	gtk_container_add (GTK_CONTAINER(appframe), da);

	_set_control (&gsar, da);
	gdk_threads_add_timeout (REDRAW_INTERVAL_MS, _on_timer, (gpointer)da);

	gtk_widget_show_all (GTK_WIDGET(appframe));
	gtk_main ();

	config_cleanup ();
	return 0;
}
