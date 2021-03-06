/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

/* chdir */
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef _MSC_VER
#include <direct.h>
#endif

#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule-file.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwylayer-mask.h>
#include <gtk/gtkfilesel.h>
#include <gtk/gtkmessagedialog.h>
#include "app.h"
#include "menu.h"
#include "settings.h"
#include "file.h"

static void              file_open_ok_cb       (GtkFileSelection *selector);
static void              file_save_as_ok_cb    (GtkFileSelection *selector);
static gboolean          file_real_open        (const gchar *filename_sys,
                                                const gchar *name);
static GtkFileSelection* create_save_as_dialog (const gchar *title,
                                                GCallback ok_callback);
static GtkFileSelection* create_open_dialog    (const gchar *title,
                                                GCallback ok_callback);
static gboolean          confirm_overwrite     (GtkWindow *parent,
                                                const gchar *filename);
static void              recent_files_update   (const gchar *filename_utf8);
static GList*            recent_files_from_settings  (void);
static void              recent_files_to_settings    (void);
static void              remove_data_window_callback (GtkWidget *selector,
                                                      GwyDataWindow *data_window);

int gwy_app_n_recent_files = 10;

static GList *recent_files = NULL;

void
gwy_app_file_open_cb(void)
{
    GtkFileSelection *selector;

    selector = create_open_dialog(_("Open file"),
                                  G_CALLBACK(file_open_ok_cb));
    gtk_widget_show_all(GTK_WIDGET(selector));
}

void
gwy_app_file_close_cb(void)
{
    GwyDataWindow *data_window;

    data_window = gwy_app_data_window_get_current();
    g_return_if_fail(GWY_IS_DATA_WINDOW(data_window));
    gtk_widget_destroy(GTK_WIDGET(data_window));
}

void
gwy_app_file_save_as_cb(void)
{
    GtkFileSelection *selector;

    selector = create_save_as_dialog(_("Save File"),
                                     G_CALLBACK(file_save_as_ok_cb));
    if (!selector)
        return;
    gtk_widget_show_all(GTK_WIDGET(selector));
}

void
gwy_app_file_save_cb(void)
{
    GwyContainer *data;
    const gchar *filename_utf8;  /* in UTF-8 */
    const gchar *filename_sys;  /* in system (disk) encoding */

    data = gwy_app_get_current_data();
    g_return_if_fail(GWY_IS_CONTAINER(data));

    if (gwy_container_contains_by_name(data, "/filename"))
        filename_utf8 = gwy_container_get_string_by_name(data, "/filename");
    else {
        gwy_app_file_save_as_cb();
        return;
    }
    gwy_debug("%s", filename_utf8);
    filename_sys = g_filename_from_utf8(filename_utf8, -1, NULL, NULL, NULL);
    if (!filename_sys || !*filename_sys || !gwy_file_save(data, filename_sys))
        gwy_app_file_save_as_cb();
    else
        g_object_set_data(G_OBJECT(data), "gwy-app-modified", NULL);
}

void
gwy_app_file_duplicate_cb(void)
{
    GtkWidget *data_window;
    GwyContainer *data, *duplicate;

    data = gwy_app_get_current_data();
    g_return_if_fail(GWY_IS_CONTAINER(data));
    duplicate = GWY_CONTAINER(gwy_serializable_duplicate(G_OBJECT(data)));
    g_return_if_fail(GWY_IS_CONTAINER(duplicate));
    data_window = gwy_app_data_window_create(duplicate);
    g_object_set_data(G_OBJECT(duplicate), "gwy-app-modified",
                      GINT_TO_POINTER(1));
    gwy_app_data_window_set_untitled(GWY_DATA_WINDOW(data_window), NULL);
}

