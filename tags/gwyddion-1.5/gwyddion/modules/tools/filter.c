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
#include <app/gwyapp.h>

#define CHECK_LAYER_TYPE(l) \
    (G_TYPE_CHECK_INSTANCE_TYPE((l), func_slots.layer_type))

typedef struct {
    GwyUnitoolState *state;
    GtkWidget *x;
    GtkWidget *y;
    GtkWidget *w;
    GtkWidget *h;
    GtkWidget *filter;
    GtkWidget *direction;
    GtkObject *size;
    GtkWidget *size_spin;
    GtkWidget *update;
    GwyFilterType fil;
    GtkOrientation dir;
    gint siz;
    gboolean upd;
    gboolean data_were_updated;
    gpointer last_preview;
} ToolControls;

static gboolean   module_register  (const gchar *name);
static gboolean   use              (GwyDataWindow *data_window,
                                    GwyToolSwitchEvent reason);
static void       layer_setup      (GwyUnitoolState *state);
static GtkWidget* dialog_create    (GwyUnitoolState *state);
static void       dialog_update    (GwyUnitoolState *state,
                                    GwyUnitoolUpdateType reason);
static void       dialog_abandon   (GwyUnitoolState *state);
static void       apply            (GwyUnitoolState *state);

static void       direction_changed_cb (GObject *item,
                                        ToolControls *controls);
static void       filter_changed_cb (GObject *item,
                                    ToolControls *controls);
static void       update_changed_cb (GtkToggleButton *button,
                                    ToolControls *controls);
static void       size_changed_cb   (ToolControls *controls);

static void       load_args        (GwyContainer *container,
                                    ToolControls *controls);
static void       save_args        (GwyContainer *container,
                                    ToolControls *controls);


/* FIXME: This belongs to tool state */
static gint old_ulcol = 0;
static gint old_ulrow = 0;
static gint old_brcol = 0;
static gint old_brrow = 0;
static gint state_changed = FALSE;

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "filter",
    "Basic filtering procedures.",
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.3",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

static GwyUnitoolSlots func_slots = {
    0,                             /* layer type, must be set runtime */
    layer_setup,                   /* layer setup func */
    dialog_create,                 /* dialog constructor */
    dialog_update,                 /* update view and controls */
    dialog_abandon,                /* dialog abandon hook */
    apply,                         /* apply action */
    NULL,                          /* nonstandard response handler */
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyToolFuncInfo func_info = {
        "filter",
        GWY_STOCK_FILTER,
        "Basic filters",
        49,
        use,
    };

    gwy_tool_func_register(name, &func_info);

    return TRUE;
}

static gboolean
use(GwyDataWindow *data_window,
    GwyToolSwitchEvent reason)
{
    static const gchar *layer_name = "GwyLayerSelect";
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
    ((ToolControls*)state->user_data)->state = state;
    state_changed = TRUE;
    return gwy_unitool_use(state, data_window, reason);
}

static void
layer_setup(GwyUnitoolState *state)
{
    ToolControls *controls;
    GwyContainer *data;
    GtkWidget *data_view;
    GObject *shadefield;

    g_assert(CHECK_LAYER_TYPE(state->layer));
    g_object_set(state->layer, "is_crop", FALSE, NULL);

    controls = (ToolControls*)state->user_data;
    if (controls->last_preview) {
        gwy_debug("last preview found %p\n", controls->last_preview);
        data_view = gwy_data_window_get_data_view(
                                     GWY_DATA_WINDOW(controls->last_preview));
        g_assert(data_view);
        data = GWY_CONTAINER(gwy_data_view_get_data(GWY_DATA_VIEW(data_view)));
        g_assert(data);
        shadefield = gwy_container_get_object_by_name(data, "/0/show");
        g_object_remove_weak_pointer(shadefield, &controls->last_preview);
        controls->last_preview = NULL;
        gwy_container_remove_by_name(data, "/0/show");
        gwy_app_data_view_update(data_view);
    }
}

