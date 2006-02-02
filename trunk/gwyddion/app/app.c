/*
 *  @(#) $Id$
 *  Copyright (C) 2003,2004 David Necas (Yeti), Petr Klapetek.
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
#include <stdarg.h>
#include <string.h>

#include <libgwyddion/gwyddion.h>
#include <libprocess/datafield.h>
#include <libgwymodule/gwymodule.h>
#include <libgwydgets/gwydgets.h>
#include <libgwydgets/gwygraphwindow.h>
#include <app/gwyapp.h>
#include "gwyappinternal.h"

static GtkWidget *gwy_app_main_window = NULL;

/* list of existing windows of different kinds. FIXME: maybe some common
 * management functions could be factored out? Unfortunately the data window
 * managements logic differs slightly from the other two. */
static GList *current_data = NULL;
static GList *current_graph = NULL;
static GList *current_3d = NULL;
static GtkWidget *current_any = NULL;

static const gchar* current_tool = NULL;
static gint untitled_no = 0;

static GHookList window_list_hook_list;

static gboolean   gwy_app_confirm_quit             (void);
static void       gather_unsaved_cb                (GwyDataWindow *data_window,
                                                    GSList **unsaved);
static gboolean   gwy_app_confirm_quit_dialog      (GSList *unsaved);
static void       gwy_app_data_view_setup_layers1  (GwyDataView *data_view);
static void       gwy_app_data_view_setup_layers2  (GwyDataView *data_view);
static void       gwy_app_data_view_mask_changed   (GwyContainer *data,
                                                    GQuark quark,
                                                    GwyDataView *data_view);
static void       gwy_app_data_view_show_changed   (GwyLayerBasic *basic_layer,
                                                    GwyDataView *data_view);
static void       gwy_app_container_setup_mask     (GwyContainer *data);
static void       gwy_app_data_window_list_updated (void);
static GtkWidget* gwy_app_menu_data_popup_create   (GtkAccelGroup *accel_group);
static gboolean   gwy_app_data_popup_menu_popup_mouse(GtkWidget *menu,
                                                      GdkEventButton *event,
                                                      GtkWidget *view);
static void       gwy_app_data_popup_menu_popup_key(GtkWidget *menu,
                                                    GtkWidget *data_window);
static void       gwy_app_set_current_window       (GtkWidget *window);
static void       gwy_app_3d_window_orphaned       (GtkWidget *view,
                                                    GtkWidget *gwy3dwindow);
static void       gwy_app_3d_window_destroyed      (GtkWidget *gwy3dwindow,
                                                    GtkWidget *view);
static void       gwy_app_3d_window_title_changed  (GtkWidget *data_window,
                                                    GtkWidget *gwy3dwindow);
static void       gwy_app_3d_window_export         (Gwy3DWindow *window);

/* FIXME: A temporary hack. */
static void
set_sensitivity(GtkItemFactory *item_factory, ...)
{
    GwySensitivityGroup *sensgroup;
    GwyMenuSensFlags mask;
    const gchar *path;
    GtkWidget *widget;
    va_list ap;

    sensgroup = gwy_app_sensitivity_get_group();
    va_start(ap, item_factory);
    while ((path = va_arg(ap, const gchar*))) {
        mask = va_arg(ap, guint);
        widget = gtk_item_factory_get_widget(item_factory, path);
        if (!widget)
            break;
        gwy_sensitivity_group_add_widget(sensgroup, widget, mask);
    }
    va_end(ap);
}

/*****************************************************************************
 *                                                                           *
 *     Main, toolbox                                                         *
 *                                                                           *
 *****************************************************************************/

gboolean
gwy_app_quit(void)
{
    GtkWidget *widget;

    gwy_debug("");
    if (!gwy_app_confirm_quit())
        return TRUE;

    /* XXX
    while ((widget = (GtkWidget*)(gwy_app_graph_window_get_current())))
        gtk_widget_destroy(widget);
    while ((widget = (GtkWidget*)(gwy_app_3d_window_get_current())))
        gtk_widget_destroy(widget);
    while ((widget = (GtkWidget*)(gwy_app_data_window_get_current())))
        gtk_widget_destroy(widget);
        */
    gwy_app_data_browser_blow_up();

    gtk_main_quit();
    return TRUE;
}

gboolean
gwy_app_main_window_save_position(void)
{
    GwyContainer *settings;
    GdkScreen *screen;
    gint x, y, w, h;

    g_return_val_if_fail(GTK_IS_WINDOW(gwy_app_main_window), FALSE);

    settings = gwy_app_settings_get();
    screen = gtk_window_get_screen(GTK_WINDOW(gwy_app_main_window));
    w = gdk_screen_get_width(screen);
    h = gdk_screen_get_height(screen);
    /* FIXME: read the gtk_window_get_position() docs about how this is
     * a broken approach */
    gtk_window_get_position(GTK_WINDOW(gwy_app_main_window), &x, &y);
    if (x >= 0 && y >= 0 && x < w && y < h) {
        gwy_container_set_int32_by_name(settings, "/app/toolbox/position/x", x);
        gwy_container_set_int32_by_name(settings, "/app/toolbox/position/y", y);
    }

    /* to be usable as an event handler */
    return FALSE;
}

void
gwy_app_main_window_restore_position(void)
{
    GwyContainer *settings;
    gint x, y;

    g_return_if_fail(GTK_IS_WINDOW(gwy_app_main_window));

    settings = gwy_app_settings_get();
    if (gwy_container_gis_int32_by_name(settings,
                                        "/app/toolbox/position/x", &x)
        && gwy_container_gis_int32_by_name(settings,
                                           "/app/toolbox/position/y", &y)) {
        gtk_window_move(GTK_WINDOW(gwy_app_main_window), x, y);
    }
}

