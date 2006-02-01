/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2004 David Necas (Yeti), Petr Klapetek.
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
#include <gdk/gdkkeysyms.h>
#include <libgwyddion/gwymacros.h>
#include <libgwydgets/gwystock.h>
#include <libgwymodule/gwymodule.h>
#include <app/gwyapp.h>

#include "gwyappinternal.h"
#include "gwyddion.h"

enum {
    DND_TARGET_STRING = 1,
};

typedef struct {
    const gchar *stock_id;
    const gchar *tooltip;
    GCallback callback;
    gconstpointer cbdata;
} Action;

typedef struct {
    const gchar *stock_id;
    const gchar *tooltip;
    const gchar *name;
} Action1;

typedef gboolean (*ActionCheckFunc)(gconstpointer);

static GtkWidget* gwy_app_menu_create_meta_menu (GtkAccelGroup *accel_group);
static GtkWidget* gwy_app_menu_create_file_menu (GtkAccelGroup *accel_group);
static GtkWidget* gwy_app_menu_create_edit_menu (GtkAccelGroup *accel_group);
static GtkWidget* gwy_app_toolbox_create_label (const gchar *text,
                                                const gchar *id,
                                                gboolean *pvisible);
static void       gwy_app_toolbox_showhide_cb  (GtkWidget *button,
                                                GtkWidget *widget);
static void       toolbox_dnd_data_received    (GtkWidget *widget,
                                                GdkDragContext *context,
                                                gint x,
                                                gint y,
                                                GtkSelectionData *data,
                                                guint info,
                                                guint time,
                                                gpointer user_data);
static void       gwy_app_meta_browser         (void);
static void       delete_app_window            (void);
static void       gwy_app_undo_cb              (void);
static void       gwy_app_redo_cb              (void);
static void       gwy_app_close_cb             (void);
static void       gwy_app_gl_view_maybe_cb     (void);

static GtkTargetEntry dnd_target_table[] = {
  { "STRING",     0, DND_TARGET_STRING },
  { "text/plain", 0, DND_TARGET_STRING },
};

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

static void
toolbox_add_menubar(GtkWidget *container,
                    GtkWidget *menu,
                    const gchar *item_label)
{
    GtkWidget *item, *alignment, *menubar;
    GtkTextDirection direction;

    menubar = gtk_menu_bar_new();
    direction = gtk_widget_get_direction(menubar);
    alignment = gtk_alignment_new(direction == GTK_TEXT_DIR_RTL ? 1.0 : 0.0,
                                  0.0, 1.0, 0.0);
    gtk_container_add(GTK_CONTAINER(container), alignment);
    gtk_container_add(GTK_CONTAINER(alignment), menubar);
    gtk_widget_set_name(menubar, "toolboxmenubar");

    item = gtk_menu_item_new_with_mnemonic(item_label);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), item);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), menu);
}

static GtkWidget*
add_button(GtkWidget *toolbar,
           guint i,
           const Action *action,
           ActionCheckFunc check_func,
           GtkTooltips *tips)
{
    GtkWidget *button;

    if (check_func && !check_func(action->cbdata))
        return NULL;

    button = gtk_button_new();
    gtk_table_attach_defaults(GTK_TABLE(toolbar), button,
                              i%4, i%4 + 1, i/4, i/4 + 1);
    gtk_container_add(GTK_CONTAINER(button),
                      gtk_image_new_from_stock(action->stock_id,
                                               GTK_ICON_SIZE_LARGE_TOOLBAR));
    g_signal_connect_swapped(button, "clicked",
                             action->callback, (gpointer)action->cbdata);
    gtk_tooltips_set_tip(tips, button, _(action->tooltip), NULL);

    return button;
}

static GtkWidget*
add_rbutton(GtkWidget *toolbar,
            guint i,
            const Action *action,
            GtkRadioButton *group,
            ActionCheckFunc check_func,
            GtkTooltips *tips)
{
    GtkWidget *button;

    if (check_func && !check_func(action->cbdata))
        return NULL;

    button = gtk_radio_button_new_from_widget(group);
    gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(button), FALSE);
    gtk_table_attach_defaults(GTK_TABLE(toolbar), button,
                              i%4, i%4 + 1, i/4, i/4 + 1);
    gtk_container_add(GTK_CONTAINER(button),
                      gtk_image_new_from_stock(action->stock_id,
                                               GTK_ICON_SIZE_LARGE_TOOLBAR));
    g_signal_connect_swapped(button, "clicked",
                             action->callback, (gpointer)action->cbdata);
    gtk_tooltips_set_tip(tips, button, _(action->tooltip), NULL);

    return button;
}

