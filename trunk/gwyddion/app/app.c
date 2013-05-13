/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2006,2013 David Necas (Yeti), Petr Klapetek.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "config.h"
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include <libgwyddion/gwymacros.h>
#include <libprocess/datafield.h>
#include <libprocess/arithmetic.h>
#include <libprocess/gwygrainvalue.h>
#include <libprocess/gwycalibration.h>
#include <libgwymodule/gwymodule.h>
#include <libgwydgets/gwydgets.h>
#include <libgwydgets/gwygraphwindow.h>
#include <app/gwyapp.h>
#include "gwyappinternal.h"

enum {
    ITEM_PIXELSQUARE,
    ITEM_REALSQUARE
};

typedef enum {
    BRICK_PREVIEW_MEAN,
    BRICK_PREVIEW_MINIMUM,
    BRICK_PREVIEW_MAXIMUM,
    BRICK_PREVIEW_RMS,
    BRICK_PREVIEW_CHANNEL,
    BRICK_PREVIEW_SECTION,
} BrickPreviewType;

static GtkWidget *gwy_app_main_window = NULL;

static GwyTool* current_tool = NULL;
static GQuark corner_item_quark = 0;

static gboolean   gwy_app_main_window_save_position   (void);
static void       gwy_app_main_window_restore_position(void);
static gboolean   gwy_app_confirm_quit                (void);
static gboolean   gwy_app_confirm_quit_dialog         (GSList *unsaved);
static GtkWidget* gwy_app_menu_data_popup_create      (GtkAccelGroup *accel_group);
static GtkWidget* gwy_app_menu_data_corner_create     (GtkAccelGroup *accel_group);
static void       gwy_app_data_window_change_square   (GtkWidget *item,
                                                       gpointer user_data);
static gboolean   gwy_app_data_corner_menu_popup_mouse(GtkWidget *menu,
                                                       GdkEventButton *event,
                                                       GtkWidget *ebox);
static gboolean   gwy_app_data_popup_menu_popup_mouse (GtkWidget *menu,
                                                       GdkEventButton *event,
                                                       GwyDataView *data_view);
static void       gwy_app_data_popup_menu_popup_key   (GtkWidget *menu,
                                                       GtkWidget *data_window);
static gboolean   gwy_app_graph_popup_menu_popup_mouse(GtkWidget *menu,
                                                       GdkEventButton *event,
                                                       GwyGraph *graph);
static void       gwy_app_graph_popup_menu_popup_key  (GtkWidget *menu,
                                                       GtkWidget *graph);
static void       gwy_app_3d_window_export            (Gwy3DWindow *window);
static void       gwy_app_3d_window_set_defaults      (Gwy3DWindow *window);
static void       change_brick_preview                (GwyDataWindow *data_window);
static GtkWidget* gwy_app_menu_brick_popup_create     (GtkAccelGroup *accel_group);
static gboolean   gwy_app_brick_popup_menu_popup_mouse(GtkWidget *menu,
                                                       GdkEventButton *event,
                                                       GwyDataView *data_view);
static void       gwy_app_brick_popup_menu_popup_key  (GtkWidget *menu,
                                                       GtkWidget *data_window);
static void       gwy_app_save_3d_export              (GtkWidget *dialog,
                                                       gint response,
                                                       Gwy3DWindow *gwy3dwindow);
static void       gwy_app_3d_window_add_overlay_menu  (Gwy3DWindow *gwy3dwindow);
static void       gwy_app_3d_window_update_chooser    (Gwy3DWindow *gwy3dwindow);
static void       gwy_app_3d_window_set_data2         (Gwy3DWindow *gwy3dwindow,
                                                       gint id,
                                                       gboolean mask);
static gboolean   gwy_app_3d_window_data2_filter      (GwyContainer *data2,
                                                       gint id2,
                                                       gpointer user_data);
static void       gwy_app_data_window_reset_zoom      (void);
static void       gwy_app_volume_window_reset_zoom    (void);
static void       metadata_browser                    (gpointer pwhat);
static void       gwy_app_change_mask_color           (void);

/*****************************************************************************
 *                                                                           *
 *     Main, toolbox                                                         *
 *                                                                           *
 *****************************************************************************/

/**
 * gwy_app_quit:
 *
 * Quits the application.
 *
 * This function may present a confirmation dialog to the user and it may
 * let the application to continue running.  If it quits the application,
 * it performs some shutdown actions and then quits the Gtk+ main loop with
 * gtk_main_quit().
 *
 * Returns: Always %TRUE to be usable as an event handler.  However, if the
 *          application is actually terminated, this function does not return.
 **/
gboolean
gwy_app_quit(void)
{
    gwy_debug("");
    if (!gwy_app_confirm_quit())
        return TRUE;

    gwy_app_data_browser_shut_down();
    gwy_app_main_window_save_position();
    gwy_object_unref(current_tool);
    /* XXX: EXIT-CLEAN-UP */
    gtk_widget_destroy(gwy_app_main_window);
    /* FIXME: sometimes fails with
     * Sensitivity group is finialized when it still contains widget lists.
     */
    g_object_unref(gwy_app_sensitivity_get_group());

    gtk_main_quit();
    return TRUE;
}

static gboolean
gwy_app_main_window_save_position(void)
{
    gwy_app_save_window_position(GTK_WINDOW(gwy_app_main_window),
                                 "/app/toolbox", TRUE, FALSE);
    return FALSE;
}

static void
gwy_app_main_window_restore_position(void)
{
    gwy_app_restore_window_position(GTK_WINDOW(gwy_app_main_window),
                                    "/app/toolbox", FALSE);
}

/**
 * gwy_app_add_main_accel_group:
 * @window: A window.
 *
 * Adds main (global) application accelerator group to a window.
 *
 * This includes accelerators for terminating Gwyddion, opening files, etc.
 **/
void
gwy_app_add_main_accel_group(GtkWindow *window)
{
    GtkWidget *main_window;
    GtkAccelGroup *accel_group;

    g_return_if_fail(GTK_IS_WINDOW(window));
    main_window = gwy_app_main_window_get();
    if (!main_window)
        return;

    g_return_if_fail(GTK_IS_WINDOW(main_window));

    accel_group = GTK_ACCEL_GROUP(g_object_get_data(G_OBJECT(main_window),
                                                    "accel_group"));
    if (accel_group)
        gtk_window_add_accel_group(window, accel_group);
}

/**
 * gwy_app_main_window_get:
 *
 * Returns Gwyddion main application window (toolbox).
 *
 * Returns: The Gwyddion toolbox.
 **/
GtkWidget*
gwy_app_main_window_get(void)
{
    return gwy_app_main_window;
}

/**
 * gwy_app_main_window_set:
 * @window: A window.
 *
 * Sets Gwyddion main application window (toolbox) for
 * gwy_app_main_window_get().
 *
 * This function can be called only once and should be called at Gwyddion
 * startup so, ignore it.
 **/
void
gwy_app_main_window_set(GtkWidget *window)
{
    g_return_if_fail(GTK_IS_WINDOW(window));
    if (gwy_app_main_window && window != gwy_app_main_window) {
        g_critical("Trying to change app main window");
        return;
    }

    gwy_app_main_window = window;
    gwy_app_main_window_restore_position();
    g_signal_connect(window, "delete-event",
                     G_CALLBACK(gwy_app_main_window_save_position), NULL);
    g_signal_connect(window, "show",
                     G_CALLBACK(gwy_app_main_window_restore_position), NULL);
}

static gboolean
gwy_app_confirm_quit(void)
{
    GSList *unsaved = NULL;
    gboolean ok G_GNUC_UNUSED;

    if (!unsaved)
        return TRUE;
    ok = gwy_app_confirm_quit_dialog(unsaved);
    g_slist_free(unsaved);

    return TRUE;
}

static gboolean
gwy_app_confirm_quit_dialog(GSList *unsaved)
{
    GtkWidget *dialog;
    gchar *text;
    gint response;

    text = NULL;
    while (unsaved) {
        GwyDataWindow *data_window = GWY_DATA_WINDOW(unsaved->data);
        /* TODO: must use filename, not channel name, undo is per-file */
        const gchar *filename = gwy_data_window_get_data_name(data_window);

        text = g_strconcat(filename, "\n", text, NULL);
        unsaved = g_slist_next(unsaved);
    }
    dialog = gtk_message_dialog_new(GTK_WINDOW(gwy_app_main_window_get()),
                                    GTK_DIALOG_MODAL,
                                    GTK_MESSAGE_QUESTION,
                                    GTK_BUTTONS_YES_NO,
                                    _("Some data are unsaved:\n"
                                      "%s\n"
                                      "Really quit?"),
                                    text);
    g_free(text);

    gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
    gtk_window_present(GTK_WINDOW(dialog));
    response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    return response == GTK_RESPONSE_YES;
}