/**
 * gwy_app_add_main_accel_group:
 * @window: A window.
 *
 * Adds main (global) application accelerator group to a window.
 **/
void
gwy_app_add_main_accel_group(GtkWindow *window)
{
    GtkWidget *main_window;
    GtkAccelGroup *accel_group;

    g_return_if_fail(GTK_IS_WINDOW(window));
    main_window = gwy_app_main_window_get();
    g_return_if_fail(GTK_IS_WINDOW(main_window));

    accel_group = GTK_ACCEL_GROUP(g_object_get_data(G_OBJECT(main_window),
                                                    "accel_group"));
    g_return_if_fail(GTK_IS_ACCEL_GROUP(accel_group));
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
    if (!gwy_app_main_window)
        g_critical("Trying to access app main window before its creation");
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
    if (gwy_app_main_window && window != gwy_app_main_window)
        g_critical("Trying to change app main window");
    if (!GTK_IS_WINDOW(window))
        g_critical("Setting app main window to a non-GtkWindow");
    gwy_app_main_window = window;
}

static gboolean
gwy_app_confirm_quit(void)
{
    GSList *unsaved = NULL;
    gboolean ok;

    gwy_app_data_window_foreach((GFunc)gather_unsaved_cb, &unsaved);
    if (!unsaved)
        return TRUE;
    ok = gwy_app_confirm_quit_dialog(unsaved);
    g_slist_free(unsaved);

    return ok;
}

static void
gather_unsaved_cb(GwyDataWindow *data_window,
                  GSList **unsaved)
{
    GwyContainer *data = gwy_data_window_get_data(data_window);

    if (gwy_undo_container_get_modified(data))
        *unsaved = g_slist_prepend(*unsaved, data_window);
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
        gchar *filename = gwy_data_window_get_base_name(data_window);

        text = g_strconcat(filename, "\n", text, NULL);
        unsaved = g_slist_next(unsaved);
        g_free(filename);
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
 * gwy_app_data_window_get_current:
 *
 * Returns the currently active data window, may be %NULL if none is active.
 *
 * Returns: The active data window as a #GwyDataWindow.
 **/
GwyDataWindow*
gwy_app_data_window_get_current(void)
{
    return current_data ? (GwyDataWindow*)current_data->data : NULL;
}

/**
 * gwy_app_get_current_data:
 *
 * Returns the data of currently active data window.
 *
 * Returns: The current data as a #GwyContainer.
 *          May return %NULL if none is currently active.
 **/
GwyContainer*
gwy_app_get_current_data(void)
{
    GwyDataWindow *data_window;

    data_window = gwy_app_data_window_get_current();
    if (!data_window)
        return NULL;

    return gwy_data_window_get_data(data_window);
}

/**
 * gwy_app_data_window_set_current:
 * @window: A data window.
 *
 * Makes a data window current, including tool switch, etc.
 *
 * The window must be present in the list.
 *
 * Returns: Always FALSE, no matter what (to be usable as an event handler).
 **/
gboolean
gwy_app_data_window_set_current(GwyDataWindow *window)
{
    GwyMenuSensFlags mask = (GWY_MENU_FLAG_DATA
                             | GWY_MENU_FLAG_UNDO
                             | GWY_MENU_FLAG_REDO
                             | GWY_MENU_FLAG_DATA_MASK
                             | GWY_MENU_FLAG_DATA_SHOW);
    GwyMenuSensFlags state = GWY_MENU_FLAG_DATA;
    static GwyDataWindow *already_current = NULL;
    GwyDataView *data_view;
    GwyPixmapLayer *layer;
    GList *item;
    GwyContainer *data;

    gwy_debug("win = %p, tool = %p", window, current_tool);
    gwy_app_set_current_window(GTK_WIDGET(window));

    data_view = gwy_data_window_get_data_view(window);
    gwy_app_data_browser_select_data_view(data_view);

    if (already_current == window) {
        g_assert(current_data && current_data->data == (gpointer)window);
        return FALSE;
    }

    g_return_val_if_fail(GWY_IS_DATA_WINDOW(window), FALSE);
    item = g_list_find(current_data, window);
    if (item) {
        current_data = g_list_remove_link(current_data, item);
        current_data = g_list_concat(item, current_data);
    }
    else
        current_data = g_list_prepend(current_data, window);

    if (current_tool)
        gwy_tool_func_use(current_tool, window, GWY_TOOL_SWITCH_WINDOW);

    data = gwy_data_view_get_data(data_view);
    if (gwy_undo_container_has_undo(data))
        state |= GWY_MENU_FLAG_UNDO;
    if (gwy_undo_container_has_redo(data))
        state |= GWY_MENU_FLAG_REDO;

    layer = gwy_data_view_get_base_layer(data_view);
    if (gwy_layer_basic_get_has_presentation(GWY_LAYER_BASIC(layer)))
        state |= GWY_MENU_FLAG_DATA_SHOW;
    if (gwy_data_view_get_alpha_layer(data_view))
        state |= GWY_MENU_FLAG_DATA_MASK;

    gwy_app_sensitivity_set_state(mask, state);
    already_current = window;

    return FALSE;
}

/**
 * gwy_app_data_window_remove:
 * @window: A data window.
 *
 * Removes the data window @window from the list of data windows.
 *
 * All associated structures are freed, active tool gets switched to %NULL
 * window.  But the widget itself is NOT destroyed by this function.
 * It just makes the application `forget' about the window.
 **/
void
gwy_app_data_window_remove(GwyDataWindow *window)
{
    GwyMenuSensFlags mask = (GWY_MENU_FLAG_DATA
                             | GWY_MENU_FLAG_UNDO
                             | GWY_MENU_FLAG_REDO
                             | GWY_MENU_FLAG_DATA_MASK
                             | GWY_MENU_FLAG_DATA_SHOW);
    GList *item;

    gwy_debug("");
    g_return_if_fail(GWY_IS_DATA_WINDOW(window));

    item = g_list_find(current_data, window);
    if (!item) {
        g_critical("Trying to remove GwyDataWindow %p not present in the list",
                   window);
        return;
    }
    current_data = g_list_delete_link(current_data, item);
    gwy_debug("Removed window, %p is new head\n",
              current_data ? current_data->data : NULL);
    if (current_data) {
        gwy_app_data_window_set_current(GWY_DATA_WINDOW(current_data->data));
        gwy_app_data_window_list_updated();
        return;
    }

    if (current_tool)
        gwy_tool_func_use(current_tool, NULL, GWY_TOOL_SWITCH_WINDOW);
    gwy_app_sensitivity_set_state(mask, 0);

    gwy_app_data_window_list_updated();
}

/**
 * gwy_app_data_window_create:
 * @data: A data container.
 *
 * Creates a new data window showing @data and does some basic setup.
 *
 * Also calls gtk_window_present() on it.
 *
 * Returns: The newly created data window.
 **/
GtkWidget*
gwy_app_data_window_create(GwyContainer *data)
{
    /*
    GtkWidget *window, *view;
    GwyDataWindow *data_window;
    GwyDataView *data_view;
    */

    g_warning("gwy_app_data_window_create() is deprecated and broken");
    gwy_app_data_browser_add(data);
    return NULL;

    /*
    g_return_val_if_fail(GWY_IS_CONTAINER(data), NULL);
    if (!popup_menu) {
        popup_menu = gwy_app_menu_data_popup_create(NULL);
        gtk_widget_show_all(popup_menu);
    }

    view = gwy_data_view_new(data);
    data_view = GWY_DATA_VIEW(view);
    gwy_app_data_view_setup_layers1(data_view);
    window = gwy_data_window_new(data_view);
    data_window = GWY_DATA_WINDOW(window);

    g_signal_connect(data_window, "focus-in-event",
                     G_CALLBACK(gwy_app_data_window_set_current), NULL);
    g_signal_connect(data_window, "destroy",
                     G_CALLBACK(gwy_app_data_window_remove), NULL);

    gwy_app_data_window_add(data_window);
    gwy_app_sensitivity_set_state(GWY_MENU_FLAG_DATA, GWY_MENU_FLAG_DATA);
    gwy_app_data_view_setup_layers2(data_view);
    gtk_window_present(GTK_WINDOW(window));

    gwy_app_data_window_list_updated();

    return window;
    */
}

void
gwy_app_data_window_setup(GwyDataWindow *data_window)
{
    static GtkWidget *popup_menu = NULL;

    GwyDataView *data_view;

    if (!popup_menu) {
        popup_menu = gwy_app_menu_data_popup_create(NULL);
        gtk_widget_show_all(popup_menu);
    }

    data_view = gwy_data_window_get_data_view(data_window);
    g_signal_connect_swapped(data_view, "button-press-event",
                             G_CALLBACK(gwy_app_data_popup_menu_popup_mouse),
                             popup_menu);
    g_signal_connect_swapped(data_window, "popup-menu",
                             G_CALLBACK(gwy_app_data_popup_menu_popup_key),
                             popup_menu);
}

static void
gwy_app_data_view_setup_layers1(GwyDataView *data_view)
{
    GwyPixmapLayer *layer;
    GwyLayerBasic *blayer;

    /* base */
    gwy_debug("%p %p", data_view, data);
    layer = gwy_layer_basic_new();
    blayer = GWY_LAYER_BASIC(layer);
    gwy_pixmap_layer_set_data_key(layer, "/0/data");
    gwy_layer_basic_set_presentation_key(blayer, "/0/show");
    gwy_layer_basic_set_gradient_key(blayer, "/0/base/palette");
    gwy_layer_basic_set_range_type_key(blayer, "/0/base/range-type");
    gwy_layer_basic_set_min_max_key(blayer, "/0/base");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(data_view), layer);
}

