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

#include <string.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <libprocess/datafield.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include "gwydgets.h"

#define GWY_3D_WINDOW_TYPE_NAME "Gwy3DWindow"

#define CBRT2 1.259921049894873164767210607277
#define DEFAULT_SIZE 360

enum {
    FOO,
    LAST_SIGNAL
};

enum {
    BUTTON_ROTATE = 0,
    BUTTON_SCALE,
    BUTTON_Z_SCALE,
    BUTTON_LIGHT,
    BUTTON_ROTATE2,
    BUTTON_SCALE2,
    BUTTON_Z_SCALE2,
    BUTTON_LIGHT2,
    BUTTON_LAST
};

/* Forward declarations */

static void     gwy_3d_window_class_init          (Gwy3DWindowClass *klass);
static void     gwy_3d_window_init                (Gwy3DWindow *gwy3dwindow);
static void     gwy_3d_window_destroy             (GtkObject *object);
static void     gwy_3d_window_finalize            (GObject *object);
static void     gwy_3d_window_set_mode            (gpointer userdata,
                                                   GtkWidget *button);
static void     gwy_3d_window_set_gradient        (GtkWidget *item,
                                                   Gwy3DWindow *gwy3dwindow);
static void     gwy_3d_window_set_material        (GtkWidget *item,
                                                   Gwy3DWindow *gwy3dwindow);
static void     gwy_3d_window_select_controls     (gpointer data,
                                                   GtkWidget *button);
static void     gwy_3d_window_set_labels          (GtkWidget *item,
                                                   Gwy3DWindow *gwy3dwindow);
static void     gwy_3d_window_ortographic_changed (GtkToggleButton *check,
                                                   Gwy3DWindow *window);
static void     gwy_3d_window_show_axes_changed   (GtkToggleButton *check,
                                                   Gwy3DWindow *window);
static void     gwy_3d_window_show_labels_changed (GtkToggleButton *check,
                                                   Gwy3DWindow *window);
static void     gwy_3d_window_display_mode_changed(GtkRadioButton *radio,
                                                   Gwy3DWindow *window);
static void     gwy_3d_window_auto_scale_changed  (GtkToggleButton *check,
                                                   Gwy3DWindow *window);
static void     gwy_3d_window_labels_entry_activate(GtkEntry *entry,
                                                   Gwy3DWindow *window);
static void     gwy_3d_window_labels_reset_clicked(GtkButton *button,
                                                   Gwy3DWindow *window);
/* Local data */

static GtkWindowClass *parent_class = NULL;

/*static guint gwy3dwindow_signals[LAST_SIGNAL] = { 0 };*/

static const gdouble zoom_factors[] = {
    G_SQRT2,
    CBRT2,
    1.0,
    0.5,
};

