/*
 *  @(#) $Id$
 *  Copyright (C) 2004 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

#include "config.h"
#include <string.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <libprocess/datafield.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwydgets/gwy3dwindow.h>
#include <libgwydgets/gwyoptionmenus.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwystock.h>

#define DEFAULT_SIZE 360

enum {
    N_BUTTONS = GWY_3D_MOVEMENT_LIGHT + 1
};

static void     gwy_3d_window_destroy              (GtkObject *object);
static void     gwy_3d_window_finalize             (GObject *object);
static GdkWindowEdge gwy_3d_window_get_grip_edge   (Gwy3DWindow *gwy3dwindow);
static void     gwy_3d_window_get_grip_rect        (Gwy3DWindow *gwy3dwindow,
                                                    GdkRectangle *rect);
static void     gwy_3d_window_set_grip_cursor      (Gwy3DWindow *gwy3dwindow);
static void     gwy_3d_window_create_resize_grip   (Gwy3DWindow *gwy3dwindow);
static void     gwy_3d_window_direction_changed    (GtkWidget *widget,
                                                    GtkTextDirection prev_dir);
static void     gwy_3d_window_destroy_resize_grip  (Gwy3DWindow *gwy3dwindow);
static gboolean gwy_3d_window_configure            (GtkWidget *widget,
                                                    GdkEventConfigure *event);
static void     gwy_3d_window_realize              (GtkWidget *widget);
static void     gwy_3d_window_unrealize            (GtkWidget *widget);
static void     gwy_3d_window_map                  (GtkWidget *widget);
static void     gwy_3d_window_unmap                (GtkWidget *widget);
static gboolean gwy_3d_window_button_press         (GtkWidget *widget,
                                                    GdkEventButton *event);
static gboolean gwy_3d_window_key_pressed          (GtkWidget *widget,
                                                    GdkEventKey *event);
static gboolean gwy_3d_window_expose               (GtkWidget *widget,
                                                    GdkEventExpose *event);
static void     gwy_3d_window_pack_buttons         (Gwy3DWindow *gwy3dwindow,
                                                    guint offset,
                                                    GtkBox *box);
static GtkWidget* gwy_3d_window_build_basic_tab    (Gwy3DWindow *window);
static GtkWidget* gwy_3d_window_build_visual_tab   (Gwy3DWindow *window);
static GtkWidget* gwy_3d_window_build_label_tab    (Gwy3DWindow *window);
static void     gwy_3d_window_set_mode             (gpointer userdata,
                                                    GtkWidget *button);
static void     gwy_3d_window_set_gradient         (GtkTreeSelection *selection,
                                                    Gwy3DWindow *gwy3dwindow);
static void     gwy_3d_window_set_material         (GtkTreeSelection *selection,
                                                    Gwy3DWindow *gwy3dwindow);
static void     gwy_3d_window_select_controls      (gpointer data,
                                                    GtkWidget *button);
static void     gwy_3d_window_set_labels           (GtkWidget *combo,
                                                    Gwy3DWindow *gwy3dwindow);
static void     gwy_3d_window_label_adj_changed    (GtkAdjustment *adj,
                                                    Gwy3DWindow *window);
static void     gwy_3d_window_projection_changed   (GtkToggleButton *check,
                                                    Gwy3DWindow *window);
static void     gwy_3d_window_show_axes_changed    (GtkToggleButton *check,
                                                    Gwy3DWindow *window);
static void     gwy_3d_window_show_labels_changed  (GtkToggleButton *check,
                                                    Gwy3DWindow *window);
static void     gwy_3d_window_display_mode_changed (GtkWidget *item,
                                                    Gwy3DWindow *window);
static void     gwy_3d_window_set_visualization    (Gwy3DWindow *window,
                                                    Gwy3DVisualization visual);
static void     gwy_3d_window_auto_scale_changed   (GtkToggleButton *check,
                                                    Gwy3DWindow *window);
static void     gwy_3d_window_labels_entry_activate(GtkEntry *entry,
                                                    Gwy3DWindow *window);
static void     gwy_3d_window_labels_reset_clicked (Gwy3DWindow *window);
static void     gwy_3d_window_set_tooltip          (GtkWidget *widget,
                                                    const gchar *tip_text);
static gboolean gwy_3d_window_view_clicked         (GtkWidget *gwy3dwindow,
                                                    GdkEventButton *event,
                                                    GtkWidget *gwy3dview);
static void     gwy_3d_window_visual_selected      (GtkWidget *item,
                                                    Gwy3DWindow *gwy3dwindow);
static void     gwy_3d_window_gradient_selected    (GtkWidget *item,
                                                    Gwy3DWindow *gwy3dwindow);
static void     gwy_3d_window_material_selected    (GtkWidget *item,
                                                    Gwy3DWindow *gwy3dwindow);

/* These are actually class data.  To put them to Class struct someone would
 * have to do class_ref() and live with this reference to the end of time. */
static GtkTooltips *tooltips = NULL;
static gboolean tooltips_set = FALSE;
static GQuark adj_property_quark = 0;

G_DEFINE_TYPE(Gwy3DWindow, gwy_3d_window, GTK_TYPE_WINDOW)

static void
gwy_3d_window_class_init(Gwy3DWindowClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
    GtkObjectClass *object_class;

    object_class = (GtkObjectClass*)klass;

    gobject_class->finalize = gwy_3d_window_finalize;
    object_class->destroy = gwy_3d_window_destroy;

    widget_class->realize = gwy_3d_window_realize;
    widget_class->unrealize = gwy_3d_window_unrealize;
    widget_class->map = gwy_3d_window_map;
    widget_class->unmap = gwy_3d_window_unmap;

    widget_class->configure_event = gwy_3d_window_configure;
    widget_class->expose_event = gwy_3d_window_expose;
    widget_class->button_press_event = gwy_3d_window_button_press;
    widget_class->key_press_event = gwy_3d_window_key_pressed;

    widget_class->direction_changed = gwy_3d_window_direction_changed;
}

static void
gwy_3d_window_init(G_GNUC_UNUSED Gwy3DWindow *gwy3dwindow)
{
    if (!tooltips_set && !tooltips) {
        tooltips = gtk_tooltips_new();
        g_object_ref(tooltips);
        gtk_object_sink(GTK_OBJECT(tooltips));
    }
}