/*****************************************************************************
 *                                                                           *
 *     Data window list management                                           *
 *                                                                           *
 *****************************************************************************/

/**
 * _gwy_app_data_view_set_current:
 * @data_view: Data view, can be %NULL.
 *
 * Updates application state upon switch to new data.
 **/
void
_gwy_app_data_view_set_current(GwyDataView *data_view)
{
    if (current_tool)
        gwy_tool_data_switched(current_tool, data_view);
}

void
_gwy_app_data_window_setup(GwyDataWindow *data_window)
{
    static GtkWidget *popup_menu = NULL;
    static GtkWidget *corner_menu = NULL;

    GwyDataView *data_view;
    GtkWidget *corner, *ebox, *main_window;
    GtkAccelGroup *accel_group;
    GwyContainer *settings;
    GwyLayerBasicRangeType range_type;

    if ((!popup_menu || !corner_menu)
        && (main_window = gwy_app_main_window_get())) {

        g_return_if_fail(GTK_IS_WINDOW(main_window));
        accel_group = GTK_ACCEL_GROUP(g_object_get_data(G_OBJECT(main_window),
                                                        "accel_group"));

        if (accel_group && !popup_menu) {
            popup_menu = gwy_app_menu_data_popup_create(accel_group);
            gtk_widget_show_all(popup_menu);
        }
        if (accel_group && !corner_menu) {
            corner_menu = gwy_app_menu_data_corner_create(accel_group);
            gtk_widget_show_all(corner_menu);
        }
    }

    gwy_app_add_main_accel_group(GTK_WINDOW(data_window));

    corner = gtk_arrow_new(GTK_ARROW_RIGHT, GTK_SHADOW_ETCHED_OUT);
    gtk_misc_set_alignment(GTK_MISC(corner), 0.5, 0.5);
    gtk_misc_set_padding(GTK_MISC(corner), 2, 0);

    ebox = gtk_event_box_new();
    gtk_container_add(GTK_CONTAINER(ebox), corner);
    gtk_widget_add_events(ebox, GDK_BUTTON_PRESS_MASK);
    gtk_widget_show_all(ebox);

    gwy_data_window_set_ul_corner_widget(data_window, ebox);

    data_view = gwy_data_window_get_data_view(data_window);
    g_signal_connect_swapped(data_view, "button-press-event",
                             G_CALLBACK(gwy_app_data_popup_menu_popup_mouse),
                             popup_menu);
    g_signal_connect_swapped(data_window, "popup-menu",
                             G_CALLBACK(gwy_app_data_popup_menu_popup_key),
                             popup_menu);
    g_signal_connect_swapped(ebox, "button-press-event",
                             G_CALLBACK(gwy_app_data_corner_menu_popup_mouse),
                             corner_menu);

    settings = gwy_app_settings_get();
    if (gwy_container_gis_enum_by_name(settings, "/app/default-range-type",
                                       &range_type)) {
        GwyPixmapLayer *layer;

        layer = gwy_data_view_get_base_layer(data_view);
        g_object_set(layer, "default-range-type", range_type, NULL);
    }
}

static GtkWidget*
gwy_app_menu_data_popup_create(GtkAccelGroup *accel_group)
{
    static struct {
        const gchar *label;
        gpointer callback;
        gpointer cbdata;
        guint key;
        GdkModifierType mods;
    }
    const menu_items[] = {
        {
            NULL, gwy_app_run_process_func,
            "mask_remove", GDK_K, GDK_CONTROL_MASK
        },
        {
            N_("Mask _Color..."),  gwy_app_change_mask_color,
            NULL, 0, 0
        },
        {
            NULL, gwy_app_run_process_func,
            "fix_zero", 0, 0
        },
        {
            NULL, gwy_app_run_process_func,
            "presentation_remove", GDK_K, GDK_CONTROL_MASK | GDK_SHIFT_MASK
        },
        {
            NULL, gwy_app_run_process_func,
            "level", GDK_L, GDK_CONTROL_MASK
        },
        {
            N_("Zoom _1:1"), gwy_app_data_window_reset_zoom,
            NULL, 0, 0
        },
        {
            N_("Metadata _Browser..."),
            metadata_browser, GUINT_TO_POINTER(GWY_APP_DATA_FIELD),
            GDK_B, GDK_CONTROL_MASK | GDK_SHIFT_MASK
        },
    };
    GwySensitivityGroup *sensgroup;
    GtkWidget *menu, *item;
    const gchar *name;
    guint i, mask;

    menu = gtk_menu_new();
    if (accel_group)
        gtk_menu_set_accel_group(GTK_MENU(menu), accel_group);
    sensgroup = gwy_app_sensitivity_get_group();
    for (i = 0; i < G_N_ELEMENTS(menu_items); i++) {
        if (menu_items[i].callback == gwy_app_run_process_func
            && !gwy_process_func_get_run_types((gchar*)menu_items[i].cbdata)) {
            g_warning("Processing function <%s> for "
                      "data view context menu is not available.",
                      (const gchar*)menu_items[i].cbdata);
            continue;
        }
        if (menu_items[i].callback == gwy_app_run_process_func) {
            name = _(gwy_process_func_get_menu_path(menu_items[i].cbdata));
            name = strrchr(name, '/');
            if (!name) {
                g_warning("Invalid translated menu path for <%s>",
                          (const gchar*)menu_items[i].cbdata);
                continue;
            }
            item = gtk_menu_item_new_with_mnemonic(name + 1);
            mask = gwy_process_func_get_sensitivity_mask(menu_items[i].cbdata);
            gwy_sensitivity_group_add_widget(sensgroup, item, mask);
        }
        else {
            item = gtk_menu_item_new_with_mnemonic(_(menu_items[i].label));

            if (menu_items[i].callback == gwy_app_change_mask_color)
                gwy_sensitivity_group_add_widget(sensgroup, item,
                                                 GWY_MENU_FLAG_DATA_MASK);
        }

        if (menu_items[i].key)
            gtk_widget_add_accelerator(item, "activate", accel_group,
                                       menu_items[i].key, menu_items[i].mods,
                                       GTK_ACCEL_VISIBLE | GTK_ACCEL_LOCKED);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
        g_signal_connect_swapped(item, "activate",
                                 G_CALLBACK(menu_items[i].callback),
                                 menu_items[i].cbdata);
    }

    return menu;
}

static gboolean
gwy_app_data_popup_menu_popup_mouse(GtkWidget *menu,
                                    GdkEventButton *event,
                                    GwyDataView *data_view)
{
    if (event->button != 3)
        return FALSE;

    gwy_app_data_browser_select_data_view(data_view);
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
                   event->button, event->time);

    return TRUE;
}

static void
gwy_app_data_popup_menu_position(G_GNUC_UNUSED GtkMenu *menu,
                                 gint *x,
                                 gint *y,
                                 gboolean *push_in,
                                 GtkWidget *window)
{
    GwyDataView *data_view;

    data_view = gwy_data_window_get_data_view(GWY_DATA_WINDOW(window));
    gdk_window_get_origin(GTK_WIDGET(data_view)->window, x, y);
    *push_in = TRUE;
}

static void
gwy_app_data_popup_menu_popup_key(GtkWidget *menu,
                                  GtkWidget *data_window)
{
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL,
                   (GtkMenuPositionFunc)gwy_app_data_popup_menu_position,
                   data_window,
                   0, gtk_get_current_event_time());
}

/**
 * gwy_app_data_corner_menu_update:
 * @menu: Data window corner menu.
 * @data_view: The corresponding data view.
 *
 * Updates corner menu to reflect data window's state before we show it.
 **/
static void
gwy_app_data_corner_menu_update(GtkWidget *menu,
                                GwyDataView *data_view)
{
    gboolean realsquare = FALSE;
    GwyContainer *data;
    const gchar *key;
    gchar *s;
    GtkWidget *item;
    GList *l;
    gulong id;
    guint i;

    /* Square mode */
    data = gwy_data_view_get_data(data_view);
    key = gwy_data_view_get_data_prefix(data_view);
    s = g_strconcat(key, "/realsquare", NULL);
    gwy_container_gis_boolean_by_name(data, s, &realsquare);
    gwy_debug("view's realsquare: %d", realsquare);
    g_free(s);

    /* Update stuff */
    l = gtk_container_get_children(GTK_CONTAINER(menu));
    while (l) {
        item = GTK_WIDGET(l->data);
        i = GPOINTER_TO_UINT(g_object_get_qdata(G_OBJECT(item),
                                                corner_item_quark));
        switch (i) {
            case ITEM_PIXELSQUARE:
            if (!realsquare) {
                gwy_debug("setting Pixelwise active");
                id = g_signal_handler_find(item, G_SIGNAL_MATCH_FUNC,
                                           0, 0, NULL,
                                           gwy_app_data_window_change_square,
                                           NULL);
                g_signal_handler_block(item, id);
                gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);
                g_signal_handler_unblock(item, id);
            }
            break;

            case ITEM_REALSQUARE:
            if (realsquare) {
                gwy_debug("setting Physical active");
                id = g_signal_handler_find(item, G_SIGNAL_MATCH_FUNC,
                                           0, 0, NULL,
                                           gwy_app_data_window_change_square,
                                           NULL);
                g_signal_handler_block(item, id);
                gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);
                g_signal_handler_unblock(item, id);
            }
            break;

            default:
            break;
        }
        l = g_list_next(l);
    }
}

