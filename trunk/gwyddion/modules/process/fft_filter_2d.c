/*
 *  $Id$
 *  Copyright (C) 2005 Christopher Anderson, Molecular Imaging Corp.
 *  E-mail: Chris Anderson (sidewinder.asu@gmail.com)
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

/*TODO: Only allow 2^n sized images (XXX: this is no longer useful with FFTW) */

#include "config.h"
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule.h>
#include <libgwydgets/gwydgets.h>
#include <app/gwyapp.h>


//XXX CRUFT:
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libgwyddion/gwyutils.h>
#include <libprocess/datafield.h>
#include <libprocess/arithmetic.h>
#include <libprocess/elliptic.h>
#include <libprocess/stats.h>
#include <libprocess/inttrans.h>
#include <libdraw/gwygradient.h>
#include <libdraw/gwypixfield.h>


#define FFTF_2D_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

#define PREVIEW_SIZE 400.0
#define CR_DEFAULT 5
#define CR_MAX 200

/* Convenience macros */
#define is_marker_zero_sized(marker) \
    (marker->left == marker->right || marker->top == marker->bottom)

#define get_toggled(obj) \
    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(obj))
#define set_toggled(obj, tog) \
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(obj), tog)

#define get_combo_index(obj, key) \
    gwy_option_menu_get_history(obj, key)
#define set_combo_index(obj, key, i) \
    gwy_option_menu_set_history(obj, key, i)

#define get_container_data(obj) \
    GWY_DATA_FIELD(gwy_container_get_object_by_name(obj, "/0/data"))

#define radio_new gtk_radio_button_new_with_mnemonic_from_widget
#define check_new gtk_check_button_new_with_mnemonic

#define get_distance(x1, x2, y1, y2) \
    sqrt((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2))


typedef enum {
    FFT_ELLIPSE_ADD    = 0,
    FFT_RECT_ADD       = 1,
    FFT_ELLIPSE_SUB    = 2,
    FFT_RECT_SUB       = 3,
} MaskEditMode;

typedef void (*FieldFillFunc)(GwyDataField*, gint, gint, gint, gint, gdouble);

enum {
    OUTPUT_FFT = 1,
    OUTPUT_IMAGE = 2
};

enum {
    RESPONSE_RESET,
};

typedef struct {
    GwyContainer *mydata;
    GwyContainer *data;

    GwyDataField *dfield;
    GwyDataField *fft;

    GtkWidget      *view;
    //GwyVectorLayer *rectangle;
    //GwyVectorLayer *ellipse;
    GwyVectorLayer *vlayer;

    MaskEditMode    edit_mode;
    GSList          *mode;

//XXX OLD CRUFT:
    GtkWidget *dialog;

    GtkWidget *scale_fft;
    GtkWidget *check_zoom;
    GtkWidget *check_origin;

    GtkWidget *button_show_fft;
    GtkWidget *button_show_original_image;
    GtkWidget *button_show_fft_preview;
    GtkWidget *button_show_image_preview;
    GtkWidget *button_show_diff_preview;

    GtkWidget *combo_output;

//    gdouble color_range;
//    gboolean preview;
//    gboolean preview_invalid;
//    gboolean output_image;
//    gboolean output_fft;
//    gdouble zoom_factor;

} ControlsType;

/* Gwyddion Module Routines */
static gboolean     module_register     (void);
static void         run_main            (GwyContainer *data,
                                         GwyRunType run);

/* Signal handlers */
static void        selection_finished_cb (GwySelection *selection,
                                         ControlsType *controls);
static void        edit_mode_changed_cb  (ControlsType *controls);

//XXX OLD CRUFT:
static void         scale_changed_fft   (GtkRange *range,
                                         ControlsType *controls);
static void         remove_all_clicked  (ControlsType *controls);
static void         display_mode_changed(ControlsType *controls);
static void         zoom_toggled        (ControlsType *controls);

/* Helper Functions */
static gboolean     run_dialog          (ControlsType *controls);
static void         build_tooltips      (GHashTable *hash_tips);
static void         save_settings       (ControlsType *controls);
static void         load_settings       (ControlsType *controls,
                                         gboolean load_defaults);