static void
gwy_3d_window_finalize(GObject *object)
{
    Gwy3DWindow *gwy3dwindow;

    gwy3dwindow = GWY_3D_WINDOW(object);

    G_OBJECT_CLASS(gwy_3d_window_parent_class)->finalize(object);
}

static void
gwy_3d_window_destroy(GtkObject *object)
{
    Gwy3DWindow *gwy3dwindow;

    gwy3dwindow = GWY_3D_WINDOW(object);
    g_free(gwy3dwindow->buttons);
    gwy3dwindow->buttons = NULL;

    GTK_OBJECT_CLASS(gwy_3d_window_parent_class)->destroy(object);
}

static GdkWindowEdge
gwy_3d_window_get_grip_edge(Gwy3DWindow *gwy3dwindow)
{
    GtkWidget *widget = GTK_WIDGET(gwy3dwindow);

    if (gtk_widget_get_direction(widget) == GTK_TEXT_DIR_LTR)
        return GDK_WINDOW_EDGE_SOUTH_EAST;
    else
        return GDK_WINDOW_EDGE_SOUTH_WEST;
}

static void
gwy_3d_window_get_grip_rect(Gwy3DWindow *gwy3dwindow,
                            GdkRectangle *rect)
{
    GtkWidget *widget;
    gint w, h;

    widget = GTK_WIDGET(gwy3dwindow);

    /* These are in effect the max/default size of the grip. */
    w = 18;
    h = 18;

    if (w > widget->allocation.width)
        w = widget->allocation.width;

    if (h > widget->allocation.height - widget->style->ythickness)
        h = widget->allocation.height - widget->style->ythickness;

    rect->width = w;
    rect->height = h;
    rect->y = widget->allocation.y + widget->allocation.height - h;

    if (gtk_widget_get_direction(widget) == GTK_TEXT_DIR_LTR)
        rect->x = widget->allocation.x + widget->allocation.width - w;
    else
        rect->x = widget->allocation.x + widget->style->xthickness;
}

static void
gwy_3d_window_set_grip_cursor(Gwy3DWindow *gwy3dwindow)
{
    GtkWidget *widget = GTK_WIDGET(gwy3dwindow);
    GdkDisplay *display = gtk_widget_get_display(widget);
    GdkCursorType cursor_type;
    GdkCursor *cursor;

    if (gtk_widget_get_direction(widget) == GTK_TEXT_DIR_LTR)
        cursor_type = GDK_BOTTOM_RIGHT_CORNER;
    else
        cursor_type = GDK_BOTTOM_LEFT_CORNER;

    cursor = gdk_cursor_new_for_display(display, cursor_type);
    gdk_window_set_cursor(gwy3dwindow->resize_grip, cursor);
    gdk_cursor_unref(cursor);
}

static void
gwy_3d_window_create_resize_grip(Gwy3DWindow *gwy3dwindow)
{
    GtkWidget *widget;
    GdkWindowAttr attributes;
    gint attributes_mask;
    GdkRectangle rect;

    g_return_if_fail(GTK_WIDGET_REALIZED(gwy3dwindow));

    widget = GTK_WIDGET(gwy3dwindow);
    gwy_3d_window_get_grip_rect(gwy3dwindow, &rect);

    attributes.x = rect.x;
    attributes.y = rect.y;
    attributes.width = rect.width;
    attributes.height = rect.height;
    attributes.window_type = GDK_WINDOW_CHILD;
    attributes.wclass = GDK_INPUT_ONLY;
    attributes.event_mask = gtk_widget_get_events(widget)
                            | GDK_BUTTON_PRESS_MASK;
    attributes_mask = GDK_WA_X | GDK_WA_Y;

    gwy3dwindow->resize_grip = gdk_window_new(widget->window,
                                              &attributes, attributes_mask);
    gdk_window_set_user_data(gwy3dwindow->resize_grip, widget);
    gwy_3d_window_set_grip_cursor(gwy3dwindow);
}

static void
gwy_3d_window_direction_changed(GtkWidget *widget,
                                G_GNUC_UNUSED GtkTextDirection prev_dir)
{
    Gwy3DWindow *gwy3dwindow = GWY_3D_WINDOW(widget);

    gwy_3d_window_set_grip_cursor(gwy3dwindow);
    /* FIXME: must switch More/Less icons too */
}

static void
gwy_3d_window_destroy_resize_grip(Gwy3DWindow *gwy3dwindow)
{
    gdk_window_set_user_data(gwy3dwindow->resize_grip, NULL);
    gdk_window_destroy(gwy3dwindow->resize_grip);
    gwy3dwindow->resize_grip = NULL;
}

static gboolean
gwy_3d_window_configure(GtkWidget *widget,
                        GdkEventConfigure *event)
{
    GdkRectangle rect;
    Gwy3DWindow *gwy3dwindow = GWY_3D_WINDOW(widget);

    GTK_WIDGET_CLASS(gwy_3d_window_parent_class)->configure_event(widget,
                                                                  event);
    gwy_3d_window_get_grip_rect(gwy3dwindow, &rect);
    gdk_window_move(gwy3dwindow->resize_grip, rect.x, rect.y);

    return FALSE;
}

static void
gwy_3d_window_realize(GtkWidget *widget)
{
    GTK_WIDGET_CLASS(gwy_3d_window_parent_class)->realize(widget);
    gwy_3d_window_create_resize_grip(GWY_3D_WINDOW(widget));
}

static void
gwy_3d_window_unrealize(GtkWidget *widget)
{
    gwy_3d_window_destroy_resize_grip(GWY_3D_WINDOW(widget));
    GTK_WIDGET_CLASS(gwy_3d_window_parent_class)->unrealize(widget);
}

static void
gwy_3d_window_map(GtkWidget *widget)
{
    GTK_WIDGET_CLASS(gwy_3d_window_parent_class)->map(widget);
    gdk_window_show(GWY_3D_WINDOW(widget)->resize_grip);
}

static void
gwy_3d_window_unmap(GtkWidget *widget)
{
    gdk_window_hide(GWY_3D_WINDOW(widget)->resize_grip);
    GTK_WIDGET_CLASS(gwy_3d_window_parent_class)->unmap(widget);
}