GtkWidget*
gwy_app_toolbox_create(void)
{
    static const Action view_actions[] = {
        {
            GWY_STOCK_ZOOM_IN,
            N_("Zoom in"),
            G_CALLBACK(gwy_app_zoom_set_cb),
            GINT_TO_POINTER(1),
        },
        {
            GWY_STOCK_ZOOM_1_1,
            N_("Zoom 1:1"),
            G_CALLBACK(gwy_app_zoom_set_cb),
            GINT_TO_POINTER(10000),
        },
        {
            GWY_STOCK_ZOOM_OUT,
            N_("Zoom out"),
            G_CALLBACK(gwy_app_zoom_set_cb),
            GINT_TO_POINTER(-1),
        },
        {
            GWY_STOCK_3D_BASE,
            N_("Display a 3D view of data"),
            G_CALLBACK(gwy_app_gl_view_maybe_cb),
            NULL,
        },
    };
    static const Action1 proc_actions[] = {
        {
            GWY_STOCK_FIX_ZERO,
            N_("Fix minimum value to zero"),
            "fix_zero",
        },
        {
            GWY_STOCK_SCALE,
            N_("Scale data"),
            "scale",
        },
        {
            GWY_STOCK_ROTATE,
            N_("Rotate by arbitrary angle"),
            "rotate",
        },
        {
            GWY_STOCK_UNROTATE,
            N_("Automatically correct rotation"),
            "unrotate",
        },
        {
            GWY_STOCK_LEVEL,
            N_("Automatically level data"),
            "level",
        },
        {
            GWY_STOCK_FACET_LEVEL,
            N_("Facet-level data"),
            "facet-level",
        },
        {
            GWY_STOCK_FFT,
            N_("Fast Fourier Transform"),
            "fft",
        },
        {
            GWY_STOCK_CWT,
            N_("Continuous Wavelet Transform"),
            "cwt",
        },
        {
            GWY_STOCK_GRAINS,
            N_("Mark grains by threshold"),
            "grain_mark",
        },
        {
            GWY_STOCK_GRAINS_WATER,
            N_("Mark grains by watershed"),
            "grain_wshed",
        },
        {
            GWY_STOCK_GRAINS_REMOVE,
            N_("Remove grains by threshold"),
            "grain_rem_threshold",
        },
        {
            GWY_STOCK_GRAINS_GRAPH,
            N_("Grain size distribution"),
            "grain_dist",
        },
        {
            GWY_STOCK_FRACTAL,
            N_("Calculate fractal dimension"),
            "fractal",
        },
        {
            GWY_STOCK_SHADER,
            N_("Shade data"),
            "shade",
        },
        {
            GWY_STOCK_POLYNOM,
            N_("Remove polynomial background"),
            "polylevel",
        },
        {
            GWY_STOCK_SCARS,
            N_("Remove scars"),
            "scars_remove",
        },
    };
    static const Action1 graph_actions[] = {
        {
            GWY_STOCK_GRAPH_MEASURE,
            N_("Fit critical dimension"),
            "graph_cd",
        },
        {
            GWY_STOCK_GRAPH_FUNCTION,
            N_("Fit functions to graph data"),
            "graph_fit",
        },
    };
    static const gchar *tool_actions[] = {
        "readvalue", "distance", "polynom", "crop", "filter", "level3",
        "stats", "sfunctions", "profile", "grain_remove_manually",
        "spotremove", "icolorange", "maskedit",
    };
    GwyMenuSensFlags sens;
    GtkWidget *toolbox, *vbox, *toolbar, *menu, *label, *button, *container;
    GtkRadioButton *group;
    GtkTooltips *tooltips;
    GtkAccelGroup *accel_group;
    Action action;
    GList *list;
    GSList *toolbars = NULL, *l;
    const gchar *first_tool = NULL;
    gboolean visible;
    guint i, j;

    toolbox = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(toolbox), g_get_application_name());
    gtk_window_set_wmclass(GTK_WINDOW(toolbox), "toolbox",
                           g_get_application_name());
    gtk_window_set_resizable(GTK_WINDOW(toolbox), FALSE);
    gwy_app_main_window_set(toolbox);
    gwy_app_main_window_restore_position();

    accel_group = gtk_accel_group_new();
    g_object_set_data(G_OBJECT(toolbox), "accel_group", accel_group);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(toolbox), vbox);
    container = vbox;

    tooltips = gwy_app_get_tooltips();

    toolbox_add_menubar(container,
                        gwy_app_menu_create_file_menu(accel_group), _("_File"));
    toolbox_add_menubar(container,
                        gwy_app_menu_create_edit_menu(accel_group), _("_Edit"));

    menu = gwy_app_build_process_menu(accel_group);
    gwy_app_process_menu_add_run_last(menu);
    gtk_accel_group_lock(gtk_menu_get_accel_group(GTK_MENU(menu)));
    toolbox_add_menubar(container, menu, _("_Data Process"));

    menu = gwy_app_build_graph_menu(accel_group);
    gtk_accel_group_lock(gtk_menu_get_accel_group(GTK_MENU(menu)));
    toolbox_add_menubar(container, menu, _("_Graph"));

    toolbox_add_menubar(container,
                        gwy_app_menu_create_meta_menu(accel_group), _("_Meta"));

    /***************************************************************/
    label = gwy_app_toolbox_create_label(_("View"), "zoom", &visible);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

    toolbar = gtk_table_new(1, 4, TRUE);
    toolbars = g_slist_append(toolbars, toolbar);
    gtk_widget_set_no_show_all(toolbar, !visible);
    gtk_box_pack_start(GTK_BOX(vbox), toolbar, TRUE, TRUE, 0);

    for (i = 0; i < G_N_ELEMENTS(view_actions); i++) {
        button = add_button(toolbar, i, view_actions + i, NULL, tooltips);
        gwy_app_sensitivity_add_widget(button, GWY_MENU_FLAG_DATA);
    }
    g_signal_connect(label, "clicked",
                     G_CALLBACK(gwy_app_toolbox_showhide_cb), toolbar);

    /***************************************************************/
    label = gwy_app_toolbox_create_label(_("Data Process"), "proc", &visible);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

    toolbar = gtk_table_new(4, 4, TRUE);
    toolbars = g_slist_append(toolbars, toolbar);
    gtk_widget_set_no_show_all(toolbar, !visible);
    gtk_box_pack_start(GTK_BOX(vbox), toolbar, TRUE, TRUE, 0);

    action.callback = G_CALLBACK(gwy_app_run_process_func);
    for (j = i = 0; i < G_N_ELEMENTS(proc_actions); i++) {
        action.stock_id = proc_actions[i].stock_id;
        action.tooltip = proc_actions[i].tooltip;
        action.cbdata = proc_actions[i].name;
        button = add_button(toolbar, j, &action,
                            (ActionCheckFunc)gwy_process_func_exists,
                            tooltips);
        if (!button)
            continue;
        sens = gwy_process_func_get_sensitivity_mask(proc_actions[i].name);
        gwy_app_sensitivity_add_widget(button, sens);
        j++;
    }
    g_signal_connect(label, "clicked",
                     G_CALLBACK(gwy_app_toolbox_showhide_cb), toolbar);

    /***************************************************************/
    label = gwy_app_toolbox_create_label(_("Graph"), "graph", &visible);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

    toolbar = gtk_table_new(1, 4, TRUE);
    toolbars = g_slist_append(toolbars, toolbar);
    gtk_widget_set_no_show_all(toolbar, !visible);
    gtk_box_pack_start(GTK_BOX(vbox), toolbar, TRUE, TRUE, 0);

    action.callback = G_CALLBACK(gwy_app_run_graph_func);
    for (j = i = 0; i < G_N_ELEMENTS(graph_actions); i++) {
        action.stock_id = graph_actions[i].stock_id;
        action.tooltip = graph_actions[i].tooltip;
        action.cbdata = graph_actions[i].name;
        button = add_button(toolbar, j, &action,
                            (ActionCheckFunc)gwy_graph_func_exists, tooltips);
        if (!button)
            continue;
        /* FIXME: implicit sensitivity, remove */
        gwy_app_sensitivity_add_widget(button, GWY_MENU_FLAG_GRAPH);
        j++;
    }
    g_signal_connect(label, "clicked",
                     G_CALLBACK(gwy_app_toolbox_showhide_cb), toolbar);

    /***************************************************************/
    label = gwy_app_toolbox_create_label(_("Tools"), "tool", &visible);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

    toolbar = gtk_table_new(4, 4, TRUE);
    toolbars = g_slist_append(toolbars, toolbar);
    gtk_widget_set_no_show_all(toolbar, !visible);
    gtk_box_pack_start(GTK_BOX(vbox), toolbar, TRUE, TRUE, 0);

    action.callback = G_CALLBACK(gwy_app_tool_use_cb);
    group = NULL;
    for (j = i = 0; i < G_N_ELEMENTS(tool_actions); i++) {
        action.stock_id = gwy_tool_func_get_stock_id(tool_actions[i]);
        action.tooltip = gwy_tool_func_get_tooltip(tool_actions[i]);
        action.cbdata = tool_actions[i];
        button = add_rbutton(toolbar, j, &action, group,
                             (ActionCheckFunc)gwy_tool_func_exists, tooltips);
        if (!button)
            continue;
        /* FIXME: implicit sensitivity, remove */
        gwy_app_sensitivity_add_widget(button, GWY_MENU_FLAG_DATA);
        if (!group) {
            group = GTK_RADIO_BUTTON(button);
            first_tool = tool_actions[i];
        }
        j++;
    }
    g_signal_connect(label, "clicked",
                     G_CALLBACK(gwy_app_toolbox_showhide_cb), toolbar);

    list = gtk_container_get_children(GTK_CONTAINER(toolbar));
    gwy_app_tool_use_cb(first_tool, NULL);
    gwy_app_tool_use_cb(first_tool, list ? GTK_WIDGET(list->data) : NULL);
    g_list_free(list);

    /***************************************************************/
    gtk_drag_dest_set(toolbox, GTK_DEST_DEFAULT_ALL,
                      dnd_target_table, G_N_ELEMENTS(dnd_target_table),
                      GDK_ACTION_COPY);
    g_signal_connect(toolbox, "drag-data-received",
                     G_CALLBACK(toolbox_dnd_data_received), NULL);

    /***************************************************************/
    /* XXX */
    g_signal_connect(toolbox, "delete-event",
                     G_CALLBACK(gwy_app_main_window_save_position), NULL);
    g_signal_connect(toolbox, "delete-event", G_CALLBACK(gwy_app_quit), NULL);

    gtk_window_add_accel_group(GTK_WINDOW(toolbox), accel_group);

    gtk_widget_show_all(toolbox);
    gwy_app_main_window_restore_position();
    for (l = toolbars; l; l = g_slist_next(l))
        gtk_widget_set_no_show_all(GTK_WIDGET(l->data), FALSE);
    g_slist_free(toolbars);

    while (gtk_events_pending())
        gtk_main_iteration_do(FALSE);

    return toolbox;
}

