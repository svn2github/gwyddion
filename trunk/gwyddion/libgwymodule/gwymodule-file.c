/*
 *  @(#) $Id$
 *  Copyright (C) 2003,2004 David Necas (Yeti), Petr Klapetek.
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

#include "config.h"
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <glib/gstdio.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwycontainer.h>
#include <libprocess/datafield.h>
#include <libgwymodule/gwymodule-file.h>
#include "gwymoduleinternal.h"

/* The file function information. */
typedef struct {
    const gchar *name;
    const gchar *description;
    GwyFileDetectFunc detect;
    GwyFileLoadFunc load;
    GwyFileSaveFunc save;
    GwyFileSaveFunc export_;
} GwyFileFuncInfo;

/* Information about current file, passed around during detection */
typedef struct {
    const gchar *winner;
    gint score;
    gboolean only_name;
    GwyFileOperationType mode;
    GwyFileDetectInfo *fileinfo;
} FileDetectData;

/* Information about successful loads and saves of a Container, kept in
 * container_list */
typedef struct {
    gpointer container;
    GQuark name;
    gchar *filename_sys;
} FileTypeInfo;

/* Auxiliary structure to pass both user callback function and data to
 * g_hash_table_foreach() lambda argument in gwy_file_func_foreach() */
typedef struct {
    GFunc function;
    gpointer user_data;
} FuncForeachData;

static gboolean gwy_file_detect_fill_info  (GwyFileDetectInfo *fileinfo,
                                            gboolean only_name);
static void     gwy_file_detect_free_info  (GwyFileDetectInfo *fileinfo);
static void     file_detect_max_score_cb   (const gchar *key,
                                            GwyFileFuncInfo *func_info,
                                            FileDetectData *ddata);
static GwyFileOperationType get_operations (const GwyFileFuncInfo *func_info);
static void     gwy_file_type_info_set     (GwyContainer *data,
                                            const gchar *name,
                                            const gchar *filename_sys);
static FileTypeInfo* gwy_file_type_info_get(GwyContainer *data,
                                            gboolean do_create);
static void    gwy_file_container_finalized(gpointer userdata,
                                            GObject *deceased_data);

static GHashTable *file_funcs = NULL;
static GList *container_list = NULL;

/**
 * gwy_file_func_register:
 * @name: Name of function to register.  It should be a valid identifier and
 *        if a module registers only one function, module and function names
 *        should be the same.
 * @description: File type description (will be used in file type selectors).
 * @detect: Detection function.  It may be %NULL, files of such a type can
 *          can be then loaded and saved only on explict user request.
 * @load: File load/import function.
 * @save: File save function.
 * @export_: File export function.
 *
 * Registered a file function.
 *
 * At least one of @load, @save, and @export_ must be non-%NULL.  See
 * #GwyFileOperationType for differences between save and export.
 *
 * Note: the string arguments are not copied as modules are not expected to
 * vanish.  If they are constructed (non-constant) strings, do not free them.
 * Should modules ever become unloadable they will get chance to clean-up.
 *
 * Returns: Normally %TRUE; %FALSE on failure.
 **/
