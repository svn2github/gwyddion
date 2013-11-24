/*
 *  $Id$
 *  Copyright (C) 2013 David Nečas (Yeti).
 *  E-mail: yeti@gwyddion.net.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <libgwyapp.h>

typedef struct {
    GtkWidget *window;
    GtkWidget *leftbar;
    GtkWidget *rightbar;
    GtkWidget *dataarea;
    GwyFile *file;
} App;

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

static void
generate_data(App *app)
{
    GwyField *field = gwy_field_new_sized(400, 400, TRUE);
    gwy_field_subtract_plane(field, 0.0, -1.0, 1.0);

    GwyChannelData *channel = gwy_channel_data_new();
    gwy_channel_data_set_field(channel, field);
    g_object_unref(field);

    GwyDataList *channels = gwy_file_get_data_list(app->file, GWY_DATA_CHANNEL);
    guint id = gwy_data_list_add(channels, GWY_DATA_ITEM(channel));
    gchar *title = g_strdup_printf("Generated %u", id);
    gwy_field_set_name(field, title);
    g_free(title);
}

static void
attach_button(GtkGrid *grid, const gchar *label,
              GCallback callback, gpointer user_data)
{
    GtkWidget *button = gtk_button_new_with_label(label);
    gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
    gtk_grid_attach_next_to(grid, button, NULL, GTK_POS_BOTTOM, 1, 1);
    g_signal_connect_swapped(button, "clicked", callback, user_data);
}

static GtkWidget*
create_leftbar(App *app)
{
    GtkWidget *menu = gtk_grid_new();
    GtkGrid *grid = GTK_GRID(menu);

    attach_button(grid, "Generate data", G_CALLBACK(generate_data), app);

    return menu;
}

static void
render_thumb(G_GNUC_UNUSED GtkTreeViewColumn *column,
             GtkCellRenderer *renderer,
             GtkTreeModel *model,
             GtkTreeIter *iter,
             G_GNUC_UNUSED gpointer data)
{
    GwyDataItem **dataitem = NULL;
    gtk_tree_model_get(model, iter, 1, &dataitem, -1);
    GdkPixbuf *pixbuf = gwy_data_item_get_thumbnail(*dataitem, 0);
    if (pixbuf) {
        g_object_set(renderer, "pixbuf", pixbuf, NULL);
        g_object_unref(pixbuf);
    }
}

static void
render_id(G_GNUC_UNUSED GtkTreeViewColumn *column,
          GtkCellRenderer *renderer,
          GtkTreeModel *model,
          GtkTreeIter *iter,
          G_GNUC_UNUSED gpointer data)
{
    GwyDataItem **dataitem = NULL;
    gtk_tree_model_get(model, iter, 1, &dataitem, -1);
    gchar buf[12];
    g_snprintf(buf, sizeof(buf), "%u", gwy_data_item_get_id(*dataitem));
    g_object_set(renderer, "text", buf, NULL);
}

static void
render_name(G_GNUC_UNUSED GtkTreeViewColumn *column,
            GtkCellRenderer *renderer,
            GtkTreeModel *model,
            GtkTreeIter *iter,
            G_GNUC_UNUSED gpointer data)
{
    GwyDataItem **dataitem = NULL;
    gtk_tree_model_get(model, iter, 1, &dataitem, -1);
    const gchar *name = gwy_data_item_get_name(*dataitem);
    g_object_set(renderer, "text", name ? name : "Untitled", NULL);
}

static GtkWidget*
create_data_list_view(GwyDataList *datalist)
{
    GwyListStore *store = gwy_list_store_new(GWY_LISTABLE(datalist));
    GtkTreeModel *model = GTK_TREE_MODEL(store);
    GtkWidget *datalistview = gtk_tree_view_new_with_model(model);
    GtkTreeView *treeview = GTK_TREE_VIEW(datalistview);

    GtkTreeViewColumn *column_thumb = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(column_thumb, "Image");
    gtk_tree_view_column_set_expand(column_thumb, FALSE);
    gtk_tree_view_append_column(treeview, column_thumb);
    GtkCellRenderer *renderer_thumb = gtk_cell_renderer_pixbuf_new();
    gtk_tree_view_column_pack_start(column_thumb, renderer_thumb, TRUE);
    gtk_tree_view_column_set_cell_data_func(column_thumb, renderer_thumb,
                                            render_thumb, NULL, NULL);

    GtkTreeViewColumn *column_id = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(column_id, "Id");
    gtk_tree_view_column_set_expand(column_id, FALSE);
    gtk_tree_view_append_column(treeview, column_id);
    GtkCellRenderer *renderer_id = gtk_cell_renderer_text_new();
    gtk_cell_renderer_set_alignment(renderer_id, 1.0, 0.5);
    gtk_tree_view_column_pack_start(column_id, renderer_id, TRUE);
    gtk_tree_view_column_set_cell_data_func(column_id, renderer_id,
                                            render_id, NULL, NULL);

    GtkTreeViewColumn *column_name = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(column_name, "Name");
    gtk_tree_view_column_set_expand(column_name, TRUE);
    gtk_tree_view_append_column(treeview, column_name);
    GtkCellRenderer *renderer_name = gtk_cell_renderer_text_new();
    gtk_cell_renderer_set_alignment(renderer_name, 1.0, 0.5);
    gtk_tree_view_column_pack_start(column_name, renderer_name, TRUE);
    gtk_tree_view_column_set_cell_data_func(column_name, renderer_name,
                                            render_name, NULL, NULL);

    return datalistview;
}

static GtkWidget*
create_rightbar(App *app)
{
    GwyDataList *channels = gwy_file_get_data_list(app->file, GWY_DATA_CHANNEL);
    g_printerr("!!! %p %s\n", channels, G_OBJECT_TYPE_NAME(channels));
    GtkWidget *channelview = create_data_list_view(channels);

    GtkWidget *scwin = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scwin),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scwin), channelview);

    return scwin;
}

static GtkWidget*
create_data_area(G_GNUC_UNUSED App *app)
{
    GtkWidget *rasterview = gwy_raster_view_new();

    return rasterview;
}

static App*
create_app_window(void)
{
    App *app = g_slice_new0(App);

    app->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    GtkWindow *window = GTK_WINDOW(app->window);
    gtk_window_set_title(window, "Gwyddion");
    gtk_window_set_default_size(window, 600, 480);

    app->file = gwy_file_new();

    GtkWidget *maingrid = gtk_grid_new();
    gtk_container_add(GTK_CONTAINER(app->window), maingrid);

    app->dataarea = create_data_area(app);
    gtk_grid_attach(GTK_GRID(maingrid), app->dataarea, 1, 0, 1, 1);

    app->leftbar = create_leftbar(app);
    gtk_grid_attach_next_to(GTK_GRID(maingrid), app->leftbar,
                            NULL, GTK_POS_LEFT, 1, 1);

    app->rightbar = create_rightbar(app);
    gtk_grid_attach_next_to(GTK_GRID(maingrid), app->rightbar,
                            NULL, GTK_POS_RIGHT, 1, 1);

    return app;
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
    gwy_register_stock_items();

    GwyErrorList *errorlist = NULL;
    gwy_register_modules(&errorlist);
    for (GwyErrorList *e = errorlist; e; e = g_slist_next(e)) {
        GError *err = (GError*)e->data;
        g_printerr("(%u:%u) %s\n", err->domain, err->code, err->message);
    }
    gwy_error_list_clear(&errorlist);

    GType type = g_type_from_name("GwyExtTest");
    g_printerr("GwyExtTest: %u\n", (guint)type);
    g_type_class_ref(type);

    App *app = create_app_window();
    g_signal_connect(app->window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    gtk_widget_show_all(app->window);

    gtk_main();

    return 0;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
