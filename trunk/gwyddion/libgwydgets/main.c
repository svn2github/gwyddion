/* @(#) $Id$ */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include <libgwydgets/gwydgets.h>
#include <libgwyddion/gwycontainer.h>
#include <libdraw/gwypalette.h>
#include <libprocess/datafield.h>

/***** VECTOR SHADE [[[ *****************************************************/
#define N 4

static gulong hid[N];
static GwySphereCoords *coords[N];

static void
foo_cb(GwySphereCoords *c, gpointer p)
{
    gint n = GPOINTER_TO_INT(p);
    gint i;
    gdouble theta, phi;

    for (i = 0; i < N; i++)
        g_signal_handler_block(G_OBJECT(coords[i]), hid[i]);
    theta = gwy_sphere_coords_get_theta(c);
    phi = gwy_sphere_coords_get_phi(c);
    i = (n + 1)%N;
    gwy_sphere_coords_set_value(coords[i],
                                gwy_sphere_coords_get_theta(coords[i]), phi);
    i = (n + N-1)%N;
    gwy_sphere_coords_set_value(coords[i],
                                theta, gwy_sphere_coords_get_phi(coords[i]));
    for (i = 0; i < N; i++)
        g_signal_handler_unblock(G_OBJECT(coords[i]), hid[i]);
}

static void
vector_shade_test(void)
{
    GtkWidget *win, *widget, *box;
    GObject *pal;
    gint i;

    win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_container_set_border_width(GTK_CONTAINER(win), 4);

    box = gtk_vbox_new(4, TRUE);
    gtk_container_add(GTK_CONTAINER(win), box);
    for (i = 0; i < N; i++) {
        widget = gwy_vector_shade_new(NULL);
        pal = gwy_palette_new(512);
        gwy_palette_setup_predef(GWY_PALETTE(pal), g_random_int()%10);
        gwy_grad_sphere_set_palette(
            GWY_GRAD_SPHERE(gwy_vector_shade_get_grad_sphere(GWY_VECTOR_SHADE(widget))),
            GWY_PALETTE(pal));
        gtk_box_pack_start(GTK_BOX(box), widget, TRUE, TRUE, 0);
        coords[i] = gwy_vector_shade_get_sphere_coords(GWY_VECTOR_SHADE(widget));
        hid[i] = g_signal_connect(G_OBJECT(coords[i]), "value_changed",
                                  G_CALLBACK(foo_cb), GINT_TO_POINTER(i));
    }

    gtk_widget_show_all(win);
    g_signal_connect(G_OBJECT(win), "destroy", gtk_main_quit, NULL);
}
/***** ]]] VECTOR SHADE *****************************************************/

/***** DATA VIEW [[[ ********************************************************/
#define FILENAME "data_field.object"

static void
data_view_test(void)
{
    GwyContainer *data;
    GtkWidget *window, *view;
    GObject *data_field;
    GwyDataViewLayer *layer;
    gchar *buffer = NULL;
    gsize size = 0;
    gsize pos = 0;
    GError *err = NULL;

    /* FIXME: this is necessary to initialize the object system */
    g_type_class_ref(gwy_data_field_get_type());

    g_file_get_contents(FILENAME, &buffer, &size, &err);
    g_assert(!err);
    data_field = gwy_serializable_deserialize(buffer, size, &pos);

    data = (GwyContainer*)gwy_container_new();
    gwy_container_set_object_by_name(data, "/0/data", data_field);

    view = gwy_data_view_new(data);
    layer = (GwyDataViewLayer*)gwy_layer_basic_new(data);
    gwy_data_view_set_base_layer(view, layer);
    g_object_unref(data);
    g_object_unref(data_field);

    window = gwy_data_window_new(GWY_DATA_VIEW(view));

    gtk_widget_show_all(window);
    g_signal_connect(G_OBJECT(window), "destroy", gtk_main_quit, NULL);
}
/***** ]]] DATA VIEW ********************************************************/

int
main(int argc, char *argv[])
{
    FILE *fh;
    guint32 seed;

    fh = fopen("/dev/urandom", "rb");
    fread(&seed, sizeof(guint32), 1, fh);
    fclose(fh);
    g_random_set_seed(seed);

    gtk_init(&argc, &argv);

    //vector_shade_test();
    data_view_test();
    gtk_main();

    return 0;
}
/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