static GtkWidget*
dialog_create(GwyUnitoolState *state)
{
    ToolControls *controls;
    GwyContainer *settings;
    GwySIValueFormat *units;
    GtkWidget *dialog, *table, *table2, *label, *frame;

    gwy_debug("");

    controls = (ToolControls*)state->user_data;
    settings = gwy_app_settings_get();
    load_args(settings, controls);

    units = state->coord_format;

    dialog = gtk_dialog_new_with_buttons(_("Filters"), NULL, 0, NULL);
    gwy_unitool_dialog_add_button_hide(dialog);
    gwy_unitool_dialog_add_button_apply(dialog);

    frame = gwy_unitool_windowname_frame_create(state);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), frame,
                       FALSE, FALSE, 0);

    table = gtk_table_new(6, 3, FALSE);

    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), table);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Origin</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(_("X"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 1, 2, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(_("Y"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 2, 3, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Size</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 3, 4, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(_("Width"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 4, 5, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(_("Height"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 5, 6, GTK_FILL, 0, 2, 2);

    controls->x = gtk_label_new("");
    controls->y = gtk_label_new("");
    controls->w = gtk_label_new("");
    controls->h = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(controls->x), 1.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(controls->y), 1.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(controls->w), 1.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(controls->h), 1.0, 0.5);
    gtk_label_set_selectable(GTK_LABEL(controls->x), TRUE);
    gtk_label_set_selectable(GTK_LABEL(controls->y), TRUE);
    gtk_label_set_selectable(GTK_LABEL(controls->w), TRUE);
    gtk_label_set_selectable(GTK_LABEL(controls->h), TRUE);
    gtk_table_attach_defaults(GTK_TABLE(table), controls->x, 2, 3, 1, 2);
    gtk_table_attach_defaults(GTK_TABLE(table), controls->y, 2, 3, 2, 3);
    gtk_table_attach_defaults(GTK_TABLE(table), controls->w, 2, 3, 4, 5);
    gtk_table_attach_defaults(GTK_TABLE(table), controls->h, 2, 3, 5, 6);

    table2 = gtk_table_new(4, 2, FALSE);

    gtk_container_set_border_width(GTK_CONTAINER(table2), 4);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), table2);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Filter:</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table2), label, 0, 1, 0, 1, GTK_FILL, 0, 2, 2);

    label = gtk_label_new(_("Type:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table2), label, 0, 1, 1, 2, GTK_FILL, 0, 2, 2);

    controls->filter
        = gwy_option_menu_filter(G_CALLBACK(filter_changed_cb),
                                    controls, controls->fil);

    gtk_table_attach(GTK_TABLE(table2), controls->filter,
                     1, 2, 1, 2, GTK_FILL, 0, 2, 2);

    label = gtk_label_new(_("Direction:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table2), label,
                     0, 1, 2, 3, GTK_FILL, 0, 2, 2);

    controls->direction
        = gwy_option_menu_direction(G_CALLBACK(direction_changed_cb),
                                                 controls, controls->dir);
    if (controls->fil == GWY_FILTER_SOBEL || controls->fil == GWY_FILTER_PREWITT)
        gtk_widget_set_sensitive(controls->direction, TRUE);
    else
        gtk_widget_set_sensitive(controls->direction, FALSE);

    gtk_table_attach(GTK_TABLE(table2), controls->direction,
                     1, 2, 2, 3, GTK_FILL, 0, 2, 2);

    label = gtk_label_new(_("Size:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table2), label,
                     0, 1, 3, 4, GTK_FILL, 0, 2, 2);

    controls->size = gtk_adjustment_new(controls->siz, 1, 20, 1, 5, 0);
    controls->size_spin = gwy_table_attach_spinbutton(table2, 3, "", "px",
                                                      controls->size);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->size), controls->siz);

    g_signal_connect_swapped(controls->size, "value-changed",
                             G_CALLBACK(size_changed_cb), controls);

    controls->update
        = gtk_check_button_new_with_label("Update preview dynamically");
    gtk_table_attach(GTK_TABLE(table2), controls->update, 0, 3, 4, 5,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->update),
                                 controls->upd);
    g_signal_connect(controls->update, "toggled",
                     G_CALLBACK(update_changed_cb), controls);

    return dialog;
}

/* TODO */
static void
apply(GwyUnitoolState *state)
{
    GwyContainer *data;
    GwyDataField *dfield;
    GwyDataViewLayer *layer;
    ToolControls *controls;
    gboolean is_selected;
    gdouble xy[4];
    gdouble ulcol, brcol, ulrow, brrow;

    gwy_debug("");
    layer = GWY_DATA_VIEW_LAYER(state->layer);
    controls = (ToolControls*)state->user_data;

    data = gwy_data_view_get_data(GWY_DATA_VIEW(layer->parent));

    gwy_app_clean_up_data(data);
    gwy_container_remove_by_name(data, "/0/show");
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));

    is_selected = gwy_vector_layer_get_selection(state->layer, xy);

    controls->siz = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->size));

    if (is_selected) {
        ulcol = (gint)gwy_data_field_rtoj(dfield, MIN(xy[0], xy[2]));
        ulrow = (gint)gwy_data_field_rtoi(dfield, MIN(xy[1], xy[3]));
        brcol = (gint)gwy_data_field_rtoj(dfield, MAX(xy[0], xy[2]));
        brrow = (gint)gwy_data_field_rtoi(dfield, MAX(xy[1], xy[3]));
    }
    else {
        ulcol = 0;
        ulrow = 0;
        brcol = gwy_data_field_get_xres(dfield);
        brrow = gwy_data_field_get_yres(dfield);
    }

    gwy_app_undo_checkpoint(data, "/0/data", NULL);

    switch (controls->fil) {
        case GWY_FILTER_MEAN:
        gwy_data_field_filter_mean(dfield, controls->siz,
                                   ulcol, ulrow, brcol, brrow);
        break;

        case GWY_FILTER_MEDIAN:
        gwy_data_field_filter_median(dfield, controls->siz,
                                     ulcol, ulrow, brcol, brrow);
        break;

        case GWY_FILTER_CONSERVATIVE:
        gwy_data_field_filter_conservative(dfield, controls->siz,
                                           ulcol, ulrow, brcol, brrow);
        break;

        case GWY_FILTER_LAPLACIAN:
        gwy_data_field_filter_laplacian(dfield, ulcol, ulrow, brcol, brrow);
        break;

        case GWY_FILTER_SOBEL:
        gwy_data_field_filter_sobel(dfield, controls->dir,
                                    ulcol, ulrow, brcol, brrow);
        break;

        case GWY_FILTER_PREWITT:
        gwy_data_field_filter_prewitt(dfield, controls->dir,
                                      ulcol, ulrow, brcol, brrow);
        break;

        default:
        g_assert_not_reached();
        break;
    }

   gwy_vector_layer_unselect(state->layer);
   gwy_data_view_update(GWY_DATA_VIEW(layer->parent));
}

