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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>

#include <libgwymodule/gwymodule.h>
#include <libgwyddion/gwyddion.h>
#include <libgwydgets/gwydgets.h>
#include <app/settings.h>

static void print_help(void);
static void process_preinit_options(int *argc,
                                    char ***argv);

int
main(int argc, char *argv[])
{
    GtkWidget *window, *view;
    GwyContainer *data, *settings;
    GwyPixmapLayer *layer;
    gchar *filename;

    /* Check for --help and --version before rash file loading and GUI
     * initializatioon */
    process_preinit_options(&argc, &argv);
    if (argc < 2) {
        print_help();
        return 0;
    }

    /* Initialize Gwyddion stuff */
    gwy_widgets_type_init();
    g_set_application_name(PACKAGE);

    /* Load gwyddion settings.  Maybe not very useful here, except for
     * files mysteriously missing mask color. */
    filename = gwy_app_settings_get_settings_filename();
    if (g_file_test(filename, G_FILE_TEST_IS_REGULAR))
        gwy_app_settings_load(filename);
    g_free(filename);
    settings = gwy_app_settings_get();

    /* Load .gwy file loader */
    gwy_module_register_module(GWY_MODULE_DIR "/file/gwyfile.so");

    /* Load the file */
    data = gwy_file_load(argv[1]);
    if (!data) {
        g_printerr("Cannot load `%s'\n", argv[1]);
        return 1;
    }

    /* Construct the GUI */
    gtk_init(&argc, &argv);

    /* Main window */
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    filename = g_path_get_basename(argv[1]);
    gtk_window_set_title(GTK_WINDOW(window), filename);
    gtk_window_set_resizable(GTK_WINDOW(window), TRUE);
    g_object_set(window, "allow-shrink", TRUE, NULL);
    g_signal_connect(window, "delete_event", gtk_main_quit, NULL);
    g_free(filename);

    /* Data view */
    view = gwy_data_view_new(data);
    gtk_container_add(GTK_CONTAINER(window), view);
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(view),
                                 GWY_PIXMAP_LAYER(gwy_layer_basic_new()));

    /* Mask */
    if (gwy_container_contains_by_name(data, "/0/mask")) {
        GwyRGBA color = { 1.0, 0.3, 0.0, 0.6 };

        layer = GWY_PIXMAP_LAYER(gwy_layer_mask_new());
        gwy_data_view_set_alpha_layer(GWY_DATA_VIEW(view), layer);
        /* Get mask color from Gwyddion settings, if not specified in the
         * file. */
        if (!gwy_rgba_get_from_container(&color, data, "/0/mask")) {
            gwy_rgba_get_from_container(&color, settings, "/mask");
            gwy_rgba_store_to_container(&color, data, "/0/mask");
        }
    }

    gtk_widget_show_all(window);
    gtk_main();

    return 0;
}

/* Check for --help and --version and eventually print help or version */
static void
process_preinit_options(int *argc,
                        char ***argv)
{
    if (*argc == 1)
        return;

    if (!strcmp((*argv)[1], "--help") || !strcmp((*argv)[1], "-h")) {
        print_help();
        exit(0);
    }

    if (!strcmp((*argv)[1], "--version") || !strcmp((*argv)[1], "-v")) {
        g_print("%s %s\n", PACKAGE, VERSION);
        exit(0);
    }
}

/* Print help */
static void
print_help(void)
{
    g_print(
"Usage: gwyiew FILENAME\n"
"A very simple Gwyddion .gwy file viewer.\n\n"
        );
    g_print(
"Options:\n"
" -h, --help                 Print this help and terminate.\n"
" -v, --version              Print version info and terminate.\n\n"
        );
    g_print("Please report bugs in Gwyddion bugzilla "
            "http://trific.ath.cx/bugzilla/\n");
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