static gboolean
gwy_3d_window_button_press(GtkWidget *widget,
                           GdkEventButton *event)
{
    Gwy3DWindow *gwy3dwindow;

    gwy3dwindow = GWY_3D_WINDOW(widget);
    if (event->type != GDK_BUTTON_PRESS
        || event->window != gwy3dwindow->resize_grip)
        return FALSE;

    if (event->button == 1)
        gtk_window_begin_resize_drag(GTK_WINDOW(widget),
                                     gwy_3d_window_get_grip_edge(gwy3dwindow),
                                     event->button,
                                     event->x_root, event->y_root, event->time);
    else if (event->button == 2)
        gtk_window_begin_move_drag(GTK_WINDOW(widget),
                                   event->button,
                                   event->x_root, event->y_root, event->time);
    else
        return FALSE;

    return TRUE;
}

static void
gwy_3d_window_copy_to_clipboard(Gwy3DWindow *gwy3dwindow)
{
    GtkClipboard *clipboard;
    GdkDisplay *display;
    GdkPixbuf *pixbuf;
    GdkAtom atom;

    display = gtk_widget_get_display(GTK_WIDGET(gwy3dwindow));
    atom = gdk_atom_intern("CLIPBOARD", FALSE);
    clipboard = gtk_clipboard_get_for_display(display, atom);
    pixbuf = gwy_3d_view_get_pixbuf(GWY_3D_VIEW(gwy3dwindow->gwy3dview));
    gtk_clipboard_set_image(clipboard, pixbuf);
    g_object_unref(pixbuf);
}

static gboolean
gwy_3d_window_key_pressed(GtkWidget *widget,
                          GdkEventKey *event)
{
    enum {
        important_mods = GDK_CONTROL_MASK | GDK_MOD1_MASK | GDK_RELEASE_MASK
    };
    Gwy3DWindow *gwy3dwindow;
    gboolean (*method)(GtkWidget*, GdkEventKey*);
    guint state, key;

    gwy_debug("state = %u, keyval = %u", event->state, event->keyval);
    gwy3dwindow = GWY_3D_WINDOW(widget);
    state = event->state & important_mods;
    key = event->keyval;
    /* TODO: it would be nice to have these working too
    if (!state && (key == GDK_minus || key == GDK_KP_Subtract))
        gwy_3d_view_window_set_zoom(3d_view_window, -1);
    else if (!state && (key == GDK_equal || key == GDK_KP_Equal
                        || key == GDK_plus || key == GDK_KP_Add))
        gwy_3d_view_window_set_zoom(3d_view_window, 1);
    else if (!state && (key == GDK_Z || key == GDK_z || key == GDK_KP_Divide))
        gwy_3d_view_window_set_zoom(3d_view_window, 10000);
    else */
    if (state == GDK_CONTROL_MASK && (key == GDK_C || key == GDK_c)) {
        gwy_3d_window_copy_to_clipboard(gwy3dwindow);
        return TRUE;
    }

    method = GTK_WIDGET_CLASS(gwy_3d_window_parent_class)->key_press_event;
    return method ? method(widget, event) : FALSE;
}

static gboolean
gwy_3d_window_expose(GtkWidget *widget,
                     GdkEventExpose *event)
{
    Gwy3DWindow *gwy3dwindow;
    GdkRectangle rect;

    gwy3dwindow = GWY_3D_WINDOW(widget);
    GTK_WIDGET_CLASS(gwy_3d_window_parent_class)->expose_event(widget, event);

    gwy_3d_window_get_grip_rect(gwy3dwindow, &rect);
    gtk_paint_resize_grip(widget->style,
                          widget->window,
                          GTK_WIDGET_STATE(widget),
                          NULL, widget, "gwy3dwindow",
                          gwy_3d_window_get_grip_edge(gwy3dwindow),
                          rect.x, rect.y, rect.width, rect.height);

    return FALSE;
}

static void
gwy_3d_window_pack_buttons(Gwy3DWindow *gwy3dwindow,
                           guint offset,
                           GtkBox *box)
{
    static struct {
        Gwy3DMovement mode;
        const gchar *stock_id;
        const gchar *tooltip;
    }
    const buttons[] = {
        {
            GWY_3D_MOVEMENT_ROTATION,
            GWY_STOCK_ROTATE,
            N_("Rotate view")
        },
        {
            GWY_3D_MOVEMENT_SCALE,
            GWY_STOCK_SCALE,
            N_("Scale view as a whole")
        },
        {
            GWY_3D_MOVEMENT_DEFORMATION,
            GWY_STOCK_SCALE_VERTICALLY,
            N_("Scale value range")
        },
        {
            GWY_3D_MOVEMENT_LIGHT,
            GWY_STOCK_LIGHT_ROTATE,
            N_("Move light source")
        },
    };
    GtkWidget *button;
    GtkRadioButton *group = NULL;
    guint i;

    for (i = 0; i < G_N_ELEMENTS(buttons); i++) {
        button = gtk_radio_button_new_from_widget(group);
        gtk_box_pack_start(GTK_BOX(box), button, FALSE, FALSE, 0);
        g_object_set(G_OBJECT(button), "draw-indicator", FALSE, NULL);
        gtk_container_add(GTK_CONTAINER(button),
                          gtk_image_new_from_stock(buttons[i].stock_id,
                                                   GTK_ICON_SIZE_LARGE_TOOLBAR));
        g_signal_connect_swapped(button, "clicked",
                                 G_CALLBACK(gwy_3d_window_set_mode),
                                 GINT_TO_POINTER(buttons[i].mode));
        g_object_set_data(G_OBJECT(button), "gwy3dwindow", gwy3dwindow);
        gwy_3d_window_set_tooltip(button, _(buttons[i].tooltip));
        gwy3dwindow->buttons[offset + buttons[i].mode] = button;
        if (!group)
            group = GTK_RADIO_BUTTON(button);
    }
}

/**
 * gwy_3d_window_new:
 * @gwy3dview: A #Gwy3DView containing the data-displaying widget to show.
 *
 * Creates a new OpenGL 3D data displaying window.
 *
 * Returns: A newly created widget, as #GtkWidget.
 **/