static gboolean
gwy_app_data_corner_menu_popup_mouse(GtkWidget *menu,
                                     GdkEventButton *event,
                                     GtkWidget *ebox)
{
    GtkWidget *window;
    GwyDataView *data_view;

    if (event->button != 1)
        return FALSE;

    window = gtk_widget_get_ancestor(ebox, GWY_TYPE_DATA_WINDOW);
    g_return_val_if_fail(window, FALSE);
    data_view = gwy_data_window_get_data_view(GWY_DATA_WINDOW(window));

    gwy_app_data_browser_select_data_view(data_view);
    gwy_app_data_corner_menu_update(menu, data_view);
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
                   event->button, event->time);

    return FALSE;
}

static GtkWidget*
gwy_app_menu_data_corner_create(GtkAccelGroup *accel_group)
{
    GtkWidget *menu, *item;
    GtkRadioMenuItem *r;

    corner_item_quark = g_quark_from_static_string("id");

    menu = gtk_menu_new();
    if (accel_group)
        gtk_menu_set_accel_group(GTK_MENU(menu), accel_group);

    item = gtk_radio_menu_item_new_with_mnemonic(NULL,
                                                 _("Pi_xelwise Square"));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    g_object_set_qdata(G_OBJECT(item), corner_item_quark,
                       GUINT_TO_POINTER(ITEM_PIXELSQUARE));
    g_signal_connect(item, "activate",
                     G_CALLBACK(gwy_app_data_window_change_square),
                     GINT_TO_POINTER(FALSE));

    r = GTK_RADIO_MENU_ITEM(item);
    item = gtk_radio_menu_item_new_with_mnemonic_from_widget(r,
                                                             _("_Physically "
                                                               "Square"));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    g_object_set_qdata(G_OBJECT(item), corner_item_quark,
                       GUINT_TO_POINTER(ITEM_REALSQUARE));
    g_signal_connect(item, "activate",
                     G_CALLBACK(gwy_app_data_window_change_square),
                     GINT_TO_POINTER(TRUE));

    return menu;
}

static void
gwy_app_data_window_change_square(GtkWidget *item,
                                  gpointer user_data)
{
    gboolean realsquare = GPOINTER_TO_INT(user_data);
    GwyDataView *data_view;
    GwyContainer *data;
    const gchar *key;
    gchar *s;

    if (!gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item))) {
        gwy_debug("bogus update");
        return;
    }

    gwy_debug("new square mode: %s", realsquare ? "Physical" : "Pixelwise");
    gwy_app_data_browser_get_current(GWY_APP_CONTAINER, &data,
                                     GWY_APP_DATA_VIEW, &data_view,
                                     0);
    data = gwy_data_view_get_data(data_view);
    key = gwy_data_view_get_data_prefix(data_view);
    g_return_if_fail(key);
    s = g_strconcat(key, "/realsquare", NULL);
    if (realsquare)
        gwy_container_set_boolean_by_name(data, s, realsquare);
    else
        gwy_container_remove_by_name(data, s);
    g_free(s);
}

/*****************************************************************************
 *                                                                           *
 *     Graph window list management                                          *
 *                                                                           *
 *****************************************************************************/

void
_gwy_app_graph_window_setup(GwyGraphWindow *graph_window)
{
    static GtkWidget *popup_menu = NULL;

    GtkWidget *graph, *main_window;
    GtkAccelGroup *accel_group;

    if (!popup_menu && (main_window = gwy_app_main_window_get())) {
        g_return_if_fail(GTK_IS_WINDOW(main_window));
        accel_group = GTK_ACCEL_GROUP(g_object_get_data(G_OBJECT(main_window),
                                                        "accel_group"));

        if (accel_group && !popup_menu) {
            GList *items;

            popup_menu = gwy_app_build_graph_menu(accel_group);
            items = gtk_container_get_children(GTK_CONTAINER(popup_menu));
            if (GTK_IS_TEAROFF_MENU_ITEM(items->data))
                gtk_widget_destroy(GTK_WIDGET(items->data));
            g_list_free(items);
            gtk_widget_show_all(popup_menu);
        }
    }

    gwy_app_add_main_accel_group(GTK_WINDOW(graph_window));

    graph = gwy_graph_window_get_graph(graph_window);
    g_signal_connect_swapped(graph, "button-press-event",
                             G_CALLBACK(gwy_app_graph_popup_menu_popup_mouse),
                             popup_menu);
    g_signal_connect_swapped(gwy_graph_get_area(GWY_GRAPH(graph)),
                             "button-press-event",
                             G_CALLBACK(gwy_app_graph_popup_menu_popup_mouse),
                             popup_menu);
    /* FIXME: Graphs don't get keyboard events. */
    g_signal_connect_swapped(graph, "popup-menu",
                             G_CALLBACK(gwy_app_graph_popup_menu_popup_key),
                             popup_menu);
}

static gboolean
gwy_app_graph_popup_menu_popup_mouse(GtkWidget *menu,
                                     GdkEventButton *event,
                                     GwyGraph *graph)
{
    if (event->button != 3)
        return FALSE;

    if (GWY_IS_GRAPH_AREA(graph)) {
        graph = GWY_GRAPH(gtk_widget_get_ancestor(GTK_WIDGET(graph),
                                                  GWY_TYPE_GRAPH));
        if (!graph)
            return FALSE;
    }
    gwy_app_data_browser_select_graph(graph);
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
                   event->button, event->time);

    return TRUE;
}

static void
gwy_app_graph_popup_menu_position(G_GNUC_UNUSED GtkMenu *menu,
                                 gint *x,
                                 gint *y,
                                 gboolean *push_in,
                                 GtkWidget *widget)
{
    gdk_window_get_origin(widget->window, x, y);
    *push_in = TRUE;
}

static void
gwy_app_graph_popup_menu_popup_key(GtkWidget *menu,
                                   GtkWidget *graph)
{
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL,
                   (GtkMenuPositionFunc)gwy_app_graph_popup_menu_position,
                   graph,
                   0, gtk_get_current_event_time());
}

/*****************************************************************************
 *                                                                           *
 *     3D window                                                             *
 *                                                                           *
 *****************************************************************************/

void
_gwy_app_3d_window_setup(Gwy3DWindow *window3d)
{
    GtkTooltips *tooltips;
    GtkWidget *button;

    gwy_app_add_main_accel_group(GTK_WINDOW(window3d));
    tooltips = gwy_3d_window_class_get_tooltips();

    button = gwy_stock_like_button_new(gwy_sgettext("verb|Save"),
                                       GTK_STOCK_SAVE);
    gtk_tooltips_set_tip(tooltips, button,
                         _("Save 3D view to an image"), NULL);
    gwy_3d_window_add_action_widget(GWY_3D_WINDOW(window3d), button);
    gwy_3d_window_add_small_toolbar_button(GWY_3D_WINDOW(window3d),
                                           GTK_STOCK_SAVE,
                                           _("Save 3D view to an image"),
                                           G_CALLBACK(gwy_app_3d_window_export),
                                           window3d);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(gwy_app_3d_window_export), window3d);

    button = gtk_button_new_with_mnemonic(_("Set as Default"));
    gtk_tooltips_set_tip(tooltips, button,
                         _("Set the current view setup as the default"), NULL);
    gwy_3d_window_add_action_widget(GWY_3D_WINDOW(window3d), button);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(gwy_app_3d_window_set_defaults),
                             window3d);

    gwy_app_3d_window_add_overlay_menu(GWY_3D_WINDOW(window3d));
}