static void
gwy_app_data_view_setup_layers2(GwyDataView *data_view)
{
    GwyLayerBasic *blayer;
    GwyContainer *data;

    data = gwy_data_view_get_data(data_view);
    blayer = GWY_LAYER_BASIC(gwy_data_view_get_base_layer(data_view));

    /* force sync */
    gwy_app_data_view_mask_changed(data, g_quark_from_string("/0/mask"),
                                   data_view);
    gwy_app_data_view_show_changed(blayer, data_view);

    g_signal_connect(data, "item-changed::/0/mask",
                     G_CALLBACK(gwy_app_data_view_mask_changed), data_view);
    g_signal_connect(blayer, "presentation-switched",
                     G_CALLBACK(gwy_app_data_view_show_changed), data_view);
}

/**
 * gwy_app_data_view_mask_changed:
 * @data: A container coreesponding to @data_view.
 * @data_view: A data view.
 *
 * Adds or removes alpha layer depending on container contents.
 **/
static void
gwy_app_data_view_mask_changed(GwyContainer *data,
                               GQuark quark,
                               GwyDataView *data_view)
{
    gboolean has_dfield, has_layer;
    const gchar *key;
    GwyPixmapLayer *layer;

    has_dfield = gwy_container_contains(data, quark);
    has_layer = gwy_data_view_get_alpha_layer(data_view) != NULL;
    gwy_debug("has_dfield: %d, has_layer: %d\n", has_dfield, has_layer);

    if (has_dfield && !has_layer) {
        key = g_quark_to_string(quark);
        gwy_app_container_setup_mask(data);
        layer = gwy_layer_mask_new();
        gwy_pixmap_layer_set_data_key(layer, key);
        /* TODO: Container */
        gwy_layer_mask_set_color_key(GWY_LAYER_MASK(layer), key);
        gwy_data_view_set_alpha_layer(data_view, layer);
    }
    else if (!has_dfield && has_layer)
        gwy_data_view_set_alpha_layer(data_view, NULL);

    if (has_dfield != has_layer
        && data == gwy_app_get_current_data()) {
        gwy_app_sensitivity_set_state(GWY_MENU_FLAG_DATA_MASK,
                                      has_dfield ? GWY_MENU_FLAG_DATA_MASK : 0);
    }
}