/*************************************************************************/
static GtkWidget*
gwy_app_menu_create_meta_menu(GtkAccelGroup *accel_group)
{
    static GtkItemFactoryEntry menu_items[] = {
        {
            "/---",
            NULL,
            NULL,
            0,
            "<Tearoff>",
            NULL
        },
        {
            N_("/Module _Browser"),
            NULL,
            gwy_module_browser,
            0,
            "<Item>",
            NULL
        },
        {
            N_("/_Metadata Browser"),
            NULL,
            gwy_app_meta_browser,
            0,
            "<Item>",
            NULL
        },
        {
            "/---",
            NULL,
            NULL,
            0,
            "<Separator>",
            NULL },
        {
            N_("/_About Gwyddion"),
            NULL,
            gwy_app_about,
            0,
            "<StockItem>",
            GTK_STOCK_ABOUT
        },
    };
    GtkItemFactory *item_factory;

    item_factory = gtk_item_factory_new(GTK_TYPE_MENU, "<meta>", accel_group);
#ifdef ENABLE_NLS
    gtk_item_factory_set_translate_func(item_factory,
                                        (GtkTranslateFunc)&gettext,
                                        NULL, NULL);
#endif
    gtk_item_factory_create_items(item_factory,
                                  G_N_ELEMENTS(menu_items), menu_items, NULL);

    set_sensitivity(item_factory,
                    "<meta>/Metadata Browser", GWY_MENU_FLAG_DATA,
                    NULL);

    return gtk_item_factory_get_widget(item_factory, "<meta>");
}

