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
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwycontainer.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>
#include <app/app.h>
#include <app/undo.h>
#include <app/menu.h>
#include <app/settings.h>
#include <app/unitool.h>

#define CHECK_LAYER_TYPE(l) \
    (G_TYPE_CHECK_INSTANCE_TYPE((l), func_slots.layer_type))

typedef enum {
    MASK_EDIT_SET,
    MASK_EDIT_ADD,
    MASK_EDIT_REMOVE,
    MASK_EDIT_INTERSECT
} MaskEditMode;

typedef struct {
    MaskEditMode mode;
    GwyUnitoolRectLabels labels;
    gulong finished_id;
} ToolControls;

static gboolean      module_register       (void);
static gboolean      use                   (GwyDataWindow *data_window,
                                            GwyToolSwitchEvent reason);
static void          layer_setup           (GwyUnitoolState *state);
static GtkWidget*    dialog_create         (GwyUnitoolState *state);
static void          dialog_update         (GwyUnitoolState *state,
                                            GwyUnitoolUpdateType reason);
static void          mode_changed_cb       (GwyUnitoolState *state,
                                            GObject *item);
static void          selection_finished_cb (GwyUnitoolState *state);
static GwyDataField* maybe_add_mask        (GwyUnitoolState *state);
static void          dialog_abandon        (GwyUnitoolState *state);
static void          load_args             (GwyContainer *container,
                                            ToolControls *controls);
static void          save_args             (GwyContainer *container,
                                            ToolControls *controls);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Mask editor tool, allows to interactively add or remove parts "
       "of mask."),
    "Yeti <yeti@gwyddion.net>",
    "1.0.1",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

static GwyUnitoolSlots func_slots = {
    0,                             /* layer type, must be set runtime */
    layer_setup,                   /* layer setup func */
    dialog_create,                 /* dialog constructor */
    dialog_update,                 /* update view and controls */
    dialog_abandon,                /* dialog abandon hook */
    NULL,                          /* apply action */
    NULL,                          /* nonstandard response handler */
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    static GwyToolFuncInfo maskedit_func_info = {
        "maskedit",
        GWY_STOCK_MASK_EDITOR,
        N_("Edit mask"),
        &use,
    };

    gwy_tool_func_register(&maskedit_func_info);

    return TRUE;
}

static gboolean
use(GwyDataWindow *data_window,
    GwyToolSwitchEvent reason)
{
    static const gchar *layer_name = "GwyLayerRectangle";
    static GwyUnitoolState *state = NULL;

    if (!state) {
        func_slots.layer_type = g_type_from_name(layer_name);
        if (!func_slots.layer_type) {
            g_warning("Layer type `%s' not available", layer_name);
            return FALSE;
        }
        state = g_new0(GwyUnitoolState, 1);
        state->func_slots = &func_slots;
        state->user_data = g_new0(ToolControls, 1);
    }
    else {
        ToolControls *controls;
        GwySelection *selection;

        controls = (ToolControls*)state->user_data;
        if (controls->finished_id) {
            selection = gwy_vector_layer_get_selection(state->layer);
            g_assert(GWY_IS_SELECTION(selection));
            g_signal_handler_disconnect(selection, controls->finished_id);
            controls->finished_id = 0;
        }
    }
    return gwy_unitool_use(state, data_window, reason);
}

static void
layer_setup(GwyUnitoolState *state)
{
    ToolControls *controls;
    GwySelection *selection;

    controls = (ToolControls*)state->user_data;
    g_assert(CHECK_LAYER_TYPE(state->layer));
    g_object_set(state->layer,
                 "selection-key", "/0/select/rectangle",
                 "is-crop", FALSE,
                 NULL);
    selection = gwy_vector_layer_get_selection(state->layer);
    controls->finished_id
        = g_signal_connect_swapped(selection, "finished",
                                   G_CALLBACK(selection_finished_cb), state);
}

static GtkWidget*
dialog_create(GwyUnitoolState *state)
{
    static struct {
        MaskEditMode mode;
        const gchar *stock_id;
        const gchar *tooltip;
    }
    const modes[] = {
        {
            MASK_EDIT_SET,
            GWY_STOCK_MASK,
            N_("Set mask to selection"),
        },
        {
            MASK_EDIT_ADD,
            GWY_STOCK_MASK_ADD,
            N_("Add selection to mask"),
        },
        {
            MASK_EDIT_REMOVE,
            GWY_STOCK_MASK_SUBTRACT,
            N_("Subtract selection from mask"),
        },
        {
            MASK_EDIT_INTERSECT,
            GWY_STOCK_MASK_INTERSECT,
            N_("Intersect selection with mask"),
        },
    };
    ToolControls *controls;
    GtkWidget *dialog, *table, *frame, *toolbox, *button, *hbox, *label;
    GtkRadioButton *group = NULL;
    GtkTooltips *tips;
    gint i, row;

    gwy_debug("");
    controls = (ToolControls*)state->user_data;
    load_args(gwy_app_settings_get(), controls);

    dialog = gtk_dialog_new_with_buttons(_("Mask Editor"), NULL, 0, NULL);
    gwy_unitool_dialog_add_button_hide(dialog);

    frame = gwy_unitool_windowname_frame_create(state);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), frame,
                       FALSE, FALSE, 0);

    table = gtk_table_new(7, 4, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), table);
    row = 0;

    hbox = gtk_hbox_new(FALSE, 4);
    gtk_table_attach(GTK_TABLE(table), hbox, 0, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    label = gtk_label_new(_("Mode:"));
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

    toolbox = gtk_table_new(1, G_N_ELEMENTS(modes), TRUE);
    tips = gwy_app_get_tooltips();
    for (i = 0; i < G_N_ELEMENTS(modes); i++) {
        button = gtk_radio_button_new_from_widget(group);
        g_object_set(G_OBJECT(button), "draw-indicator", FALSE, NULL);
        gtk_table_attach_defaults(GTK_TABLE(toolbox), button, i, i+1, 0, 1);
        gtk_container_add(GTK_CONTAINER(button),
                          gtk_image_new_from_stock(modes[i].stock_id,
                                                  GTK_ICON_SIZE_LARGE_TOOLBAR));
        g_signal_connect_swapped(button, "clicked",
                                 G_CALLBACK(mode_changed_cb), state);
        gtk_tooltips_set_tip(tips, button, _(modes[i].tooltip), NULL);

        if (!group)
            group = GTK_RADIO_BUTTON(button);
        g_object_set_data(G_OBJECT(button), "select-mode",
                          GUINT_TO_POINTER(modes[i].mode));
        if (modes[i].mode == controls->mode)
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
    }
    gtk_box_pack_start(GTK_BOX(hbox), toolbox, FALSE, FALSE, 0);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    row += gwy_unitool_rect_info_table_setup(&controls->labels,
                                             GTK_TABLE(table), 0, row);

    return dialog;
}