gboolean
gwy_file_func_register(const gchar *name,
                       const gchar *description,
                       GwyFileDetectFunc detect,
                       GwyFileLoadFunc load,
                       GwyFileSaveFunc save,
                       GwyFileSaveFunc export_)
{
    GwyFileFuncInfo *func_info;

    g_return_val_if_fail(name, FALSE);
    g_return_val_if_fail(load || save || export_, FALSE);
    g_return_val_if_fail(description, FALSE);
    gwy_debug("name = %s, desc = %s, detect = %p, "
              "load = %p, save = %p, export = %p",
              name, description, detect, load, save, export_);

    if (!file_funcs) {
        gwy_debug("Initializing...");
        file_funcs = g_hash_table_new_full(g_str_hash, g_str_equal,
                                           NULL, &g_free);
    }

    if (!gwy_strisident(name, "_-", NULL))
        g_warning("Function name `%s' is not a valid identifier. "
                  "It may be rejected in future.", name);
    if (g_hash_table_lookup(file_funcs, name)) {
        g_warning("Duplicate function %s, keeping only first", name);
        return FALSE;
    }

    func_info = g_new0(GwyFileFuncInfo, 1);
    func_info->name = name;
    func_info->description = description;
    func_info->detect = detect;
    func_info->load = load;
    func_info->save = save;
    func_info->export_ = export_;

    g_hash_table_insert(file_funcs, (gpointer)func_info->name, func_info);
    if (!_gwy_module_add_registered_function(GWY_MODULE_PREFIX_FILE, name)) {
        g_hash_table_remove(file_funcs, func_info->name);
        return FALSE;
    }

    return TRUE;
}

/**
 * gwy_file_func_run_detect:
 * @name: A file type function name.
 * @filename: A file name to detect.
 * @only_name: Whether to use only file name for a guess, or try to actually
 *             access the file.
 *
 * Runs a file type detection function identified by @name.
 *
 * Value of @only_name should be %TRUE if the file doesn't exist (is to be
 * written) so its contents can't be used for file type detection.
 *
 * This is a low-level function, consider using gwy_file_detect() if you
 * simply want to detect a file type.
 *
 * Returns: An integer score expressing the likehood of the file being
 *          loadable as this type. A basic scale is 20 for a good extension,
 *          100 for good magic header, more for more thorough tests.
 **/
gint
gwy_file_func_run_detect(const gchar *name,
                         const gchar *filename,
                         gboolean only_name)
{
    GwyFileFuncInfo *func_info;
    GwyFileDetectInfo fileinfo;
    gint score = 0;

    g_return_val_if_fail(filename, 0);
    func_info = g_hash_table_lookup(file_funcs, name);
    g_return_val_if_fail(func_info, 0);
    if (!func_info->detect)
        return 0;

    fileinfo.name = filename;
    /* File must exist if not only_name */
    if (gwy_file_detect_fill_info(&fileinfo, only_name)) {
        score = func_info->detect(&fileinfo, only_name, name);
        gwy_file_detect_free_info(&fileinfo);
    }

    return score;
}

/**
 * gwy_file_func_run_load:
 * @name: A file load function name.
 * @filename: A file name to load data from.
 * @mode: Run mode.
 * @error: Return location for a #GError (or %NULL).
 *
 * Runs a file load function identified by @name.
 *
 * This is a low-level function, consider using gwy_file_load() if you
 * simply want to load a file.
 *
 * Returns: A new #GwyContainer with data from @filename, or %NULL.
 **/
GwyContainer*
gwy_file_func_run_load(const gchar *name,
                       const gchar *filename,
                       GwyRunType mode,
                       GError **error)
{
    GwyFileFuncInfo *func_info;
    GwyContainer *data;

    g_return_val_if_fail(!error || !*error, NULL);
    g_return_val_if_fail(filename, NULL);
    func_info = g_hash_table_lookup(file_funcs, name);
    g_return_val_if_fail(func_info, NULL);
    g_return_val_if_fail(func_info->load, NULL);

    data = func_info->load(filename, mode, error, name);
    if (data)
        gwy_file_type_info_set(data, name, filename);

    return data;
}

/**
 * gwy_file_func_run_save:
 * @name: A file save function name.
 * @data: A #GwyContainer to save.
 * @filename: A file name to save @data as.
 * @mode: Run mode.
 * @error: Return location for a #GError (or %NULL).
 *
 * Runs a file save function identified by @name.
 *
 * It guarantees the container lifetime spans through the actual file saving,
 * so the module function doesn't have to care about it.
 *
 * This is a low-level function, consider using gwy_file_save() if you
 * simply want to save a file.
 *
 * Returns: %TRUE if file save succeeded, %FALSE otherwise.
 **/