void
gwy_app_file_export_cb(const gchar *name)
{
    GtkFileSelection *selector;

    selector = create_save_as_dialog(_("Export Data"),
                                     G_CALLBACK(file_save_as_ok_cb));
    if (!selector)
        return;
    gtk_file_selection_set_filename(selector, "");
    g_object_set_data(G_OBJECT(selector), "file-type", (gpointer)name);
    gtk_widget_show_all(GTK_WIDGET(selector));
}

void
gwy_app_file_import_cb(const gchar *name)
{
    GtkFileSelection *selector;

    selector = create_open_dialog(_("Import Data"),
                                  G_CALLBACK(file_open_ok_cb));
    g_object_set_data(G_OBJECT(selector), "file-type", (gpointer)name);
    gtk_widget_show_all(GTK_WIDGET(selector));
}

static GtkFileSelection*
create_save_as_dialog(const gchar *title,
                      GCallback ok_callback)
{
    GtkFileSelection *selector;
    GwyDataWindow *data_window;
    GwyContainer *data;
    const gchar *filename_utf8;  /* in UTF-8 */
    const gchar *filename_sys;  /* in system (disk) encoding */

    data_window = gwy_app_data_window_get_current();
    g_return_val_if_fail(GWY_IS_DATA_WINDOW(data_window), NULL);
    data = gwy_app_get_current_data();
    g_return_val_if_fail(GWY_IS_CONTAINER(data), NULL);

    selector = GTK_FILE_SELECTION(gtk_file_selection_new(title));
    if (gwy_container_contains_by_name(data, "/filename"))
        filename_utf8 = gwy_container_get_string_by_name(data, "/filename");
    else
        filename_utf8 = "";
    filename_sys = g_filename_from_utf8(filename_utf8, -1, NULL, NULL, NULL);
    gtk_file_selection_set_filename(selector, filename_sys);
    g_object_set_data(G_OBJECT(selector), "data", data);
    g_object_set_data(G_OBJECT(selector), "window", data_window);

    g_signal_connect_swapped(selector->ok_button, "clicked",
                             ok_callback, selector);
    g_signal_connect_swapped(selector->cancel_button, "clicked",
                             G_CALLBACK(gtk_widget_destroy), selector);
    g_signal_connect(selector, "destroy",
                     G_CALLBACK(remove_data_window_callback), data_window);
    g_signal_connect_swapped(data_window, "destroy",
                             G_CALLBACK(gtk_widget_destroy), selector);

    return selector;
}

static GtkFileSelection*
create_open_dialog(const gchar *title,
                   GCallback ok_callback)
{
    GtkFileSelection *selector;

    selector = GTK_FILE_SELECTION(gtk_file_selection_new(title));
    gtk_file_selection_set_filename(selector, "");

    g_signal_connect_swapped(selector->ok_button, "clicked",
                             G_CALLBACK(ok_callback), selector);
    g_signal_connect_swapped(selector->cancel_button, "clicked",
                             G_CALLBACK(gtk_widget_destroy), selector);

    return selector;
}

static void
file_open_ok_cb(GtkFileSelection *selector)
{
    const gchar *filename_sys;  /* in system (disk) encoding */
    const gchar *name;

    filename_sys = gtk_file_selection_get_filename(selector);
    if (!g_file_test(filename_sys,
                     G_FILE_TEST_IS_REGULAR | G_FILE_TEST_IS_SYMLINK))
        return;

    name = (const gchar*)g_object_get_data(G_OBJECT(selector), "file-type");
    if (file_real_open(filename_sys, name))
        gtk_widget_destroy(GTK_WIDGET(selector));
}

void
gwy_app_file_open_recent_cb(GObject *item)
{
    const gchar *filename_utf8;  /* in UTF-8 */
    gchar *filename_sys;  /* in system (disk) encoding */

    filename_utf8 = g_object_get_data(G_OBJECT(item), "filename");
    g_return_if_fail(filename_utf8);
    filename_sys = g_filename_from_utf8(filename_utf8, -1, NULL, NULL, NULL);
    file_real_open(filename_sys, NULL);
    g_free(filename_sys);
}