/* a widget for the 3dwindow as overlay chooser */
static void
gwy_app_3d_window_add_overlay_menu(Gwy3DWindow *gwy3dwindow)
{
    GtkWidget *menu, *lay;
    Gwy3DView *view;
    GQuark data2_ref = 0;
    gint* ids, *nids;
    gint activeid = -1;
    const guchar *key;
    gchar refkey[40];
    gboolean showmask = FALSE;
    GwyContainer *settings;

    view = GWY_3D_VIEW(gwy_3d_window_get_3d_view(gwy3dwindow));
    lay = gtk_hbox_new(FALSE, 5);
    menu = gwy_data_chooser_new_channels();

    gwy_data_chooser_set_filter(GWY_DATA_CHOOSER(menu),
                                gwy_app_3d_window_data2_filter, view, NULL);

    ids = gwy_app_data_browser_get_data_ids(view->data);
    g_strlcpy(refkey,
              g_quark_to_string(view->data_key),
              4);
    g_strlcat(refkey, "3d/data2ref", sizeof(refkey));
    if (gwy_container_gis_string_by_name(view->data, refkey, &key)) {
        data2_ref = g_quark_from_string(key);
    };

    for (nids = ids; *nids != -1; nids++) {
        if (gwy_app_get_data_key_for_id(*nids) == data2_ref) {
            activeid = *nids;
            break;
        };
        if (gwy_app_get_data_key_for_id(*nids) == view->data_key
            && activeid == -1) {
            activeid = *nids;
        };
    };
    g_free(ids);

    gwy_data_chooser_set_active(GWY_DATA_CHOOSER(menu), view->data, activeid);
    gwy_app_3d_window_set_data2(gwy3dwindow, activeid, FALSE);

    g_signal_connect_swapped(menu, "changed",
                             G_CALLBACK(gwy_app_3d_window_update_chooser),
                             gwy3dwindow);

    gtk_box_pack_start(GTK_BOX(lay), menu, FALSE, FALSE, 0);
    g_object_set_data(G_OBJECT(lay), "c", menu);

    menu = gtk_check_button_new_with_mnemonic(_("_Show mask"));
    settings = gwy_app_settings_get();
    gwy_container_gis_boolean_by_name(settings, "/app/3d/show-mask", &showmask);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(menu), showmask);

    g_signal_connect_swapped(menu, "toggled",
                             G_CALLBACK(gwy_app_3d_window_update_chooser),
                             gwy3dwindow);
    gtk_box_pack_start(GTK_BOX(lay), menu, FALSE, FALSE, 0);
    g_object_set_data(G_OBJECT(lay), "m", menu);

    gwy_3d_window_set_overlay_chooser(gwy3dwindow, lay);
    /* XXX: Gross! It does not take initial state of the checkbox into
     * account.*/
    if (showmask)
        gwy_app_3d_window_update_chooser(gwy3dwindow);
}

/* set overlay source to channel id for Gwy3DView in gwy3dwindow.
 * Show mask if mask==True
 */
static void
gwy_app_3d_window_set_data2(Gwy3DWindow *gwy3dwindow,
                            gint id,
                            gboolean mask)
{
    Gwy3DView *view;
    GQuark key;
    GwyPixmapLayer *ovplay[2];
    GwyDataViewLayer* dvl;
    guchar name[48];



    view = GWY_3D_VIEW(gwy_3d_window_get_3d_view(gwy3dwindow));

    key = gwy_app_get_data_key_for_id(id);

    ovplay[0] = gwy_layer_basic_new();
    /* Plug */
    /*    gwy_data_view_layer_plugged(dvl); */
    dvl = GWY_DATA_VIEW_LAYER(ovplay[0]);
    dvl->data = view->data;
    g_object_ref(dvl->data);

    gwy_pixmap_layer_set_data_key(ovplay[0], g_quark_to_string(key));
    gwy_layer_basic_set_gradient_key(GWY_LAYER_BASIC(ovplay[0]),
                                     gwy_3d_view_get_gradient_key(view));
    g_snprintf(name, sizeof(name), "/%d/base",  id);
    gwy_layer_basic_set_min_max_key(GWY_LAYER_BASIC(ovplay[0]),
                                    name);
    g_snprintf(name, sizeof(name), "/%d/base/range-type", id);
    gwy_layer_basic_set_range_type_key(GWY_LAYER_BASIC(ovplay[0]),
                                    name);

    if (mask) {
        key = gwy_app_get_mask_key_for_id(id);
        ovplay[1] = gwy_layer_mask_new();
        dvl = GWY_DATA_VIEW_LAYER(ovplay[1]);
        dvl->data = view->data;
        g_object_ref(dvl->data);
        gwy_pixmap_layer_set_data_key(ovplay[1], g_quark_to_string(key));
        gwy_layer_mask_set_color_key(GWY_LAYER_MASK(ovplay[1]),
                                     g_quark_to_string(key));
        gwy_3d_view_set_ovlay(view, ovplay, 2);
    }
    else {
        gwy_3d_view_set_ovlay(view, ovplay, 1);
    };
};

/* callback for the chooser created by gwy_app_3d_window_add_overlay_menu */
static void
gwy_app_3d_window_update_chooser(Gwy3DWindow *gwy3dwindow)
{
    gint id;
    gboolean mask = FALSE;
    GtkWidget *temp;
    guchar refkey[40];
    const guchar *name;
    GQuark key;
    Gwy3DView *view;

    temp = g_object_get_data(G_OBJECT(gwy3dwindow->dataov_menu), "m");


    mask = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(temp));
    temp = g_object_get_data(G_OBJECT(gwy3dwindow->dataov_menu), "c");

    gwy_data_chooser_get_active(GWY_DATA_CHOOSER(temp), &id);
    key = gwy_app_get_data_key_for_id(id);
    name = g_quark_to_string(key);

    view = GWY_3D_VIEW(gwy_3d_window_get_3d_view(gwy3dwindow));
    key = gwy_app_get_data_key_for_id(id);
    name = g_quark_to_string(key);
    g_strlcpy(refkey,
              g_quark_to_string(view->data_key),
              4);
    g_strlcat(refkey, "3d/data2ref", sizeof(refkey));
    gwy_container_set_string_by_name(view->data, refkey, g_strdup(name));

    gwy_app_3d_window_set_data2(gwy3dwindow, id, mask);
}

static gboolean
gwy_app_3d_window_data2_filter(GwyContainer *data2,
                               gint id2,
                               gpointer user_data)
{
    Gwy3DView* view;
    GwyDataField *data_field1, *data_field2;
    GQuark quark2;
    GwyContainer* data1;

    quark2 = gwy_app_get_data_key_for_id(id2);
    data_field2 = gwy_container_get_object(data2, quark2);
    view = GWY_3D_VIEW(user_data);
    data1 = gwy_3d_view_get_data(view);
    if (data1 != data2)
        return FALSE;

    gwy_container_gis_object_by_name(data1,
                                     gwy_3d_view_get_data_key(view),
                                     &data_field1);

    return !gwy_data_field_check_compatibility(data_field2, data_field1,
                                               GWY_DATA_COMPATIBILITY_RES);
};

static void
gwy_app_save_3d_export(GtkWidget *dialog,
                       gint response,
                       Gwy3DWindow *gwy3dwindow)
{
    gchar *filename_sys, *filename_utf8, *s, *filetype = NULL;
    GdkPixbuf *pixbuf;
    GtkWidget *gwy3dview;
    GError *err = NULL;

    if (response != GTK_RESPONSE_OK) {
        gtk_widget_destroy(dialog);
        return;
    }

    gwy3dview = gwy_3d_window_get_3d_view(GWY_3D_WINDOW(gwy3dwindow));
    filename_sys = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
    gtk_widget_destroy(dialog);

    pixbuf = gwy_3d_view_get_pixbuf(GWY_3D_VIEW(gwy3dview));
    filename_utf8 = g_filename_to_utf8(filename_sys, -1, NULL, NULL, NULL);
    if ((s = strrchr(filename_utf8, '.'))) {
        filetype = g_ascii_strdown(s+1, -1);
        if (gwy_strequal(filetype, "jpg")) {
            g_free(filetype);
            filetype = g_strdup("jpeg");
        }
        else if (gwy_strequal(filetype, "tif")) {
            g_free(filetype);
            filetype = g_strdup("tiff");
        }
    }
    if (!gdk_pixbuf_save(pixbuf, filename_sys, filetype ? filetype : "png",
                         &err, NULL)) {
        dialog = gtk_message_dialog_new(NULL,
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_ERROR,
                                        GTK_BUTTONS_OK,
                                        _("Saving of 3D view to `%s' failed"),
                                        filename_utf8);
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
                                                 "%s", err->message);
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        g_clear_error(&err);
    }
    g_free(filetype);
    g_free(filename_sys);
    g_object_unref(pixbuf);
    g_free(g_object_get_data(G_OBJECT(gwy3dwindow), "gwy-app-export-filename"));
    g_object_set_data(G_OBJECT(gwy3dwindow), "gwy-app-export-filename",
                      filename_utf8);
}

