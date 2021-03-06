/*
 *  @(#) $Id$
 *  Copyright (C) 2007 David Necas (Yeti), Petr Klapetek.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libprocess/datafield.h>
#include <app/file.h>
#include <app/gwymoduleutils.h>

typedef struct {
    const gchar *data;
    gssize len;
} GwySaveAuxiliaryData;

static gchar*
gwy_save_auxiliary_data_create(gpointer user_data,
                               gssize *data_len)
{
    GwySaveAuxiliaryData *savedata = (GwySaveAuxiliaryData*)user_data;

    *data_len = savedata->len;

    return (gchar*)savedata->data;
}

/**
 * gwy_save_auxiliary_data:
 * @title: File chooser dialog title.
 * @parent: Parent window for the file chooser dialog (may be %NULL).
 * @data_len: The length of @data in bytes.  Pass -1 if @data is text, it must
 *            be nul-terminated then and it will be saved in text mode (this
 *            matters if the operating system distinguishes between text and
 *            binary).  A non-negative value causes the data to be saved as
 *            binary.
 * @data: The data to save.
 *
 * Saves a report or other auxiliary data to a user specified file.
 *
 * This is actually a simple gwy_save_auxiliary_with_callback() wrapper, see
 * its description for details.
 *
 * Returns: %TRUE if the data was save, %FALSE if it was not saved for any
 *          reason.
 *
 * Since: 2.3
 **/
gboolean
gwy_save_auxiliary_data(const gchar *title,
                        GtkWindow *parent,
                        gssize data_len,
                        const gchar *data)
{
    GwySaveAuxiliaryData savedata;

    g_return_val_if_fail(data, FALSE);
    savedata.data = data;
    savedata.len = data_len;

    return gwy_save_auxiliary_with_callback(title, parent,
                                            &gwy_save_auxiliary_data_create,
                                            NULL,
                                            &savedata);
}

/**
 * gwy_save_auxiliary_with_callback:
 * @title: File chooser dialog title.
 * @parent: Parent window for the file chooser dialog (may be %NULL).
 * @create: Function to create the data (it will not be called if the user
 *          cancels the saving).
 * @destroy: Function to destroy the data (if will be called iff @create will
 *           be called), it may be %NULL.
 * @user_data: User data passed to @create and @destroy.
 *
 * Saves a report or other auxiliary data to a user specified file.
 *
 * Returns: %TRUE if the data was save, %FALSE if it was not saved for any
 *          reason (I/O error, cancellation, overwrite cancellation, etc.).
 *
 * Since: 2.3
 **/
gboolean
gwy_save_auxiliary_with_callback(const gchar *title,
                                 GtkWindow *parent,
                                 GwySaveAuxiliaryCreate create,
                                 GwySaveAuxiliaryDestroy destroy,
                                 gpointer user_data)
{
    gchar *data;
    gchar *filename_sys, *filename_utf8;
    gssize data_len;
    gint myerrno;
    GtkWidget *dialog, *chooser;
    gint response;
    FILE *fh;

    g_return_val_if_fail(!parent || GTK_IS_WINDOW(parent), FALSE);
    g_return_val_if_fail(create, FALSE);

    chooser = gtk_file_chooser_dialog_new(title, parent,
                                          GTK_FILE_CHOOSER_ACTION_SAVE,
                                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                          GTK_STOCK_SAVE, GTK_RESPONSE_OK,
                                          NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(chooser), GTK_RESPONSE_OK);
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(chooser),
                                        gwy_app_get_current_directory());
    response = gtk_dialog_run(GTK_DIALOG(chooser));
    filename_sys = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));

    if (!gwy_app_file_confirm_overwrite(chooser))
        response = GTK_RESPONSE_CANCEL;

    gtk_widget_destroy(chooser);

    if (!filename_sys || response != GTK_RESPONSE_OK) {
        g_free(filename_sys);
        return FALSE;
    }

    data_len = -1;
    data = create(user_data, &data_len);
    g_return_val_if_fail(data, FALSE);

    if ((fh = g_fopen(filename_sys, "wb"))) {
        gchar *mydata = NULL;

        /* Write everything in binary and just convert the EOLs by manually.
         * This seems to actually work as we want. */
        if (data_len <= 0) {
#ifdef G_OS_WIN32
            mydata = gwy_strreplace(data, "\n", "\r\n", (gsize)-1);
            data_len = strlen(mydata);
#else
            data_len = strlen(data);
#endif
        }
        if (fwrite(mydata ? mydata : data, data_len, 1, fh) != 1) {
            myerrno = errno;
            /* This is just best-effort clean-up */
            fclose(fh);
            g_unlink(filename_sys);
            fh = NULL;
        }
        else
            myerrno = 0;  /* GCC */

        g_free(mydata);
    }
    else
        myerrno = errno;

    if (destroy)
        destroy(data, user_data);

    if (fh) {
        /* Everything went all right. */
        fclose(fh);
        gwy_app_set_current_directory(filename_sys);
        g_free(filename_sys);
        return TRUE;
    }

    filename_utf8 = g_filename_to_utf8(filename_sys, -1, NULL, NULL, NULL);
    dialog = gtk_message_dialog_new(parent, 0, GTK_MESSAGE_ERROR,
                                    GTK_BUTTONS_OK,
                                    _("Saving of `%s' failed"),
                                    filename_utf8);
    g_free(filename_sys);
    g_free(filename_utf8);
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
                                             _("Cannot write to file: %s."),
                                             g_strerror(myerrno));
    gtk_widget_show_all(dialog);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    return FALSE;
}