static gboolean
file_real_open(const gchar *filename_sys,
               const gchar *name)
{
    const gchar *filename_utf8;  /* in UTF-8 */
    GwyContainer *data;
    gchar *dirname;

    if (name)
        data = gwy_file_func_run_load(name, filename_sys);
    else
        data = gwy_file_load(filename_sys);

    filename_utf8 = g_filename_to_utf8(filename_sys, -1, NULL, NULL, NULL);
    if (data) {
        gwy_container_set_string_by_name(data, "/filename", filename_utf8);
        gwy_app_data_window_create(data);
        recent_files_update(filename_utf8);

        /* change directory to that of the loaded file */
        dirname = g_path_get_dirname(filename_sys);   /* FIXME: utf-8? */
        if (strcmp(dirname, "."))
            chdir(dirname);
        g_free(dirname);
    }
    else {
        /* TODO: show some warning */
        g_warning("Open of <%s> failed", filename_utf8);
    }

    return data != NULL;
}

static void
file_save_as_ok_cb(GtkFileSelection *selector)
{
    GtkWindow *data_window;
    const gchar *filename_utf8;  /* in UTF-8 */
    const gchar *filename_sys;  /* in system (disk) encoding */
    GwyContainer *data;
    const gchar *name;
    gboolean ok;
    gchar *dirname;

    data = GWY_CONTAINER(g_object_get_data(G_OBJECT(selector), "data"));
    g_return_if_fail(GWY_IS_CONTAINER(data));
    data_window = GTK_WINDOW(g_object_get_data(G_OBJECT(selector), "window"));
    g_return_if_fail(GWY_IS_DATA_WINDOW(data_window));

    name = (const gchar*)g_object_get_data(G_OBJECT(selector), "file-type");

    filename_sys = gtk_file_selection_get_filename(selector);
    filename_utf8 = g_filename_to_utf8(filename_sys, -1, NULL, NULL, NULL);
    if (g_file_test(filename_sys,
                     G_FILE_TEST_IS_REGULAR | G_FILE_TEST_IS_SYMLINK)) {
        if (!confirm_overwrite(GTK_WINDOW(selector), filename_utf8))
            return;
    }
    else if (g_file_test(filename_sys, G_FILE_TEST_EXISTS)) {
        g_warning("Not a regular file `%s'", filename_utf8);
        return;
    }

    if (name)
        ok = gwy_file_func_run_save(name, data, filename_sys);
    else
        ok = gwy_file_save(data, filename_sys);
    if (!ok)
        return;

    if (!name
        || (gwy_file_func_get_operations(name) & GWY_FILE_LOAD))
        g_object_set_data(G_OBJECT(data), "gwy-app-modified", NULL);

    if (!name) {
        gwy_container_set_string_by_name(data, "/filename", filename_utf8);
        gwy_container_remove_by_name(data, "/filename/untitled");
        recent_files_update(filename_utf8);
    }
    gtk_widget_destroy(GTK_WIDGET(selector));
    gwy_data_window_update_title(GWY_DATA_WINDOW(data_window));

    /* change directory to that of the saved file */
    dirname = g_path_get_dirname(filename_sys);   /* FIXME: utf-8? */
    if (strcmp(dirname, "."))
        chdir(dirname);
    g_free(dirname);
}

static gboolean
confirm_overwrite(GtkWindow *parent,
                  const gchar *filename)
{
    GtkWidget *dialog;
    gint response;

    dialog = gtk_message_dialog_new(parent,
                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_QUESTION,
                                    GTK_BUTTONS_YES_NO,
                                    _("File %s already exists. Overwrite?"),
                                    filename);
    gtk_window_set_title(GTK_WINDOW(dialog), _("Overwrite?"));
    response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    return response == GTK_RESPONSE_YES;
}

