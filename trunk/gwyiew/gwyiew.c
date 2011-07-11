/*
 *  @(#) $Id$
 *  Copyright (C) 2004,2005 David Necas (Yeti).
 *  E-mail: yeti@gwyddion.net.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <libgwymodule/gwymodule.h>
#include <libgwyddion/gwyddion.h>
#include <libgwydgets/gwydgets.h>
#include <app/gwyapp.h>

static void     create_view_for_channel(gint id);
static gboolean key_pressed            (GtkWidget *widget,
                                        GdkEventKey *event);
static void     print_help             (void);

/* Name of the file to display */
const gchar *filename;

/* The data file contents */
GwyContainer *data;

/* List of all channel numbers in the file. */
static gint *channel_ids;
static gint n_channels;

/* What we currently display (index in channel_ids[]) */
static gint current_i;

int
main(int argc, char *argv[])
{
    GError *err = NULL;

    /* Check for --help and --version fist */
    if (argc < 2)
        print_help();
    if (gwy_strequal(argv[1], "--help") || gwy_strequal(argv[1], "-h"))
        print_help();
    if (gwy_strequal(argv[1], "--version") || gwy_strequal(argv[1], "-v")) {
        g_print("%s %s\n", PACKAGE, VERSION);
        return 0;
    }
    filename = argv[1];

    /* Initialize Gtk+ */
    gtk_init(&argc, &argv);
    g_set_application_name(PACKAGE);

    /* Initialize Gwyddion stuff */
    gwy_app_init_common(NULL, "layer", "file", NULL);

    /* Load the file */
    data = gwy_file_load(filename, GWY_RUN_NONINTERACTIVE, &err);
    if (!data) {
        g_printerr("Cannot load `%s': %s\n", argv[1], err->message);
        return 1;
    }

    /* Register data to the data browser to be able to use
     * gwy_app_data_browser_get_data_ids() */
    gwy_app_data_browser_add(data);
    /* But do not let it manage our file */
    gwy_app_data_browser_set_keep_invisible(data, TRUE);

    /* Obtain the list of channel numbers and check whether there are any */
    channel_ids = gwy_app_data_browser_get_data_ids(data);
    for (n_channels = 0; channel_ids[n_channels] != -1; n_channels++)
        ;
    if (!n_channels) {
        g_printerr("File `%s' contains no channel to display\n", argv[1]);
        return 1;
    }

    /* Construct the window for the first channel */
    current_i = 0;
    create_view_for_channel(channel_ids[current_i]);
    gtk_main();

    return 0;
}

static void
create_view_for_channel(gint id)
{
    GtkWidget *window, *view;
    GwyContainer *settings;
    GwyPixmapLayer *layer;
    GQuark quark;
    gchar *key, *channel_title, *basename, *title;

    /* Construct window title */
    channel_title = gwy_app_get_data_field_title(data, id);
    basename = g_path_get_basename(filename);
    title = g_strdup_printf("%s [%s]", basename, channel_title);
    g_free(channel_title);
    g_free(basename);

    /* Window */
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), title);
    g_free(title);
    gtk_window_set_resizable(GTK_WINDOW(window), TRUE);
    g_object_set(window, "allow-shrink", TRUE, NULL);
    g_signal_connect(window, "destroy", gtk_main_quit, NULL);
    g_signal_connect(window, "key-press-event", G_CALLBACK(key_pressed), NULL);

    /* Data view */
    view = gwy_data_view_new(data);
    gtk_container_add(GTK_CONTAINER(window), view);
    /* The basic data display layer, construct it if called the first time */
    layer = gwy_layer_basic_new();

    /* Set up the locations to display */
    quark = gwy_app_get_data_key_for_id(id);
    gwy_data_view_set_data_prefix(GWY_DATA_VIEW(view),
                                  g_quark_to_string(quark));
    gwy_pixmap_layer_set_data_key(layer, g_quark_to_string(quark));
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(view), layer);

    /* There is no helper function for palette keys, set it up manually */
    key = g_strdup_printf("/%d/base/palette", id);
    gwy_layer_basic_set_gradient_key(GWY_LAYER_BASIC(layer), key);
    g_free(key);

    /* Mask layer, only set it up if there is a mask present */
    quark = gwy_app_get_mask_key_for_id(id);
    if (gwy_container_contains(data, quark)) {
        GwyRGBA color = { 1.0, 0.3, 0.0, 0.6 };   /* Fallback colour */
        const gchar *prefix;

        settings = gwy_app_settings_get();

        layer = gwy_layer_mask_new();
        prefix = g_quark_to_string(quark);
        gwy_pixmap_layer_set_data_key(layer, prefix);
        gwy_data_view_set_alpha_layer(GWY_DATA_VIEW(view), layer);
        /* Get mask color from Gwyddion settings, if unspecified in the file. */
        if (!gwy_rgba_get_from_container(&color, data, prefix)) {
            gwy_rgba_get_from_container(&color, settings, "/mask");
            gwy_rgba_store_to_container(&color, data, prefix);
        }
    }

    gtk_widget_show_all(window);
}

/* Display previous/next channel on PageUp/PageDown */
static gboolean
key_pressed(GtkWidget *widget,
            GdkEventKey *event)
{
    gint move;

    /* Don't do anything if there is only a signle channel. */
    if (n_channels == 1)
        return FALSE;

    if (event->keyval == GDK_Next)
        move = 1;
    else if (event->keyval == GDK_Prior)
        move = -1;
    else
        return FALSE;

    /* Prevent program termination and destroy the old window */
    g_signal_handlers_disconnect_by_func(widget, gtk_main_quit, NULL);
    gtk_widget_destroy(widget);

    /* Change channel and display it in a new window */
    current_i = (current_i + move + n_channels) % n_channels;
    create_view_for_channel(channel_ids[current_i]);

    return TRUE;
}

/* Print help */
static void
print_help(void)
{
    g_print(
"Usage: gwyiew FILENAME\n"
"A very simple SPM file viewer, serving mainly as a sample application using\n"
"Gwyddion libraries.  Use PageUp/PageDown to switch channels..\n\n"
        );
    g_print(
"Options:\n"
" -h, --help                 Print this help and terminate.\n"
" -v, --version              Print version info and terminate.\n\n"
        );
    g_print("Report bugs to <yeti@gwyddion.net>\n");

    exit(0);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