gboolean
gwy_file_func_run_save(const gchar *name,
                       GwyContainer *data,
                       const gchar *filename,
                       GwyRunType mode,
                       GError **error)
{
    GwyFileFuncInfo *func_info;
    gboolean status;

    g_return_val_if_fail(!error || !*error, FALSE);
    g_return_val_if_fail(filename, FALSE);
    g_return_val_if_fail(GWY_IS_CONTAINER(data), FALSE);
    func_info = g_hash_table_lookup(file_funcs, name);
    g_return_val_if_fail(func_info, FALSE);
    g_return_val_if_fail(func_info->save, FALSE);

    g_object_ref(data);
    status = func_info->save(data, filename, mode, error, name);
    if (status)
        gwy_file_type_info_set(data, name, filename);
    g_object_unref(data);

    return status;
}

/**
 * gwy_file_func_run_export:
 * @name: A file save function name.
 * @data: A #GwyContainer to save.
 * @filename: A file name to save @data as.
 * @mode: Run mode.
 * @error: Return location for a #GError (or %NULL).
 *
 * Runs a file export function identified by @name.
 *
 * It guarantees the container lifetime spans through the actual file saving,
 * so the module function doesn't have to care about it.
 *
 * This is a low-level function, consider using gwy_file_save() if you
 * simply want to save a file.
 *
 * Returns: %TRUE if file save succeeded, %FALSE otherwise.
 **/
gboolean
gwy_file_func_run_export(const gchar *name,
                         GwyContainer *data,
                         const gchar *filename,
                         GwyRunType mode,
                         GError **error)
{
    GwyFileFuncInfo *func_info;
    gboolean status;

    g_return_val_if_fail(!error || !*error, FALSE);
    g_return_val_if_fail(filename, FALSE);
    g_return_val_if_fail(GWY_IS_CONTAINER(data), FALSE);
    func_info = g_hash_table_lookup(file_funcs, name);
    g_return_val_if_fail(func_info, FALSE);
    g_return_val_if_fail(func_info->export_, FALSE);

    g_object_ref(data);
    status = func_info->export_(data, filename, mode, error, name);
    g_object_unref(data);

    return status;
}

static void
file_detect_max_score_cb(const gchar *key,
                         GwyFileFuncInfo *func_info,
                         FileDetectData *ddata)
{
    gint score;

    g_assert(gwy_strequal(key, func_info->name));

    if (!func_info->detect)
        return;
    if ((ddata->mode & GWY_FILE_OPERATION_LOAD) && !func_info->load)
        return;
    if ((ddata->mode & GWY_FILE_OPERATION_SAVE) && !func_info->save)
        return;
    if ((ddata->mode & GWY_FILE_OPERATION_EXPORT) && !func_info->export_)
        return;

    score = func_info->detect(ddata->fileinfo, ddata->only_name,
                                   func_info->name);
    if (score > ddata->score) {
        ddata->winner = func_info->name;
        ddata->score = score;
    }
}

/**
 * gwy_file_detect:
 * @filename: A file name to detect type of.
 * @only_name: Whether to use only file name for a guess, or try to actually
 *             access the file.
 * @operations: The file operations the file type must support (it must
 *              support all of them to be considered).
 *
 * Detects the type of a file.
 *
 * Returns: The type name (i.e., the same name as passed to
 *          e.g. gwy_run_file_load_func()) of most probable type of @filename,
 *          or %NULL if there's no probable one.
 **/
const gchar*
gwy_file_detect(const gchar *filename,
                gboolean only_name,
                GwyFileOperationType operations)
{
    return gwy_file_detect_with_score(filename, only_name, operations, NULL);
}

