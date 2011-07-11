/*
 *  gwybatch.c
 *  $Id$
 *  Example of completely non-interactive SPM data processing with Gwyddion.
 *  Copyright (C) 2009 David Necas (Yeti).
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
#include <gtk/gtk.h>
#include <app/gwyapp.h>
#include <libgwyddion/gwyddion.h>
#include <libprocess/gwyprocess.h>
#include <libdraw/gwydraw.h>
#include <libgwydgets/gwydgets.h>
#include <libgwymodule/gwymodule.h>

/*
 * Load the necessary file modules.
 *
 * We need to register all "line" modules too as they define new objects that
 * can be present in gwy files.  For non-gwy files it does not matter but there
 * is only a handful of "line" modules so just load them.
 *
 * It would be nice to use gwy_app_init_common(), unfortunately, the GUI and
 * non-GUI stuff is not well-separated in Gwyddion 2.x, so this function does
 * too much.  Emulate the module loading part instead.
 *
 * See load_file_module() for a simple alternative.
 */
static void
load_modules(void)
{
    const gchar *const module_types[] = { "file", "layer", NULL };
    GPtrArray *module_dirs;
    const gchar *upath;
    gchar *mpath;
    guint i;

    module_dirs = g_ptr_array_new();

    /* System modules */
    mpath = gwy_find_self_dir("modules");
    for (i = 0; module_types[i]; i++) {
        g_ptr_array_add(module_dirs,
                        g_build_filename(mpath, module_types[i], NULL));
    }
    g_free(mpath);

    /* User modules */
    upath = gwy_get_user_dir();
    for (i = 0; module_types[i]; i++) {
        g_ptr_array_add(module_dirs,
                        g_build_filename(upath, module_types[i], NULL));
    }

    /* Register all found there, in given order. */
    g_ptr_array_add(module_dirs, NULL);
    gwy_module_register_modules((const gchar**)module_dirs->pdata);

    for (i = 0; module_dirs->pdata[i]; i++)
        g_free(module_dirs->pdata[i]);
    g_ptr_array_free(module_dirs, TRUE);
}

/*
 * Load a single file module, given as the argument.
 *
 * See load_modules() for a complex alternative.
 */
static void
load_file_module(const gchar *name)
{
    gchar *mpath, *modfile, *modfilefull;
    GError *err = NULL;

    modfile = g_strconcat(name, ".", G_MODULE_SUFFIX, NULL);
    mpath = gwy_find_self_dir("modules");
    modfilefull = g_build_filename(mpath, "file", modfile, NULL);

    if (!gwy_module_register_module(modfilefull, &err)) {
        g_printerr("Failed to register %s: %s\n", modfilefull, err->message);
        g_clear_error(&err);
        exit(EXIT_FAILURE);
    }

    g_free(modfilefull);
    g_free(mpath);
    g_free(modfile);
}

/*
 * If the file is in a complex multichannel format it is necessary to find
 * something to operate on.  This function finds and returns the channel with
 * the lowest id, which might or might not be the right thing.
 *
 * See get_channel0() for a simple alternative.
 */
static GwyDataField*
get_first_channel(GwyContainer *data, gint *pid)
{
    gint *channel_ids;
    gint nchannels, first;

    /* Register data to the data browser to be able to use
     * gwy_app_data_browser_get_data_ids() */
    gwy_app_data_browser_add(data);

    /* But do not let it manage our file */
    gwy_app_data_browser_set_keep_invisible(data, TRUE);

    /* Obtain the list of channel numbers and check whether there are any.
     * If we deal with a complex format, the ids can be arbitrary numbers,
     * we cannot just count from 0.  */
    channel_ids = gwy_app_data_browser_get_data_ids(data);
    first = -1;
    for (nchannels = 0; channel_ids[nchannels] != -1; nchannels++) {
        g_print("Found channel #%d\n", channel_ids[nchannels]);
        if (first == -1 || channel_ids[nchannels] < first)
            first = channel_ids[nchannels];
    }

    gwy_app_data_browser_remove(data);

    *pid = first;
    if (first == -1)
        return NULL;

    return gwy_container_get_object(data, gwy_app_get_data_key_for_id(first));
}

/*
 * If the file is in a simple format that can contain only one image, this
 * is sufficient to obtain it.
 *
 * See get_first_channel() for a complex alternative.
 */
static GwyDataField*
get_channel0(GwyContainer *data)
{
    return gwy_container_get_object(data, gwy_app_get_data_key_for_id(0));
}