static void
gwy_app_data_view_show_changed(GwyLayerBasic *basic_layer,
                               GwyDataView *data_view)
{
    gboolean has_show;

    if (gwy_data_view_get_data(data_view) != gwy_app_get_current_data())
        return;

    has_show = gwy_layer_basic_get_has_presentation(basic_layer);
    gwy_app_sensitivity_set_state(GWY_MENU_FLAG_DATA_SHOW,
                                  has_show ? GWY_MENU_FLAG_DATA_SHOW: 0);
}

/**
 * gwy_app_container_setup_mask:
 * @data: A data container.
 *
 * Eventually copies default mask color to particular data mask color.
 **/
static void
gwy_app_container_setup_mask(GwyContainer *data)
{
    static const gchar *keys[] = {
        "/0/mask/red", "/0/mask/green", "/0/mask/blue", "/0/mask/alpha"
    };

    GwyContainer *settings;
    gdouble x;
    guint i;

    settings = gwy_app_settings_get();
    for (i = 0; i < G_N_ELEMENTS(keys); i++) {
        if (gwy_container_contains_by_name(data, keys[i]))
            continue;
        /* be noisy when we don't have default mask color */
        x = gwy_container_get_double_by_name(settings, keys[i] + 2);
        gwy_container_set_double_by_name(data, keys[i], x);
    }
}

static void
gwy_app_data_window_list_updated(void)
{
    gwy_debug("");

    if (!window_list_hook_list.is_setup)
        return;

    g_hook_list_invoke(&window_list_hook_list, FALSE);
}

/**
 * gwy_app_data_window_list_add_hook:
 * @func: Function to be called (with @data as its only argument).
 * @data: Data passed to @func.
 *
 * Adds a hook function called just after a data window is created or
 * destroyed.
 *
 * Returns: Hook id to be used in gwy_app_data_window_list_remove_hook().
 **/
gulong
gwy_app_data_window_list_add_hook(gpointer func,
                                  gpointer data)
{
    GHook *hook;

    gwy_debug("");
    if (!window_list_hook_list.is_setup) {
        gwy_debug("initializing window_list_hook_list");
        g_hook_list_init(&window_list_hook_list, sizeof(GHook));
    }

    hook = g_hook_alloc(&window_list_hook_list);
    hook->func = func;
    hook->data = data;
    g_hook_append(&window_list_hook_list, hook);
    gwy_debug("id = %lu", hook->hook_id);

    return hook->hook_id ;
}

/**
 * gwy_app_data_window_list_remove_hook:
 * @hook_id: Hook id, as returned by gwy_app_data_window_list_add_hook().
 *
 * Removes a data window list hook function added by
 * gwy_app_data_window_list_add_hook().
 *
 * Returns: Whether such a hook was found and removed.
 **/
gboolean
gwy_app_data_window_list_remove_hook(gulong hook_id)
{
    gwy_debug("");
    g_return_val_if_fail(window_list_hook_list.is_setup, FALSE);

    return g_hook_destroy(&window_list_hook_list, hook_id);
}

/**
 * gwy_app_data_window_foreach:
 * @func: A function to call on each data window.
 * @user_data: Data to pass to @func.
 *
 * Calls @func on each data window, in no particular order.
 *
 * The function should not create or remove data windows.
 **/
void
gwy_app_data_window_foreach(GFunc func,
                            gpointer user_data)
{
    GList *l;

    for (l = current_data; l; l = g_list_next(l))
        func(l->data, user_data);
}

/**
 * gwy_app_data_window_get_for_data:
 * @data: A data container.
 *
 * Finds a data window displaying given data.
 *
 * Returns: Data window displaying given data, or %NULL if there is no such
 *          data window.
 **/
GwyDataWindow*
gwy_app_data_window_get_for_data(GwyContainer *data)
{
    GList *l;

    for (l = current_data; l; l = g_list_next(l)) {
        if (gwy_data_window_get_data((GwyDataWindow*)l->data) == data)
            return (GwyDataWindow*)l->data;
    }
    return NULL;
}

/*****************************************************************************
 *                                                                           *
 *     Graph window list management                                          *
 *                                                                           *
 *****************************************************************************/

/**
 * gwy_app_graph_window_get_current:
 *
 * Returns the currently active graph window.
 *
 * Returns: The active graph window as a #GtkWidget.
 *          May return %NULL if none is currently active.
 **/
GtkWidget*
gwy_app_graph_window_get_current(void)
{
    return current_graph ? current_graph->data : NULL;
}

/**
 * gwy_app_graph_window_set_current:
 * @window: A graph window.
 *
 * Makes a graph window current.
 *
 * Eventually adds @window it to the graph window list if it isn't present
 * there.
 *
 * Returns: Always FALSE, no matter what (to be usable as an event handler).
 **/