/**
 * gwy_file_detect_with_score:
 * @filename: A file name to detect type of.
 * @only_name: Whether to use only file name for a guess, or try to actually
 * @operations: The file operations the file type must support (it must
 * @score: Location to store the maximum score (corresponding to the returned
 *         type) to.
 *
 * Detects the type of a file and gives the score.
 *
 * Returns: The type name (i.e., the same name as passed to
 *          e.g. gwy_run_file_load_func()) of most probable type of @filename,
 *          or %NULL if there's no probable one.
 *
 * Since: 2.1
 **/
const gchar*
gwy_file_detect_with_score(const gchar *filename,
                           gboolean only_name,
                           GwyFileOperationType operations,
                           gint *score)
{
    FileDetectData ddata;
    GwyFileDetectInfo fileinfo;

    if (!file_funcs)
        return NULL;

    fileinfo.name = filename;
    /* File must exist if not only_name */
    if (!gwy_file_detect_fill_info(&fileinfo, only_name))
        return NULL;

    ddata.fileinfo = &fileinfo;
    ddata.winner = NULL;
    ddata.score = 0;
    ddata.only_name = only_name;
    ddata.mode = operations;
    g_hash_table_foreach(file_funcs, (GHFunc)file_detect_max_score_cb, &ddata);
    gwy_file_detect_free_info(&fileinfo);

    if (score)
        *score = ddata.score;
    return ddata.score ? ddata.winner : NULL;
}

static gboolean
gwy_file_detect_fill_info(GwyFileDetectInfo *fileinfo,
                          gboolean only_name)
{
    struct stat st;
    FILE *fh;

    g_return_val_if_fail(fileinfo && fileinfo->name, FALSE);

    fileinfo->name_lowercase = g_ascii_strdown(fileinfo->name, -1);
    fileinfo->file_size = 0;
    fileinfo->buffer_len = 0;
    fileinfo->head = NULL;
    fileinfo->tail = NULL;
    if (only_name)
        return TRUE;

    if (g_stat(fileinfo->name, &st) != 0) {
        gwy_file_detect_free_info(fileinfo);
        return FALSE;
    }
    fileinfo->file_size = st.st_size;
    if (!fileinfo->file_size) {
        gwy_file_detect_free_info(fileinfo);
        return FALSE;
    }

    if (!(fh = g_fopen(fileinfo->name, "rb"))) {
        gwy_file_detect_free_info(fileinfo);
        return FALSE;
    }

    fileinfo->buffer_len = MIN(fileinfo->file_size + 1,
                               GWY_FILE_DETECT_BUFFER_SIZE);
    fileinfo->head = g_new0(guchar, 2*fileinfo->buffer_len);
    fileinfo->tail = fileinfo->head + fileinfo->buffer_len;
    if (fread((gchar*)fileinfo->head, fileinfo->buffer_len - 1, 1, fh) < 1
        || fseek(fh, 1-(gint)fileinfo->buffer_len, SEEK_END) != 0
        || fread((gchar*)fileinfo->tail, fileinfo->buffer_len - 1, 1, fh) < 1) {
        fclose(fh);
        gwy_file_detect_free_info(fileinfo);
        return FALSE;
    }

    fclose(fh);
    ((gchar*)fileinfo->head)[fileinfo->buffer_len - 1] = '\0';
    ((gchar*)fileinfo->tail)[fileinfo->buffer_len - 1] = '\0';

    return TRUE;
}

static void
gwy_file_detect_free_info(GwyFileDetectInfo *fileinfo)
{
    g_free((gpointer)fileinfo->name_lowercase);
    g_free((gpointer)fileinfo->head);
    fileinfo->head = NULL;
    fileinfo->tail = NULL;
    fileinfo->name_lowercase = NULL;
    fileinfo->buffer_len = 0;
}

/**
 * gwy_file_load:
 * @filename: A file name to load data from, in GLib encoding.
 * @mode: Run mode.
 * @error: Return location for a #GError (or %NULL).
 *
 * Loads a data file, autodetecting its type.
 *
 * Returns: A new #GwyContainer with data from @filename, or %NULL.
 **/