GtkWidget*
gwy_3d_window_new(Gwy3DView *gwy3dview)
{
    Gwy3DWindow *gwy3dwindow;
    GtkRequisition size_req;
    GtkWidget *vbox, *hbox, *hbox2, *button;
    GtkTextDirection direction;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), NULL);

    if (!adj_property_quark) {
        adj_property_quark
            = g_quark_from_static_string("gwy-3d-window-label-property-id");
    }

    gwy3dwindow = (Gwy3DWindow*)g_object_new(GWY_TYPE_3D_WINDOW, NULL);
    gtk_window_set_wmclass(GTK_WINDOW(gwy3dwindow), "data",
                           g_get_application_name());
    gtk_window_set_resizable(GTK_WINDOW(gwy3dwindow), TRUE);
    direction = gtk_widget_get_direction(GTK_WIDGET(gwy3dwindow));

    gwy3dwindow->buttons = g_new0(GtkWidget*, 2*N_BUTTONS);
    gwy3dwindow->in_update = FALSE;

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(gwy3dwindow), hbox);

    gwy3dwindow->gwy3dview = (GtkWidget*)gwy3dview;
    gtk_box_pack_start(GTK_BOX(hbox), gwy3dwindow->gwy3dview, TRUE, TRUE, 0);
    g_signal_connect_swapped(gwy3dwindow->gwy3dview, "button-press-event",
                             G_CALLBACK(gwy_3d_window_view_clicked),
                             gwy3dwindow);
    gwy_3d_view_set_movement_type(GWY_3D_VIEW(gwy3dwindow->gwy3dview),
                                  GWY_3D_MOVEMENT_ROTATION);

    /* Small toolbar */
    gwy3dwindow->vbox_small = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_end(GTK_BOX(hbox), gwy3dwindow->vbox_small, FALSE, FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(gwy3dwindow->vbox_small), 4);

    button = gtk_button_new();
    gtk_box_pack_start(GTK_BOX(gwy3dwindow->vbox_small), button,
                       FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(button),
                      gtk_image_new_from_stock(direction == GTK_TEXT_DIR_LTR
                                               ? GWY_STOCK_LESS
                                               : GWY_STOCK_MORE,
                                               GTK_ICON_SIZE_LARGE_TOOLBAR));
    gwy_3d_window_set_tooltip(button, _("Show full controls"));
    g_object_set_data(G_OBJECT(button), "gwy3dwindow", gwy3dwindow);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(gwy_3d_window_select_controls),
                             GINT_TO_POINTER(FALSE));

    gwy_3d_window_pack_buttons(gwy3dwindow, 0,
                               GTK_BOX(gwy3dwindow->vbox_small));

    /* Large toolbar */
    gwy3dwindow->vbox_large = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_end(GTK_BOX(hbox), gwy3dwindow->vbox_large, FALSE, FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(gwy3dwindow->vbox_large), 4);
    gtk_widget_set_no_show_all(gwy3dwindow->vbox_large, TRUE);

    hbox2 = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(gwy3dwindow->vbox_large), hbox2,
                       FALSE, FALSE, 0);

    button = gtk_button_new();
    gtk_box_pack_end(GTK_BOX(hbox2), button, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(button),
                      gtk_image_new_from_stock(direction == GTK_TEXT_DIR_LTR
                                               ? GWY_STOCK_MORE
                                               : GWY_STOCK_LESS,
                                               GTK_ICON_SIZE_LARGE_TOOLBAR));
    gwy_3d_window_set_tooltip(button, _("Hide full controls"));
    g_object_set_data(G_OBJECT(button), "gwy3dwindow", gwy3dwindow);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(gwy_3d_window_select_controls),
                             GINT_TO_POINTER(TRUE));

    gwy_3d_window_pack_buttons(gwy3dwindow, N_BUTTONS, GTK_BOX(hbox2));

    gwy3dwindow->notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(gwy3dwindow->vbox_large), gwy3dwindow->notebook,
                       TRUE, TRUE, 0);

    /* Basic table */
    vbox = gwy_3d_window_build_basic_tab(gwy3dwindow);
    gtk_notebook_append_page(GTK_NOTEBOOK(gwy3dwindow->notebook),
                             vbox, gtk_label_new(_("Basic")));

    /* Light & Material table */
    vbox = gwy_3d_window_build_visual_tab(gwy3dwindow);
    gtk_notebook_append_page(GTK_NOTEBOOK(gwy3dwindow->notebook),
                             vbox, gtk_label_new(_("Light & Material")));

    /* Labels table */
    vbox = gwy_3d_window_build_label_tab(gwy3dwindow);
    gtk_notebook_append_page(GTK_NOTEBOOK(gwy3dwindow->notebook),
                             vbox, gtk_label_new(_("Labels")));

    gtk_widget_show_all(hbox);

    /* make the 3D view at least DEFAULT_SIZE x DEFAULT_SIZE */
    gtk_widget_size_request(gwy3dwindow->vbox_large, &size_req);
    size_req.height = MAX(size_req.height, DEFAULT_SIZE);
    gtk_window_set_default_size(GTK_WINDOW(gwy3dwindow),
                                size_req.width/2 + size_req.height,
                                size_req.height);

    return GTK_WIDGET(gwy3dwindow);
}