gboolean
gwy_app_graph_window_set_current(GtkWidget *window)
{
    GtkWidget *graph;
    GList *item;

    gwy_debug("%p", window);

    item = g_list_find(current_graph, window);
    if (item) {
        current_graph = g_list_remove_link(current_graph, item);
        current_graph = g_list_concat(item, current_graph);
    }
    else
        current_graph = g_list_prepend(current_graph, window);

    gwy_app_sensitivity_set_state(GWY_MENU_FLAG_GRAPH, GWY_MENU_FLAG_GRAPH);
    graph = gwy_graph_window_get_graph(GWY_GRAPH_WINDOW(window));
    gwy_app_data_browser_select_graph(GWY_GRAPH(graph));
    gwy_app_set_current_window(window);

    return FALSE;
}

/**
 * gwy_app_graph_window_remove:
 * @window: A graph window.
 *
 * Removes the graph window @window from the list of graph windows.
 *
 * All associated structures are freed, but the widget itself is NOT destroyed
 * by this function.  It just makes the application `forget' about the window.
 **/
void
gwy_app_graph_window_remove(GtkWidget *window)
{
    GList *item;

    item = g_list_find(current_graph, window);
    if (!item) {
        g_critical("Trying to remove GwyGraph %p not present in the list",
                   window);
        return;
    }
    current_graph = g_list_delete_link(current_graph, item);
    if (current_graph)
        gwy_app_graph_window_set_current(current_graph->data);
    else
        gwy_app_sensitivity_set_state(GWY_MENU_FLAG_GRAPH, 0);
}

/**
 * gwy_app_graph_window_foreach:
 * @func: A function to call on each graph window.
 * @user_data: Data to pass to @func.
 *
 * Calls @func on each graph window, in no particular order.
 *
 * The function should not create or remove graph windows.
 **/
void
gwy_app_graph_window_foreach(GFunc func,
                             gpointer user_data)
{
    GList *l;

    for (l = current_graph; l; l = g_list_next(l))
        func(l->data, user_data);
}

/**
 * gwy_app_graph_window_create:
 * @graph: A graph widget.
 * @data: A data container to put the graph model to.
 *
 * Creates a new graph window showing a graph and does some basic setup.
 *
 * It calls gwy_app_graph_list_add() and does not assume a reference on the
 * graph model, so you usually wish to unreference it after this call.
 *
 * Also calls gtk_window_present() on the newly created window, associates
 * it with a data window, and sets its title.
 *
 * Returns: The newly created graph window.
 **/
GtkWidget*
gwy_app_graph_window_create(GwyGraph *graph,
                            GwyContainer *data)
{
    g_return_val_if_fail(GWY_IS_GRAPH(graph), NULL);
    g_return_val_if_fail(GWY_IS_CONTAINER(data), NULL);

    g_warning("gwy_app_graph_window_create() is deprecated and broken");
    gwy_app_data_browser_add_graph_model(gwy_graph_get_model(graph),
                                         data, FALSE);
    gtk_widget_destroy(GTK_WIDGET(graph));
    return NULL;
}

/*****************************************************************************
 *                                                                           *
 *     3D window list management                                             *
 *                                                                           *
 *****************************************************************************/

void
gwy_app_3d_view_cb(void)
{
    GwyDataWindow *data_window;

    data_window = gwy_app_data_window_get_current();
    g_return_if_fail(data_window);
    gwy_app_3d_window_create(data_window);
}

/**
 * gwy_app_3d_window_create:
 * @data_window: A data window to create associated 3D view for.
 *
 * Creates a new 3D view window showing the same data as @data_window.
 *
 * Also does some housekeeping and calls gtk_window_present() on it.
 *
 * Returns: The newly created 3D view window.
 **/
GtkWidget*
gwy_app_3d_window_create(GwyDataWindow *data_window)
{
    GtkWidget *gwy3dview, *gwy3dwindow, *button;
    GwyDataView *view;
    GwyContainer *data;
    gchar *name, *title;

    g_return_val_if_fail(GWY_IS_DATA_WINDOW(data_window), NULL);
    view = gwy_data_window_get_data_view(data_window);
    data = gwy_data_view_get_data(view);
    g_return_val_if_fail(GWY_IS_CONTAINER(data), NULL);

    gwy3dview = gwy_3d_view_new(data);
    gwy_3d_view_set_data_key(GWY_3D_VIEW(gwy3dview), "/0/data");
    gwy_3d_view_set_gradient_key(GWY_3D_VIEW(gwy3dview), "/0/3d/palette");
    gwy_3d_view_set_material_key(GWY_3D_VIEW(gwy3dview), "/0/3d/material");
    gwy3dwindow = gwy_3d_window_new(GWY_3D_VIEW(gwy3dview));
    gwy_app_add_main_accel_group(GTK_WINDOW(gwy3dwindow));

    name = gwy_data_window_get_base_name(data_window);
    title = g_strconcat("3D ", name, NULL);
    gtk_window_set_title(GTK_WINDOW(gwy3dwindow), title);
    g_free(title);
    g_free(name);

    button = gwy_stock_like_button_new(_("Export"), GTK_STOCK_SAVE);
    gwy_3d_window_add_action_widget(GWY_3D_WINDOW(gwy3dwindow), button);
    gwy_3d_window_add_small_toolbar_button(GWY_3D_WINDOW(gwy3dwindow),
                                           GTK_STOCK_SAVE,
                                           _("Export 3D view to PNG image"),
                                           G_CALLBACK(gwy_app_3d_window_export),
                                           gwy3dwindow);

    current_3d = g_list_append(current_3d, gwy3dwindow);
    /*
     * There are some signal cross-connections.
     *
     * object    signal            handler           data      destroy in
     *
     * window    "title-changed"   title_changed()   3dwindow  destroyed()
     * window    "destroyed"       orphaned()        3dwindow  destroyed()
     * view      "updated"         3d_view_update()  3dview    destroyed()
     * 3dwindow  "destroy"         destroyed()       window    orphaned()
     * 3dwindow  "focun-in-event"  set_current()     NULL      --
     * button    "clicked"         export()          3dwindow  --
     */
    g_signal_connect(data_window, "title-changed",
                     G_CALLBACK(gwy_app_3d_window_title_changed), gwy3dwindow);
    g_signal_connect(data_window, "destroy",
                     G_CALLBACK(gwy_app_3d_window_orphaned), gwy3dwindow);
    g_signal_connect(gwy3dwindow, "focus-in-event",
                     G_CALLBACK(gwy_app_3d_window_set_current), NULL);
    g_signal_connect(gwy3dwindow, "destroy",
                     G_CALLBACK(gwy_app_3d_window_destroyed), data_window);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(gwy_app_3d_window_export), gwy3dwindow);

    gtk_widget_show_all(gwy3dwindow);
    gtk_window_present(GTK_WINDOW(gwy3dwindow));

    return gwy3dwindow;
}