static void
dialog_update(GwyUnitoolState *state,
              GwyUnitoolUpdateType reason)
{
    GwySIValueFormat *units;
    ToolControls *controls;
    GwyContainer *data;
    GwyDataField *shadefield, *dfield;
    GwyDataViewLayer *layer;
    gdouble xy[4];
    gboolean is_visible, is_selected;
    gint ulcol, brcol, ulrow, brrow;

    gwy_debug("");
    is_visible = state->is_visible;

    controls = (ToolControls*)state->user_data;
    units = state->coord_format;
    layer = GWY_DATA_VIEW_LAYER(state->layer);
    data = gwy_data_view_get_data(GWY_DATA_VIEW(layer->parent));
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));

    is_selected = gwy_vector_layer_get_selection(state->layer, xy);

    if (!is_visible && !is_selected)
        return;
    if (is_selected) {
        gwy_unitool_update_label(units, controls->x, MIN(xy[0], xy[2]));
        gwy_unitool_update_label(units, controls->y, MIN(xy[1], xy[3]));
        gwy_unitool_update_label(units, controls->w, fabs(xy[2] - xy[0]));
        gwy_unitool_update_label(units, controls->h, fabs(xy[3] - xy[1]));
    }
    else {
        gwy_unitool_update_label(units, controls->x, 0);
        gwy_unitool_update_label(units, controls->y, 0);
        gwy_unitool_update_label(units, controls->w,
                                 gwy_data_field_get_xreal(dfield));
        gwy_unitool_update_label(units, controls->h,
                                 gwy_data_field_get_yreal(dfield));
    }

    controls->siz = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->size));

    if (is_selected) {
        ulcol = (gint)gwy_data_field_rtoj(dfield, MIN(xy[0], xy[2]));
        ulrow = (gint)gwy_data_field_rtoi(dfield, MIN(xy[1], xy[3]));
        brcol = (gint)gwy_data_field_rtoj(dfield, MAX(xy[0], xy[2]));
        brrow = (gint)gwy_data_field_rtoi(dfield, MAX(xy[1], xy[3]));
        if ((brrow-ulrow)<=0 || (brcol-ulcol)<=0)
        {
            ulcol = 0;
            ulrow = 0;
            brcol = gwy_data_field_get_xres(dfield);
            brrow = gwy_data_field_get_yres(dfield);
        }
    }
    else {
        ulcol = 0;
        ulrow = 0;
        brcol = gwy_data_field_get_xres(dfield);
        brrow = gwy_data_field_get_yres(dfield);
    }


    if ((old_ulcol != ulcol)
        || (old_ulrow != ulrow)
        || (old_brcol != brcol)
        || (old_brrow != brrow)) {
        state_changed = TRUE;
        old_ulcol = ulcol;
        old_ulrow = ulrow;
        old_brcol = brcol;
        old_brrow = brrow;
    }

    if (reason == GWY_UNITOOL_UPDATED_DATA
        && controls->data_were_updated == FALSE) {
        state_changed = TRUE;
        controls->data_were_updated = TRUE;
    }
    else {
        controls->data_were_updated = FALSE;
    }


    if (state_changed) {
        if (gwy_container_contains_by_name(data, "/0/show")) {
            shadefield
                = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                                                  "/0/show"));
            gwy_data_field_resample(shadefield,
                                    gwy_data_field_get_xres(dfield),
                                    gwy_data_field_get_yres(dfield),
                                    GWY_INTERPOLATION_NONE);
            gwy_data_field_copy(dfield, shadefield);
        }
        else {
            shadefield
                = GWY_DATA_FIELD(gwy_serializable_duplicate(G_OBJECT(dfield)));
            gwy_container_set_object_by_name(data, "/0/show",
                                             G_OBJECT(shadefield));
            g_object_unref(shadefield);

            g_assert(!controls->last_preview);
            controls->last_preview = state->data_window;
            gwy_debug("setting last preview %p", controls->last_preview);
            g_object_add_weak_pointer(G_OBJECT(shadefield),
                                      &controls->last_preview);
        }
        g_object_set_data(G_OBJECT(shadefield), "is_preview",
                          GINT_TO_POINTER(TRUE));

        if (controls->upd) {
            switch (controls->fil) {
                case GWY_FILTER_MEAN:
                gwy_data_field_filter_mean(shadefield, controls->siz,
                                           ulcol, ulrow, brcol, brrow);
                break;

                case GWY_FILTER_MEDIAN:
                gwy_data_field_filter_median(shadefield, controls->siz,
                                             ulcol, ulrow, brcol, brrow);
                break;

                case GWY_FILTER_CONSERVATIVE:
                gwy_data_field_filter_conservative(shadefield, controls->siz,
                                                   ulcol, ulrow, brcol, brrow);
                break;

                case GWY_FILTER_LAPLACIAN:
                gwy_data_field_filter_laplacian(shadefield,
                                                ulcol, ulrow, brcol, brrow);
                break;

                case GWY_FILTER_SOBEL:
                gwy_data_field_filter_sobel(shadefield, controls->dir,
                                            ulcol, ulrow, brcol, brrow);
                break;

                case GWY_FILTER_PREWITT:
                gwy_data_field_filter_prewitt(shadefield, controls->dir,
                                              ulcol, ulrow, brcol, brrow);
                break;

                default:
                g_assert_not_reached();
                break;
            }
        }
        state_changed = FALSE;
        gwy_data_view_update(GWY_DATA_VIEW(layer->parent));
    }
}