GType
gwy_3d_window_get_type(void)
{
    static GType gwy_3d_window_type = 0;

    if (!gwy_3d_window_type) {
        static const GTypeInfo gwy_3d_window_info = {
            sizeof(Gwy3DWindowClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_3d_window_class_init,
            NULL,
            NULL,
            sizeof(Gwy3DWindow),
            0,
            (GInstanceInitFunc)gwy_3d_window_init,
            NULL,
        };
        gwy_debug("");
        gwy_3d_window_type = g_type_register_static(GTK_TYPE_WINDOW,
                                                    GWY_3D_WINDOW_TYPE_NAME,
                                                    &gwy_3d_window_info,
                                                    0);
    }

    return gwy_3d_window_type;
}

static void
gwy_3d_window_class_init(Gwy3DWindowClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkObjectClass *object_class;

    gwy_debug("");

    object_class = (GtkObjectClass*)klass;
    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_3d_window_finalize;
    object_class->destroy = gwy_3d_window_destroy;

    /*
    gwy3dwindow_signals[TITLE_CHANGED] =
        g_signal_new("title_changed",
                     G_OBJECT_CLASS_TYPE(object_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(Gwy3DWindowClass, title_changed),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);
                     */
}

static void
gwy_3d_window_init(Gwy3DWindow *gwy3dwindow)
{
    gwy_debug("");

    gwy3dwindow->gwy3dview = NULL;
    gwy3dwindow->zoom_mode = GWY_ZOOM_MODE_HALFPIX;
}

static void
gwy_3d_window_finalize(GObject *object)
{
    Gwy3DWindow *gwy3dwindow;

    gwy_debug("finalizing a Gwy3DWindow %p (refcount = %u)",
              object, object->ref_count);

    gwy3dwindow = GWY_3D_WINDOW(object);
    /*
    g_free(gwy3dwindow->coord_format);
    g_free(gwy3dwindow->value_format);
    */

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void
gwy_3d_window_destroy(GtkObject *object)
{
    Gwy3DWindow *gwy3dwindow;

    gwy3dwindow = GWY_3D_WINDOW(object);
    gwy_object_unref(gwy3dwindow->vbox);
    gwy_object_unref(gwy3dwindow->small_toolbar);
}

/**
 * gwy_3d_window_new:
 * @gwy3dview: A #Gwy3DView containing the data-displaying widget to show.
 *
 * Creates a new OpenGL 3D data displaying window.
 *
 * Returns: A newly created widget, as #GtkWidget.
 *
 * Since: 1.5
 **/
GtkWidget*
gwy_3d_window_new(Gwy3DView *gwy3dview)
{
    static const GwyEnum label_entries[] = {
        { N_("X-axis"),          GWY_3D_VIEW_LABEL_X, },
        { N_("Y-axis"),          GWY_3D_VIEW_LABEL_Y, },
        { N_("Minimum z value"), GWY_3D_VIEW_LABEL_MIN, },
        { N_("Maximum z value"), GWY_3D_VIEW_LABEL_MAX, }
    };
    /* there are two modes, represented by a boolean, so we simply use the
     * boolean value an `enum' */
    static const GwyEnum display_modes[] = {
        { N_("_Lighting"),    TRUE },
        { N_("P_alette"),     FALSE },
    };
    Gwy3DWindow *gwy3dwindow;
    GwyGLMaterial *material;
    GtkRequisition size_req;
    const gchar *name;
    GtkWidget *vbox, *hbox, *table, *toolbar, *spin, *button, *omenu, *group,
               *label, *check, *entry, *hbox2;
    GSList *display_mode_group;
    gboolean lights_on;
    gint row;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), NULL);

    gwy3dwindow = (Gwy3DWindow*)g_object_new(GWY_TYPE_3D_WINDOW, NULL);
    gtk_window_set_wmclass(GTK_WINDOW(gwy3dwindow), "data",
                           g_get_application_name());
    gtk_window_set_resizable(GTK_WINDOW(gwy3dwindow), TRUE);

    gwy3dwindow->buttons = g_new0(GtkWidget*, BUTTON_LAST);
    gwy3dwindow->in_update = FALSE;

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(gwy3dwindow), hbox);

    gwy3dwindow->gwy3dview = (GtkWidget*)gwy3dview;
    gtk_box_pack_start(GTK_BOX(hbox), gwy3dwindow->gwy3dview, TRUE, TRUE, 0);

    gwy3dwindow->vbox = gtk_vbox_new(FALSE, 4);
    g_object_ref(gwy3dwindow->vbox);
    gtk_container_set_border_width(GTK_CONTAINER(gwy3dwindow->vbox), 6);

    /* Small toolbar */
    toolbar = gwy_toolbox_new(1);
    gtk_container_set_border_width(GTK_CONTAINER(toolbar), 4);
    gtk_box_pack_end(GTK_BOX(hbox), toolbar, FALSE, FALSE, 0);
    gwy3dwindow->small_toolbar = toolbar;
    g_object_ref(gwy3dwindow->small_toolbar);

    button = gwy_toolbox_append(GWY_TOOLBOX(toolbar), GTK_TYPE_BUTTON,
                                NULL, _("Show full controls"),
                                NULL, GWY_STOCK_LESS,
                                G_CALLBACK(gwy_3d_window_select_controls),
                                GINT_TO_POINTER(TRUE));
    g_object_set_data(G_OBJECT(button), "gwy3dwindow", gwy3dwindow);

    button = gwy_toolbox_append(GWY_TOOLBOX(toolbar), GTK_TYPE_RADIO_BUTTON,
                                NULL, _("Rotate view"),
                                NULL, GWY_STOCK_ROTATE,
                                G_CALLBACK(gwy_3d_window_set_mode),
                                GINT_TO_POINTER(GWY_3D_ROTATION));
    group = button;
    gwy3dwindow->buttons[BUTTON_ROTATE2] = button;
    g_object_set_data(G_OBJECT(button), "gwy3dwindow", gwy3dwindow);

    button = gwy_toolbox_append(GWY_TOOLBOX(toolbar), GTK_TYPE_RADIO_BUTTON,
                                group, _("Scale view as a whole"),
                                NULL, GWY_STOCK_SCALE,
                                G_CALLBACK(gwy_3d_window_set_mode),
                                GINT_TO_POINTER(GWY_3D_SCALE));
    gwy3dwindow->buttons[BUTTON_SCALE2] = button;
    g_object_set_data(G_OBJECT(button), "gwy3dwindow", gwy3dwindow);

    button = gwy_toolbox_append(GWY_TOOLBOX(toolbar), GTK_TYPE_RADIO_BUTTON,
                                group, _("Scale value range"),
                                NULL, GWY_STOCK_Z_SCALE,
                                G_CALLBACK(gwy_3d_window_set_mode),
                                GINT_TO_POINTER(GWY_3D_DEFORMATION));
    gwy3dwindow->buttons[BUTTON_Z_SCALE2] = button;
    g_object_set_data(G_OBJECT(button), "gwy3dwindow", gwy3dwindow);

    button = gwy_toolbox_append(GWY_TOOLBOX(toolbar), GTK_TYPE_RADIO_BUTTON,
                                group, _("Move light source"),
                                NULL, GWY_STOCK_LIGHT_ROTATE,
                                G_CALLBACK(gwy_3d_window_set_mode),
                                GINT_TO_POINTER(GWY_3D_LIGHT_MOVEMENT));
    gwy3dwindow->buttons[BUTTON_LIGHT2] = button;
    g_object_set_data(G_OBJECT(button), "gwy3dwindow", gwy3dwindow);

    /* Toolbar */
    hbox2 = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(gwy3dwindow->vbox), hbox2, FALSE, FALSE, 0);

    toolbar = gwy_toolbox_new(4);
    gtk_box_pack_start(GTK_BOX(hbox2), toolbar, FALSE, FALSE, 0);

    button = gwy_toolbox_append(GWY_TOOLBOX(toolbar), GTK_TYPE_RADIO_BUTTON,
                                NULL, _("Rotate view"),
                                NULL, GWY_STOCK_ROTATE,
                                G_CALLBACK(gwy_3d_window_set_mode),
                                GINT_TO_POINTER(GWY_3D_ROTATION));
    group = button;
    gwy3dwindow->buttons[BUTTON_ROTATE] = button;
    g_object_set_data(G_OBJECT(button), "gwy3dwindow", gwy3dwindow);

    button = gwy_toolbox_append(GWY_TOOLBOX(toolbar), GTK_TYPE_RADIO_BUTTON,
                                group, _("Scale view as a whole"),
                                NULL, GWY_STOCK_SCALE,
                                G_CALLBACK(gwy_3d_window_set_mode),
                                GINT_TO_POINTER(GWY_3D_SCALE));
    gwy3dwindow->buttons[BUTTON_SCALE] = button;
    g_object_set_data(G_OBJECT(button), "gwy3dwindow", gwy3dwindow);

    button = gwy_toolbox_append(GWY_TOOLBOX(toolbar), GTK_TYPE_RADIO_BUTTON,
                                group, _("Scale value range"),
                                NULL, GWY_STOCK_Z_SCALE,
                                G_CALLBACK(gwy_3d_window_set_mode),
                                GINT_TO_POINTER(GWY_3D_DEFORMATION));
    gwy3dwindow->buttons[BUTTON_Z_SCALE] = button;
    g_object_set_data(G_OBJECT(button), "gwy3dwindow", gwy3dwindow);

    button = gwy_toolbox_append(GWY_TOOLBOX(toolbar), GTK_TYPE_RADIO_BUTTON,
                                group, _("Move light source"),
                                NULL, GWY_STOCK_LIGHT_ROTATE,
                                G_CALLBACK(gwy_3d_window_set_mode),
                                GINT_TO_POINTER(GWY_3D_LIGHT_MOVEMENT));
    gwy3dwindow->buttons[BUTTON_LIGHT] = button;
    g_object_set_data(G_OBJECT(button), "gwy3dwindow", gwy3dwindow);

    toolbar = gwy_toolbox_new(1);
    gtk_box_pack_end(GTK_BOX(hbox2), toolbar, FALSE, FALSE, 0);

    button = gwy_toolbox_append(GWY_TOOLBOX(toolbar), GTK_TYPE_BUTTON,
                                NULL, _("Hide full controls"),
                                NULL, GWY_STOCK_MORE,
                                G_CALLBACK(gwy_3d_window_select_controls),
                                GINT_TO_POINTER(FALSE));
    g_object_set_data(G_OBJECT(button), "gwy3dwindow", gwy3dwindow);

    gwy3dwindow->notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(gwy3dwindow->vbox), gwy3dwindow->notebook,
                       TRUE, TRUE, 0);

    /* Basic table */
    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 2);
    gtk_notebook_append_page(GTK_NOTEBOOK(gwy3dwindow->notebook),
                             vbox, gtk_label_new(_("Basic")));

    table = gtk_table_new(7, 3, FALSE);
    gtk_box_pack_start(GTK_BOX(vbox), table, TRUE, TRUE, 0);
    row = 0;

    spin = gwy_table_attach_spinbutton
               (table, row++, _("_Phi:"), _("deg"),
                (GtkObject*)gwy_3d_view_get_rot_x_adjustment(gwy3dview));
    spin = gwy_table_attach_spinbutton
               (table, row++, _("_Theta:"), _("deg"),
                (GtkObject*)gwy_3d_view_get_rot_y_adjustment(gwy3dview));
    spin = gwy_table_attach_spinbutton
               (table, row++, _("_Scale:"), NULL,
                (GtkObject*)gwy_3d_view_get_view_scale_adjustment(gwy3dview));
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 2);
    spin = gwy_table_attach_spinbutton
               (table, row++, _("_Value scale:"), NULL,
                (GtkObject*)gwy_3d_view_get_z_deformation_adjustment(gwy3dview));
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 2);

    check = gtk_check_button_new_with_mnemonic(_("Show _axes"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check),
                                 gwy_3d_view_get_show_axes(gwy3dview));
    gtk_table_attach(GTK_TABLE(table), check,
                     0, 3, row, row+1, GTK_FILL, 0, 2, 2);
    g_signal_connect(check, "toggled",
                     G_CALLBACK(gwy_3d_window_show_axes_changed), gwy3dwindow);
    row++;

    check = gtk_check_button_new_with_mnemonic(_("Show _labels"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check),
                                 gwy_3d_view_get_show_labels(gwy3dview));
    gtk_table_attach(GTK_TABLE(table), check,
                     0, 3, row, row+1, GTK_FILL, 0, 2, 2);
    g_signal_connect(check, "toggled",
                     G_CALLBACK(gwy_3d_window_show_labels_changed),
                     gwy3dwindow);
    row++;

    check = gtk_check_button_new_with_mnemonic(_("_Ortographic projection"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check),
                                 gwy_3d_view_get_orthographic(gwy3dview));
    gtk_table_attach(GTK_TABLE(table), check,
                     0, 3, row, row+1, GTK_FILL, 0, 2, 2);
    g_signal_connect(check, "toggled",
                     G_CALLBACK(gwy_3d_window_ortographic_changed),
                     gwy3dwindow);
    row++;

    /* Light & Material table */
    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 2);
    gtk_notebook_append_page(GTK_NOTEBOOK(gwy3dwindow->notebook),
                             vbox, gtk_label_new(_("Light & Material")));

    table = gtk_table_new(4, 3, FALSE);
    gtk_box_pack_start(GTK_BOX(vbox), table, TRUE, TRUE, 0);
    row = 0;

    lights_on = gwy_3d_view_get_use_lights(gwy3dview);
    display_mode_group
        = gwy_radio_buttons_create(display_modes, G_N_ELEMENTS(display_modes),
                                   "display-mode",
                                   G_CALLBACK(gwy_3d_window_display_mode_changed),
                                   gwy3dwindow,
                                   lights_on);
    gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(display_mode_group->data),
                     0, 3, row, row+1, GTK_FILL, 0, 2, 2);
    row++;

    label = gtk_label_new_with_mnemonic(_("_Material:"));
    gwy3dwindow->material_label = label;
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    gtk_widget_set_sensitive(label, lights_on);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 3, row, row+1, GTK_FILL, 0, 2, 2);
    row++;

    material = gwy_3d_view_get_material(gwy3dview);
    name = gwy_gl_material_get_name(material);
    omenu = gwy_option_menu_gl_material(G_CALLBACK(gwy_3d_window_set_material),
                                        gwy3dwindow, name);
    gwy3dwindow->material_menu = omenu;
    gtk_widget_set_sensitive(omenu, lights_on);
    gtk_table_attach(GTK_TABLE(table), omenu,
                     0, 3, row, row+1, GTK_FILL, 0, 2, 2);
    row++;

    spin = gwy_table_attach_spinbutton
               (table, row++, _("Light _phi:"), _("deg"),
                (GtkObject*)gwy_3d_view_get_light_z_adjustment(gwy3dview));
    gwy3dwindow->lights_spin1 = spin;
    gtk_widget_set_sensitive(spin, lights_on);

    spin = gwy_table_attach_spinbutton
               (table, row++, _("Light _theta:"), _("deg"),
                (GtkObject*)gwy_3d_view_get_light_y_adjustment(gwy3dview));
    gwy3dwindow->lights_spin2 = spin;
    gtk_widget_set_sensitive(spin, lights_on);
    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 12);
    gtk_widget_set_sensitive(gwy3dwindow->buttons[BUTTON_LIGHT], lights_on);
    gtk_widget_set_sensitive(gwy3dwindow->buttons[BUTTON_LIGHT2], lights_on);
    row++;

    gtk_table_attach(GTK_TABLE(table),
                     GTK_WIDGET(display_mode_group->next->data),
                     0, 3, row, row+1, GTK_FILL, 0, 2, 2);
    row++;

    name = gwy_3d_view_get_gradient(gwy3dview);
    omenu = gwy_option_menu_gradient(G_CALLBACK(gwy_3d_window_set_gradient),
                                     gwy3dwindow, name);
    gtk_widget_set_sensitive(omenu, !lights_on);
    gwy3dwindow->palette_menu = omenu;
    gtk_table_attach(GTK_TABLE(table), omenu,
                     0, 3, row, row+1, GTK_FILL, 0, 2, 2);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    /* Labels table */
    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 2);
    gtk_notebook_append_page(GTK_NOTEBOOK(gwy3dwindow->notebook),
                             vbox, gtk_label_new(_("Labels")));

    table = gtk_table_new(8, 3, FALSE);
    gtk_box_pack_start(GTK_BOX(vbox), table, TRUE, TRUE, 0);
    row = 0;

    omenu = gwy_option_menu_create(label_entries, G_N_ELEMENTS(label_entries),
                                   "labels-type",
                                   G_CALLBACK(gwy_3d_window_set_labels),
                                   gwy3dwindow, -1);
    gwy_table_attach_row(table, row, _("_Label:"), NULL, omenu);
    gwy3dwindow->labels_menu = omenu;
    row++;


    label = gtk_label_new_with_mnemonic(_("_Text:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_FILL, 0, 2, 2);

    entry = gtk_entry_new_with_max_length(100);
    g_signal_connect (G_OBJECT (entry), "activate",
                      G_CALLBACK (gwy_3d_window_labels_entry_activate),
                      (gpointer) gwy3dwindow);
    gtk_entry_set_text(GTK_ENTRY(entry), gwy_3d_view_get_label_description(
                       gwy3dview, GWY_3D_VIEW_LABEL_X)->text);
    gtk_editable_select_region(GTK_EDITABLE(entry),
                               0, GTK_ENTRY(entry)->text_length);

    gtk_table_attach(GTK_TABLE(table), entry,
                     1, 3, row, row+1, GTK_FILL, 0, 2, 2);
    gwy3dwindow->labels_text = entry;
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    label = gtk_label_new(_("Move label"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_FILL, 0, 2, 2);
    row++;

    spin = gwy_table_attach_spinbutton
               (table, row++, _("_Horizontally:"), _("pixels"),
                (GtkObject*)gwy_3d_view_get_label_description(
                    gwy3dview, GWY_3D_VIEW_LABEL_X)->delta_x);
    gwy3dwindow->labels_delta_x = spin;
    row++;

    spin = gwy_table_attach_spinbutton
               (table, row++, _("_Vertically:"), _("pixels"),
                (GtkObject*)gwy_3d_view_get_label_description(
                    gwy3dview, GWY_3D_VIEW_LABEL_X)->delta_y);
    gwy3dwindow->labels_delta_y = spin;
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    check = gtk_check_button_new_with_mnemonic(_("Scale size _automatically"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check),
                                 gwy_3d_view_get_label_description(
                                     gwy3dview,
                                     GWY_3D_VIEW_LABEL_X)->auto_scale);
    gtk_table_attach(GTK_TABLE(table), check,
                     0, 3, row, row+1, GTK_FILL, 0, 2, 2);
    g_signal_connect(check, "toggled",
                     G_CALLBACK(gwy_3d_window_auto_scale_changed),
                     gwy3dwindow);
    gwy3dwindow->labels_autosize_check = check;
    row++;

    spin = gwy_table_attach_spinbutton
               (table, row++, _("Si_ze:"), _("pixels"),
                (GtkObject*)gwy_3d_view_get_label_description(
                    gwy3dview, GWY_3D_VIEW_LABEL_X)->size);
    gtk_widget_set_sensitive(spin,
                !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check)));
    gwy3dwindow->labels_size = spin;
    row++;

    button = gtk_button_new_with_mnemonic(_("_Reset"));
    g_signal_connect(button, "clicked",
                     G_CALLBACK(gwy_3d_window_labels_reset_clicked),
                     gwy3dwindow);
    gtk_table_attach(GTK_TABLE(table), button,
                     0, 1, row, row+1, GTK_FILL, 0, 2, 2);
    gwy3dwindow->actions = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(gwy3dwindow->vbox), gwy3dwindow->actions,
                       FALSE, FALSE, 0);

    gtk_widget_show_all(hbox);

    /* make the 3D view at least DEFAULT_SIZE x DEFAULT_SIZE */
    gtk_widget_size_request(gwy3dwindow->vbox, &size_req);
    size_req.height = MAX(size_req.height, DEFAULT_SIZE);
    gtk_window_set_default_size(GTK_WINDOW(gwy3dwindow),
                                size_req.width/2 + size_req.height,
                                size_req.height);

    return GTK_WIDGET(gwy3dwindow);
}