/**
 * gwy_app_3d_window_get_current:
 *
 * Returns the currently active 3D view window.
 *
 * Returns: The active 3D view window as a #GtkWidget.
 *          May return %NULL if none is currently active.
 **/
GtkWidget*
gwy_app_3d_window_get_current(void)
{
    return current_3d ? current_3d->data : NULL;
}

/**
 * gwy_app_3d_window_set_current:
 * @window: A 3D view window.
 *
 * Makes a 3D view window current.
 *
 * Eventually adds @window it to the 3D view window list if it isn't present
 * there.
 *
 * Returns: Always FALSE, no matter what (to be usable as an event handler).
 **/
gboolean
gwy_app_3d_window_set_current(GtkWidget *window)
{
    /*
    static const GwyMenuSensData sens_data = {
        GWY_MENU_FLAG_3D, GWY_MENU_FLAG_3D
    };
    */
    GList *item;

    gwy_debug("%p", window);

    item = g_list_find(current_3d, window);
    g_return_val_if_fail(item, FALSE);
    current_3d = g_list_remove_link(current_3d, item);
    current_3d = g_list_concat(item, current_3d);

    gwy_app_set_current_window(window);

    return FALSE;
}

/**
 * gwy_app_3d_window_remove:
 * @window: A 3D view window.
 *
 * Removes the 3D view window @window from the list of 3D view windows.
 *
 * All associated structures are freed, but the widget itself is NOT destroyed
 * by this function.  It just makes the application `forget' about the window.
 **/
void
gwy_app_3d_window_remove(GtkWidget *window)
{
    /*
    static const GwyMenuSensData sens_data = {
        GWY_MENU_FLAG_3D, 0
    };
    */
    GList *item;

    g_return_if_fail(GWY_IS_3D_WINDOW(window));

    item = g_list_find(current_3d, window);
    if (!item) {
        g_critical("Trying to remove 3D window %p not present in the list",
                   window);
        return;
    }
    g_free(g_object_get_data(G_OBJECT(item->data), "gwy-app-export-filename"));
    current_3d = g_list_delete_link(current_3d, item);
    if (current_3d)
        gwy_app_3d_window_set_current(current_3d->data);
    /* FIXME: hangs.
    else
        gwy_app_toolbox_update_state(&sens_data);
        */
}

static void
gwy_app_3d_window_orphaned(GtkWidget *data_window,
                           GtkWidget *gwy3dwindow)
{
    g_signal_handlers_disconnect_matched(gwy3dwindow,
                                         G_SIGNAL_MATCH_FUNC
                                         | G_SIGNAL_MATCH_DATA,
                                         0, 0, NULL,
                                         gwy_app_3d_window_destroyed,
                                         data_window);
}

static void
gwy_app_3d_window_title_changed(GtkWidget *data_window,
                                GtkWidget *gwy3dwindow)
{
    gchar *name, *title;

    name = gwy_data_window_get_base_name(GWY_DATA_WINDOW(data_window));
    title = g_strconcat("3D ", name, NULL);
    gtk_window_set_title(GTK_WINDOW(gwy3dwindow), title);
    g_free(title);
    g_free(name);
}

static void
gwy_app_3d_window_destroyed(GtkWidget *gwy3dwindow,
                            GtkWidget *data_window)
{
    g_signal_handlers_disconnect_matched(data_window,
                                         G_SIGNAL_MATCH_FUNC
                                         | G_SIGNAL_MATCH_DATA,
                                         0, 0, NULL,
                                         gwy_app_3d_window_title_changed,
                                         gwy3dwindow);
    g_signal_handlers_disconnect_matched(data_window,
                                         G_SIGNAL_MATCH_FUNC
                                         | G_SIGNAL_MATCH_DATA,
                                         0, 0, NULL,
                                         gwy_app_3d_window_orphaned,
                                         gwy3dwindow);
    gwy_app_3d_window_remove(gwy3dwindow);
}


/*****************************************************************************
 *                                                                           *
 *     Any window list management                                            *
 *                                                                           *
 *****************************************************************************/

static void
gwy_app_set_current_window(GtkWidget *window)
{
    g_return_if_fail(GTK_IS_WINDOW(window));

    current_any = window;
}

/**
 * gwy_app_get_current_window:
 * @type: Type of window to return.
 *
 * Returns the currently active window of a given type.
 *
 * Returns: The window, or %NULL if there is no such window.
 **/