GwyContainer*
gwy_file_load(const gchar *filename,
              GwyRunType mode,
              GError **error)
{
    const gchar *winner;
    FILE *fh;

    g_return_val_if_fail(filename, NULL);

    /* When the file does not exist (for instance), we would get a correct but
     * very unheplful message that no module can load such a file. */
    if (!(fh = g_fopen(filename, "rb"))) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_IO,
                    _("Cannot open file for reading: %s."), g_strerror(errno));
        return NULL;
    }
    fclose(fh);

    winner = gwy_file_detect(filename, FALSE, GWY_FILE_OPERATION_LOAD);
    if (!winner) {
        g_set_error(error, GWY_MODULE_FILE_ERROR,
                    GWY_MODULE_FILE_ERROR_UNIMPLEMENTED,
                    _("No module can load this file type."));
        return NULL;
    }

    return gwy_file_func_run_load(winner, filename, mode, error);
}

/**
 * gwy_file_save:
 * @data: A #GwyContainer to save.
 * @filename: A file name to save the data as, in GLib encoding.
 * @mode: Run mode.
 * @error: Return location for a #GError (or %NULL).
 *
 * Saves a data file, deciding to save as what type from the file name.
 *
 * It tries to find a module implementing %GWY_FILE_OPERATION_SAVE first, when
 * it does not succeed, it falls back to %GWY_FILE_OPERATION_EXPORT.
 *
 * Returns: The save operation that was actually realized on success, zero
 *          on failure.
 **/
GwyFileOperationType
gwy_file_save(GwyContainer *data,
              const gchar *filename,
              GwyRunType mode,
              GError **error)
{
    FileDetectData ddata;
    GwyFileDetectInfo fileinfo;

    if (!file_funcs)
        goto gwy_file_save_fail;

    fileinfo.name = filename;
    gwy_file_detect_fill_info(&fileinfo, TRUE);

    ddata.fileinfo = &fileinfo;
    ddata.winner = NULL;
    ddata.score = 0;
    ddata.only_name = TRUE;
    ddata.mode = GWY_FILE_OPERATION_SAVE;
    g_hash_table_foreach(file_funcs, (GHFunc)file_detect_max_score_cb, &ddata);

    if (ddata.winner) {
        gwy_file_detect_free_info(&fileinfo);
        if (gwy_file_func_run_save(ddata.winner,
                                   data, filename, mode, error))
            return ddata.mode;
        return 0;
    }

    ddata.mode = GWY_FILE_OPERATION_EXPORT;
    g_hash_table_foreach(file_funcs, (GHFunc)file_detect_max_score_cb, &ddata);
    gwy_file_detect_free_info(&fileinfo);

    if (ddata.winner) {
        if (gwy_file_func_run_export(ddata.winner,
                                     data, filename, mode, error))
            return ddata.mode;
        return 0;
    }

gwy_file_save_fail:
    g_set_error(error, GWY_MODULE_FILE_ERROR,
                GWY_MODULE_FILE_ERROR_UNIMPLEMENTED,
                _("No module can save to this file type."));

    return 0;
}

static void
gwy_file_func_user_cb(gpointer key,
                      G_GNUC_UNUSED gpointer value,
                      gpointer user_data)
{
    FuncForeachData *ffd = (FuncForeachData*)user_data;

    ffd->function(key, ffd->user_data);
}

/**
 * gwy_file_func_foreach:
 * @function: Function to run for each file function.  It will get function
 *            name (constant string owned by module system) as its first
 *            argument, @user_data as the second argument.
 * @user_data: Data to pass to @function.
 *
 * Calls a function for each file function.
 **/
void
gwy_file_func_foreach(GFunc function,
                      gpointer user_data)
{
    FuncForeachData ffd;

    if (!file_funcs)
        return;

    ffd.user_data = user_data;
    ffd.function = function;
    g_hash_table_foreach(file_funcs, gwy_file_func_user_cb, &ffd);
}