/**
 * gwy_3d_window_get_3d_view:
 * @gwy3dwindow: A 3D data view window.
 *
 * Returns the #Gwy3DView widget this 3D window currently shows.
 *
 * Returns: The currently shown #GwyDataView.
 *
 * Since: 1.5
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
 *
 * Since: 1.5
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
 * @callback: Callback action.
 * @cbdata: Data to pass to @callback.
 *
 * Adds a button to small @gwy3dwindow toolbar.
 *
 * The small toolbar is those visible when full controls are hidden.  Due to
 * space constraints the button must be contain only a pixmap.
 *
 * Since: 1.6
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

    button = gwy_toolbox_append(GWY_TOOLBOX(gwy3dwindow->small_toolbar),
                                GTK_TYPE_BUTTON,
                                NULL, tooltip, NULL, stock_id,
                                callback, cbdata);
    g_object_set_data(G_OBJECT(button), "gwy3dwindow", gwy3dwindow);
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
    switch (movement) {
        case GWY_3D_ROTATION:
        gtk_toggle_button_set_active
            (GTK_TOGGLE_BUTTON(gwy3dwindow->buttons[BUTTON_ROTATE]), TRUE);
        gtk_toggle_button_set_active
            (GTK_TOGGLE_BUTTON(gwy3dwindow->buttons[BUTTON_ROTATE2]), TRUE);
        break;

        case GWY_3D_SCALE:
        gtk_toggle_button_set_active
            (GTK_TOGGLE_BUTTON(gwy3dwindow->buttons[BUTTON_SCALE]), TRUE);
        gtk_toggle_button_set_active
            (GTK_TOGGLE_BUTTON(gwy3dwindow->buttons[BUTTON_SCALE2]), TRUE);
        break;

        case GWY_3D_DEFORMATION:
        gtk_toggle_button_set_active
            (GTK_TOGGLE_BUTTON(gwy3dwindow->buttons[BUTTON_Z_SCALE]), TRUE);
        gtk_toggle_button_set_active
            (GTK_TOGGLE_BUTTON(gwy3dwindow->buttons[BUTTON_Z_SCALE2]), TRUE);
        break;

        case GWY_3D_LIGHT_MOVEMENT:
        gtk_toggle_button_set_active
            (GTK_TOGGLE_BUTTON(gwy3dwindow->buttons[BUTTON_LIGHT]), TRUE);
        gtk_toggle_button_set_active
            (GTK_TOGGLE_BUTTON(gwy3dwindow->buttons[BUTTON_LIGHT]), TRUE);
        break;

        default:
        g_assert_not_reached();
        break;
    }
    gwy_3d_view_set_status(GWY_3D_VIEW(gwy3dwindow->gwy3dview), movement);
    gwy3dwindow->in_update = FALSE;
}

static void
gwy_3d_window_set_gradient(GtkWidget *item,
                           Gwy3DWindow *gwy3dwindow)
{
    gwy_3d_view_set_gradient(GWY_3D_VIEW(gwy3dwindow->gwy3dview),
                             (gchar*)g_object_get_data(G_OBJECT(item),
                                                       "gradient-name"));
}

static void
gwy_3d_window_set_material(GtkWidget *item,
                           Gwy3DWindow *gwy3dwindow)
{
    gchar *material_name;
    GwyGLMaterial *material;

    material_name = g_object_get_data(G_OBJECT(item), "material-name");
    material = gwy_gl_material_get_by_name(material_name);
    gwy_3d_view_set_material(GWY_3D_VIEW(gwy3dwindow->gwy3dview), material);
}

static void
gwy_3d_window_select_controls(gpointer data, GtkWidget *button)
{
    Gwy3DWindow *gwy3dwindow;
    GtkWidget *hbox;

    gwy3dwindow = (Gwy3DWindow*)g_object_get_data(G_OBJECT(button),
                                                  "gwy3dwindow");
    g_return_if_fail(GWY_IS_3D_WINDOW(gwy3dwindow));

    hbox = GTK_BIN(gwy3dwindow)->child;
    g_return_if_fail(GTK_IS_HBOX(GTK_BIN(gwy3dwindow)->child));
    if (GPOINTER_TO_INT(data)) {
        gtk_container_remove(GTK_CONTAINER(hbox), gwy3dwindow->small_toolbar);
        gtk_box_pack_end(GTK_BOX(hbox), gwy3dwindow->vbox, FALSE, FALSE, 0);
        gtk_widget_show_all(gwy3dwindow->vbox);
    }
    else {
        gtk_container_remove(GTK_CONTAINER(hbox), gwy3dwindow->vbox);
        gtk_box_pack_end(GTK_BOX(hbox), gwy3dwindow->small_toolbar,
                         FALSE, FALSE, 0);
        gtk_widget_show_all(gwy3dwindow->small_toolbar);
    }
}

static void
gwy_3d_window_set_labels(G_GNUC_UNUSED GtkWidget *item,
                         Gwy3DWindow *gwy3dwindow)
{
    gint idx;
    Gwy3DLabelDescription * ld;

    idx = gtk_option_menu_get_history(GTK_OPTION_MENU(gwy3dwindow->labels_menu));
    ld = gwy_3d_view_get_label_description(
        GWY_3D_VIEW(gwy3dwindow->gwy3dview), idx);
    g_return_if_fail(ld);

    gtk_entry_set_text(GTK_ENTRY(gwy3dwindow->labels_text), ld->text);
    gtk_spin_button_set_adjustment
        (GTK_SPIN_BUTTON(gwy3dwindow->labels_delta_x), ld->delta_x);
    gtk_spin_button_set_value
        (GTK_SPIN_BUTTON(gwy3dwindow->labels_delta_x), ld->delta_x->value);
    gtk_spin_button_set_adjustment
        (GTK_SPIN_BUTTON(gwy3dwindow->labels_delta_y), ld->delta_y);
    gtk_spin_button_set_value
        (GTK_SPIN_BUTTON(gwy3dwindow->labels_delta_y), ld->delta_y->value);
    gtk_toggle_button_set_active
        (GTK_TOGGLE_BUTTON(gwy3dwindow->labels_autosize_check), ld->auto_scale);
    gtk_spin_button_set_adjustment
        (GTK_SPIN_BUTTON(gwy3dwindow->labels_size),    ld->size);
    gtk_spin_button_set_value
        (GTK_SPIN_BUTTON(gwy3dwindow->labels_size),    ld->size->value);
}

static void
gwy_3d_window_ortographic_changed(GtkToggleButton *check,
                                  Gwy3DWindow *window)
{
    gwy_3d_view_set_orthographic(GWY_3D_VIEW(window->gwy3dview),
                                 gtk_toggle_button_get_active(check));
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
gwy_3d_window_display_mode_changed(GtkRadioButton *radio,
                                   Gwy3DWindow *window)
{
    gboolean lights_on;
    GwyGLMaterial *material;
    GtkWidget *menu, *item;

    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radio)))
        return;

    lights_on
        = gwy_radio_buttons_get_current(gtk_radio_button_get_group(radio),
                                        "display-mode");
    gwy_3d_view_set_use_lights(GWY_3D_VIEW(window->gwy3dview), lights_on);
    gtk_widget_set_sensitive(window->material_menu, lights_on);
    gtk_widget_set_sensitive(window->material_label, lights_on);
    gtk_widget_set_sensitive(window->palette_menu, !lights_on);
    gtk_widget_set_sensitive(window->lights_spin1, lights_on);
    gtk_widget_set_sensitive(window->lights_spin2, lights_on);
    gtk_widget_set_sensitive(window->buttons[BUTTON_LIGHT], lights_on);
    gtk_widget_set_sensitive(window->buttons[BUTTON_LIGHT2], lights_on);
    if (lights_on) {
        material = gwy_3d_view_get_material(GWY_3D_VIEW(window->gwy3dview));
        /* FIXME: A hack. Maybe should GLMaterial set default different from
         * None? */
        if (strcmp(gwy_gl_material_get_name(material), GWY_GL_MATERIAL_NONE)
            == 0) {
            menu = gtk_option_menu_get_menu
                                 (GTK_OPTION_MENU(window->material_menu));
            item = gtk_menu_get_active(GTK_MENU(menu));
            gwy_3d_window_set_material(item, window);
        }
    }
}