GtkWidget*
gwy_app_get_current_window(GwyAppWindowType type)
{
    switch (type) {
        case GWY_APP_WINDOW_TYPE_ANY:
        return current_any;
        break;

        case GWY_APP_WINDOW_TYPE_DATA:
        return current_data ? (GtkWidget*)current_data->data : NULL;
        break;

        case GWY_APP_WINDOW_TYPE_GRAPH:
        return current_graph ? (GtkWidget*)current_graph->data : NULL;
        break;

        case GWY_APP_WINDOW_TYPE_3D:
        return current_3d ? (GtkWidget*)current_3d->data : NULL;
        break;

        default:
        g_assert_not_reached();
        return NULL;
        break;
    }
}

/*****************************************************************************
 *                                                                           *
 *     Miscellaneous                                                         *
 *                                                                           *
 *****************************************************************************/

/**
 * gwy_app_data_window_set_untitled:
 * @window: A data window.
 * @templ: A title template string.
 *
 * Clears any file name for @window and sets its "/filename/untitled"
 * data.
 *
 * The template tring @templ can be either %NULL, the window then gets a
 * title like "Untitled 37", or a string "Foo" not containing `%', the window
 * then gets a title like "Foo 42", or a string "Bar %%d" containing a single
 * '%%d', the window then gets a title like "Bar 666".
 *
 * Returns: The number that will appear in the title (probably useless).
 **/
gint
gwy_app_data_window_set_untitled(GwyDataWindow *window,
                                 const gchar *templ)
{
    GwyDataView *data_view;
    GwyContainer *data;
    gchar *title, *p;

    data_view = gwy_data_window_get_data_view(window);
    data = gwy_data_view_get_data(data_view);
    gwy_container_remove_by_prefix(data, "/filename");
    untitled_no++;
    if (!templ)
        title = g_strdup_printf(_("Untitled %d"), untitled_no);
    else {
        do {
            p = strchr(templ, '%');
        } while (p && p[1] == '%' && (p += 2));

        if (!p)
            title = g_strdup_printf("%s %d", templ, untitled_no);
        else if (p[1] == 'd' && !strchr(p+2, '%'))
            title = g_strdup_printf(templ, untitled_no);
        else {
            g_warning("Wrong template `%s'", templ);
            title = g_strdup_printf(_("Untitled %d"), untitled_no);
        }
    }
    gwy_container_set_string_by_name(data, "/filename/untitled", title);

    return untitled_no;
}

static GtkWidget*
gwy_app_menu_data_popup_create(GtkAccelGroup *accel_group)
{
    static struct {
        const gchar *path;
        gpointer callback;
        gpointer cbdata;
    }
    const menu_items[] = {
        { N_("/Remove _Mask"), gwy_app_mask_kill_cb, NULL },
        { N_("/Mask _Color..."),  gwy_app_change_mask_color_cb, NULL },
        { N_("/Fix _Zero"), gwy_app_run_process_func, "fix_zero" },
        { N_("/Remove _Presentation"), gwy_app_show_kill_cb, NULL },
        { N_("/_Level"), gwy_app_run_process_func, "level" },
        { N_("/Zoom _1:1"), gwy_app_zoom_set_cb, GINT_TO_POINTER(10000) },
    };
    GtkItemFactoryEntry entry = { NULL, NULL, NULL, 0, NULL, NULL };
    GtkItemFactory *item_factory;
    gsize i;

    /* XXX: it is probably wrong to use this accel group */
    item_factory = gtk_item_factory_new(GTK_TYPE_MENU, "<data-popup>",
                                        accel_group);
#ifdef ENABLE_NLS
    gtk_item_factory_set_translate_func(item_factory,
                                        (GtkTranslateFunc)&gettext,
                                        NULL, NULL);
#endif
    for (i = 0; i < G_N_ELEMENTS(menu_items); i++) {
        if (menu_items[i].callback == gwy_app_run_process_func
            && !gwy_process_func_get_run_types((gchar*)menu_items[i].cbdata)) {
            g_warning("Data processing function <%s> for right-click menu "
                      "is not available.", (gchar*)menu_items[i].cbdata);
            continue;
        }
        entry.path = (gchar*)menu_items[i].path;
        entry.callback = (GtkItemFactoryCallback)menu_items[i].callback;
        gtk_item_factory_create_item(item_factory, &entry,
                                     menu_items[i].cbdata, 1);
    }

    set_sensitivity(item_factory,
                    "<data-popup>/Remove Mask",         GWY_MENU_FLAG_DATA_MASK,
                    "<data-popup>/Mask Color...",       GWY_MENU_FLAG_DATA_MASK,
                    "<data-popup>/Remove Presentation", GWY_MENU_FLAG_DATA_SHOW,
                    NULL);

    return gtk_item_factory_get_widget(item_factory, "<data-popup>");
}

static gboolean
gwy_app_data_popup_menu_popup_mouse(GtkWidget *menu,
                                    GdkEventButton *event,
                                    GtkWidget *view)
{
    GtkWidget *window;

    if (event->button != 3)
        return FALSE;

    window = gtk_widget_get_toplevel(view);
    g_return_val_if_fail(window, FALSE);
    gwy_app_data_window_set_current(GWY_DATA_WINDOW(window));

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

void
gwy_app_tool_use_cb(const gchar *toolname,
                     GtkWidget *button)
{
    static GtkWidget *old_button = NULL;
    GwyDataWindow *data_window;
    gboolean ok = TRUE;

    gwy_debug("%s", toolname ? toolname : "NONE");
    /* don't catch deactivations */
    if (button && !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button))) {
        gwy_debug("deactivation");
        old_button = button;
        return;
    }

    if (current_tool && (!toolname || strcmp(current_tool, toolname)))
        gwy_tool_func_use(current_tool, NULL, GWY_TOOL_SWITCH_TOOL);
    if (toolname) {
        data_window = gwy_app_data_window_get_current();
        if (data_window)
            ok = gwy_tool_func_use(toolname, data_window, GWY_TOOL_SWITCH_TOOL);
    }
    if (ok) {
        current_tool = toolname;
        old_button = button;
        return;
    }
    /* FIXME: this is really ugly */
    g_signal_emit_by_name(old_button, "clicked");
}

