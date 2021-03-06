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

#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwycontainer.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>
#include <app/settings.h>
#include <app/unitool.h>
#include <app/undo.h>

#define CHECK_LAYER_TYPE(l) \
    (G_TYPE_CHECK_INSTANCE_TYPE((l), func_slots.layer_type))

static gboolean   module_register       (const gchar *name);
static gboolean   use                   (GwyDataWindow *data_window,
                                         GwyToolSwitchEvent reason);
static GtkWidget* dialog_create         (GwyUnitoolState *state);
static void       selection_finished_cb (GwyUnitoolState *state);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "grain_remove_manually",
    N_("Grain (mask) removal tool."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.1",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

static GwyUnitoolSlots func_slots = {
    0,                             /* layer type, must be set runtime */
    NULL,                          /* layer setup func */
    dialog_create,                 /* dialog constructor */
    NULL,                          /* update view and controls */
    NULL,                          /* dialog abandon hook */
    NULL,                          /* apply action */
    NULL,                          /* nonstandard response handler */
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyToolFuncInfo func_info = {
        "grain_remove_manually",
        GWY_STOCK_GRAINS_REMOVE,
        N_("Manually remove grains (continuous parts of mask)"),
        98,
        &use,
    };

    gwy_tool_func_register(name, &func_info);

    return TRUE;
}

static gboolean
use(GwyDataWindow *data_window,
    GwyToolSwitchEvent reason)
{
    static const gchar *layer_name = "GwyLayerPointer";
    static GwyUnitoolState *state = NULL;

    if (!state) {
        func_slots.layer_type = g_type_from_name(layer_name);
        if (!func_slots.layer_type) {
            g_warning("Layer type `%s' not available", layer_name);
            return FALSE;
        }
        state = g_new0(GwyUnitoolState, 1);
        state->func_slots = &func_slots;
        state->user_data = NULL;
    }
    return gwy_unitool_use(state, data_window, reason);
}

static GtkWidget*
dialog_create(GwyUnitoolState *state)
{
    GtkWidget *dialog, *table, *label, *frame;

    gwy_debug("");

    dialog = gtk_dialog_new_with_buttons(_("Remove Grains"), NULL, 0, NULL);
    gwy_unitool_dialog_add_button_hide(dialog);

    frame = gwy_unitool_windowname_frame_create(state);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), frame,
                       FALSE, FALSE, 0);

    table = gtk_table_new(2, 2, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 0);

    label = gtk_label_new(_("This tool has no options."));
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, GTK_FILL, 0, 2, 2);

    g_signal_connect_swapped(state->layer, "selection-finished",
                             G_CALLBACK(selection_finished_cb), state);

    return dialog;
}


static void
selection_finished_cb(GwyUnitoolState *state)
{
    GwyContainer *data;
    GwyDataField *dfield;
    GwyDataViewLayer *layer;
    gdouble xy[2];
    gint col, row;
    gboolean is_visible, is_selected;

    gwy_debug("");
    layer = GWY_DATA_VIEW_LAYER(state->layer);
    data = gwy_data_view_get_data(GWY_DATA_VIEW(layer->parent));
    if (!gwy_container_gis_object_by_name(data, "/0/mask",
                                          (GObject**)&dfield)) {
        gwy_debug("No mask");
        return;
    }

    is_visible = state->is_visible;
    is_selected = gwy_vector_layer_get_selection(state->layer, xy);

    row = ROUND(gwy_data_field_rtoj(dfield, xy[1]));
    col = ROUND(gwy_data_field_rtoj(dfield, xy[0]));

    if (!is_visible && !is_selected)
        return;

    if (!gwy_data_field_get_val(dfield, col, row))
        return;

    gwy_app_undo_checkpoint(data, "/0/mask", NULL);
    gwy_data_field_grains_remove_grain(dfield, col, row);
    gwy_vector_layer_unselect(state->layer);
    gwy_data_view_update(GWY_DATA_VIEW(layer->parent));
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