/**
 * gwy_set_data_preview_size:
 * @data_view: A data view used for module preview.
 * @max_size: Maximum allowed @data_view size (width and height).
 *
 * Sets up data view zoom to not exceed specified size.
 *
 * Before calling this function, data keys have be set, data fields and layers
 * have to be present and physically square mode set in the container.
 * Sizing of both pixel-wise square and physically square displays is performed
 * correctly.
 *
 * Since: 2.7
 **/
void
gwy_set_data_preview_size(GwyDataView *data_view,
                          gint max_size)
{
    GwyContainer *container;
    GwyDataField *data_field;
    GwyPixmapLayer *layer;
    gdouble zoomval, scale, xreal, yreal;
    gboolean realsquare;
    gint xres, yres;
    const gchar *prefix;
    gchar *key;

    g_return_if_fail(GWY_IS_DATA_VIEW(data_view));
    g_return_if_fail(max_size >= 2);

    container = gwy_data_view_get_data(data_view);
    g_return_if_fail(GWY_IS_CONTAINER(container));

    layer = gwy_data_view_get_base_layer(data_view);
    g_return_if_fail(GWY_IS_PIXMAP_LAYER(layer));
    prefix = gwy_pixmap_layer_get_data_key(layer);
    g_return_if_fail(prefix);

    data_field = gwy_container_get_object_by_name(container, prefix);
    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));

    prefix = gwy_data_view_get_data_prefix(data_view);
    g_return_if_fail(prefix);
    key = g_strconcat(prefix, "/realsquare", NULL);
    realsquare = FALSE;
    gwy_container_gis_boolean_by_name(container, key, &realsquare);
    g_free(key);

    xres = gwy_data_field_get_xres(data_field);
    yres = gwy_data_field_get_yres(data_field);
    if (!realsquare)
        zoomval = max_size/(gdouble)MAX(xres, yres);
    else {
        xreal = gwy_data_field_get_xreal(data_field);
        yreal = gwy_data_field_get_yreal(data_field);
        scale = MAX(xres/xreal, yres/yreal);
        zoomval = max_size/(scale*MAX(xreal, yreal));
    }
    gwy_data_view_set_zoom(data_view, zoomval);
}

/************************** Documentation ****************************/

/**
 * SECTION:gwymoduleutils
 * @title: module utils
 * @short_description: Module utility functions
 * @include: app/gwymoduleutils.h
 **/

/**
 * GwySaveAuxiliaryCreate:
 * @user_data: The data passed to gwy_save_auxiliary_with_callback() as
 *             @user_data.
 * @data_len: The length of the returned data in bytes.  Leaving it unset has
 *            the same effect as setting it to a negative value.  See
 *            gwy_save_auxiliary_data() for details.
 *
 * The type of auxiliary saved data creation function.
 *
 * Returns: The data to save.  It must not return %NULL.
 *
 * Since: 2.3
 **/

/**
 * GwySaveAuxiliaryDestroy:
 * @data: The data returned by the corresponding #GwySaveAuxiliaryCreate
 *        function.
 * @user_data: The data passed to gwy_save_auxiliary_with_callback() as
 *             @user_data.
 *
 * The type of auxiliary saved data destruction function.
 *
 * Since: 2.3
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