static void         fft_filter_2d       (GwyDataField *input,
                                         GwyDataField *output_image,
                                         GwyDataField *output_fft,
                                         GSList *markers);
static void         do_fft              (GwyDataField *dataInput,
                                         GwyDataField *dataOutput);
static void         set_dfield_modulus  (GwyDataField *re,
                                         GwyDataField *im,
                                         GwyDataField *target);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("2D FFT Filtering"),
    "Chris Anderson <sidewinder.asu@gmail.com>",
    "1.1",
    "Chris Anderson, Molecular Imaging Corp.",
    "2005",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("fft_filter_2d",
                              (GwyProcessFunc)&run_main,
                              N_("/_Correct Data/_2D FFT filtering..."),
                              NULL,
                              FFTF_2D_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Two-dimensional FFT filtering"));

    return TRUE;
}

static void
run_main(GwyContainer *data, GwyRunType run)
{
    ControlsType controls;
    gboolean response;

    g_return_if_fail(run & FFTF_2D_RUN_MODES);

    /* Initialize */
    controls.edit_mode = FFT_RECT_ADD;

    controls.data = data;

    /* Run the dialog */
    response = run_dialog(&controls);
}

static GwyDataField*
create_mask_field(GwyDataField *dfield)
{
    GwyDataField *mfield;
    GwySIUnit *siunit;

    mfield = gwy_data_field_new_alike(dfield, TRUE);
    siunit = gwy_si_unit_new("");
    gwy_data_field_set_si_unit_z(mfield, siunit);
    g_object_unref(siunit);

    return mfield;
}