static GtkWidget*
gwy_app_menu_create_file_menu(GtkAccelGroup *accel_group)
{
    static GtkItemFactoryEntry menu_items[] = {
        {
            "/---",
            NULL,
            NULL,
            0,
            "<Tearoff>",
            NULL
        },
        {
            N_("/_Open"),
            "<control>O",
            gwy_app_file_open,
            0,
            "<StockItem>",
            GTK_STOCK_OPEN
        },
        {
            N_("/Open _Recent"),
            NULL,
            NULL,
            0,
            "<Branch>",
            NULL
        },
        {
            N_("/Open Recent/---"),
            NULL,
            NULL,
            0,
            "<Tearoff>",
            NULL
        },
        {
            N_("/_Save"),
            "<control>S",
            gwy_app_file_save,
            0,
            "<StockItem>",
            GTK_STOCK_SAVE
        },
        {
            N_("/Save _As"),
            "<control><shift>S",
            gwy_app_file_save_as,
            0,
            "<StockItem>",
            GTK_STOCK_SAVE_AS
        },
        {
            N_("/Close _Window"),
            "<control>W",
            gwy_app_close_cb,
            0,
            "<StockItem>",
            GTK_STOCK_CLOSE
        },
        {
            "/---",
            NULL,
            NULL,
            0,
            "<Separator>",
            NULL 
        },
        {
            N_("/_Quit"),
            "<control>Q",
            delete_app_window,
            0,
            "<StockItem>",
            GTK_STOCK_QUIT
        },
    };
    GtkItemFactory *item_factory;

    item_factory = gtk_item_factory_new(GTK_TYPE_MENU, "<file>", accel_group);
#ifdef ENABLE_NLS
    gtk_item_factory_set_translate_func(item_factory,
                                        (GtkTranslateFunc)&gettext,
                                        NULL, NULL);
#endif
    gtk_item_factory_create_items(item_factory,
                                  G_N_ELEMENTS(menu_items), menu_items, NULL);

    set_sensitivity(item_factory,
                    "<file>/Save",         GWY_MENU_FLAG_DATA,
                    "<file>/Save As",      GWY_MENU_FLAG_DATA,
                    "<file>/Close Window", GWY_MENU_FLAG_DATA,
                    NULL);

    gwy_app_menu_set_recent_files_menu
             (gtk_item_factory_get_widget(item_factory, "<file>/Open Recent"));

    return gtk_item_factory_get_widget(item_factory, "<file>");
}