static void
gwy_app_3d_window_export(Gwy3DWindow *gwy3dwindow)
{
    GwyContainer *data;
    GtkWidget *dialog, *gwy3dview;
    const guchar *filename_utf8;
    gchar *filename_sys;
    gboolean need_free_utf = FALSE;

    gwy3dview = gwy_3d_window_get_3d_view(gwy3dwindow);
    data = gwy_3d_view_get_data(GWY_3D_VIEW(gwy3dview));

    filename_utf8 = g_object_get_data(G_OBJECT(gwy3dwindow),
                                      "gwy-app-export-filename");
    if (!filename_utf8) {
        if (gwy_container_gis_string_by_name(data, "/filename",
                                             &filename_utf8)) {
            /* FIXME: this is ugly, invent a better filename */
            filename_utf8 = g_strconcat(filename_utf8, ".png", NULL);
            need_free_utf = TRUE;
        }
        else
            filename_utf8 = "3d.png";
    }
    filename_sys = g_filename_from_utf8(filename_utf8, -1, NULL, NULL, NULL);
    if (need_free_utf)
        g_free((gpointer)filename_utf8);

    dialog = gtk_file_chooser_dialog_new(_("Export 3D View"),
                                         GTK_WINDOW(gwy3dwindow),
                                         GTK_FILE_CHOOSER_ACTION_SAVE,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_SAVE, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog),
                                        gwy_app_get_current_directory());
    gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), filename_sys);
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog),
                                                   TRUE);
    g_free(filename_sys);

    g_signal_connect(dialog, "response",
                     G_CALLBACK(gwy_app_save_3d_export), gwy3dwindow);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_widget_show_all(dialog);
}

static void
gwy_app_3d_window_set_defaults(Gwy3DWindow *window)
{
    Gwy3DView *view;
    Gwy3DSetup *setup;
    GObject *lay;
    GtkToggleButton *toggle;
    GwyContainer *settings;

    view = (Gwy3DView*)gwy_3d_window_get_3d_view(window);
    setup = gwy_3d_view_get_setup(view);
    g_return_if_fail(GWY_IS_3D_SETUP(setup));

    settings = gwy_app_settings_get();
    gwy_container_set_boolean_by_name(settings, "/app/3d/axes-visible",
                                      setup->axes_visible);
    gwy_container_set_boolean_by_name(settings, "/app/3d/labels-visible",
                                      setup->labels_visible);
    gwy_container_set_boolean_by_name(settings, "/app/3d/fmscale-visible",
                                      setup->fmscale_visible);
    gwy_container_set_double_by_name(settings, "/app/3d/rotation-x",
                                     setup->rotation_x);
    gwy_container_set_double_by_name(settings, "/app/3d/rotation-y",
                                     setup->rotation_y);
    gwy_container_set_double_by_name(settings, "/app/3d/scale",
                                     setup->scale);
    gwy_container_set_double_by_name(settings, "/app/3d/z-scale",
                                     setup->z_scale);
    gwy_container_set_double_by_name(settings, "/app/3d/light-phi",
                                     setup->light_phi);
    gwy_container_set_double_by_name(settings, "/app/3d/light-theta",
                                     setup->light_theta);
    gwy_container_set_enum_by_name(settings, "/app/3d/visualization",
                                   setup->visualization);
    gwy_container_set_enum_by_name(settings, "/app/3d/projection",
                                   setup->projection);

    lay = G_OBJECT(window->dataov_menu);
    toggle = GTK_TOGGLE_BUTTON(g_object_get_data(lay, "m"));
    gwy_container_set_boolean_by_name(settings, "/app/3d/show-mask",
                                      gtk_toggle_button_get_active(toggle));
}

gboolean
_gwy_app_3d_view_init_setup(GwyContainer *container,
                            const gchar *setup_prefix)
{
    GwyContainer *settings;
    Gwy3DSetup *setup;
    Gwy3DProjection projection;
    Gwy3DVisualization visualization;
    gdouble dblvalue;
    gboolean boolvalue;
    gchar *key;

    g_return_val_if_fail(GWY_IS_CONTAINER(container), FALSE);
    g_return_val_if_fail(setup_prefix, FALSE);

    key = g_strconcat(setup_prefix, "/setup", NULL);
    if (gwy_container_gis_object_by_name(container, key, &setup)
        && GWY_IS_3D_SETUP(setup)) {
        g_free(key);
        return FALSE;
    }

    setup = gwy_3d_setup_new();
    settings = gwy_app_settings_get();
    if (gwy_container_gis_boolean_by_name(settings, "/app/3d/axes-visible",
                                          &boolvalue))
        g_object_set(setup, "axes-visible", boolvalue, NULL);
    if (gwy_container_gis_boolean_by_name(settings, "/app/3d/labels-visible",
                                          &boolvalue))
        g_object_set(setup, "labels-visible", boolvalue, NULL);
    if (gwy_container_gis_boolean_by_name(settings, "/app/3d/fmscale-visible",
                                          &boolvalue))
        g_object_set(setup, "fmscale-visible", boolvalue, NULL);
    if (gwy_container_gis_double_by_name(settings, "/app/3d/rotation-x",
                                         &dblvalue))
        g_object_set(setup, "rotation-x", dblvalue, NULL);
    if (gwy_container_gis_double_by_name(settings, "/app/3d/rotation-y",
                                         &dblvalue))
        g_object_set(setup, "rotation-y", dblvalue, NULL);
    if (gwy_container_gis_double_by_name(settings, "/app/3d/scale",
                                         &dblvalue))
        g_object_set(setup, "scale", dblvalue, NULL);
    if (gwy_container_gis_double_by_name(settings, "/app/3d/z-scale",
                                         &dblvalue))
        g_object_set(setup, "z-scale", dblvalue, NULL);
    if (gwy_container_gis_double_by_name(settings, "/app/3d/light-phi",
                                         &dblvalue))
        g_object_set(setup, "light-phi", dblvalue, NULL);
    if (gwy_container_gis_double_by_name(settings, "/app/3d/light-theta",
                                         &dblvalue))
        g_object_set(setup, "light-theta", dblvalue, NULL);
    if (gwy_container_gis_enum_by_name(settings, "/app/3d/visualization",
                                       &visualization))
        g_object_set(setup, "visualization", visualization, NULL);
    if (gwy_container_gis_enum_by_name(settings, "/app/3d/projection",
                                       &projection))
        g_object_set(setup, "projection", projection, NULL);

    gwy_container_set_object_by_name(container, key, setup);
    g_object_unref(setup);
    g_free(key);

    return TRUE;
}

/*****************************************************************************
 *                                                                           *
 *     Spectra                                                               *
 *                                                                           *
 *****************************************************************************/

void
_gwy_app_spectra_set_current(GwySpectra *spectra)
{
    if (current_tool)
        gwy_tool_spectra_switched(current_tool, spectra);
}

/*****************************************************************************
 *                                                                           *
 *     Bricks                                                                *
 *                                                                           *
 *****************************************************************************/

void
_gwy_app_brick_window_setup(GwyDataWindow *data_window)
{
    static GtkWidget *popup_menu = NULL;

    GtkAccelGroup *accel_group;
    GwyDataView *data_view;
    GtkWidget *main_window;
    GtkWidget *vbox, *hbox, *button, *label;

    if (!popup_menu
        && (main_window = gwy_app_main_window_get())) {

        g_return_if_fail(GTK_IS_WINDOW(main_window));
        accel_group = GTK_ACCEL_GROUP(g_object_get_data(G_OBJECT(main_window),
                                                        "accel_group"));

        if (accel_group && !popup_menu) {
            popup_menu = gwy_app_menu_brick_popup_create(accel_group);
            gtk_widget_show_all(popup_menu);
        }
    }

    gwy_app_add_main_accel_group(GTK_WINDOW(data_window));

    data_view = gwy_data_window_get_data_view(data_window);
    g_signal_connect_swapped(data_view, "button-press-event",
                             G_CALLBACK(gwy_app_brick_popup_menu_popup_mouse),
                             popup_menu);
    g_signal_connect_swapped(data_window, "popup-menu",
                             G_CALLBACK(gwy_app_brick_popup_menu_popup_key),
                             popup_menu);

    hbox = gtk_hbox_new(FALSE, 0);

    vbox = gtk_bin_get_child(GTK_BIN(data_window));
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
    gtk_box_reorder_child(GTK_BOX(vbox), hbox, 0);

    button = gtk_button_new_with_mnemonic(_("_Change Preview"));
    GTK_WIDGET_UNSET_FLAGS(button, GTK_CAN_FOCUS);
    gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(change_brick_preview), data_window);

    label = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);

    g_object_set_data(G_OBJECT(data_window), "gwy-brick-info", label);
}

