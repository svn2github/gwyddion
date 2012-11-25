/*
 *  $Id$
 *  Copyright (C) 2012 David Nečas (Yeti).
 *  E-mail: yeti@gwyddion.net.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <cairo/cairo-xlib.h>
#include <gtk/gtk.h>
#include <libgwy/libgwy.h>
#include <libgwyui/libgwyui.h>

#define SMALL_WIDTH  240
#define SMALL_HEIGHT 75
#define MEDIUM_WIDTH 240
#define MEDIUM_HEIGHT 165
#define LARGE_WIDTH 240
#define LARGE_HEIGHT 240

typedef GtkWidget* (*WidgetCreatorFunc)(void);

static gboolean save(gpointer user_data);
static gboolean next(gpointer user_data);

GSList *ctors = NULL;
GtkWidget *dummywindow;

static gboolean
save(gpointer user_data)
{
    GtkOffscreenWindow *offscrwindow = GTK_OFFSCREEN_WINDOW(user_data);
    GtkWidget *whatever = gtk_bin_get_child(GTK_BIN(offscrwindow));
    const gchar *name = G_OBJECT_TYPE_NAME(whatever);
    cairo_surface_t *xlibsurface
        = gtk_offscreen_window_get_surface(GTK_OFFSCREEN_WINDOW(offscrwindow));
    cairo_surface_type_t surftype = cairo_surface_get_type(xlibsurface);
    if (surftype != CAIRO_SURFACE_TYPE_XLIB) {
        g_warning("Cairo surface is of type %u instead of %u.\n",
                  surftype, CAIRO_SURFACE_TYPE_XLIB);
        return FALSE;
    }
    guint w = cairo_xlib_surface_get_width(xlibsurface);
    guint h = cairo_xlib_surface_get_height(xlibsurface);
    cairo_surface_t *pngsurface = cairo_image_surface_create(CAIRO_FORMAT_RGB24,
                                                             w, h);

    cairo_t *cr = cairo_create(pngsurface);
    cairo_set_source_surface(cr, xlibsurface, 0.0, 0.0);
    cairo_paint(cr);

    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.5);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_BEVEL);
    cairo_set_line_width(cr, 1.0);
    cairo_rectangle(cr, 0.5, 0.5, w-1.0, h-1.0);
    cairo_stroke(cr);

    gchar *filename = g_strconcat("images/", name, ".png", NULL);
    cairo_status_t status = cairo_surface_write_to_png(pngsurface, filename);
    g_printerr("%s: %d (%s)\n",
               filename, status, cairo_status_to_string(status));
    g_free(filename);
    cairo_destroy(cr);
    cairo_surface_destroy(pngsurface);
    g_idle_add(next, offscrwindow);
    return FALSE;
}

static gboolean
widget_drawn(GtkWidget *offscrwindow)
{
    g_idle_add(save, offscrwindow);
    return FALSE;
}

static gboolean
next(gpointer user_data)
{
    GtkWidget *offscrwindow = (GtkWidget*)user_data;
    if (!ctors) {
        gtk_widget_destroy(offscrwindow);
        gtk_widget_destroy(dummywindow);
        return FALSE;
    }

    GtkWidget *widget = gtk_bin_get_child(GTK_BIN(offscrwindow));
    if (widget)
        gtk_widget_destroy(widget);

    WidgetCreatorFunc ctor = (WidgetCreatorFunc)ctors->data;
    g_assert(ctor);
    widget = ctor();

    g_assert(GTK_IS_WIDGET(widget));
    gtk_container_add(GTK_CONTAINER(offscrwindow), widget);
    gtk_widget_show(widget);
    gtk_widget_queue_draw(offscrwindow);
    g_signal_connect_swapped(widget, "draw",
                             G_CALLBACK(widget_drawn), offscrwindow);

    ctors = ctors->next;

    return FALSE;
}

static GtkWidget*
create_color_axis(void)
{
    GtkWidget *widget = g_object_new(GWY_TYPE_COLOR_AXIS,
                                     "edge", GTK_POS_RIGHT,
                                     "snap-to-ticks", FALSE,
                                     "ticks-at-edges", TRUE,
                                     "max-tick-level", GWY_AXIS_TICK_MINOR,
                                     "gradient", gwy_gradients_get("Sky"),
                                     NULL);
    GwyAxis *axis = GWY_AXIS(widget);
    gwy_unit_set_from_string(gwy_axis_get_unit(axis), "A", NULL);
    gwy_axis_request_range(axis, &(GwyRange){ 0.0, 53e-9 });
    gtk_widget_set_size_request(widget, -1, MEDIUM_HEIGHT);
    return widget;
}

static GtkWidget*
create_ruler(void)
{
    GtkWidget *widget = g_object_new(GWY_TYPE_RULER,
                                     "edge", GTK_POS_TOP,
                                     "snap-to-ticks", FALSE,
                                     "ticks-at-edges", FALSE,
                                     "max-tick-level", GWY_AXIS_TICK_MICRO,
                                     NULL);
    GwyAxis *axis = GWY_AXIS(widget);
    gwy_unit_set_from_string(gwy_axis_get_unit(axis), "m", NULL);
    gwy_axis_request_range(axis, &(GwyRange){ -2.5e-6, 7.8e-6 });
    gtk_widget_set_size_request(widget, LARGE_WIDTH, -1);
    return widget;
}

int
main(int argc, char *argv[])
{
    gtk_init(&argc, &argv);
    gwy_resource_type_load(GWY_TYPE_GRADIENT, NULL);

    ctors = g_slist_prepend(ctors, create_color_axis);
    ctors = g_slist_prepend(ctors, create_ruler);

    dummywindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_show(dummywindow);
    g_signal_connect(dummywindow, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *offscrwindow = gtk_offscreen_window_new();
    gtk_container_set_border_width(GTK_CONTAINER(offscrwindow), 8);
    gtk_widget_show(offscrwindow);
    g_idle_add(next, offscrwindow);

    gtk_main();

    return 0;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