GtkWidget*
gwy_app_menu_create_edit_menu(GtkAccelGroup *accel_group)
{
    static GtkItemFactoryEntry menu_items[] = {
        {
            "/---",
            NULL,
            NULL,
            0,
            "<Tearoff>",
            NULL 
        },
        {
            N_("/_Undo"),
            "<control>Z",
            gwy_app_undo_cb,
            0,
            "<StockItem>",
            GTK_STOCK_UNDO 
        },
        {
            N_("/_Redo"),
            "<control>Y",
            gwy_app_redo_cb,
            0,
            "<StockItem>",
            GTK_STOCK_REDO 
        },
        /*
        {
            N_("/_Duplicate"),
            "<control>D",
            gwy_app_file_duplicate_cb,
            0,
            "<StockItem>",
            GTK_STOCK_COPY 
        },
        */
        {
            "/---",
            NULL,
            NULL,
            0,
            "<Separator>",
            NULL 
        },
        {
            N_("/Remove _Mask"),
            "<control>K",
            gwy_app_mask_kill_cb,
            0,
            NULL,
            NULL 
        },
        {
            N_("/Remove _Presentation"),
            "<control><shift>K",
            gwy_app_show_kill_cb,
            0,
            NULL,
            NULL 
        },
        {
            "/---",
            NULL,
            NULL,
            0,
            "<Separator>",
            NULL 
        },
        {
            N_("/Mask _Color..."),
            NULL,
            gwy_app_change_mask_color_cb,
            0,
            NULL,
            NULL 
        },
        {
            N_("/Default Mask _Color..."),
            NULL,
            gwy_app_change_mask_color_cb,
            1,
            NULL,
            NULL 
        },
        {
            N_("/Color _Gradients..."),
            NULL,
            gwy_app_gradient_editor,
            1,
            NULL,
            NULL 
        },
        {
            N_("/G_L Materials..."),
            NULL,
            gwy_app_gl_material_editor,
            1,
            NULL,
            NULL 
        },
    };
    GtkItemFactory *item_factory;

    item_factory = gtk_item_factory_new(GTK_TYPE_MENU, "<edit>", accel_group);
#ifdef ENABLE_NLS
    gtk_item_factory_set_translate_func(item_factory,
                                        (GtkTranslateFunc)&gettext,
                                        NULL, NULL);
#endif
    gtk_item_factory_create_items(item_factory,
                                  G_N_ELEMENTS(menu_items), menu_items, NULL);

    set_sensitivity(item_factory,
                    /* "<edit>/Duplicate", GWY_MENU_FLAG_DATA, */
                    "<edit>/Undo", GWY_MENU_FLAG_UNDO,
                    "<edit>/Redo", GWY_MENU_FLAG_REDO,
                    "<edit>/Remove Mask", GWY_MENU_FLAG_DATA_MASK,
                    "<edit>/Remove Presentation", GWY_MENU_FLAG_DATA_SHOW,
                    "<edit>/Mask Color...", GWY_MENU_FLAG_DATA_MASK,
                    NULL);

    return gtk_item_factory_get_widget(item_factory, "<edit>");
}