static void
remove_data_window_callback(GtkWidget *selector,
                            GwyDataWindow *data_window)
{
    g_signal_handlers_disconnect_matched(selector,
                                         G_SIGNAL_MATCH_FUNC
                                         | G_SIGNAL_MATCH_DATA,
                                         0, 0, NULL,
                                         remove_data_window_callback,
                                         data_window);
    g_signal_handlers_disconnect_matched(data_window,
                                         G_SIGNAL_MATCH_FUNC
                                         | G_SIGNAL_MATCH_DATA,
                                         0, 0, NULL,
                                         gtk_widget_destroy,
                                         selector);

}

static void
recent_files_update(const gchar *filename_utf8)
{
    GList *item;

    gwy_debug("%s", filename_utf8);
    if (!recent_files)
        recent_files = recent_files_from_settings();

    item = g_list_find_custom(recent_files, filename_utf8,
                              (GCompareFunc)strcmp);
    if (item) {
        if (item == recent_files)
            return;
        recent_files = g_list_remove_link(recent_files, item);
        recent_files = g_list_concat(item, recent_files);
    }
    else
        recent_files = g_list_prepend(recent_files, g_strdup(filename_utf8));

    recent_files_to_settings();
    gwy_app_menu_recent_files_update(recent_files);
}

static GList*
recent_files_from_settings(void)
{
    const gchar *prefix = "/app/recent";
    GwyContainer *settings;
    gchar buffer[16];
    GList *list = NULL;
    const gchar *s;
    gint i;
    gsize len;

    gwy_debug("");
    settings = gwy_app_settings_get();
    g_return_val_if_fail(GWY_IS_CONTAINER(settings), NULL);
    len = strlen(prefix);
    strcpy(buffer, prefix);
    g_snprintf(buffer + len, sizeof(buffer) - len, "/%d", 0);
    for (i = 1; gwy_container_contains_by_name(settings, buffer); i++) {
        s = gwy_container_get_string_by_name(settings, buffer);
        gwy_debug("<%s> is %d", s, i);
        list = g_list_prepend(list, g_strdup(s));
        g_snprintf(buffer + len, sizeof(buffer) - len, "/%d", i);
    }
    list = g_list_reverse(list);

    return list;
}

static void
recent_files_to_settings(void)
{
    const gchar *prefix = "/app/recent";
    GwyContainer *settings;
    gchar buffer[16];
    GList *l;
    gint i;
    gsize len;

    gwy_debug("");
    settings = gwy_app_settings_get();
    g_return_if_fail(GWY_IS_CONTAINER(settings));
    gwy_container_remove_by_prefix(settings, prefix);
    len = strlen(prefix);
    strcpy(buffer, prefix);
    for (l = recent_files, i = 0;
         l && i < gwy_app_n_recent_files;
         l = g_list_next(l), i++) {
        gwy_debug("storing %s", (gchar*)l->data);
        g_snprintf(buffer + len, sizeof(buffer) - len, "/%d", i);
        gwy_container_set_string_by_name(settings, buffer,
                                         g_strdup((gchar*)l->data));
    }
}

/**
 * gwy_app_file_open_initial:
 * @args: A file list.
 * @n: The number of items in @args.
 *
 * Opens a list of files [given on command line] and created recent-files
 * menu.
 *
 * Note the file names are assumed to be in system encoding, not UTF-8, but
 * who knows, what we get on the command line...
 **/
void
gwy_app_file_open_initial(gchar **args, gint n)
{
    gchar **p;
    gchar *cwd, *filename;

    /* FIXME: cwd is probably in UTF-8?! */
    cwd = g_get_current_dir();
    for (p = args; n; p++, n--) {
        if (g_path_is_absolute(*p))
            filename = g_strdup(*p);
        else
            filename = g_build_filename(cwd, *p, NULL);
        file_real_open(filename, NULL);
        g_free(filename);
    }
    g_free(cwd);

    if (!recent_files)
        recent_files = recent_files_from_settings();

    gwy_app_menu_recent_files_update(recent_files);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
