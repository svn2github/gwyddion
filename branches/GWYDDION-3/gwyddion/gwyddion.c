#include <stdlib.h>
#include <gtk/gtk.h>
#include "libgwy/libgwy.h"
#include "libgwyui/libgwyui.h"

typedef struct {
    GwyField *field1;
    GwyField *field2;
    GwyField *result;
    gdouble phase;
} Mix;

static void
set_data(GwyField *field)
{
    GwyRand *rng = gwy_rand_new();

    gdouble *d = field->data;
    for (guint i = field->xres*field->yres; i; i--, d++)
        *d = gwy_rand_double(rng);
    gwy_field_invalidate(field);

    gwy_field_filter_gaussian(field, NULL, field, 3.0, 3.0,
                              GWY_EXTERIOR_PERIODIC, 0.0);

    gwy_rand_free(rng);
}

G_GNUC_UNUSED
static void
update_mix(Mix *mix, GwyFieldPart *fpart)
{
    GwyField *result = mix->result;
    gwy_field_copy(mix->field1, fpart, result, 0, 0);
    gwy_field_multiply(result, fpart, NULL, 0, cos(mix->phase));
    gwy_field_add_field(mix->field2, fpart, result, 0, 0, sin(mix->phase));
    gwy_field_data_changed(result, fpart);
}

static Mix*
create_mix(gdouble xres, gdouble yres)
{
    Mix *mix = g_slice_new(Mix);

    mix->phase = 0.0;
    mix->result = gwy_field_new_sized(xres, yres, FALSE);
    gwy_unit_set_from_string(gwy_field_get_unit_x(mix->result), "m", NULL);
    gwy_unit_set_from_string(gwy_field_get_unit_y(mix->result), "m", NULL);
    gwy_unit_set_from_string(gwy_field_get_unit_z(mix->result), "N A^3/s", NULL);
    mix->field1 = gwy_field_new_alike(mix->result, FALSE);
    mix->field2 = gwy_field_new_alike(mix->result, FALSE);
    set_data(mix->field1);
    set_data(mix->field2);
    update_mix(mix, NULL);

    return mix;
}

G_GNUC_UNUSED
static gboolean
update(gpointer user_data)
{
    Mix *mix = (Mix*)user_data;
    mix->phase += 0.1;
    //GwyFieldPart fpart = { 0, 0, mix->result->xres/2, mix->result->yres/2 };
    //update_mix(mix, &fpart);
    update_mix(mix, NULL);
    return TRUE;
}

G_GNUC_NORETURN
static gboolean
print_version(G_GNUC_UNUSED const gchar *name,
              G_GNUC_UNUSED const gchar *value,
              G_GNUC_UNUSED gpointer data,
              G_GNUC_UNUSED GError **error)
{
    g_print("Gwyddion %s (running with libgwy %s).\n",
            GWY_VERSION_STRING, gwy_version_string());
    exit(EXIT_SUCCESS);
}

static GtkWidget*
create_raster_window(guint xres, guint yres)
{
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Gwy3 Raster View Test");
    gtk_window_set_default_size(GTK_WINDOW(window), xres, yres);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *rasterview = gwy_raster_view_new();
    gtk_container_add(GTK_CONTAINER(window), rasterview);

    GwyRasterArea *rasterarea = gwy_raster_view_get_area(GWY_RASTER_VIEW(rasterview));
    g_object_set(rasterarea,
                 "zoom", 3.0,
                 "real-aspect-ratio", FALSE,
                 "mask-color", &(GwyRGBA){ 0.4, 0.7, 0.1, 0.4 },
                 "number-grains", FALSE,
                 "range-from-method", GWY_COLOR_RANGE_FULL,
                 "range-to-method", GWY_COLOR_RANGE_FULL,
                 //"user-range", &(GwyRange){ 0.2, 0.8 },
                 NULL);

    Mix *mix = create_mix(3*xres/2, 3*yres/2);
    GwyMaskField *mask = gwy_mask_field_new_from_field(mix->result, NULL,
                                                       0.52, G_MAXDOUBLE, FALSE);
    gwy_raster_area_set_field(rasterarea, mix->result);
    gwy_raster_area_set_mask(rasterarea, mask);

    GwyCoords *coords = gwy_coords_point_new();
    gdouble xy[] = { 0.5, 0.5 };
    gwy_coords_set(coords, 0, xy);
    xy[0] = 0.4;
    gwy_coords_set(coords, 1, xy);
    xy[1] = 0.25;
    gwy_coords_set(coords, 2, xy);

    GwyShapes *shapes = gwy_shapes_point_new();
    g_object_set(shapes,
                 "radius", 8.0,
                 "max-shapes", 5,
                 //"editable", FALSE,
                 NULL);
    gwy_shapes_set_coords(shapes, coords);
    g_object_unref(coords);

    gwy_raster_area_set_shapes(rasterarea, shapes);

    return window;
}