static gboolean
run_dialog(ControlsType *controls)
{
    static GwyEnum output_types[] = {
        { N_("Filtered Image"), OUTPUT_IMAGE              },
        { N_("Filtered FFT"),   OUTPUT_FFT                },
        { N_("Both"),           OUTPUT_IMAGE | OUTPUT_FFT },
    };
    GtkWidget *dialog;
    GtkWidget *table, *hbox, *hbox2, *hbox3;
    GtkWidget *table2;
    GtkWidget *label;
    GtkWidget *scale_fft;
    GtkWidget *image;
    GtkWidget *button = NULL;
    GtkWidget *combo;
    GtkTooltips *tips;
    GHashTable *hash_tips;

    gint response;
    gint i;
    gint row;
    // OLD CRUFT ABOVE


    static struct {
        guint edit_mode;
        const gchar *stock_id;
        const gchar *text;
    }
    const modes[] = {
        {
            FFT_ELLIPSE_ADD,
            GWY_STOCK_MASK_CIRCLE_INCLUSIVE,
            N_("Add an ellipse to the FFT mask"),
        },
        {
            FFT_RECT_ADD,
            GWY_STOCK_MASK_RECT_INCLUSIVE,
            N_("Add a rectangle to the FFT mask"),
        },
        {
            FFT_ELLIPSE_SUB,
            GWY_STOCK_MASK_CIRCLE_EXCLUSIVE,
            N_("Subtract an ellipse from the FFT mask"),
        },
        {
            FFT_RECT_SUB,
            GWY_STOCK_MASK_RECT_EXCLUSIVE,
            N_("Subtract a rectange from the FFT mask"),
        },
    };

    GtkRadioButton *group;
    GwyPixmapLayer *layer, *mlayer;
    //GwyVectorLayer *vlayer;
    GwySelection *selection;
    GwyDataField *dfield = NULL, *mask;
    GwyRGBA rgba;
    GQuark mquark;
    gint id;
    gdouble zoomval;

    /* Get the currently active GwyDataField */
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     GWY_APP_MASK_FIELD_KEY, &mquark,
                                     0);
    if (!dfield)
        return FALSE;

    /* Duplicate the original datafield, calculate and store the fft image */
    controls->dfield = gwy_data_field_duplicate(dfield);
    controls->fft = gwy_data_field_new_alike(dfield, FALSE);
    do_fft(controls->dfield, controls->fft);

    /* Setup the dialog window */
    dialog = gtk_dialog_new_with_buttons(_("2D FFT Filtering"), NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    controls->dialog = dialog;

    /* XXX Setup tooltips XXX */
    hash_tips = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, NULL);
    build_tooltips(hash_tips);
    tips = gtk_tooltips_new();
    g_object_ref(tips);
    gtk_object_sink(GTK_OBJECT(tips));

    /* Main Horizontal Box (contains the GwyDataView and the control panel) */
    hbox = gtk_hbox_new(FALSE, 5);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, TRUE, TRUE, 5);

    /* Setup the GwyDataView */
    controls->mydata = gwy_container_new();
    gwy_container_set_object_by_name(controls->mydata, "/0/data",
                                     controls->fft);
    gwy_container_set_string_by_name(controls->mydata, "/0/base/fft_palette",
                                     g_strdup("DFit"));
    gwy_app_copy_data_items(controls->data, controls->mydata, id, 0,
                            GWY_DATA_ITEM_PALETTE,
                            GWY_DATA_ITEM_MASK_COLOR,
                            GWY_DATA_ITEM_RANGE,
                            0);
    if (!gwy_rgba_get_from_container(&rgba, controls->mydata, "/0/mask")) {
        gwy_rgba_get_from_container(&rgba, gwy_app_settings_get(), "/mask");
        gwy_rgba_store_to_container(&rgba, controls->mydata, "/0/mask");
    }
    controls->view = gwy_data_view_new(controls->mydata);

    /* setup base layer */
    layer = gwy_layer_basic_new();
    gwy_pixmap_layer_set_data_key(layer, "/0/data");
    gwy_layer_basic_set_gradient_key(GWY_LAYER_BASIC(layer),
                                     "/0/base/fft_palette");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls->view), layer);
    zoomval = PREVIEW_SIZE/(gdouble)MAX(gwy_data_field_get_xres(dfield),
                                        gwy_data_field_get_yres(dfield));
    gwy_data_view_set_zoom(GWY_DATA_VIEW(controls->view), zoomval);
    gtk_box_pack_start(GTK_BOX(hbox), controls->view, FALSE, FALSE, 5);

    /* setup vector layers */
    /*
    controls->rectangle = g_object_new(g_type_from_name("GwyLayerRectangle"),
                                       NULL);
    controls->ellipse = g_object_new(g_type_from_name("GwyLayerEllipse"),
                                     NULL);
    gwy_vector_layer_set_selection_key(controls->rectangle,
                                       "/0/select/pointer");
    gwy_vector_layer_set_selection_key(controls->ellipse,
                                       "/0/select/pointer");
    */
    switch(controls->edit_mode)
    {
        case FFT_RECT_ADD:
        case FFT_RECT_SUB:
            controls->vlayer =
                g_object_new(g_type_from_name("GwyLayerRectangle"), NULL);
            break;

        case FFT_ELLIPSE_ADD:
        case FFT_ELLIPSE_SUB:
            controls->vlayer =
                g_object_new(g_type_from_name("GwyLayerEllipse"), NULL);
            break;

        default:
            break;
            /*XXX Shouldn't Occur */
    }
    gwy_vector_layer_set_selection_key(controls->vlayer, "/0/select/pointer");
    gwy_data_view_set_top_layer(GWY_DATA_VIEW(controls->view),controls->vlayer);
    selection = gwy_vector_layer_get_selection(controls->vlayer);
    g_signal_connect(selection, "finished",
                     G_CALLBACK(selection_finished_cb), controls);

    /* setup mask layer */
    mask = create_mask_field(dfield);
    gwy_container_set_object_by_name(controls->mydata, "/0/mask", mask);
    g_object_unref(mask);
    mlayer = gwy_layer_mask_new();
    gwy_pixmap_layer_set_data_key(mlayer, "/0/mask");
    gwy_layer_mask_set_color_key(GWY_LAYER_MASK(mlayer), "/0/mask");
    gwy_data_view_set_alpha_layer(GWY_DATA_VIEW(controls->view), mlayer);



    /* Setup the control panel */
    table = gtk_table_new(20, 2, FALSE);
    gtk_box_pack_start(GTK_BOX(hbox), table, TRUE, TRUE, 10);
    row = 0;

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Filter Drawing</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
    gtk_table_attach(GTK_TABLE(table), label, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 5);
    row++;

    hbox2 = gtk_hbox_new(FALSE, 0);
    gtk_table_attach(GTK_TABLE(table), hbox2, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);

    /* MODE/SHAPE Buttons */
    group = NULL;
    for (i = 0; i < G_N_ELEMENTS(modes); i++) {
        button = gtk_radio_button_new_from_widget(group);
        g_object_set(button, "draw-indicator", FALSE, NULL);
        image = gtk_image_new_from_stock(modes[i].stock_id,
                                         GTK_ICON_SIZE_BUTTON
                                         /*GTK_ICON_SIZE_LARGE_TOOLBAR*/);
        gtk_container_add(GTK_CONTAINER(button), image);
        g_object_set_data(G_OBJECT(button), "select-mode",
                          GUINT_TO_POINTER(modes[i].edit_mode));
        gtk_box_pack_start(GTK_BOX(hbox2), button, FALSE, FALSE, 0);
        gtk_tooltips_set_tip(tips, button, gettext(modes[i].text), NULL);
        g_signal_connect_swapped(button, "clicked",
                                 G_CALLBACK(edit_mode_changed_cb), controls);
        if (!group)
            group = GTK_RADIO_BUTTON(button);
    }
    controls->mode = gtk_radio_button_get_group(group);
    gwy_radio_buttons_set_current(controls->mode, "select-mode",
                                  controls->edit_mode);

    /* Remaining controls: */
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 5);
    row++;

    controls->check_origin = check_new(_("_Snap to origin"));
    gtk_tooltips_set_tip(GTK_TOOLTIPS(tips), controls->check_origin,
                         g_hash_table_lookup(hash_tips, "origin"), "");
    gtk_table_attach(GTK_TABLE(table), controls->check_origin, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 5);
    row++;

    hbox2 = gtk_hbutton_box_new();
    gtk_button_box_set_layout(GTK_BUTTON_BOX(hbox2), GTK_BUTTONBOX_START);
    gtk_table_attach(GTK_TABLE(table), hbox2, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);

    button = gtk_button_new_with_mnemonic(_("Remove _All"));
    gtk_tooltips_set_tip(GTK_TOOLTIPS(tips), button,
                         g_hash_table_lookup(hash_tips, "remove_all"), "");
    gtk_container_add(GTK_CONTAINER(hbox2), button);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(remove_all_clicked), controls);

    gtk_table_set_row_spacing(GTK_TABLE(table), row, 15);
    row++;

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Preview Options</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
    gtk_table_attach(GTK_TABLE(table), label, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 5);
    row++;

    hbox2 = gtk_hbox_new(FALSE, 0);
    gtk_table_attach(GTK_TABLE(table), hbox2, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    scale_fft = gtk_hscale_new(GTK_ADJUSTMENT(gtk_adjustment_new(CR_DEFAULT,
                                                                 0.1,
                                                                 CR_MAX,
                                                                 0.1, 1, 1)));
    gtk_scale_set_digits(GTK_SCALE(scale_fft), 3);
    gtk_scale_set_draw_value(GTK_SCALE(scale_fft), FALSE);
    gtk_tooltips_set_tip(GTK_TOOLTIPS(tips), scale_fft,
                         g_hash_table_lookup(hash_tips, "color_range"), "");
    gtk_box_pack_end(GTK_BOX(hbox2), scale_fft, TRUE, TRUE, 0);
    controls->scale_fft = scale_fft;

    label = gtk_label_new_with_mnemonic(_("Color Ran_ge:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), scale_fft);
    gtk_box_pack_start(GTK_BOX(hbox2), label, FALSE, FALSE, 0);

    gtk_table_set_row_spacing(GTK_TABLE(table), row, 5);
    row++;

    button = gtk_check_button_new_with_mnemonic(_("_Zoom"));
    g_signal_connect_swapped(button, "toggled",
                             G_CALLBACK(zoom_toggled), controls);
    gtk_table_attach(GTK_TABLE(table), button, 0, 1, row, row+1,
                     GTK_FILL, GTK_FILL, 0, 2);
    controls->check_zoom = button;
    row++;

    label = gtk_label_new(_("Display mode:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.05);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
                     GTK_FILL, GTK_FILL, 0, 2);
    row++;

    table2 = gtk_table_new(3, 2, FALSE);
    gtk_table_attach(GTK_TABLE(table), table2, 0, 2, row, row+1,
                     GTK_FILL, GTK_FILL, 0, 2);

    button = gtk_radio_button_new_with_mnemonic(NULL, _("Original _FFT"));
    g_signal_connect_swapped(button, "toggled",
                             G_CALLBACK(display_mode_changed), controls);
    gtk_table_attach(GTK_TABLE(table2), button, 0, 1, 0, 1,
                     GTK_FILL, GTK_FILL, 0, 1);
    controls->button_show_fft = button;

    button = radio_new(GTK_RADIO_BUTTON(button), _("Original _Image"));
    g_signal_connect_swapped(button, "toggled",
                             G_CALLBACK(display_mode_changed), controls);
    gtk_table_attach(GTK_TABLE(table2), button, 1, 2, 0, 1,
                     GTK_FILL, GTK_FILL, 5, 1);
    controls->button_show_original_image = button;

    button = radio_new(GTK_RADIO_BUTTON(button), _("Filtered _Image"));
    g_signal_connect_swapped(button, "toggled",
                             G_CALLBACK(display_mode_changed), controls);
    gtk_table_attach(GTK_TABLE(table2), button, 1, 2, 1, 2,
                     GTK_FILL, GTK_FILL, 5, 1);
    controls->button_show_image_preview = button;

    button = radio_new(GTK_RADIO_BUTTON(button), _("Image _Difference"));
    g_signal_connect_swapped(button, "toggled",
                             G_CALLBACK(display_mode_changed), controls);
    gtk_table_attach(GTK_TABLE(table2), button, 1, 2, 2, 3,
                     GTK_FILL, GTK_FILL, 5, 1);
    controls->button_show_diff_preview = button;

    button = radio_new(GTK_RADIO_BUTTON(button), _("Filtered _FFT"));
    g_signal_connect_swapped(button, "toggled",
                             G_CALLBACK(display_mode_changed), controls);
    gtk_table_attach(GTK_TABLE(table2), button, 0, 1, 1, 2,
                     GTK_FILL, GTK_FILL, 0, 1);
    controls->button_show_fft_preview = button;
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 15);
    row++;

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Output Options</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
    gtk_table_attach(GTK_TABLE(table), label, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 5);
    row++;

    combo = gwy_enum_combo_box_new(output_types, G_N_ELEMENTS(output_types),
                                   NULL, NULL, OUTPUT_IMAGE, TRUE);

    //combo = gwy_option_menu_create(output_types, G_N_ELEMENTS(output_types),
    //                               "output-type", NULL, NULL, OUTPUT_IMAGE);
    gtk_table_attach(GTK_TABLE(table), combo, 1, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    controls->combo_output = combo;

    label = gtk_label_new_with_mnemonic(_("Output _type:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), combo);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
                     GTK_FILL, 0, 0, 0);

    gtk_table_set_row_spacing(GTK_TABLE(table), row, 5);
    row++;

    /* Set up signal/event handlers */
    /*
    g_signal_connect(scale_fft, "value_changed",
                     G_CALLBACK(scale_changed_fft), controls);
    g_signal_connect(controls->draw_fft, "expose_event",
                     G_CALLBACK(paint_fft), controls);
    g_signal_connect_swapped(controls->draw_fft, "button_press_event",
                             G_CALLBACK(mouse_down_fft), controls);
    g_signal_connect_swapped(controls->draw_fft, "button_release_event",
                             G_CALLBACK(mouse_up_fft), controls);
    g_signal_connect(controls->draw_fft, "motion_notify_event",
                     G_CALLBACK(mouse_move_fft), controls);
    gtk_widget_set_events(controls->draw_fft,
                          GDK_EXPOSURE_MASK
                          | GDK_BUTTON_PRESS_MASK
                          | GDK_BUTTON_RELEASE_MASK
                          | GDK_POINTER_MOTION_MASK
                          | GDK_POINTER_MOTION_HINT_MASK);
    */

    gtk_widget_show_all(dialog);
    load_settings(controls, FALSE);



    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            case GTK_RESPONSE_NONE:
            save_settings(controls);
            gtk_widget_destroy(dialog);
            return FALSE;

            case GTK_RESPONSE_OK:
            save_settings(controls);
            break;

            case RESPONSE_RESET:
            load_settings(controls, TRUE);
            set_toggled(controls->button_show_fft, TRUE);
            //set_toggled(controls->button_circle_inc, TRUE);
            //gtk_widget_queue_draw_area(controls->draw_fft, 0, 0,
            //                           PREVIEW_SIZE, PREVIEW_SIZE);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    i = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(controls->combo_output));
    //i = get_combo_index(controls->combo_output, "output-type");
    //controls->output_image = i & OUTPUT_IMAGE;
    //controls->output_fft = i & OUTPUT_FFT;

    /* Finalize */
    gtk_widget_destroy(dialog);
    g_object_unref(tips);
    g_hash_table_destroy(hash_tips);

    return TRUE;
}

static void
edit_mode_changed_cb(ControlsType *controls)
{
    MaskEditMode new_mode;
    GwyVectorLayer *vlayer = NULL;
    GwySelection *selection;

    new_mode = gwy_radio_buttons_get_current(controls->mode, "select-mode");

    g_debug("Edit Mode Changed. Old Mode: %i   New Mode: %i",
            controls->edit_mode, new_mode);

    switch(controls->edit_mode)
    {
        case FFT_RECT_ADD:
        case FFT_RECT_SUB:
        vlayer = g_object_new(g_type_from_name("GwyLayerRectangle"), NULL);
        break;

        case FFT_ELLIPSE_ADD:
        case FFT_ELLIPSE_SUB:
        vlayer = g_object_new(g_type_from_name("GwyLayerEllipse"), NULL);
        break;

        default:
            break;
            /*XXX Shouldn't Occur */
    }

    if (vlayer) {
        gwy_data_view_set_top_layer(GWY_DATA_VIEW(controls->view), vlayer);
        gwy_vector_layer_set_selection_key(vlayer, "/0/select/pointer");
    }

    controls->edit_mode = new_mode;


    //g_signal_connect(selection, "finished",
    //                 G_CALLBACK(selection_finished_cb), controls);


    /*


    switch(controls->edit_mode)
    {
        case FFT_RECT_ADD:
        case FFT_RECT_SUB:
            vlayer = controls->rectangle;
            break;

        case FFT_ELLIPSE_ADD:
        case FFT_ELLIPSE_SUB:
            vlayer = controls->ellipse;
            break;

        default:
            vlayer = controls->ellipse;
    }
    gwy_data_view_set_top_layer(GWY_DATA_VIEW(controls->view), vlayer);
    //selection = gwy_vector_layer_get_selection(vlayer);
    //g_signal_connect(selection, "finished",
    //                 G_CALLBACK(selection_finished_cb), controls);


    /*
    GwyPlainTool *plain_tool;
    MaskEditShape shape;

    shape = gwy_radio_buttons_get_current(tool->shape, "shape-type");
    if (shape == tool->args.shape)
        return;
    tool->args.shape = shape;

    plain_tool = GWY_PLAIN_TOOL(tool);
    switch (tool->args.shape) {
        case MASK_SHAPE_RECTANGLE:
            gwy_plain_tool_connect_selection(plain_tool, tool->layer_type_rect,
                                             "rectangle");
            break;

        case MASK_SHAPE_ELLIPSE:
            gwy_plain_tool_connect_selection(plain_tool, tool->layer_type_ell,
                                             "ellipse");
            break;

        default:
            g_return_if_reached();
            break;
    }
    */
}

static void
selection_finished_cb(GwySelection *selection,
                          ControlsType *controls)
{
    GwyDataField *mfield  = NULL;
    FieldFillFunc fill_func;
    gdouble sel[4];
    gint isel[4];

//    if (controls->in_update)
//        return;

    /* get the mask */
    mfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                            "/0/mask"));
    //XXX handle no mfield?

    /* get the selection coordinates */
    if (!gwy_selection_get_object(selection, 0, sel))
        return;
    isel[0] = gwy_data_field_rtoj(mfield, sel[0]);
    isel[1] = gwy_data_field_rtoj(mfield, sel[1]);
    isel[2] = gwy_data_field_rtoj(mfield, sel[2]) + 1;
    isel[3] = gwy_data_field_rtoj(mfield, sel[3]) + 1;
    isel[2] -= isel[0];
    isel[3] -= isel[1];

    /* decide between rectangle and ellipse */
    switch (controls->edit_mode) {
        case FFT_RECT_ADD:
        case FFT_RECT_SUB:
            fill_func = &gwy_data_field_area_fill;
            break;

        case FFT_ELLIPSE_ADD:
        case FFT_ELLIPSE_SUB:
            fill_func = (FieldFillFunc)&gwy_data_field_elliptic_area_fill;
            break;

        default:
            g_return_if_reached();
            break;
    }

    /* apply change to mask */
    //XXX gwy_app_undo_qcheckpointv(plain_tool->container, 1, &quark);
    fill_func(mfield, isel[0], isel[1], isel[2], isel[3], 1.0);
    if (mfield)
        gwy_data_field_data_changed(mfield);

    gwy_selection_clear(selection);
}




/*XXX: fix */
static void
build_tooltips(GHashTable *hash_tips)
{
    g_hash_table_insert(hash_tips, "circle_inclusive",
        _("Draw a new inclusive circle marker."));
    g_hash_table_insert(hash_tips, "rectangle_inclusive",
        _("Draw a new inclusive rectangle marker."));
    g_hash_table_insert(hash_tips, "circle_exclusive",
        _("Draw a new exclusive circle marker."));
    g_hash_table_insert(hash_tips, "rectangle_exclusive",
        _("Draw a new exclusive rectangle marker."));
    g_hash_table_insert(hash_tips, "drag",
        _("Click and drag a marker to resize. Hold Shift to move marker."));
    g_hash_table_insert(hash_tips, "remove",
        _("Click on a marker to remove it."));
    g_hash_table_insert(hash_tips, "remove_all",
        _("Removes all markers."));
    g_hash_table_insert(hash_tips, "origin",
        _("Forces new markers to center around the origin."));
    g_hash_table_insert(hash_tips, "color_range",
        _("Changes the range of values displayed in the FFT."));
}

static void
save_settings(ControlsType *controls)
{
    GwyContainer *settings;
    gint i;

    settings = gwy_app_settings_get();

    i = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(controls->combo_output));
    gwy_container_set_int32_by_name(settings,
                                    "/module/fft_filter_2d/combo_output",
                                    i);
    //gwy_container_set_int32_by_name(settings,
    //                                "/module/fft_filter_2d/combo_output",
    //                                get_combo_index(controls->combo_output,
    //                                                "output-type"));

    gwy_container_set_boolean_by_name(settings,
                                      "/module/fft_filter_2d/check_origin",
                                      get_toggled(controls->check_origin));
    gwy_container_set_double_by_name(settings,
                    "/module/fft_filter_2d/color_range",
                    gtk_range_get_value(GTK_RANGE(controls->scale_fft)));
    gwy_container_set_boolean_by_name(settings,
                                      "/module/fft_filter_2d/zoom",
                                      get_toggled(controls->check_zoom));
}

static void
load_settings(ControlsType *controls, gboolean load_defaults)
{
    GwyContainer *settings;
    gint output;
    gboolean origin;
    gdouble color;
    gboolean zoom;

    /* Set defaults */
    output = 2;
    origin = FALSE;
    color = CR_DEFAULT;
    zoom = FALSE;

    /* Load settings */
    if (!load_defaults) {
        settings = gwy_app_settings_get();
        gwy_container_gis_int32_by_name(settings,
                                        "/module/fft_filter_2d/combo_output",
                                        &output);
        gwy_container_gis_boolean_by_name(settings,
                                          "/module/fft_filter_2d/check_origin",
                                          &origin);
        gwy_container_gis_double_by_name(settings,
                                         "/module/fft_filter_2d/color_range",
                                         &color);
        gwy_container_gis_boolean_by_name(settings,
                                          "/module/fft_filter_2d/zoom",
                                          &zoom);
    }

    /* Change controls to match settings */
    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->combo_output),
                                  output);
    //set_combo_index(controls->combo_output, "output-type", output);
    set_toggled(controls->check_origin, origin);
    gtk_range_set_value(GTK_RANGE(controls->scale_fft), color);
    set_toggled(controls->check_zoom, zoom);
}