static void
dialog_abandon(GwyUnitoolState *state)
{

    GwyContainer *settings;
    GwyContainer *data;
    ToolControls *controls;
    GwyDataViewLayer *layer;

    settings = gwy_app_settings_get();

    controls = (ToolControls*)state->user_data;
    layer = GWY_DATA_VIEW_LAYER(state->layer);
    data = gwy_data_view_get_data(GWY_DATA_VIEW(layer->parent));

    save_args(settings, controls);
    gwy_container_remove_by_name(data, "/0/show");
    gwy_data_view_update(GWY_DATA_VIEW(layer->parent));

    memset(state->user_data, 0, sizeof(ToolControls));
}

static void
direction_changed_cb(GObject *item, ToolControls *controls)
{
    gwy_debug("");
    controls->dir = GPOINTER_TO_INT(g_object_get_data(item, "direction-type"));
    state_changed = TRUE;
    dialog_update(controls->state, GWY_UNITOOL_UPDATED_CONTROLS);
}

static void
filter_changed_cb(GObject *item, ToolControls *controls)
{
    gboolean direction_sensitive = FALSE;
    gboolean size_sensitive = FALSE;

    gwy_debug("");
    controls->fil = GPOINTER_TO_INT(g_object_get_data(item, "filter-type"));
    state_changed = TRUE;

    switch (controls->fil) {
        case GWY_FILTER_LAPLACIAN:
        break;

        case GWY_FILTER_SOBEL:
        case GWY_FILTER_PREWITT:
        direction_sensitive = TRUE;
        break;

        case GWY_FILTER_MEAN:
        case GWY_FILTER_MEDIAN:
        case GWY_FILTER_CONSERVATIVE:
        size_sensitive = TRUE;
        break;

        default:
        g_assert_not_reached();
        break;
    }
    gtk_widget_set_sensitive(controls->direction, direction_sensitive);
    gtk_widget_set_sensitive(controls->size_spin, size_sensitive);

    dialog_update(controls->state, GWY_UNITOOL_UPDATED_CONTROLS);
}