static GtkWidget*
gwy_app_toolbox_create_label(const gchar *text,
                             const gchar *id,
                             gboolean *pvisible)
{
    GwyContainer *settings;
    GtkWidget *label, *hbox, *button, *arrow;
    gboolean visible = TRUE;
    gchar *s, *key;
    GQuark quark;

    settings = gwy_app_settings_get();
    key = g_strconcat("/app/toolbox/visible/", id, NULL);
    quark = g_quark_from_string(key);
    g_free(key);
    gwy_container_gis_boolean(settings, quark, &visible);

    hbox = gtk_hbox_new(FALSE, 2);

    arrow = gtk_arrow_new(visible ? GTK_ARROW_DOWN : GTK_ARROW_RIGHT,
                          GTK_SHADOW_ETCHED_IN);
    gtk_box_pack_start(GTK_BOX(hbox), arrow, FALSE, FALSE, 0);

    s = g_strconcat("<small>", text, "</small>", NULL);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), s);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    g_free(s);
    gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);

    button = gtk_button_new();
    gtk_widget_set_name(button, "toolboxheader");
    gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_HALF);
    g_object_set(button, "can-focus", FALSE, "can-default", FALSE, NULL);
    gtk_container_add(GTK_CONTAINER(button), hbox);

    g_object_set_data(G_OBJECT(button), "arrow", arrow);
    g_object_set_data(G_OBJECT(button), "key", GUINT_TO_POINTER(quark));

    *pvisible = visible;

    return button;
}

static void
gwy_app_toolbox_showhide_cb(GtkWidget *button,
                            GtkWidget *widget)
{
    GwyContainer *settings;
    GtkWidget *arrow;
    gboolean visible;
    GQuark quark;

    settings = gwy_app_settings_get();
    quark = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(button), "key"));
    visible = gwy_container_get_boolean(settings, quark);
    arrow = GTK_WIDGET(g_object_get_data(G_OBJECT(button), "arrow"));
    g_assert(GTK_IS_ARROW(arrow));
    visible = !visible;
    gwy_container_set_boolean(settings, quark, visible);

    if (visible)
        gtk_widget_show_all(widget);
    else
        gtk_widget_hide(widget);
    g_object_set(arrow, "arrow-type",
                 visible ? GTK_ARROW_DOWN : GTK_ARROW_RIGHT,
                 NULL);
}

