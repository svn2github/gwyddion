#include <stdlib.h>
#include <gtk/gtk.h>
#include "libgwy/libgwy.h"
#include "libgwyui/libgwyui.h"

enum { NAXES = 7 };

static GwyGraphCurve*
make_random_curve(GwyRand *rng)
{
    guint n = 60;
    GwyCurve *curve = gwy_curve_new_sized(n);
    gdouble x = 0.0, y = gwy_rand_double(rng), vy = 0.0;
    for (guint i = 0; i < n; i++) {
        curve->data[i].x = 1e-15*x;
        curve->data[i].y = 1e9*y;
        x += 0.05 + 0.01*gwy_rand_double(rng);
        y += vy;
        vy += 0.01*gwy_rand_normal(rng);
    }

    GwyGraphCurve *graphcurve = gwy_graph_curve_new();
    gwy_graph_curve_set_curve(graphcurve, curve);
    g_object_unref(curve);
    g_object_set(graphcurve,
                 "point-color", gwy_rgba_get_preset_color(gwy_rand_int(rng)),
                 "line-color", gwy_rgba_get_preset_color(gwy_rand_int(rng)),
                 "plot-type", (guint)(gwy_rand_int(rng) % 5),
                 "point-type", (guint)(gwy_rand_int(rng) % 18),
                 "line-type", (guint)(gwy_rand_int(rng) % 3),
                 NULL);

    return graphcurve;
}

static GtkWidget*
create_graph_window(void)
{
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(window), 640, 360);
    gtk_window_set_title(GTK_WINDOW(window), "Gwy3 Graphing Test");
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *graphwidget = gwy_graph_new();
    GwyGraph *graph = GWY_GRAPH(graphwidget);
    gtk_container_add(GTK_CONTAINER(window), graphwidget);
    gwy_graph_set_x_log_scale(graph, TRUE);

    GwyGraphArea *area = gwy_graph_get_area(graph);
    GwyRand *rng = gwy_rand_new();

    for (guint i = 0; i < 6; i++) {
        GwyGraphCurve *graphcurve = make_random_curve(rng);
        gwy_graph_area_add(area, graphcurve);
    }
    gwy_rand_free(rng);

    GwyGraphAxis *gaxis;
    gaxis = gwy_graph_get_top_axis(graph);
    gwy_graph_axis_set_label(gaxis, "Top");
    gaxis = gwy_graph_get_bottom_axis(graph);
    gwy_graph_axis_set_label(gaxis, "Bottom");
    gaxis = gwy_graph_get_left_axis(graph);
    gwy_graph_axis_set_label(gaxis, "Left");
    gaxis = gwy_graph_get_right_axis(graph);
    gwy_graph_axis_set_label(gaxis, "Right");

    return window;
}

static gboolean
axis_draw_line(GtkWidget *axis,
               cairo_t *cr)
{
    gdouble w = gtk_widget_get_allocated_width(axis);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, 0.0, 0.5);
    cairo_line_to(cr, w, 0.5);
    cairo_stroke(cr);
    return FALSE;
}

static void
configure_axes(GtkGrid *grid)
{
    static const GwyRange linear_ranges[] = {
        { 0.0, 15.0 },
        { 1.0, 2.0 },
        { 0.0, 1235.0 },
        { -5.0, 0.1 },
        { -0.000001, -0.00001 },
        { 3.234231e6, 8.34321e8 },
        { -0.1, 2.33e4 },
    };
    static const GwyRange logarithmic_ranges[] = {
        { 1e-5, 1e2 },
        { 0.2, 24.0 },
        { 5.0, 14.0 },
        { 3e-6, 4e12 },
        { 9e3, 2e6 },
        { 1e-30, 1e50 },
        { 2.3e6, 7e6 },
    };

    gboolean increasing = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(grid),
                                                             "increasing"));
    gboolean logscale = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(grid),
                                                           "logscale"));

    static const GwyRange *ranges;
    ranges = logscale ? logarithmic_ranges : linear_ranges;

    for (guint i = 0; i < NAXES; i++) {
        GtkWidget *axis = gtk_grid_get_child_at(grid, 0, i);
        gwy_graph_axis_set_log_scale(GWY_GRAPH_AXIS(axis), logscale);
        GwyRange range = ranges[i];
        if (!increasing)
            GWY_SWAP(gdouble, range.from, range.to);
        gwy_axis_request_range(GWY_AXIS(axis), &range);
    }
}

static void
axes_toggle_changed(GtkToggleButton *toggle,
                    GtkGrid *grid)
{
    const gchar *id = (const gchar*)g_object_get_data(G_OBJECT(toggle), "id");
    gboolean value = gtk_toggle_button_get_active(toggle);
    g_object_set_data(G_OBJECT(grid), id, GUINT_TO_POINTER(value));
    configure_axes(grid);
}

static GtkWidget*
create_axes_window(void)
{
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(window), 640, 360);
    gtk_window_set_title(GTK_WINDOW(window), "Gwy3 Axis Test");
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkGrid *grid = (GtkGrid*)gtk_grid_new();
    gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(grid));

    for (guint i = 0; i < NAXES; i++) {
        GtkWidget *axis = gwy_graph_axis_new();
        g_object_set(axis, "hexpand", TRUE, "margin", 8, NULL);
        gwy_axis_set_edge(GWY_AXIS(axis), GTK_POS_BOTTOM);
        gtk_grid_attach(grid, GTK_WIDGET(axis), 0, i, 1, 1);
        g_signal_connect(axis, "draw", G_CALLBACK(axis_draw_line), NULL);
    }

    GtkWidget *hbox = gtk_grid_new();
    gtk_grid_attach(grid, hbox, 0, NAXES, 1, 1);

    GtkWidget *increasing = gtk_check_button_new_with_label("Increasing");
    g_object_set_data(G_OBJECT(increasing), "id", (gpointer)"increasing");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(increasing), TRUE);
    gtk_grid_attach(GTK_GRID(hbox), increasing, 0, 0, 1, 1);
    g_object_set_data(G_OBJECT(grid), "increasing", GUINT_TO_POINTER(TRUE));
    g_signal_connect(increasing, "toggled",
                     G_CALLBACK(axes_toggle_changed), grid);

    GtkWidget *logscale = gtk_check_button_new_with_label("Logscale");
    g_object_set_data(G_OBJECT(logscale), "id", (gpointer)"logscale");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(logscale), FALSE);
    gtk_grid_attach(GTK_GRID(hbox), logscale, 1, 0, 1, 1);
    g_object_set_data(G_OBJECT(grid), "logscale", GUINT_TO_POINTER(FALSE));
    g_signal_connect(logscale, "toggled",
                     G_CALLBACK(axes_toggle_changed), grid);

    configure_axes(grid);

    return window;
}

int
main(int argc, char *argv[])
{
    gtk_init(&argc, &argv);

    GtkWidget *gwindow = create_graph_window();
    gtk_widget_show_all(gwindow);

    GtkWidget *awindow = create_axes_window();
    gtk_widget_show_all(awindow);

    gtk_main();

    return 0;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
