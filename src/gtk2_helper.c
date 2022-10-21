#include <gtk/gtk.h>

#include <stdint.h>
#include "common/pixel.h"
struct gtk2_helper_;
#define gtk_helper_s struct gtk2_helper_
#include "gtk2_helper.h"

#include "vt100.h"
#include "assert.h"

#include <stdbool.h>
#include <stdlib.h> // exit
#include <string.h>
#include <errno.h>

typedef uint32_t u32;
typedef  uint8_t u8;
#define ALIGN32(x) (-32 & (x) +32 -1)

///////////////////////////////////////////////////////////////////////////////
// non-state

GtkWidget *gtk_widget_get_child (GtkWidget *checked, GType find_type)
{
	if (G_TYPE_CHECK_INSTANCE_TYPE(checked, find_type))
		return GTK_WIDGET(checked);

	if (GTK_IS_NOTEBOOK(checked)) {
gint page_num;
		page_num = gtk_notebook_get_current_page (GTK_NOTEBOOK(checked));
		checked = gtk_notebook_get_nth_page (GTK_NOTEBOOK(checked), page_num);
		return gtk_widget_get_child (checked, find_type);
	}

	if (!GTK_IS_CONTAINER(checked))
		return NULL;
GList *list;
#if GTK_CHECK_VERSION(2,22,0)
	list = gtk_container_get_children (GTK_CONTAINER(checked));
#else
	if (GTK_IS_VBOX(checked)) {
GtkVBox *vbox;
		vbox = GTK_VBOX(checked);
		list = vbox->box->children;
	}
	else
		list = gtk_container_get_children (GTK_CONTAINER(checked));
#endif

	while (list) {
		if (checked = gtk_widget_get_child (GTK_WIDGET(list->data), find_type))
			return checked;
		list = list->next;
	}
	return NULL;
}

///////////////////////////////////////////////////////////////////////////////
// private

typedef struct gtk2_helper_ {
	GtkWidget *da;
	GdkDrawable *drawable;
	cairo_t *cr;
} gtk2_helper_s;

///////////////////////////////////////////////////////////////////////////////
// dtor/ctor

static void gtk2_helper_reset (gtk2_helper_s *m_)
{
GtkWidget *da;
	da = m_->da;
	memset (m_, 0, sizeof(gtk2_helper_s));
	m_->da = da;
}

void gtk2_helper_release (struct gtk2_helper *this_)
{
gtk2_helper_s *m_;
	m_ = (gtk2_helper_s *)this_;
	if (m_->cr)
		cairo_destroy (m_->cr);
	gtk2_helper_reset (m_);
}

void gtk2_helper_ctor (struct gtk2_helper *this_, unsigned cb, GtkWidget *da)
{
ASSERT2(sizeof(gtk2_helper_s) <= cb, " %d <= " VTRR "%d" VTO, (unsigned)sizeof(gtk2_helper_s), cb)
gtk2_helper_s *m_;
	m_ = (gtk2_helper_s *)this_;
BUG(m_)
	m_->da = da;
BUG(m_->da)
BUG(GTK_IS_DRAWING_AREA(m_->da))
	gtk2_helper_reset (m_);
}

///////////////////////////////////////////////////////////////////////////////
// getter (GDK/GTK)

static GdkDrawable *gtk2_helper_get_gdk_drawable (gtk2_helper_s *m_)
{
BUG(m_)
	if (NULL == m_->drawable) {
GdkWindow *window;
		window = gtk_widget_get_window (m_->da);
BUG(GDK_IS_DRAWABLE(window))
		m_->drawable = GDK_DRAWABLE(window);
	}
	return m_->drawable;
}

///////////////////////////////////////////////////////////////////////////////
// getter (cairo)

// NOTE: (*1) this pointer must be released by 'cairo_destroy()'
static cairo_t *gtk2_helper_get_cairo_t (gtk2_helper_s *m_)
{
BUG(m_)
	if (NULL == m_->cr) {
GdkDrawable *drawable;
		drawable = gtk2_helper_get_gdk_drawable (m_);
		m_->cr = gdk_cairo_create (drawable); // (*1)
BUG(m_->cr)
	}
	return m_->cr;
}

///////////////////////////////////////////////////////////////////////////////
// interface

static void gtk2_helper_paint_data (gtk2_helper_s *m_, int x, int y, int width, int height, enum pixel_format pf, const void *rgb24_le)
{
BUG(m_ && (PF_RGB888 == pf || PF_RGBA8888 == pf))
	if (! (PF_RGB888 == pf || PF_RGBA8888 == pf))
		return;
gboolean has_alpha;
	has_alpha = (PF_RGBA8888 == pf) ? TRUE : FALSE;
cairo_t *cr;
	cr = gtk2_helper_get_cairo_t (m_);
GdkPixbuf *pixbuf;
	if (NULL == (pixbuf = gdk_pixbuf_new_from_data ((guchar *)rgb24_le, GDK_COLORSPACE_RGB, has_alpha, 8, width, height, ALIGN32(width * 24) / 8, NULL, NULL)))
			return;
	gdk_cairo_set_source_pixbuf (cr, pixbuf, x, y);
	g_object_unref (pixbuf);
	cairo_paint (cr);
}

///////////////////////////////////////////////////////////////////////////////

static struct gtk_helper_i o_gtk_helper_i = {
	.paint_data     = gtk2_helper_paint_data,
};
struct gtk_helper_i *gtk2_helper_query_gtk_helper_i (struct gtk2_helper *this_) { return &o_gtk_helper_i; }