static gboolean
brick_preview_filter(GwyContainer *data,
                     gint id,
                     gpointer user_data)
{
    GwyDataField *field;
    GQuark key = gwy_app_get_data_key_for_id(id);
    GwySIUnit *funit, *bunit;
    GwyBrick *brick = (GwyBrick*)user_data;

    field = GWY_DATA_FIELD(gwy_container_get_object(data, key));
    g_return_val_if_fail(field, FALSE);

    if (field->xres != brick->xres
        || field->yres != brick->yres
        || ABS(log(field->xreal/brick->xreal)) > 1e-6
        || ABS(log(field->yreal/brick->yreal)) > 1e-6)
        return FALSE;

    bunit = gwy_brick_get_si_unit_x(brick);
    funit = gwy_data_field_get_si_unit_xy(field);
    if (!gwy_si_unit_equal(bunit, funit))
        return FALSE;
    bunit = gwy_brick_get_si_unit_y(brick);
    if (!gwy_si_unit_equal(bunit, funit))
        return FALSE;

    return TRUE;
}

static void
update_brick_preview_sens(BrickPreviewType type,
                          GObject *dialog)
{
    GwyDataChooser *chooser;
    GtkWidget *widget;
    gboolean ok = TRUE;

    widget = GTK_WIDGET(g_object_get_data(dialog, "channel-chooser"));
    chooser = GWY_DATA_CHOOSER(widget);
    gtk_widget_set_sensitive(widget, type == BRICK_PREVIEW_CHANNEL);

    widget = GTK_WIDGET(g_object_get_data(dialog, "section-scale"));
    gtk_widget_set_sensitive(widget, type == BRICK_PREVIEW_SECTION);
    widget = GTK_WIDGET(g_object_get_data(dialog, "section-spin"));
    gtk_widget_set_sensitive(widget, type == BRICK_PREVIEW_SECTION);
    widget = GTK_WIDGET(g_object_get_data(dialog, "section-units"));
    gtk_widget_set_sensitive(widget, type == BRICK_PREVIEW_SECTION);

    if (type == BRICK_PREVIEW_CHANNEL) {
        gint id;
        GwyContainer *container = gwy_data_chooser_get_active(chooser, &id);
        ok = !!container;
    }
    gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog), GTK_RESPONSE_OK, ok);
}

static void
brick_preview_type_changed(GtkRadioButton *button,
                           GObject *dialog)
{
    GSList *group = gtk_radio_button_get_group(button);
    BrickPreviewType type = gwy_radio_buttons_get_current(group);
    update_brick_preview_sens(type, dialog);
}

static void
change_brick_preview(GwyDataWindow *data_window)
{
    GtkAdjustment *leveladj;
    GtkWidget *dialog, *label, *table, *chooser, *scale, *spin;
    GwyContainer *data, *cdata;
    GwyDataField *preview;
    GwyBrick *brick;
    GSList *group, *l;
    gint response, id, cid, i, level;
    BrickPreviewType type;
    gchar key[40];

    gwy_app_data_browser_get_current(GWY_APP_BRICK, &brick,
                                     GWY_APP_BRICK_ID, &id,
                                     GWY_APP_CONTAINER, &data,
                                     0);
    g_return_if_fail(GWY_IS_BRICK(brick));
    g_return_if_fail(id >= 0);

    dialog = gtk_dialog_new_with_buttons(_("Change Volume Data Preview"),
                                         GTK_WINDOW(data_window),
                                         GTK_DIALOG_MODAL
                                         | GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    table = gtk_table_new(5, 4, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 8);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 0);
    label = gtk_label_new(_("Preview quantity:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, GTK_FILL, 0, 0, 0);
    type = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(brick),
                                              "gwy-preview-type"));
    level = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(brick),
                                              "gwy-preview-level"));
    group = gwy_radio_buttons_createl(G_CALLBACK(brick_preview_type_changed),
                                      dialog,
                                      type,
                                      _("Mean"), BRICK_PREVIEW_MEAN,
                                      _("Minimum"), BRICK_PREVIEW_MINIMUM,
                                      _("Maximum"), BRICK_PREVIEW_MAXIMUM,
                                      _("RMS"), BRICK_PREVIEW_RMS,
                                      _("Channel:"), BRICK_PREVIEW_CHANNEL,
                                      _("Section:"), BRICK_PREVIEW_SECTION,
                                      NULL);
    l = group;
    i = 1;
    while (l) {
        gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(l->data),
                         0, 1, i, i+1, GTK_FILL, 0, 0, 0);
        i++;
        l = g_slist_next(l);
    }
    chooser = gwy_data_chooser_new_channels();
    gwy_data_chooser_set_filter(GWY_DATA_CHOOSER(chooser),
                                &brick_preview_filter, brick, NULL);
    gtk_table_attach(GTK_TABLE(table), chooser,
                     1, 4, 5, 6, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_object_set_data(G_OBJECT(dialog), "channel-chooser", chooser);

    leveladj = (GtkAdjustment*)gtk_adjustment_new(level, 0, brick->zres-1,
                                                  1, 10, 0);
    scale = gtk_hscale_new(leveladj);
    gtk_scale_set_draw_value(GTK_SCALE(scale), FALSE);
    gtk_widget_set_size_request(scale, 120, -1);
    gtk_table_attach(GTK_TABLE(table), scale,
                     1, 2, 6, 7, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_object_set_data(G_OBJECT(dialog), "section-scale", scale);
    spin = gtk_spin_button_new(leveladj, 0.0, 0);
    gtk_table_attach(GTK_TABLE(table), spin, 2, 3, 6, 7, GTK_FILL, 0, 0, 0);
    g_object_set_data(G_OBJECT(dialog), "section-spin", spin);
    label = gtk_label_new("px");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 3, 4, 6, 7, GTK_FILL, 0, 0, 0);
    g_object_set_data(G_OBJECT(dialog), "section-units", label);

    update_brick_preview_sens(gwy_radio_buttons_get_current(group),
                              G_OBJECT(dialog));

    gtk_widget_show_all(dialog);
    response = gtk_dialog_run(GTK_DIALOG(dialog));
    type = gwy_radio_buttons_get_current(group);
    cdata = gwy_data_chooser_get_active(GWY_DATA_CHOOSER(chooser), &cid);
    level = gwy_adjustment_get_int(leveladj);
    level = CLAMP(level, 0, brick->zres-1);
    gtk_widget_destroy(dialog);
    if (response != GTK_RESPONSE_OK)
        return;

    g_snprintf(key, sizeof(key), "/brick/%d/preview", id);
    if (!gwy_container_gis_object_by_name(data, key, (GObject**)&preview)) {
        g_warning("No preview field found for brick %d.", id);
        return;
    }

    if (type == BRICK_PREVIEW_MEAN)
        gwy_brick_mean_plane(brick, preview,
                             0, 0, 0,
                             brick->xres, brick->yres, -1,
                             TRUE);
    else if (type == BRICK_PREVIEW_MINIMUM)
        gwy_brick_min_plane(brick, preview,
                            0, 0, 0,
                            brick->xres, brick->yres, -1,
                            TRUE);
    else if (type == BRICK_PREVIEW_MAXIMUM)
        gwy_brick_max_plane(brick, preview,
                            0, 0, 0,
                            brick->xres, brick->yres, -1,
                            TRUE);
    else if (type == BRICK_PREVIEW_RMS)
        gwy_brick_rms_plane(brick, preview,
                            0, 0, 0,
                            brick->xres, brick->yres, -1,
                            TRUE);
    else if (type == BRICK_PREVIEW_CHANNEL) {
        GQuark quark = gwy_app_get_data_key_for_id(cid);
        GObject *field = gwy_container_get_object(cdata, quark);
        gwy_serializable_clone(field, G_OBJECT(preview));
    }
    else if (type == BRICK_PREVIEW_SECTION)
        gwy_brick_extract_plane(brick, preview,
                                0, 0, level,
                                brick->xres, brick->yres, -1,
                                TRUE);
    else {
        g_return_if_reached();
    }

    g_object_set_data(G_OBJECT(brick),
                      "gwy-preview-type", GUINT_TO_POINTER(type));
    g_object_set_data(G_OBJECT(brick),
                      "gwy-preview-level", GUINT_TO_POINTER(level));
    gwy_data_field_data_changed(preview);
}

