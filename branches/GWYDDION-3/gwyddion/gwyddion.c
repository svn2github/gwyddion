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
    gwy_unit_set_from_string(gwy_field_get_unit_xy(mix->result), "m", NULL);
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
    gtk_window_set_title(GTK_WINDOW(window), "Gwy3 UI Test");
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

    gwy_resource_type_load(GWY_TYPE_GRADIENT, NULL);
    gwy_register_stock_items();

    g_object_set(gwy_gradients_get("Gray"), "preferred", TRUE, NULL);
    g_object_set(gwy_gradients_get("Sky"), "preferred", TRUE, NULL);
    g_object_set(gwy_gradients_get("Gwyddion.net"), "preferred", TRUE, NULL);
    g_object_set(gwy_gradients_get("Spectral"), "preferred", TRUE, NULL);


    GtkWidget *window = create_raster_window(600, 400);
    gtk_widget_show_all(window);
    //g_timeout_add(100, update, mix);

    gtk_main();

    return 0;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