/* FIXME: we should zoom whatever is currently active: datawindow, 3dwindow,
 * graph */
void
gwy_app_zoom_set_cb(gpointer data)
{
    GwyDataWindow *data_window;

    data_window = gwy_app_data_window_get_current();
    g_return_if_fail(data_window);
    gwy_data_window_set_zoom(data_window, GPOINTER_TO_INT(data));
}

static void
gwy_app_save_3d_export(GtkWidget *button, Gwy3DWindow *gwy3dwindow)
{
    const gchar *filename;
    gchar *filename_sys, *filename_utf8;
    GdkPixbuf *pixbuf;
    GtkWidget *dialog, *gwy3dview;
    GError *err = NULL;

    gwy3dview = gwy_3d_window_get_3d_view(GWY_3D_WINDOW(gwy3dwindow));

    dialog = gtk_widget_get_toplevel(button);
    filename = gtk_file_selection_get_filename(GTK_FILE_SELECTION(dialog));
    filename_sys = g_strdup(filename);
    gtk_widget_destroy(dialog);

    pixbuf = gwy_3d_view_get_pixbuf(GWY_3D_VIEW(gwy3dview));
    filename_utf8 = g_filename_to_utf8(filename_sys, -1, NULL, NULL, NULL);
    if (!gdk_pixbuf_save(pixbuf, filename_sys, "png", &err, NULL)) {
        dialog = gtk_message_dialog_new(NULL,
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_ERROR,
                                        GTK_BUTTONS_OK,
                                        "Cannot save report to %s.\n%s\n",
                                        filename_utf8,
                                        err->message);
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        g_clear_error(&err);
    }
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

    dialog = gtk_file_selection_new(_("Export 3D View"));
    gtk_file_selection_set_filename(GTK_FILE_SELECTION(dialog), filename_sys);
    g_free(filename_sys);

    g_signal_connect(GTK_FILE_SELECTION(dialog)->ok_button, "clicked",
                     G_CALLBACK(gwy_app_save_3d_export), gwy3dwindow);
    g_signal_connect_swapped(GTK_FILE_SELECTION(dialog)->cancel_button,
                             "clicked",
                             G_CALLBACK(gtk_widget_destroy), dialog);
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(gwy3dwindow));
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_widget_show_all(dialog);
}

void
gwy_app_change_mask_color_cb(G_GNUC_UNUSED gpointer unused,
                             gboolean defaultc)
{
    GwyDataWindow *data_window;
    GwyDataView *data_view;
    GwyContainer *data, *settings;
    GwyRGBA rgba;

    gwy_debug("defaultc = %d", defaultc);

    settings = gwy_app_settings_get();
    if (defaultc) {
        gwy_color_selector_for_mask(_("Change Default Mask Color"),
                                    NULL, settings, "/mask");
        return;
    }

    data_window = gwy_app_data_window_get_current();
    g_return_if_fail(GWY_IS_DATA_WINDOW(data_window));
    data_view = gwy_data_window_get_data_view(data_window);
    data = gwy_data_view_get_data(data_view);
    g_assert(data);

    /* copy defaults to data container if necessary */
    if (!gwy_rgba_get_from_container(&rgba, data, "/0/mask")) {
        gwy_rgba_get_from_container(&rgba, settings, "/mask");
        gwy_rgba_store_to_container(&rgba, data, "/0/mask");
    }
    gwy_color_selector_for_mask(NULL, NULL, data, "/0/mask");
}

/* FIXME: this functionality is provided by modules now -- remove? */
void
gwy_app_mask_kill_cb(void)
{
    GwyDataWindow *data_window;
    GwyDataView *data_view;
    GwyContainer *data;

    data_window = gwy_app_data_window_get_current();
    g_return_if_fail(GWY_IS_DATA_WINDOW(data_window));
    data_view = gwy_data_window_get_data_view(data_window);
    data = gwy_data_view_get_data(data_view);
    if (gwy_container_contains_by_name(data, "/0/mask")) {
        gwy_app_undo_checkpoint(data, "/0/mask", NULL);
        gwy_container_remove_by_name(data, "/0/mask");
    }
}

void
gwy_app_show_kill_cb(void)
{
    GwyLayerBasic *layer;
    GwyDataWindow *data_window;
    GwyDataView *data_view;
    GwyContainer *data;
    const gchar *key;

    data_window = gwy_app_data_window_get_current();
    g_return_if_fail(GWY_IS_DATA_WINDOW(data_window));
    data_view = gwy_data_window_get_data_view(data_window);
    layer = GWY_LAYER_BASIC(gwy_data_view_get_base_layer(data_view));
    if (gwy_layer_basic_get_has_presentation(layer)) {
        key = gwy_layer_basic_get_presentation_key(layer);
        data = gwy_data_view_get_data(data_view);
        gwy_app_undo_checkpoint(data, key, NULL);
        gwy_container_remove_by_name(data, key);
    }
}

/************************** Documentation ****************************/

/**
 * SECTION:app
 * @title: app
 * @short_description: Core application interface, window management
 **/

/**
 * GwyAppWindowType:
 * @GWY_APP_WINDOW_TYPE_DATA: Normal 2D data window.
 * @GWY_APP_WINDOW_TYPE_GRAPH: Graph window.
 * @GWY_APP_WINDOW_TYPE_3D: 3D view window.
 * @GWY_APP_WINDOW_TYPE_ANY: Window of any type.
 *
 * Application window types.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