static GtkWidget*
gwy_3d_window_build_basic_tab(Gwy3DWindow *window)
{
    Gwy3DView *view;
    GtkWidget *vbox, *spin, *table, *check;
    gint row;

    view = GWY_3D_VIEW(window->gwy3dview);

    vbox = gtk_vbox_new(FALSE, 0);

    table = gtk_table_new(7, 3, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(vbox), table, TRUE, TRUE, 0);
    row = 0;

    spin = gwy_table_attach_spinbutton
               (table, row++, _("_Phi:"), _("deg"),
                (GtkObject*)gwy_3d_view_get_rot_x_adjustment(view));
    spin = gwy_table_attach_spinbutton
               (table, row++, _("_Theta:"), _("deg"),
                (GtkObject*)gwy_3d_view_get_rot_y_adjustment(view));
    spin = gwy_table_attach_spinbutton
               (table, row++, _("_Scale:"), NULL,
                (GtkObject*)gwy_3d_view_get_view_scale_adjustment(view));
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 2);
    spin = gwy_table_attach_spinbutton
               (table, row++, _("_Value scale:"), NULL,
                (GtkObject*)gwy_3d_view_get_z_deformation_adjustment(view));
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 2);

    check = gtk_check_button_new_with_mnemonic(_("Show _axes"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check),
                                 gwy_3d_view_get_show_axes(view));
    gtk_table_attach(GTK_TABLE(table), check,
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    g_signal_connect(check, "toggled",
                     G_CALLBACK(gwy_3d_window_show_axes_changed), window);
    row++;

    check = gtk_check_button_new_with_mnemonic(_("Show _labels"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check),
                                 gwy_3d_view_get_show_labels(view));
    gtk_table_attach(GTK_TABLE(table), check,
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    g_signal_connect(check, "toggled",
                     G_CALLBACK(gwy_3d_window_show_labels_changed), window);
    row++;

    check = gtk_check_button_new_with_mnemonic(_("_Ortographic projection"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check),
                                 !gwy_3d_view_get_projection(view));
    gtk_table_attach(GTK_TABLE(table), check,
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    g_signal_connect(check, "toggled",
                     G_CALLBACK(gwy_3d_window_projection_changed), window);
    row++;

    return vbox;
}

static GtkWidget*
gwy_3d_window_build_visual_tab(Gwy3DWindow *window)
{
    static const GwyEnum display_modes[] = {
        { N_("_Lighting"),    GWY_3D_VISUALIZATION_LIGHTING },
        { N_("_Gradient"),    GWY_3D_VISUALIZATION_GRADIENT },
    };

    Gwy3DView *view;
    GtkWidget *vbox, *spin, *table, *menu, *label;
    Gwy3DVisualization visual;
    gint row;

    view = GWY_3D_VIEW(window->gwy3dview);

    vbox = gtk_vbox_new(FALSE, 0);

    table = gtk_table_new(4, 3, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(vbox), table, TRUE, TRUE, 0);
    row = 0;

    visual = gwy_3d_view_get_visualization(view);
    window->visual_mode_group
        = gwy_radio_buttons_create(display_modes, G_N_ELEMENTS(display_modes),
                                   G_CALLBACK(gwy_3d_window_display_mode_changed),
                                   window,
                                   visual);
    gtk_table_attach(GTK_TABLE(table),
                     GTK_WIDGET(window->visual_mode_group->data),
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    label = gtk_label_new_with_mnemonic(_("_Material:"));
    window->material_label = label;
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    gtk_widget_set_sensitive(label, visual);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    /* TODO: get selected from 3D view */
    menu = gwy_gl_material_selection_new(G_CALLBACK(gwy_3d_window_set_material),
                                         window, NULL);
    window->material_menu = menu;
    gtk_widget_set_sensitive(menu, visual == GWY_3D_VISUALIZATION_LIGHTING);
    gtk_table_attach(GTK_TABLE(table), menu,
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    spin = gwy_table_attach_spinbutton
                         (table, row++, _("Light _phi:"), _("deg"),
                          (GtkObject*)gwy_3d_view_get_light_z_adjustment(view));
    window->lights_spin1 = spin;
    gtk_widget_set_sensitive(spin, visual);

    spin = gwy_table_attach_spinbutton
                         (table, row++, _("Light _theta:"), _("deg"),
                          (GtkObject*)gwy_3d_view_get_light_y_adjustment(view));
    window->lights_spin2 = spin;
    gtk_widget_set_sensitive(spin, visual);
    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 12);
    gtk_widget_set_sensitive(window->buttons[GWY_3D_MOVEMENT_LIGHT],
                             visual);
    gtk_widget_set_sensitive(window->buttons[N_BUTTONS
                             + GWY_3D_MOVEMENT_LIGHT],
                             visual);
    row++;

    gtk_table_attach(GTK_TABLE(table),
                     GTK_WIDGET(window->visual_mode_group->next->data),
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    /* TODO: get selected from 3D view */
    menu = gwy_gradient_selection_new(G_CALLBACK(gwy_3d_window_set_gradient),
                                      window, NULL);
    gtk_widget_set_sensitive(menu, visual == GWY_3D_VISUALIZATION_GRADIENT);
    window->gradient_menu = menu;
    gtk_table_attach(GTK_TABLE(table), menu,
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    return vbox;
}

static GtkWidget*
gwy_3d_window_build_label_tab(Gwy3DWindow *window)
{
    static const GwyEnum label_entries[] = {
        { N_("X-axis"),          GWY_3D_VIEW_LABEL_X, },
        { N_("Y-axis"),          GWY_3D_VIEW_LABEL_Y, },
        { N_("Minimum z value"), GWY_3D_VIEW_LABEL_MIN, },
        { N_("Maximum z value"), GWY_3D_VIEW_LABEL_MAX, }
    };

    Gwy3DView *view;
    GtkWidget *vbox, *spin, *table, *combo, *label, *entry, *check, *button;
    Gwy3DLabel *gwy3dlabel;
    GtkObject *adj;
    gint row;

    view = GWY_3D_VIEW(window->gwy3dview);

    vbox = gtk_vbox_new(FALSE, 0);

    table = gtk_table_new(8, 3, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(vbox), table, TRUE, TRUE, 0);
    row = 0;

    combo = gwy_enum_combo_box_new(label_entries, G_N_ELEMENTS(label_entries),
                                   G_CALLBACK(gwy_3d_window_set_labels),
                                   window, -1, TRUE);
    gwy_table_attach_row(table, row, _("_Label:"), NULL, combo);
    window->labels_menu = combo;
    row++;


    label = gtk_label_new_with_mnemonic(_("_Text:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_FILL, 0, 0, 0);

    gwy3dlabel = gwy_3d_view_get_label(view, GWY_3D_VIEW_LABEL_X);
    entry = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(entry), 100);
    g_signal_connect(entry, "activate",
                     G_CALLBACK(gwy_3d_window_labels_entry_activate), window);
    gtk_entry_set_text(GTK_ENTRY(entry), gwy_3d_label_get_text(gwy3dlabel));
    gtk_editable_select_region(GTK_EDITABLE(entry),
                               0, GTK_ENTRY(entry)->text_length);

    gtk_table_attach(GTK_TABLE(table), entry,
                     1, 3, row, row+1, GTK_FILL, 0, 0, 0);
    window->labels_text = entry;
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    label = gtk_label_new(_("Move label"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    adj = gtk_adjustment_new(gwy3dlabel->delta_x,
                             -1000.0, 1000.0, 1.0, 10.0, 0.0);
    g_object_set_qdata(G_OBJECT(adj), adj_property_quark, "delta-x");
    spin = gwy_table_attach_spinbutton(table, row++, _("_Horizontally:"), "px",
                                       adj);
    window->labels_delta_x = spin;
    g_signal_connect(adj, "value-changed",
                     G_CALLBACK(gwy_3d_window_label_adj_changed), window);
    row++;

    adj = gtk_adjustment_new(gwy3dlabel->delta_y,
                             -1000.0, 1000.0, 1.0, 10.0, 0.0);
    g_object_set_qdata(G_OBJECT(adj), adj_property_quark, "delta-y");
    spin = gwy_table_attach_spinbutton(table, row++, _("_Vertically:"), "px",
                                       adj);
    window->labels_delta_y = spin;
    g_signal_connect(adj, "value-changed",
                     G_CALLBACK(gwy_3d_window_label_adj_changed), window);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    check = gtk_check_button_new_with_mnemonic(_("Scale size _automatically"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check),
                                 !gwy3dlabel->fixed_size);
    gtk_table_attach(GTK_TABLE(table), check,
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    window->labels_autosize = check;
    g_signal_connect(check, "toggled",
                     G_CALLBACK(gwy_3d_window_auto_scale_changed), window);
    row++;

    adj = gtk_adjustment_new(gwy3dlabel->size, 1.0, 100.0, 1.0, 10.0, 0.0);
    g_object_set_qdata(G_OBJECT(adj), adj_property_quark, "size");
    spin = gwy_table_attach_spinbutton(table, row++, _("Si_ze:"), _("pixels"),
                                       adj);
    gtk_widget_set_sensitive(spin, gwy3dlabel->fixed_size);
    window->labels_size = spin;
    g_signal_connect(adj, "value-changed",
                     G_CALLBACK(gwy_3d_window_label_adj_changed), window);
    row++;

    button = gtk_button_new_with_mnemonic(_("_Reset"));
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(gwy_3d_window_labels_reset_clicked),
                             window);
    gtk_table_attach(GTK_TABLE(table), button,
                     0, 1, row, row+1, GTK_FILL, 0, 0, 0);
    window->actions = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(window->vbox_large), window->actions,
                       FALSE, FALSE, 0);

    return vbox;
}

/**
 * gwy_3d_window_get_3d_view:
 * @gwy3dwindow: A 3D data view window.
 *
 * Returns the #Gwy3DView widget this 3D window currently shows.
 *
 * Returns: The currently shown #GwyDataView.
 **/
GtkWidget*
gwy_3d_window_get_3d_view(Gwy3DWindow *gwy3dwindow)
{
    g_return_val_if_fail(GWY_IS_3D_WINDOW(gwy3dwindow), NULL);

    return gwy3dwindow->gwy3dview;
}

/**
 * gwy_3d_window_add_action_widget:
 * @gwy3dwindow: A 3D data view window.
 * @widget: A widget to pack into the action area.
 *
 * Adds a widget (usually a button) to 3D window action area.
 *
 * The action area is located under the parameter notebook.
 **/
void
gwy_3d_window_add_action_widget(Gwy3DWindow *gwy3dwindow,
                                GtkWidget *widget)
{
    g_return_if_fail(GWY_IS_3D_WINDOW(gwy3dwindow));
    g_return_if_fail(GTK_IS_WIDGET(widget));

    gtk_box_pack_start(GTK_BOX(gwy3dwindow->actions), widget,
                       FALSE, FALSE, 0);
}

/**
 * gwy_3d_window_add_small_toolbar_button:
 * @gwy3dwindow: A 3D data view window.
 * @stock_id: Button pixmap stock id, like #GTK_STOCK_SAVE.
 * @tooltip: Button tooltip.
 * @callback: Callback action for "clicked" signal.  It is connected swapped,
 *            that is it gets @cbdata as its first argument, the clicked button
 *            as the last.
 * @cbdata: Data to pass to @callback.
 *
 * Adds a button to small @gwy3dwindow toolbar.
 *
 * The small toolbar is those visible when full controls are hidden.  Due to
 * space constraints the button must be contain only a pixmap.
 **/
void
gwy_3d_window_add_small_toolbar_button(Gwy3DWindow *gwy3dwindow,
                                       const gchar *stock_id,
                                       const gchar *tooltip,
                                       GCallback callback,
                                       gpointer cbdata)
{
    GtkWidget *button;

    g_return_if_fail(GWY_IS_3D_WINDOW(gwy3dwindow));
    g_return_if_fail(stock_id);

    button = gtk_button_new();
    gtk_box_pack_start(GTK_BOX(gwy3dwindow->vbox_small), button,
                       FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(button),
                      gtk_image_new_from_stock(stock_id,
                                               GTK_ICON_SIZE_LARGE_TOOLBAR));
    gwy_3d_window_set_tooltip(button, tooltip);
    g_object_set_data(G_OBJECT(button), "gwy3dwindow", gwy3dwindow);
    g_signal_connect_swapped(button, "clicked", G_CALLBACK(callback), cbdata);
}

/**
 * gwy_3d_window_class_set_tooltips:
 * @tips: Tooltips object #Gwy3DWindow's should use for setting tooltips.
 *        A %NULL value disables tooltips altogether.
 *
 * Sets the tooltips object to use for adding tooltips to 3D window parts.
 *
 * This is a class method.  It affects only newly cerated 3D windows, existing
 * 3D windows will continue to use the tooltips they were constructed with.
 *
 * If no class tooltips object is set before first #Gwy3DWindow is created,
 * the class instantiates one on its own.  You can normally obtain it with
 * gwy_3d_window_class_get_tooltips() then.  The class takes a reference on
 * the tooltips in either case.
 **/
void
gwy_3d_window_class_set_tooltips(GtkTooltips *tips)
{
    g_return_if_fail(!tips || GTK_IS_TOOLTIPS(tips));

    if (tips) {
        g_object_ref(tips);
        gtk_object_sink(GTK_OBJECT(tips));
    }
    gwy_object_unref(tooltips);
    tooltips = tips;
    tooltips_set = TRUE;
}

/**
 * gwy_3d_window_class_get_tooltips:
 *
 * Gets the tooltips object used for adding tooltips to 3D window parts.
 *
 * Returns: The #GtkTooltips object.
 **/
GtkTooltips*
gwy_3d_window_class_get_tooltips(void)
{
    return tooltips;
}

static void
gwy_3d_window_set_mode(gpointer userdata, GtkWidget *button)
{
    Gwy3DWindow *gwy3dwindow;
    Gwy3DMovement movement;

    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
        return;

    gwy3dwindow = (Gwy3DWindow*)g_object_get_data(G_OBJECT(button),
                                                  "gwy3dwindow");
    g_return_if_fail(GWY_IS_3D_WINDOW(gwy3dwindow));
    if (gwy3dwindow->in_update)
        return;

    gwy3dwindow->in_update = TRUE;
    movement = GPOINTER_TO_INT(userdata);
    gtk_toggle_button_set_active
        (GTK_TOGGLE_BUTTON(gwy3dwindow->buttons[movement]), TRUE);
    gtk_toggle_button_set_active
        (GTK_TOGGLE_BUTTON(gwy3dwindow->buttons[movement + N_BUTTONS]), TRUE);

    gwy_3d_view_set_movement_type(GWY_3D_VIEW(gwy3dwindow->gwy3dview),
                                  movement);
    gwy3dwindow->in_update = FALSE;
}

static void
gwy_3d_window_set_gradient(GtkTreeSelection *selection,
                           Gwy3DWindow *gwy3dwindow)
{
    Gwy3DView *view;
    GwyResource *resource;
    GtkTreeModel *model;
    GtkTreeIter iter;
    const gchar *name;

    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gtk_tree_model_get(model, &iter, 0, &resource, -1);
        view = GWY_3D_VIEW(gwy3dwindow->gwy3dview);
        name = gwy_resource_get_name(resource);
        gwy_container_set_string_by_name(gwy_3d_view_get_data(view),
                                         gwy_3d_view_get_gradient_key(view),
                                         g_strdup(name));
    }
}

static void
gwy_3d_window_set_material(GtkTreeSelection *selection,
                           Gwy3DWindow *gwy3dwindow)
{
    Gwy3DView *view;
    GwyResource *resource;
    GtkTreeModel *model;
    GtkTreeIter iter;
    const gchar *name;

    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gtk_tree_model_get(model, &iter, 0, &resource, -1);
        view = GWY_3D_VIEW(gwy3dwindow->gwy3dview);
        name = gwy_resource_get_name(resource);
        gwy_container_set_string_by_name(gwy_3d_view_get_data(view),
                                         gwy_3d_view_get_material_key(view),
                                         g_strdup(name));
    }
}

static void
gwy_3d_window_select_controls(gpointer data, GtkWidget *button)
{
    Gwy3DWindow *gwy3dwindow;
    GtkWidget *show, *hide;

    gwy3dwindow = (Gwy3DWindow*)g_object_get_data(G_OBJECT(button),
                                                  "gwy3dwindow");
    g_return_if_fail(GWY_IS_3D_WINDOW(gwy3dwindow));

    show = data ? gwy3dwindow->vbox_small : gwy3dwindow->vbox_large;
    hide = data ? gwy3dwindow->vbox_large : gwy3dwindow->vbox_small;
    gtk_widget_hide(hide);
    gtk_widget_set_no_show_all(hide, TRUE);
    gtk_widget_set_no_show_all(show, FALSE);
    gtk_widget_show_all(show);
}

static void
gwy_3d_window_set_labels(GtkWidget *combo,
                         Gwy3DWindow *gwy3dwindow)
{
    gint id;
    Gwy3DLabel *label;

    id = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
    label = gwy_3d_view_get_label(GWY_3D_VIEW(gwy3dwindow->gwy3dview), id);
    g_return_if_fail(label);

    gtk_entry_set_text(GTK_ENTRY(gwy3dwindow->labels_text),
                       gwy_3d_label_get_text(label));
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(gwy3dwindow->labels_delta_x),
                              label->delta_x);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(gwy3dwindow->labels_delta_y),
                              label->delta_y);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(gwy3dwindow->labels_size),
                              label->size);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gwy3dwindow->labels_autosize),
                                 !label->fixed_size);
}