static void
dialog_update(GwyUnitoolState *state,
              G_GNUC_UNUSED GwyUnitoolUpdateType reason)
{
    gboolean is_visible, is_selected;
    GwySelection *selection;
    ToolControls *controls;

    gwy_debug("");

    controls = (ToolControls*)state->user_data;
    is_visible = state->is_visible;
    selection = gwy_vector_layer_get_selection(state->layer);
    is_selected = gwy_selection_get_data(selection, NULL);
    if (!is_visible && !is_selected)
        return;

    gwy_unitool_rect_info_table_fill(state, &controls->labels, NULL, NULL);
}

static void
mode_changed_cb(GwyUnitoolState *state, GObject *item)
{
    ToolControls *controls;

    controls = (ToolControls*)state->user_data;
    controls->mode = GPOINTER_TO_UINT(g_object_get_data(item, "select-mode"));
}

static void
selection_finished_cb(GwyUnitoolState *state)
{
    GwyContainer *data;
    GwyDataField *dfield, *mask = NULL;
    GwyDataViewLayer *layer;
    GwySelection *selection;
    ToolControls *controls;
    gint isel[4];

    controls = (ToolControls*)state->user_data;
    layer = GWY_DATA_VIEW_LAYER(state->layer);
    selection = gwy_vector_layer_get_selection(state->layer);
    data = gwy_data_window_get_data(state->data_window);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    gwy_container_gis_object_by_name(data, "/0/mask", &mask);
    gwy_unitool_rect_info_table_fill(state, &controls->labels, NULL, isel);

    switch (controls->mode) {
        case MASK_EDIT_SET:
        gwy_app_undo_checkpoint(data, "/0/mask", NULL);
        mask = maybe_add_mask(state);
        gwy_data_field_fill(mask, 0.0);
        gwy_data_field_area_fill(mask, isel[0], isel[1], isel[2], isel[3],
                                 1.0);
        break;

        case MASK_EDIT_ADD:
        gwy_app_undo_checkpoint(data, "/0/mask", NULL);
        mask = maybe_add_mask(state);
        gwy_data_field_area_fill(mask, isel[0], isel[1], isel[2], isel[3],
                                 1.0);
        break;

        case MASK_EDIT_REMOVE:
        if (mask) {
            gwy_app_undo_checkpoint(data, "/0/mask", NULL);
            gwy_data_field_area_fill(mask, isel[0], isel[1], isel[2], isel[3],
                                     0.0);
        }
        break;

        case MASK_EDIT_INTERSECT:
        if (mask) {
            gwy_app_undo_checkpoint(data, "/0/mask", NULL);
            gwy_data_field_clamp(mask, 0.0, 1.0);
            gwy_data_field_area_add(mask, isel[0], isel[1], isel[2], isel[3],
                                    1.0);
            gwy_data_field_add(mask, -1.0);
            gwy_data_field_clamp(mask, 0.0, 1.0);
        }
        break;

        default:
        break;
    }
    gwy_selection_clear(selection);
    gwy_data_field_data_changed(mask);
}

static GwyDataField*
maybe_add_mask(GwyUnitoolState *state)
{
    GwyDataField *mask;
    GwyContainer *data;
    GwyDataView *data_view;

    data_view = gwy_data_window_get_data_view(state->data_window);
    data = gwy_data_view_get_data(data_view);
    if (!gwy_container_gis_object_by_name(data, "/0/mask", &mask)) {
        mask = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                                               "/0/data"));
        mask = gwy_data_field_new_alike(mask, TRUE);
        gwy_container_set_object_by_name(data, "/0/mask", mask);
        g_object_unref(mask);
    }
    return mask;
}

static void
dialog_abandon(GwyUnitoolState *state)
{
    ToolControls *controls;
    GwySelection *selection;

    controls = (ToolControls*)state->user_data;
    if (controls->finished_id) {
        selection = gwy_vector_layer_get_selection(state->layer);
        g_signal_handler_disconnect(selection, controls->finished_id);
    }
    save_args(gwy_app_settings_get(), controls);
    memset(state->user_data, 0, sizeof(ToolControls));
}

static const gchar mode_key[] = "/tool/maskedit/mode";

static void
save_args(GwyContainer *container, ToolControls *controls)
{
    gwy_container_set_enum_by_name(container, mode_key, controls->mode);
}


static void
load_args(GwyContainer *container, ToolControls *controls)
{
    controls->mode = MASK_EDIT_SET;

    gwy_container_gis_enum_by_name(container, mode_key, &controls->mode);

    /* sanitize */
    controls->mode = MIN(controls->mode, MASK_EDIT_INTERSECT);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