static GtkWidget*
gwy_app_menu_brick_popup_create(GtkAccelGroup *accel_group)
{
    static struct {
        const gchar *label;
        gpointer callback;
        gpointer cbdata;
        guint key;
        GdkModifierType mods;
    }
    const menu_items[] = {
        {
            N_("Zoom _1:1"), gwy_app_volume_window_reset_zoom,
            NULL, 0, 0
        },
        {
            N_("Metadata _Browser..."),
            metadata_browser, GUINT_TO_POINTER(GWY_APP_BRICK),
            GDK_B, GDK_CONTROL_MASK | GDK_SHIFT_MASK
        },
    };
    GwySensitivityGroup *sensgroup;
    GtkWidget *menu, *item;
    const gchar *name;
    guint i, mask;

    menu = gtk_menu_new();
    if (accel_group)
        gtk_menu_set_accel_group(GTK_MENU(menu), accel_group);
    sensgroup = gwy_app_sensitivity_get_group();
    for (i = 0; i < G_N_ELEMENTS(menu_items); i++) {
        if (menu_items[i].callback == gwy_app_run_volume_func
            && !gwy_volume_func_get_run_types((gchar*)menu_items[i].cbdata)) {
            g_warning("Brick function <%s> for "
                      "data view context menu is not available.",
                      (const gchar*)menu_items[i].cbdata);
            continue;
        }
        if (menu_items[i].callback == gwy_app_run_volume_func) {
            name = _(gwy_volume_func_get_menu_path(menu_items[i].cbdata));
            name = strrchr(name, '/');
            if (!name) {
                g_warning("Invalid translated menu path for <%s>",
                          (const gchar*)menu_items[i].cbdata);
                continue;
            }
            item = gtk_menu_item_new_with_mnemonic(name + 1);
            mask = gwy_volume_func_get_sensitivity_mask(menu_items[i].cbdata);
            gwy_sensitivity_group_add_widget(sensgroup, item, mask);
        }
        else {
            item = gtk_menu_item_new_with_mnemonic(_(menu_items[i].label));
        }

        if (menu_items[i].key)
            gtk_widget_add_accelerator(item, "activate", accel_group,
                                       menu_items[i].key, menu_items[i].mods,
                                       GTK_ACCEL_VISIBLE | GTK_ACCEL_LOCKED);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
        g_signal_connect_swapped(item, "activate",
                                 G_CALLBACK(menu_items[i].callback),
                                 menu_items[i].cbdata);
    }

    return menu;
}

static gboolean
gwy_app_brick_popup_menu_popup_mouse(GtkWidget *menu,
                                     GdkEventButton *event,
                                     GwyDataView *data_view)
{
    if (event->button != 3)
        return FALSE;

    gwy_app_data_browser_select_volume(data_view);
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
                   event->button, event->time);

    return TRUE;
}

static void
gwy_app_brick_popup_menu_position(G_GNUC_UNUSED GtkMenu *menu,
                                  gint *x,
                                  gint *y,
                                  gboolean *push_in,
                                  GtkWidget *window)
{
    GwyDataView *data_view;

    data_view = gwy_data_window_get_data_view(GWY_DATA_WINDOW(window));
    gdk_window_get_origin(GTK_WIDGET(data_view)->window, x, y);
    *push_in = TRUE;
}

static void
gwy_app_brick_popup_menu_popup_key(GtkWidget *menu,
                                   GtkWidget *data_window)
{
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL,
                   (GtkMenuPositionFunc)gwy_app_brick_popup_menu_position,
                   data_window,
                   0, gtk_get_current_event_time());
}

/*****************************************************************************
 *                                                                           *
 *     Miscellaneous                                                         *
 *                                                                           *
 *****************************************************************************/

/**
 * gwy_app_switch_tool:
 * @toolname: Tool name, that is #GType name of the tool type.
 *
 * Switches the current tool.
 **/
void
gwy_app_switch_tool(const gchar *toolname)
{
    GwyTool *newtool;
    GwyDataView *data_view;
    GType type;

    gwy_debug("%s", toolname ? toolname : "NONE");
    type = g_type_from_name(toolname);
    g_return_if_fail(type);

    gwy_app_data_browser_get_current(GWY_APP_DATA_VIEW, &data_view, 0);
    if (current_tool && type == G_TYPE_FROM_INSTANCE(current_tool)) {
        if (!gwy_tool_is_visible(current_tool))
            gwy_tool_show(current_tool);
        else
            gwy_tool_hide(current_tool);
        return;
    }

    gwy_object_unref(current_tool);
    newtool = (GwyTool*)g_object_new(type, NULL);
    current_tool = newtool;
    g_return_if_fail(GWY_IS_TOOL(newtool));

    if (data_view) {
        GwySpectra *spectra;

        gwy_tool_data_switched(current_tool, data_view);
        gwy_app_data_browser_get_current(GWY_APP_SPECTRA, &spectra, 0);
        gwy_tool_spectra_switched(current_tool, spectra);
        gwy_tool_show(current_tool);
    }
}

static void
gwy_app_data_window_reset_zoom(void)
{
    GtkWidget *window, *view;

    gwy_app_data_browser_get_current(GWY_APP_DATA_VIEW, &view, 0);
    window = gtk_widget_get_ancestor(view, GWY_TYPE_DATA_WINDOW);
    g_return_if_fail(window);
    gwy_data_window_set_zoom(GWY_DATA_WINDOW(window), 10000);
}

static void
gwy_app_volume_window_reset_zoom(void)
{
    GtkWidget *window, *view;

    gwy_app_data_browser_get_current(GWY_APP_VOLUME_VIEW, &view, 0);
    window = gtk_widget_get_ancestor(view, GWY_TYPE_DATA_WINDOW);
    g_return_if_fail(window);
    gwy_data_window_set_zoom(GWY_DATA_WINDOW(window), 10000);
}

static void
metadata_browser(gpointer pwhat)
{
    GwyAppWhat what = GPOINTER_TO_UINT(pwhat);
    GtkWidget *view;
    GwyContainer *container;
    gint id;

    if (what == GWY_APP_DATA_FIELD)
        gwy_app_data_browser_get_current(GWY_APP_DATA_VIEW, &view,
                                         GWY_APP_CONTAINER, &container,
                                         GWY_APP_DATA_FIELD_ID, &id,
                                         0);
    else if (what == GWY_APP_BRICK)
        gwy_app_data_browser_get_current(GWY_APP_VOLUME_VIEW, &view,
                                         GWY_APP_CONTAINER, &container,
                                         GWY_APP_BRICK_ID, &id,
                                         0);
    else {
        g_return_if_reached();
    }

    if (!view || !container || id == -1)
        return;

    if (what == GWY_APP_DATA_FIELD)
        gwy_app_metadata_browser_for_channel(container, id);
    else if (what == GWY_APP_BRICK)
        gwy_app_metadata_browser_for_volume(container, id);
}

static void
gwy_app_change_mask_color(void)
{
    GwyDataView *data_view;

    gwy_app_data_browser_get_current(GWY_APP_DATA_VIEW, &data_view, 0);
    g_return_if_fail(data_view);
    gwy_app_data_view_change_mask_color(data_view);
}

/**
 * gwy_app_data_view_change_mask_color:
 * @data_view: A data view (of application's data window).  It must have a
 *             mask.
 *
 * Runs mask color selector on a data view.
 *
 * This is a convenience function to run gwy_color_selector_for_mask(),
 * possibly taking the initial color from settings.
 **/
void
gwy_app_data_view_change_mask_color(GwyDataView *data_view)
{
    GwyPixmapLayer *layer;
    GwyContainer *data, *settings;
    const gchar *key;
    GwyRGBA rgba;

    g_return_if_fail(GWY_IS_DATA_VIEW(data_view));
    data = gwy_data_view_get_data(data_view);
    g_return_if_fail(GWY_IS_CONTAINER(data));
    layer = gwy_data_view_get_alpha_layer(data_view);
    g_return_if_fail(GWY_IS_LAYER_MASK(layer));
    key = gwy_layer_mask_get_color_key(GWY_LAYER_MASK(layer));
    g_return_if_fail(key);
    gwy_debug("<%s>", key);

    /* copy defaults to data container if necessary */
    if (!gwy_rgba_get_from_container(&rgba, data, key)) {
        settings = gwy_app_settings_get();
        gwy_rgba_get_from_container(&rgba, settings, "/mask");
        gwy_rgba_store_to_container(&rgba, data, key);
    }
    gwy_color_selector_for_mask(NULL, NULL, data, key);
}

/**
 * gwy_app_save_window_position:
 * @window: A window to save position of.
 * @prefix: Unique prefix in settings to store the information under.
 * @position: %TRUE to save position information.
 * @size: %TRUE to save size information.
 *
 * Saves position and/or size of a window to settings.
 *
 * Some sanity checks are included, therefore if window position and/or size
 * is too suspicious, it is not saved.
 **/