static void
scale_changed_fft(GtkRange *range, ControlsType *controls)
{/*
    GwyGradient *gradient_fft;
    GwyDataField *dfield;
    gdouble value;
    gdouble min, max;

    value = gtk_range_get_value(range);
    dfield = get_container_data(controls->cont_fft);
    min = gwy_data_field_get_min(dfield);
    max = gwy_data_field_get_rms(dfield) * value;
    controls->color_range = max;

    gradient_fft = gwy_gradients_get_gradient("DFit");
    gwy_pixbuf_draw_data_field_with_range(controls->buf_fft, dfield,
                                          gradient_fft, min, max);

    set_toggled(controls->button_show_fft, TRUE);

    controls->preview_invalid = TRUE;
    gtk_widget_queue_draw_area(controls->draw_fft, 0, 0,
                               PREVIEW_SIZE, PREVIEW_SIZE);
 */
}

static void
remove_all_clicked(ControlsType *controls)
{

}

static void
display_mode_changed(ControlsType *controls)
{

}

static void
zoom_toggled(ControlsType *controls)
{

}

static void
set_dfield_modulus(GwyDataField *re, GwyDataField *im, GwyDataField *target)
{
    gint i, j;
    gint xres;
    gdouble rval, ival;
    gdouble *re_data, *im_data, *target_data;

    xres = gwy_data_field_get_xres(re);

    g_debug("Xres: %i", xres);

    re_data = gwy_data_field_get_data(re);
    im_data = gwy_data_field_get_data(im);
    target_data = gwy_data_field_get_data(target);

    for (i = 0; i < xres; i++) {
        for (j = 0; j < xres; j++) {
            rval = re_data[j*xres + i];
            ival = im_data[j*xres + i];
            target_data[j*xres + i] = sqrt(sqrt(rval*rval + ival*ival));
        }
    }
}