static void
gwy_3d_window_label_adj_changed(GtkAdjustment *adj,
                                Gwy3DWindow *window)
{
    Gwy3DLabel *label;
    const gchar *key;
    gdouble oldval, newval;
    gint id;

    id = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(window->labels_menu));
    label = gwy_3d_view_get_label(GWY_3D_VIEW(window->gwy3dview), id);
    key = g_object_get_qdata(G_OBJECT(adj), adj_property_quark);
    g_object_get(label, key, &oldval, NULL);
    newval = gtk_adjustment_get_value(adj);
    if (oldval != newval)
        g_object_set(label, key, newval, NULL);
}

static void
gwy_3d_window_projection_changed(GtkToggleButton *check,
                                 Gwy3DWindow *window)
{
    gwy_3d_view_set_projection(GWY_3D_VIEW(window->gwy3dview),
                               gtk_toggle_button_get_active(check)
                               ? GWY_3D_PROJECTION_ORTHOGRAPHIC
                               : GWY_3D_PROJECTION_PERSPECTIVE);
}

static void
gwy_3d_window_show_axes_changed(GtkToggleButton *check,
                                Gwy3DWindow *window)
{
    gwy_3d_view_set_show_axes(GWY_3D_VIEW(window->gwy3dview),
                              gtk_toggle_button_get_active(check));
}