/**
 * gwy_file_func_exists:
 * @name: File type function name.
 *
 * Checks whether a file type function exists.
 *
 * Returns: %TRUE if function @name exists, %FALSE otherwise.
 **/
gboolean
gwy_file_func_exists(const gchar *name)
{
    return file_funcs && g_hash_table_lookup(file_funcs, name);
}

/**
 * gwy_file_func_get_operations:
 * @name: File type function name.
 *
 * Returns operations supported by a file type function.
 *
 * Returns: The file operation bit mask, zero if @name does not exist.
 **/
GwyFileOperationType
gwy_file_func_get_operations(const gchar *name)
{
    GwyFileFuncInfo *func_info;

    g_return_val_if_fail(file_funcs, 0);
    func_info = g_hash_table_lookup(file_funcs, name);
    g_return_val_if_fail(func_info, 0);

    return get_operations(func_info);
}

static GwyFileOperationType
get_operations(const GwyFileFuncInfo *func_info)
{
    GwyFileOperationType capable = 0;

    if (!func_info)
        return capable;

    capable |= func_info->load ? GWY_FILE_OPERATION_LOAD : 0;
    capable |= func_info->save ? GWY_FILE_OPERATION_SAVE : 0;
    capable |= func_info->export_ ? GWY_FILE_OPERATION_EXPORT : 0;
    capable |= func_info->detect ? GWY_FILE_OPERATION_DETECT : 0;

    return capable;
}

/**
 * gwy_file_func_get_description:
 * @name: File type function name.
 *
 * Gets file function description.
 *
 * That is, the @file_desc field of #GwyFileFuncInfo.
 *
 * Returns: File function description, as a string owned by module loader.
 **/
const gchar*
gwy_file_func_get_description(const gchar *name)
{
    GwyFileFuncInfo *func_info;

    g_return_val_if_fail(file_funcs, NULL);
    func_info = g_hash_table_lookup(file_funcs, name);
    g_return_val_if_fail(func_info, NULL);

    return func_info->description;
}

gboolean
_gwy_file_func_remove(const gchar *name)
{
    gwy_debug("%s", name);
    if (!g_hash_table_remove(file_funcs, name)) {
        g_warning("Cannot remove function %s", name);
        return FALSE;
    }
    return TRUE;
}

/**
 * gwy_file_get_data_info:
 * @data: A #GwyContainer.
 * @name: Location to store file type (that is file function name) of @data,
 *        or %NULL.  The returned string is owned by module system.
 * @filename_sys: Location to store file name of @data (in GLib encoding), or
 *                %NULL.  The returned string is owned by module system and is
 *                valid only until the container is destroyed or saved again.
 *
 * Gets file information about a data.
 *
 * The information is set on two ocasions: file load and successful file save.
 * File export does not set it.
 *
 * Returns: %TRUE if information about @data was found and @name and/or
 *          @filename was filled.
 **/
gboolean
gwy_file_get_data_info(GwyContainer *data,
                       const gchar **name,
                       const gchar **filename_sys)
{
    FileTypeInfo *fti;

    fti = gwy_file_type_info_get(data, FALSE);
    if (!fti)
        return FALSE;

    if (name)
        *name = g_quark_to_string(fti->name);
    if (filename_sys)
        *filename_sys = fti->filename_sys;

    return TRUE;
}

/**
 * gwy_module_file_error_quark:
 *
 * Returns error domain for file module functions.
 *
 * See and use %GWY_MODULE_FILE_ERROR.
 *
 * Returns: The error domain.
 **/
GQuark
gwy_module_file_error_quark(void)
{
    static GQuark error_domain = 0;

    if (!error_domain)
        error_domain = g_quark_from_static_string("gwy-module-file-"
                                                  "error-quark");

    return error_domain;
}