static void
gwy_3d_window_auto_scale_changed(GtkToggleButton *check,
                                  Gwy3DWindow *window)
{
    gboolean active = gtk_toggle_button_get_active(check);
    gint idx = gtk_option_menu_get_history(GTK_OPTION_MENU(window->labels_menu));
    gtk_widget_set_sensitive(window->labels_size, !active);
    gwy_3d_label_description_set_autoscale(
        gwy_3d_view_get_label_description(GWY_3D_VIEW(window->gwy3dview), idx), active);
}

static void
gwy_3d_window_labels_entry_activate(GtkEntry *entry,  Gwy3DWindow *window)
{
    gint idx;

    idx = gtk_option_menu_get_history(GTK_OPTION_MENU(window->labels_menu));
    gwy_debug("label id:%d, label text: %s",
              idx, gtk_entry_get_text(GTK_ENTRY(entry)));
    gwy_3d_label_description_set_text(
        gwy_3d_view_get_label_description(
            GWY_3D_VIEW(window->gwy3dview), idx),
        gtk_entry_get_text(GTK_ENTRY(entry))
    );
}

static void
gwy_3d_window_labels_reset_clicked(G_GNUC_UNUSED GtkButton *button,
                                   Gwy3DWindow *window)
{
    gint idx = gtk_option_menu_get_history(GTK_OPTION_MENU(window->labels_menu));
    gwy_3d_label_description_reset(
        gwy_3d_view_get_label_description(
            GWY_3D_VIEW(window->gwy3dview), idx)
     );
     gtk_toggle_button_set_active
         (GTK_TOGGLE_BUTTON(window->labels_autosize_check), TRUE);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