static void
attach_adjustment(GtkGrid *grid,
                  guint row,
                  GtkAdjustment *adj,
                  const gchar *name,
                  const gchar *units,
                  GFunc function,
                  gpointer user_data)
{
    GtkWidget *bar = gwy_adjust_bar_new();
    gwy_adjust_bar_set_adjustment(GWY_ADJUST_BAR(bar), adj);
    GtkWidget *namelabel = gtk_bin_get_child(GTK_BIN(bar));
    gtk_label_set_text_with_mnemonic(GTK_LABEL(namelabel), name);
    gtk_grid_attach(grid, bar, 0, row, 1, 1);

    GtkWidget *spin = gwy_spin_button_new(adj, 0.0, 1);
    gtk_label_set_mnemonic_widget(GTK_LABEL(namelabel), spin);
    gtk_grid_attach(grid, spin, 1, row, 1, 1);

    GtkWidget *unitlabel = gtk_label_new(units);
    gtk_widget_set_halign(unitlabel, GTK_ALIGN_START);
    gtk_grid_attach(grid, unitlabel, 2, row, 1, 1);

    if (function) {
        function(bar, user_data);
        function(spin, user_data);
        function(unitlabel, user_data);
    }
}

static void
set_sensitive(gpointer widget, gpointer user_data)
{
    gtk_widget_set_sensitive(widget, GPOINTER_TO_INT(user_data));
}

static GtkWidget*
create_widget_test(void)
{
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Gwy3 Widget Test");
    gtk_window_set_default_size(GTK_WINDOW(window), 400, 300);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkGrid *grid = GTK_GRID(gtk_grid_new());
    gtk_grid_set_column_spacing(grid, 2);
    gtk_grid_set_row_spacing(grid, 2);
    gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(grid));

    GtkAdjustment *adj1 = gtk_adjustment_new(10.0, 0.0, 1000.0, 1.0, 10.0, 0.0);
    attach_adjustment(grid, 0, adj1, "_Value with a long name:", "µm",
                      NULL, NULL);

    GtkAdjustment *adj2 = gtk_adjustment_new(10.0, 0.0, 1000.0, 1.0, 10.0, 0.0);
    attach_adjustment(grid, 1, adj2, "_Short value:", "px",
                      set_sensitive, GINT_TO_POINTER(FALSE));

    GtkWidget *entry = gtk_entry_new();
    gtk_grid_attach(grid, entry, 1, 2, 2, 1);
    GtkWidget *label = gtk_label_new_with_mnemonic("_Entry:");
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_grid_attach(grid, label, 0, 2, 1, 1);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);

    return window;
}

int
main(int argc, char *argv[])
{
    static GOptionEntry entries[] = {
        { "version", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, &print_version, "Print version", NULL },
        { NULL, 0, 0, 0, NULL, NULL, NULL }
    };
    GOptionContext *context = g_option_context_new("");
    g_option_context_add_main_entries(context, entries, GETTEXT_PACKAGE);
    g_option_context_add_group(context, gtk_get_option_group(TRUE));
    GError *error = NULL;
    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        g_printerr("%s\n", error->message);
        g_clear_error(&error);
        exit(EXIT_FAILURE);
    }

    gwy_resources_load(NULL);
    g_object_set(gwy_gradients_get("Gray"), "preferred", TRUE, NULL);
    g_object_set(gwy_gradients_get("Sky"), "preferred", TRUE, NULL);
    g_object_set(gwy_gradients_get("Gwyddion.net"), "preferred", TRUE, NULL);
    g_object_set(gwy_gradients_get("Spectral"), "preferred", TRUE, NULL);
    gwy_register_stock_items();

    GtkWidget *rwindow = create_raster_window(600, 400);
    gtk_widget_show_all(rwindow);

    GtkWidget *twindow = create_widget_test();
    gtk_widget_show_all(twindow);
    //g_timeout_add(100, update, mix);

    gtk_main();

    return 0;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