static void
gwy_file_type_info_set(GwyContainer *data,
                       const gchar *name,
                       const gchar *filename_sys)
{
    FileTypeInfo *fti;

    fti = gwy_file_type_info_get(data, TRUE);
    fti->name = g_quark_from_string(name);
    /* filename_sys can be *physically* the same string as fti->filename_sys */
    if (fti->filename_sys == filename_sys)
        return;
    if (fti->filename_sys)
        g_free(fti->filename_sys);
    fti->filename_sys = g_strdup(filename_sys);
}

static FileTypeInfo*
gwy_file_type_info_get(GwyContainer *data,
                       gboolean do_create)
{
    FileTypeInfo *fti;
    GList *l;

    for (l = container_list; l; l = g_list_next(l)) {
        fti = (FileTypeInfo*)l->data;
        if ((gpointer)data == fti->container)
            break;
    }
    if (!l) {
        if (!do_create)
            return NULL;

        fti = g_new0(FileTypeInfo, 1);
        fti->container = data;
        container_list = g_list_prepend(container_list, fti);
        g_object_weak_ref(G_OBJECT(data), gwy_file_container_finalized, NULL);

        return fti;
    }

    /* move container to head */
    if (l != container_list) {
        container_list = g_list_remove_link(container_list, l);
        container_list = g_list_concat(l, container_list);
    }

    return fti;
}

static void
gwy_file_container_finalized(G_GNUC_UNUSED gpointer userdata,
                             GObject *deceased_data)
{
    FileTypeInfo *fti;

    /* must not typecast with GWY_CONTAINER(), it doesn't exist any more */
    fti = gwy_file_type_info_get((GwyContainer*)deceased_data, FALSE);
    g_return_if_fail(fti);
    /* gwy_file_type_info_get() moves the item to list head */
    g_assert(fti == container_list->data);
    container_list = g_list_delete_link(container_list, container_list);
    g_free(fti->filename_sys);
    g_free(fti);
}

/************************** Documentation ****************************/

/**
 * SECTION:gwymodule-file
 * @title: gwymodule-file
 * @short_description: File loading and saving modules
 *
 * File modules implement file loading, saving and file type detection
 * functions.  Not all fuctions has to be implemented, a file module can be
 * import-only or export-only.  If it does not implement file type detection,
 * files of this type can be read/written only on user's explicite request.
 *
 * For file module writers, the only useful function here is the registration
 * function gwy_file_func_register() and the signatures of particular file
 * operations: #GwyFileDetectFunc, #GwyFileLoadFunc, and #GwyFileSaveFunc.
 **/

/**
 * GwyFileDetectFunc:
 * @fileinfo: Information about file to detect the filetype of,
 *            see #GwyFileDetectInfo.
 * @only_name: Whether the type should be guessed only from file name.
 * @name: Function name from as registered with gwy_file_func_register()
 *        (single-function modules can safely ignore this argument).
 *
 * The type of file type detection function.
 *
 * When called with %TRUE @only_name it should not try to access the file.
 *
 * Returns: An integer likehood score (see gwy_file_func_run_detect() for
 *          description).
 **/

/**
 * GwyFileLoadFunc:
 * @filename: A file name to load data from.
 * @mode: Run mode, this is either @GWY_RUN_NONINTERACTIVE
 *        or @GWY_RUN_INTERACTIVE.
 * @error: Return location for a #GError (or %NULL).
 * @name: Function name from as registered with gwy_file_func_register()
 *        (single-function modules can safely ignore this argument).
 *
 * The type of file loading function.
 *
 * Returns: A newly created data container, or %NULL on failure.
 **/

/**
 * GwyFileSaveFunc:
 * @data: A #GwyContainer to save.
 * @filename: A file name to save @data as.
 * @mode: Run mode, this is either @GWY_RUN_NONINTERACTIVE
 *        or @GWY_RUN_INTERACTIVE.
 * @error: Return location for a #GError (or %NULL).
 * @name: Function name from as registered with gwy_file_func_register()
 *        (single-function modules can safely ignore this argument).
 *
 * The type of file saving function.
 *
 * Returns: %TRUE if file save succeeded, %FALSE otherwise.
 **/