static void
gwy_3d_window_show_labels_changed(GtkToggleButton *check,
                                  Gwy3DWindow *window)
{
    gwy_3d_view_set_show_labels(GWY_3D_VIEW(window->gwy3dview),
                                gtk_toggle_button_get_active(check));
}

static void
gwy_3d_window_display_mode_changed(GtkWidget *item,
                                   Gwy3DWindow *window)
{
    Gwy3DVisualization visual;
    GSList *list;

    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(item)))
        return;

    list = gtk_radio_button_get_group(GTK_RADIO_BUTTON(item));
    visual = gwy_radio_buttons_get_current(list);
    gwy_3d_window_set_visualization(window, visual);
}

static void
gwy_3d_window_set_visualization(Gwy3DWindow *window,
                                Gwy3DVisualization visual)
{
    gwy_3d_view_set_visualization(GWY_3D_VIEW(window->gwy3dview), visual);
    gtk_widget_set_sensitive(window->material_menu, visual);
    gtk_widget_set_sensitive(window->material_label, visual);
    gtk_widget_set_sensitive(window->gradient_menu, !visual);
    gtk_widget_set_sensitive(window->lights_spin1, visual);
    gtk_widget_set_sensitive(window->lights_spin2, visual);
    gtk_widget_set_sensitive(window->buttons[GWY_3D_MOVEMENT_LIGHT], visual);
    gtk_widget_set_sensitive(window->buttons[N_BUTTONS + GWY_3D_MOVEMENT_LIGHT],
                             visual);
    if (visual == GWY_3D_VISUALIZATION_GRADIENT
        && gwy_3d_view_get_movement_type(GWY_3D_VIEW(window->gwy3dview))
            == GWY_3D_MOVEMENT_LIGHT)
        g_signal_emit_by_name(window->buttons[GWY_3D_MOVEMENT_ROTATION],
                              "clicked");
}