/*
 * A function that does something with the data field.  Not necessarily a
 * good grain marking method...
 */
static GwyDataField*
detect_grains(GwyDataField *dfield)
{
    GwyDataField *mfield;
    gdouble avg, rms;
    gint *grain_numbers;
    gint xres, yres, ngrains;

    /* A simplistic grain marking method. */
    gwy_data_field_get_stats(dfield, &avg, NULL, &rms, NULL, NULL);
    mfield = gwy_data_field_duplicate(dfield);
    gwy_data_field_threshold(mfield, avg + 2.5*rms, 0, 1);
    gwy_data_field_grains_remove_by_size(mfield, 5);

    /* Check the area covered by grains */
    avg = gwy_data_field_get_avg(mfield);
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    grain_numbers = g_new0(gint, xres*yres);
    ngrains = gwy_data_field_number_grains(mfield, grain_numbers);

    g_print("Found %d grains covering %g%% of the image.\n", ngrains, 100*avg);

    g_free(grain_numbers);

    /* Return the mask only if the grains cover certain area */
    if (avg < 0.01)
        gwy_object_unref(mfield);

    return mfield;
}

int
main(int argc,
     char *argv[])
{
    gint i;

    /* Initialize Gtk+ and lower level libraries, without invoking anything
     * that tries to communicate with the X server. */
    gtk_parse_args(&argc, &argv);

    if (argc == 1) {
        g_print("Usage: gwybatch FILES...\n");
        g_print("Batch data processing using Gwyddion example.\n");
        g_print("Version: $Id$.\n");
        return 0;
    }

    /* Initialize Gwyddion, here it could get a bit more hairy if we wanted to
     * render data to images and such.  But we don't.  */
    gwy_widgets_type_init();

    load_modules();
    /* Or just: load_file_module("nanoscope"); */

    /* Process files given on the command line. */
    for (i = 1; i < argc; i++) {
        const gchar *filename = argv[i];
        GError *err = NULL;
        GwyDataField *dfield, *mfield;
        GwyContainer *data;
        int id;

        data = gwy_file_load(filename, GWY_RUN_NONINTERACTIVE, &err);
        if (!data) {
            g_printerr("Failed to load %s: %s\n", filename, err->message);
            g_clear_error(&err);
            continue;
        }

        g_print("Processing %s\n", filename);

        dfield = get_first_channel(data, &id);
        /* Or just:
        dfield = get_channel0(data);
        id = 0;
         */

        if (!dfield) {
            g_printerr("Did not find any data field in %s\n", filename);
            g_object_unref(data);
            continue;
        }

        /* Find grains. */
        mfield = detect_grains(dfield);

        /* If there are some, save the marked data field into a new file. */
        if (mfield) {
            GwyContainer *newdata;
            gchar *newfilename;
            GError *err = NULL;

            /* Create a new data container */
            newdata = gwy_container_new();

            /* Put the data and mask there. */
            gwy_container_set_object(newdata, gwy_app_get_data_key_for_id(0),
                                     dfield);
            gwy_container_set_object(newdata, gwy_app_get_mask_key_for_id(0),
                                     mfield);
            /* Be nice and also copy title, metadata and other properties if
             * the original file has them. */
            gwy_app_sync_data_items(data, newdata, id, 0, FALSE,
                                    GWY_DATA_ITEM_GRADIENT,
                                    GWY_DATA_ITEM_MASK_COLOR,
                                    GWY_DATA_ITEM_TITLE,
                                    GWY_DATA_ITEM_RANGE,
                                    GWY_DATA_ITEM_RANGE_TYPE,
                                    GWY_DATA_ITEM_REAL_SQUARE,
                                    GWY_DATA_ITEM_SELECTIONS,
                                    GWY_DATA_ITEM_META,
                                    0);

            /* Save the new data */
            newfilename = g_strconcat(filename, "-marked.gwy", NULL);
            g_print("Saving marked data to %s\n", newfilename);
            if (!gwy_file_save(newdata, newfilename, GWY_RUN_NONINTERACTIVE,
                               &err)) {
                g_printerr("Failed to save %s: %s\n",
                           newfilename, err->message);
                g_clear_error(&err);
            }
            g_free(newfilename);

            /* Free temporary objects */
            g_object_unref(newdata);
            g_object_unref(mfield);
        }
        else {
            g_print("Ignoring %s as it does not contain grains.\n", filename);
        }

        g_object_unref(data);
    }

    return 0;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