/**
 * GwyFileOperationType:
 * @GWY_FILE_OPERATION_DETECT: Posibility to detect files are of this file type,
 * @GWY_FILE_OPERATION_LOAD: Posibility to load files of this type.
 * @GWY_FILE_OPERATION_SAVE: Posibility to save files of this type.
 * @GWY_FILE_OPERATION_EXPORT: Posibility to export files of this type.
 * @GWY_FILE_OPERATION_MASK: The mask for all the flags.
 *
 * File type function file operations (capabilities).
 *
 * The difference between save and export is that save is supposed to create
 * a file containing fairly complete representation of the container, while
 * export is the possibility to write some information to given file type.
 * Generally only native file format module implements
 * %GWY_FILE_OPERATION_SAVE, all others implement %GWY_FILE_OPERATION_EXPORT.
 **/

/**
 * GwyFileDetectInfo:
 * @name: File name.
 * @name_lowercase: File name in lowercase (for eventual case-insensitive
 *                  name check).
 * @file_size: File size in bytes.  Undefined if @only_name.
 * @buffer_len: The size of @head and @tail in bytes.  Normally it's
 *              @GWY_FILE_DETECT_BUFFER_SIZE except when file is shorter than
 *              that.  Undefined if @only_name.
 * @head: Initial part of file.  Undefined if @only_name.
 * @tail: Final part of file.  Undefined if @only_name.
 *
 * File detection data for #GwyFileDetectFunc.
 *
 * It contains common information file type detection routines need to obtain.
 * It is shared between file detection functions and they must not modify its
 * contents.  Some fields are set only when detection routines are to
 * check file contents, these are marked `Undefined if @only_name'.
 *
 * The @head and @tail buffers are always nul-terminated and thus safely usable
 * with string functions.  When file is shorter than
 * @GWY_FILE_DETECT_BUFFER_SIZE bytes, <literal>'\0'</literal> is appended
 * to the end (therefore @buffer_len = @file_size + 1), otherwise the last
 * byte is overwritten with <literal>'\0'</literal>.  In either case the
 * last byte of @head and @tail cannot be assumed to be identical as in
 * the file (or being part of the file at all).
 **/

/**
 * GWY_FILE_DETECT_BUFFER_SIZE:
 *
 * The size of #GwyFileDetectInfo buffer for initial part of file.  It should
 * be enough for any normal kind of magic header test.
 **/

/**
 * GWY_MODULE_FILE_ERROR:
 *
 * Error domain for file module operations.  Errors in this domain will be from
 * the #GwyModuleFileError enumeration. See #GError for information on
 * error domains.
 **/

/**
 * GwyModuleFileError:
 * @GWY_MODULE_FILE_ERROR_CANCELLED: Interactive operation was cancelled by
 *                                   user.
 * @GWY_MODULE_FILE_ERROR_UNIMPLEMENTED: No module implements requested
 *                                       operation.
 * @GWY_MODULE_FILE_ERROR_IO: Input/output error occured.
 * @GWY_MODULE_FILE_ERROR_DATA: Data is corrupted or in an unsupported format.
 * @GWY_MODULE_FILE_ERROR_INTERACTIVE: Operation requires user input, but
 *                                     it was run as GWY_RUN_NONINTERACTIVE.
 * @GWY_MODULE_FILE_ERROR_SPECIFIC: Special module errors that do not fall into
 *                                  any other category.  Should be rarely used.
 *
 * Error codes returned by file module operations.
 *
 * File module functions can return any of these codes, except
 * @GWY_MODULE_FILE_ERROR_UNIMPLEMENTED which is normally only returned by
 * high-level functions gwy_file_load() and gwy_file_save().  Module functions
 * can return it only when they are called with a wrong function name.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