static void
gwy_3d_window_auto_scale_changed(GtkToggleButton *check,
                                 Gwy3DWindow *window)
{
    Gwy3DLabel *label;
    gboolean active;
    gint id;

    active = gtk_toggle_button_get_active(check);
    gtk_widget_set_sensitive(window->labels_size, !active);

    id = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(window->labels_menu));
    label = gwy_3d_view_get_label(GWY_3D_VIEW(window->gwy3dview), id);
    if (!label->fixed_size != active)
        g_object_set(label, "fixed-size", !active, NULL);
}

static void
gwy_3d_window_labels_entry_activate(GtkEntry *entry,
                                    Gwy3DWindow *window)
{
    Gwy3DLabel *label;
    gint id;

    id = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(window->labels_menu));
    label = gwy_3d_view_get_label(GWY_3D_VIEW(window->gwy3dview), id);
    gwy_3d_label_set_text(label, gtk_entry_get_text(entry));
}

static void
gwy_3d_window_labels_reset_clicked(Gwy3DWindow *window)
{
    Gwy3DLabel *label;
    gint id;

    id = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(window->labels_menu));
    label = gwy_3d_view_get_label(GWY_3D_VIEW(window->gwy3dview), id);
    /* TODO: This is not reflected by the controls now */
    gwy_3d_label_reset(label);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(window->labels_autosize),
                                 TRUE);
}

static void
gwy_3d_window_set_tooltip(GtkWidget *widget,
                          const gchar *tip_text)
{
    if (tooltips)
        gtk_tooltips_set_tip(tooltips, widget, tip_text, NULL);
}

static gboolean
gwy_3d_window_view_clicked(GtkWidget *gwy3dwindow,
                           GdkEventButton *event,
                           GtkWidget *gwy3dview)
{
    Gwy3DVisualization visual;
    GtkWidget *menu, *item;

    if (event->button != 3)
        return FALSE;

    switch (gwy_3d_view_get_visualization(GWY_3D_VIEW(gwy3dview))) {
        case GWY_3D_VISUALIZATION_GRADIENT:
        menu = gwy_menu_gradient(G_CALLBACK(gwy_3d_window_gradient_selected),
                                 gwy3dwindow);
        item = gtk_menu_item_new_with_mnemonic(_("S_witch to Lighting Mode"));
        visual = GWY_3D_VISUALIZATION_LIGHTING;
        break;


        case GWY_3D_VISUALIZATION_LIGHTING:
        menu = gwy_menu_gl_material(G_CALLBACK(gwy_3d_window_material_selected),
                                    gwy3dwindow);
        item = gtk_menu_item_new_with_mnemonic(_("S_witch to Color Gradient Mode"));
        visual = GWY_3D_VISUALIZATION_GRADIENT;
        break;

        default:
        g_return_val_if_reached(FALSE);
        break;
    }
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    g_object_set_data(G_OBJECT(item), "display-mode", GINT_TO_POINTER(visual));
    g_signal_connect(item, "activate",
                     G_CALLBACK(gwy_3d_window_visual_selected), gwy3dwindow);

    gtk_widget_show_all(menu);
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
                   event->button, event->time);
    g_signal_connect(menu, "selection-done",
                     G_CALLBACK(gtk_widget_destroy), NULL);

    return FALSE;
}

static void
gwy_3d_window_visual_selected(GtkWidget *item,
                              Gwy3DWindow *gwy3dwindow)
{
    Gwy3DVisualization visual;

    visual = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "display-mode"));
    gwy_radio_buttons_set_current(gwy3dwindow->visual_mode_group, visual);
}

static void
gwy_3d_window_gradient_selected(GtkWidget *item,
                                Gwy3DWindow *gwy3dwindow)
{
    Gwy3DView *view;
    const gchar *name;

    name = g_object_get_data(G_OBJECT(item), "gradient-name");
    gwy_gradient_selection_set_active(gwy3dwindow->gradient_menu, name);
    /* FIXME: Double update if tree view is visible. Remove once selection
     * buttons can emit signals. */
    view = GWY_3D_VIEW(gwy3dwindow->gwy3dview);
    gwy_container_set_string_by_name(gwy_3d_view_get_data(view),
                                     gwy_3d_view_get_gradient_key(view),
                                     g_strdup(name));
}

static void
gwy_3d_window_material_selected(GtkWidget *item,
                                Gwy3DWindow *gwy3dwindow)
{
    Gwy3DView *view;
    const gchar *name;

    name = g_object_get_data(G_OBJECT(item), "gl-material-name");
    gwy_gl_material_selection_set_active(gwy3dwindow->material_menu, name);
    /* FIXME: Double update if tree view is visible. Remove once selection
     * buttons can emit signals. */
    view = GWY_3D_VIEW(gwy3dwindow->gwy3dview);
    gwy_container_set_string_by_name(gwy_3d_view_get_data(view),
                                     gwy_3d_view_get_material_key(view),
                                     g_strdup(name));
}

/************************** Documentation ****************************/

/**
 * SECTION:gwy3dwindow
 * @title: Gwy3DWindow
 * @short_description: 3D data display window
 * @see_also: #Gwy3DView -- the basic 3D data display widget
 *
 * #Gwy3DWindow encapsulates a #Gwy3DView together with appropriate controls.
 * You can create a 3D window for a 3D view with gwy_3d_window_new(). It has an
 * `action area' below the controls where additional widgets can be packed with
 * gwy_3d_window_add_action_widget().
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