void
gwy_app_save_window_position(GtkWindow *window,
                             const gchar *prefix,
                             gboolean position,
                             gboolean size)
{
    GwyContainer *settings;
    GdkScreen *screen;
    gint x, y, w, h, scw, sch;
    guint len;
    gchar *key;

    g_return_if_fail(GTK_IS_WINDOW(window));
    g_return_if_fail(prefix);
    if (!(position || size))
        return;

    len = strlen(prefix);
                              /* The longest suffix */
    key = g_newa(gchar, len + sizeof("/position/height"));
    strcpy(key, prefix);

    settings = gwy_app_settings_get();
    screen = gtk_window_get_screen(window);
    scw = gdk_screen_get_width(screen);
    sch = gdk_screen_get_height(screen);
    /* FIXME: read the gtk_window_get_position() docs about how this is
     * a broken approach */
    if (position) {
        gtk_window_get_position(window, &x, &y);
        if (x >= 0 && y >= 0 && x+1 < scw && y+1 < sch) {
            strcpy(key + len, "/position/x");
            gwy_container_set_int32_by_name(settings, key, x);
            strcpy(key + len, "/position/y");
            gwy_container_set_int32_by_name(settings, key, y);
        }
    }
    if (size) {
        gtk_window_get_size(window, &w, &h);
        if (w > 1 && h > 1) {
            strcpy(key + len, "/position/width");
            gwy_container_set_int32_by_name(settings, key, w);
            strcpy(key + len, "/position/height");
            gwy_container_set_int32_by_name(settings, key, h);
        }
    }
}

/**
 * gwy_app_restore_window_position:
 * @window: A window to restore position of.
 * @prefix: Unique prefix in settings to get the information from (the same as
 *          in gwy_app_save_window_position()).
 * @grow_only: %TRUE to only attempt set the window default size bigger than it
 *              requests, never smaller.
 *
 * Restores a window position and/or size from settings.
 *
 * Unlike gwy_app_save_window_position(), this function has no @position and
 * @size arguments, it simply restores all attributes that were saved.
 *
 * Note to restore position (not size) it should be called twice for each
 * window to accommodate sloppy window managers: once before the window is
 * shown, second time immediately after showing the window.
 *
 * Some sanity checks are included, therefore if saved window position and/or
 * size is too suspicious, it is not restored.
 **/
void
gwy_app_restore_window_position(GtkWindow *window,
                                const gchar *prefix,
                                gboolean grow_only)
{
    GwyContainer *settings;
    GtkRequisition req;
    GdkScreen *screen;
    gint x, y, w, h, scw, sch;
    guint len;
    gchar *key;

    g_return_if_fail(GTK_IS_WINDOW(window));
    g_return_if_fail(prefix);

    len = strlen(prefix);
                              /* The longest suffix */
    key = g_newa(gchar, len + sizeof("/position/height"));
    strcpy(key, prefix);

    settings = gwy_app_settings_get();
    screen = gtk_window_get_screen(window);
    scw = gdk_screen_get_width(screen);
    sch = gdk_screen_get_height(screen);
    x = y = w = h = -1;
    strcpy(key + len, "/position/x");
    gwy_container_gis_int32_by_name(settings, key, &x);
    strcpy(key + len, "/position/y");
    gwy_container_gis_int32_by_name(settings, key, &y);
    strcpy(key + len, "/position/width");
    gwy_container_gis_int32_by_name(settings, key, &w);
    strcpy(key + len, "/position/height");
    gwy_container_gis_int32_by_name(settings, key, &h);
    if (x >= 0 && y >= 0 && x+1 < scw && y+1 < sch)
        gtk_window_move(window, x, y);
    if (w > 1 && h > 1) {
        if (grow_only) {
            gtk_widget_size_request(GTK_WIDGET(window), &req);
            w = MAX(w, req.width);
            h = MAX(h, req.height);
        }
        gtk_window_set_default_size(window, w, h);
    }
}

gint
_gwy_app_get_n_recent_files(void)
{
    return 10;
}

/**
 * gwy_app_init_widget_styles:
 *
 * Sets up style properties for special Gwyddion widgets.
 *
 * Normally not needed to call explicitly.
 **/
void
gwy_app_init_widget_styles(void)
{
    gtk_rc_parse_string(/* data window corner buttons */
                        "style \"cornerbutton\" {\n"
                        "GtkButton::focus_line_width = 0\n"
                        "GtkButton::focus_padding = 0\n"
                        "}\n"
                        "widget \"*.cornerbutton\" style \"cornerbutton\"\n"
                        "\n"
                        /* toolbox group header buttons */
                        "style \"toolboxheader\" {\n"
                        "GtkButton::focus_line_width = 0\n"
                        "GtkButton::focus_padding = 0\n"
                        "}\n"
                        "widget \"*.toolboxheader\" style \"toolboxheader\"\n"
                        "\n"
                        /* toolbox single-item menubars */
                        "style \"toolboxmenubar\" {\n"
                        "GtkMenuBar::shadow_type = 0\n"
                        "}\n"
                        "widget \"*.toolboxmenubar\" style \"toolboxmenubar\"\n"
                        "\n");
}

/**
 * gwy_app_init_i18n:
 *
 * Initializes internationalization.
 *
 * Normally not needed to call explicitly.
 **/
void
gwy_app_init_i18n(void)
{
#ifdef ENABLE_NLS
    gchar *locdir;

    locdir = gwy_find_self_dir("locale");
    bindtextdomain(PACKAGE, locdir);
    g_free(locdir);
    textdomain(PACKAGE);
    if (!bind_textdomain_codeset(PACKAGE, "UTF-8"))
        g_critical("Cannot bind gettext `%s' codeset to UTF-8", PACKAGE);
#endif  /* ENABLE_NLS */
}

/**
 * gwy_app_init_common:
 * @error: Error location for settings loading error.
 * @...: List of module types to load, terminated with %NULL.  Possible types
 *       are "file", "graph", "layer", "process", "tool" plus two special
 *       values "" and "all" for untyped modules (like plug-in proxy) and
 *       all modules, respectively.
 *
 * Performs common initialization.
 *
 * FIXME: Much more to say.
 *
 * Returns: Settings loading status.
 **/
gboolean
gwy_app_init_common(GError **error,
                    ...)
{
    gchar *settings_file;
    va_list ap;
    const gchar *dir;
    gboolean ok = TRUE;

    gwy_widgets_type_init();
    gwy_app_init_widget_styles();
    gwy_app_init_i18n();

    gwy_data_window_class_set_tooltips(gwy_app_get_tooltips());
    gwy_3d_window_class_set_tooltips(gwy_app_get_tooltips());
    gwy_graph_window_class_set_tooltips(gwy_app_get_tooltips());

    /* Register resources */
    gwy_stock_register_stock_items();
    gwy_resource_class_load(g_type_class_peek(GWY_TYPE_GRADIENT));
    gwy_resource_class_load(g_type_class_peek(GWY_TYPE_GL_MATERIAL));
    gwy_resource_class_load(g_type_class_peek(GWY_TYPE_GRAIN_VALUE));
    gwy_resource_class_load(g_type_class_peek(GWY_TYPE_CALIBRATION));


    /* Load settings */
    settings_file = gwy_app_settings_get_settings_filename();
    if (g_file_test(settings_file, G_FILE_TEST_IS_REGULAR))
        ok = gwy_app_settings_load(settings_file, error);
    gwy_app_settings_get();

    /* Register modules */
    va_start(ap, error);
    dir = va_arg(ap, const gchar*);
    va_end(ap);
    if (dir && gwy_strequal(dir, "all")) {
        gchar **module_dirs;

        module_dirs = gwy_app_settings_get_module_dirs();
        gwy_module_register_modules((const gchar**)module_dirs);
        g_strfreev(module_dirs);
    }
    else {
        GPtrArray *module_dirs;
        gchar *p;
        guint i;

        module_dirs = g_ptr_array_new();

        p = gwy_find_self_dir("modules");
        va_start(ap, error);
        while ((dir = va_arg(ap, const gchar*))) {
            g_ptr_array_add(module_dirs,
                            g_build_filename(p, *dir ? dir : NULL,
                                             NULL));
        }
        va_end(ap);
        g_free(p);

        va_start(ap, error);
        while ((dir = va_arg(ap, const gchar*))) {
            g_ptr_array_add(module_dirs,
                            g_build_filename(p, "modules", *dir ? dir : NULL,
                                             NULL));
        }
        va_end(ap);

        g_ptr_array_add(module_dirs, NULL);
        gwy_module_register_modules((const gchar**)module_dirs->pdata);

        for (i = 0; i < module_dirs->len-1; i++)
            g_free(module_dirs->pdata[i]);
        g_ptr_array_free(module_dirs, TRUE);
    }

    return ok;
}

/************************** Documentation ****************************/

/**
 * SECTION:app
 * @title: app
 * @short_description: Core application interface, window management
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