static void
toolbox_dnd_data_received(G_GNUC_UNUSED GtkWidget *widget,
                          GdkDragContext *context,
                          G_GNUC_UNUSED gint x,
                          G_GNUC_UNUSED gint y,
                          GtkSelectionData *data,
                          G_GNUC_UNUSED guint info,
                          guint time,
                          G_GNUC_UNUSED gpointer user_data)
{
    gchar *filename;
    gchar **file_list;
    gboolean ok = FALSE;
    gint i, n;

    if (data->length <= 0 || data->format != 8) {
        gtk_drag_finish(context, FALSE, FALSE, time);
        return;
    }

    file_list = g_strsplit((gchar*)data->data, "\n", 0);
    if (!file_list) {
        gtk_drag_finish(context, FALSE, FALSE, time);
        return;
    }

    /* Stop on an empty line too.
     * This (1) kills the last empty line (2) prevents some cases of total
     * bogus to be processed any further */
    for (n = 0; file_list[n] && file_list[n][0]; n++)
        ;

    for (i = 0; i < n; i++) {
        filename = g_strstrip(file_list[i]);
        if (g_str_has_prefix(filename, "file://"))
            filename += sizeof("file://") - 1;
        gwy_debug("filename = %s", filename);
        if (g_file_test(filename, G_FILE_TEST_IS_REGULAR
                                  | G_FILE_TEST_IS_SYMLINK)) {
            /* FIXME: what about charset conversion? */
            if (gwy_app_file_load(filename, NULL, NULL))
                ok = TRUE;    /* FIXME: what if we accept only some? */
        }
    }
    g_strfreev(file_list);
    gtk_drag_finish(context, ok, FALSE, time);
}

static void
gwy_app_meta_browser(void)
{
    gwy_app_metadata_browser(gwy_app_data_window_get_current());
}

static void
delete_app_window(void)
{
    gboolean boo;

    g_signal_emit_by_name(gwy_app_main_window_get(), "delete-event",
                          NULL, &boo);
}

static void
gwy_app_undo_cb(void)
{
    GwyContainer *data;

    if ((data = gwy_data_window_get_data(gwy_app_data_window_get_current())))
        gwy_app_undo_undo_container(data);
}

static void
gwy_app_redo_cb(void)
{
    GwyContainer *data;

    if ((data = gwy_data_window_get_data(gwy_app_data_window_get_current())))
        gwy_app_undo_redo_container(data);
}

static void
gwy_app_close_cb(void)
{
    GtkWidget *window;
    gboolean boo;

    if ((window = gwy_app_get_current_window(GWY_APP_WINDOW_TYPE_ANY)))
        g_signal_emit_by_name(window, "delete-event", NULL, &boo);
}

static void
gwy_app_gl_view_maybe_cb(void)
{
    static GtkWidget *dialog = NULL;

    if (gwy_app_gl_is_ok()) {
        gwy_app_3d_view_cb();
        return;
    }

    if (dialog) {
        gtk_window_present(GTK_WINDOW(dialog));
        return;
    }

    dialog = gtk_message_dialog_new(NULL, 0, GTK_MESSAGE_INFO,
                                    GTK_BUTTONS_CLOSE,
                                    _("OpenGL 3D graphics not available"));
    gtk_message_dialog_format_secondary_markup
        (GTK_MESSAGE_DIALOG(dialog),
#ifdef GWYDDION_HAS_OPENGL
         /* FIXME: Makes sense only on Unix */
         /* FIXME: It would be nice to give a more helpful message, but the
          * trouble is we don't know why the silly thing failed either. */
         _("Initialization of OpenGL failed.  Check output of "
           "<tt>glxinfo</tt> and warning messages printed to console during "
           "Gwyddion startup.")
#else
         _("This version of Gwyddion was built without OpenGL support.")
#endif
        );
    g_signal_connect(dialog, "response", G_CALLBACK(gtk_widget_destroy), NULL);
    g_object_add_weak_pointer(G_OBJECT(dialog), (gpointer*)&dialog);
    gtk_widget_show(dialog);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