static void
update_changed_cb(GtkToggleButton *button, ToolControls *controls)
{
    gwy_debug("");
    controls->upd = gtk_toggle_button_get_active(button);
    state_changed = TRUE;
    dialog_update(controls->state, GWY_UNITOOL_UPDATED_CONTROLS);
}

static void
size_changed_cb(ToolControls *controls)
{
    gwy_debug("");
    controls->siz = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->size));
    if (controls->upd)
    {
        state_changed = TRUE;
        dialog_update(controls->state, GWY_UNITOOL_UPDATED_CONTROLS);
    }
}


static const gchar *upd_key = "/tool/filter/update";
static const gchar *siz_key = "/tool/filter/size";
static const gchar *fil_key = "/tool/filter/filter";
static const gchar *dir_key = "/tool/filter/direction";

static void
save_args(GwyContainer *container, ToolControls *controls)
{
    /* TODO: remove someday, old misnamed keys */
    gwy_container_remove_by_name(container, "/tool/profile/size");
    gwy_container_remove_by_name(container, "/tool/profile/filter");
    gwy_container_remove_by_name(container, "/tool/profile/direction");

    gwy_container_set_boolean_by_name(container, upd_key, controls->upd);
    gwy_container_set_int32_by_name(container, siz_key, controls->siz);
    gwy_container_set_enum_by_name(container, fil_key, controls->fil);
    gwy_container_set_enum_by_name(container, dir_key, controls->dir);
}

static void
load_args(GwyContainer *container, ToolControls *controls)
{
    controls->upd = FALSE;
    controls->siz = 6;
    controls->fil = GWY_FILTER_MEAN;
    controls->dir = GTK_ORIENTATION_HORIZONTAL;

    gwy_container_gis_boolean_by_name(container, upd_key, &controls->upd);
    gwy_container_gis_int32_by_name(container, siz_key, &controls->siz);
    gwy_container_gis_enum_by_name(container, fil_key, &controls->fil);
    gwy_container_gis_enum_by_name(container, dir_key, &controls->dir);

    /* sanitize */
    controls->upd = !!controls->upd;
    controls->siz = CLAMP(controls->siz, 1, 20);
    controls->fil = MIN(controls->fil, GWY_FILTER_PREWITT);
    controls->dir = MIN(controls->dir, GTK_ORIENTATION_VERTICAL);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