static void
do_fft(GwyDataField *data_input, GwyDataField *data_output)
{
    GwyDataField *i_in, *r_out, *i_out;
    GwyWindowingType window = GWY_WINDOWING_NONE;
    GwyTransformDirection direction = GWY_TRANSFORM_DIRECTION_FORWARD;
    GwyInterpolationType interp = GWY_INTERPOLATION_BILINEAR;

    i_in = gwy_data_field_new_alike(data_input, TRUE);
    r_out = gwy_data_field_new_alike(data_input, TRUE);
    i_out = gwy_data_field_new_alike(data_input, TRUE);

    g_debug("Data Fields Created");

    gwy_data_field_2dfft(data_input, i_in, r_out, i_out,
                         window, direction, interp, FALSE, FALSE);

    g_debug("FFT Completed");

    gwy_data_field_2dfft_humanize(r_out);
    gwy_data_field_2dfft_humanize(i_out);

    g_debug("Data is Humanized");

    set_dfield_modulus(r_out, i_out, data_output);

    g_debug("Get Data Worked");

    g_object_unref(i_out);
    g_object_unref(r_out);
    g_object_unref(i_in);
}

static void
fft_filter_2d(GwyDataField *input, GwyDataField *output_image,
              GwyDataField *output_fft, GSList *markers)
{

}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
