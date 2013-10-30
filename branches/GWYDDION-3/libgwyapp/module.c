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

#include "config.h"
#include <stdarg.h>
#include <glib/gi18n-lib.h>
#include <gio/gio.h>
#include "libgwy/macros.h"
#include "libgwy/main.h"
#include "libgwy/strfuncs.h"
#include "libgwy/object-utils.h"
#include "libgwyapp/module.h"

typedef struct {
    const GwyModuleInfo *module_info;
    GModule *mod;
    GQuark qname;
    gulong priority;
    gboolean from_library : 1;
    gboolean did_type_registration : 1;
} ModuleInfo;

typedef struct {
    GError *error;
    gchar *path;
    GQuark qname;
    gboolean from_library;
} FailedModuleInfo;

typedef struct {
    GQuark qname;
    GQuark module_qname;
    GType type;
} TypeInfo;

static GQuark        queue_one_module            (const gchar *filename,
                                                  GError **error);
static gboolean      queue_opened_module         (GModule *mod,
                                                  const GwyModuleInfo *module_info,
                                                  GQuark qname,
                                                  GError **error);
static guint         queue_module_library        (const gchar *filename,
                                                  GwyErrorList **errorlist);
static guint         queue_opened_module_library (GModule *mod,
                                                  const GwyModuleLibraryRecord *records,
                                                  GwyErrorList **errorlist);
static guint         queue_one_module_or_library (const gchar *filename,
                                                  GwyErrorList **errorlist);
static GModule*      open_module_file            (const gchar *filename,
                                                  GError **error);
static gconstpointer find_module_info            (GModule *module,
                                                  const gchar *module_info_symbol,
                                                  GError **error);
static void          close_module                (GModule *mod,
                                                  const gchar *filename);
static gboolean      handle_overriding           (ModuleInfo *modinfo,
                                                  GError **error);
static ModuleInfo*   fill_module_info            (const GwyModuleInfo *module_info,
                                                  GQuark qname,
                                                  GModule *mod,
                                                  gboolean from_library,
                                                  GError **error);
static gboolean      err_unset_metadata          (GError **error,
                                                  const gchar *metadata,
                                                  const gchar *name,
                                                  GModule *mod);
static void          log_module_failure          (GError *error,
                                                  GQuark qname,
                                                  const gchar *filename,
                                                  gboolean from_library);
static gchar*        module_name_from_path       (const gchar *filename,
                                                  GError **error);
static gboolean      module_name_is_valid        (const gchar *name,
                                                  GError **error);
static GPtrArray*    gather_unregistered_modules (void);
static gint          compare_unregistered_modinfo(gconstpointer a,
                                                  gconstpointer b);
static gboolean      register_one_type           (const GwyModuleProvidedType *mptype,
                                                  GQuark module_qname,
                                                  GError **error);
static ModuleInfo*   lookup_module               (const gchar *name,
                                                  gboolean registered,
                                                  gboolean queued);
static GHashTable*   ensure_module_table         (void);
static void          module_info_free            (gpointer p);
static GArray*       ensure_failed_module_table  (void);
static void          failed_module_info_free     (gpointer p);
static GHashTable*   ensure_type_table           (void);
static void          type_info_free              (gpointer p);

static gulong module_priority = 0;

/**
 * gwy_module_error_quark:
 *
 * Provides error domain for module loading.
 *
 * See and use %GWY_MODULE_ERROR.
 *
 * Returns: The error domain.
 **/
GQuark
gwy_module_error_quark(void)
{
    static GQuark error_domain = 0;

    if (!error_domain)
        error_domain = g_quark_from_static_string("gwy-module-error-quark");

    return error_domain;
}

/**
 * gwy_register_modules:
 * @errorlist: (allow-none):
 *             Location to store possible errors.
 *             Errors from %GWY_MODULE_ERROR or %G_IO_ERROR domain can occur.
 *
 * Loads modules and registers the extension classes using the default rules.
 *
 * The module search paths are given by the compiled-in path, installation
 * path, environment variables and user home directory.
 *
 * Returns: The number of loaded modules and module libraries.  Since the
 *          failure to load a single module is not fatal, it is possible that
 *          non-zero count is returned and simultaneously some errors are
 *          logged to @errorlist.
 **/
guint
gwy_register_modules(GwyErrorList **errorlist)
{
    gchar **pathlist = gwy_library_search_path("modules");
    guint count = 0;
    for (guint i = 0; pathlist[i]; i++) {
        if (!g_file_test(pathlist[i], G_FILE_TEST_IS_DIR))
            continue;

        count += gwy_module_load_directory(pathlist[i], errorlist);
    }
    gwy_module_register_types(errorlist);
    return count;
}

/**
 * gwy_module_list:
 *
 * Obtains the list of all loaded and registered modules.
 *
 * Modules that are only queued for type registration are not included.
 *
 * Returns: (array zero-terminated=1) (transfer container):
 *          A %NULL-terminated array of module names, in no particular order.
 *          The array itself must be freed with g_free(), the strings must not.
 **/
const gchar**
gwy_module_list(void)
{
    GHashTable *modules = ensure_module_table();
    GPtrArray *name_list = g_ptr_array_new();
    GHashTableIter iter;
    gpointer key, value;

    g_hash_table_iter_init(&iter, modules);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        ModuleInfo *modinfo = (ModuleInfo*)value;
        if (!modinfo->did_type_registration)
            continue;

        g_ptr_array_add(name_list, (gpointer)g_quark_to_string(modinfo->qname));
    }

    g_ptr_array_add(name_list, NULL);
    return (const gchar**)g_ptr_array_free(name_list, FALSE);
}

/**
 * gwy_module_get_author:
 * @name: Module name.
 *
 * Obtains the author of a registered module.
 *
 * Returns: The author as a string owned by the module.
 **/
const gchar*
gwy_module_get_author(const gchar *name)
{
    ModuleInfo *modinfo = lookup_module(name, TRUE, FALSE);
    return modinfo ? modinfo->module_info->author : NULL;
}

/**
 * gwy_module_get_description:
 * @name: Module name.
 *
 * Obtains the description of a registered module.
 *
 * Returns: The description as a string owned by the module.
 **/
const gchar*
gwy_module_get_description(const gchar *name)
{
    ModuleInfo *modinfo = lookup_module(name, TRUE, FALSE);
    // FIXME: Translate here or later?
    return modinfo ? modinfo->module_info->description : NULL;
}

/**
 * gwy_module_get_version:
 * @name: Module name.
 *
 * Obtains the version of a registered module.
 *
 * Returns: The version as a string owned by the module.
 **/
const gchar*
gwy_module_get_version(const gchar *name)
{
    ModuleInfo *modinfo = lookup_module(name, TRUE, FALSE);
    return modinfo ? modinfo->module_info->version : NULL;
}

/**
 * gwy_module_get_copyright:
 * @name: Module name.
 *
 * Obtains the copyright holder of a registered module.
 *
 * Returns: The copyright holder as a string owned by the module.
 **/
const gchar*
gwy_module_get_copyright(const gchar *name)
{
    ModuleInfo *modinfo = lookup_module(name, TRUE, FALSE);
    return modinfo ? modinfo->module_info->copyright : NULL;
}

/**
 * gwy_module_get_date:
 * @name: Module name.
 *
 * Obtains the date of a registered module.
 *
 * Returns: The date as a string owned by the module.
 **/
const gchar*
gwy_module_get_date(const gchar *name)
{
    ModuleInfo *modinfo = lookup_module(name, TRUE, FALSE);
    return modinfo ? modinfo->module_info->date : NULL;
}

/**
 * gwy_module_load:
 * @filename: Absolute file name of the module file to load.
 * @error: Location to store possible error.
 *         Errors from %GWY_MODULE_ERROR domain can occur.
 *
 * Loads a single module from given file.
 *
 * This function only loads the module and queues it for registration.  You
 * need to call also gwy_module_register_types() some time in the future to
 * finish the registration.
 *
 * Note the module is loaded using g_module_open() so its rules for file name
 * amendments apply.
 *
 * Returns: (allow-none):
 *          The module information for the module, or %NULL on failure.
 **/
const GwyModuleInfo*
gwy_module_load(const gchar *filename,
                GError **error)
{
    g_return_val_if_fail(filename, NULL);

    GQuark qname = queue_one_module(filename, error);
    if (!qname)
        return NULL;

    GHashTable *modules = ensure_module_table();
    gpointer key = GUINT_TO_POINTER(qname);
    return g_hash_table_lookup(modules, key);
}

/**
 * gwy_module_load_library:
 * @filename: Absolute file name of the module library file to load.
 * @errorlist: (allow-none):
 *             Location to store possible errors.
 *             Errors from %GWY_MODULE_ERROR domain can occur.
 *
 * Loads a single module library from given file.
 *
 * This function only loads the library, examines the modules inside and queues
 * them for registration.  You need to call also gwy_module_register_types()
 * some time in the future to finish the registration.
 *
 * Note the module library is loaded using g_module_open() so its rules for
 * file name amendments apply.
 *
 * Returns: The number of modules successfully loaded from the library (they
 *          may be overriden later though).  A library providing no modules
 *          is considered failed.
 **/
guint
gwy_module_load_library(const gchar *filename,
                        GwyErrorList **errorlist)
{
    g_return_val_if_fail(filename, 0);
    return queue_module_library(filename, errorlist);
}

/**
 * gwy_module_load_directory:
 * @path: Name of directory to be scanned for modules and module libraries.
 *        Subdirectories are <emphasis>not</emphasis> scanned.
 * @errorlist: (allow-none):
 *             Location to store possible errors.
 *             Errors from %GWY_MODULE_ERROR or %G_IO_ERROR domain can occur.
 *
 * Loads modules and module libraries from a directory.
 *
 * This function only loads the modules or libraries and queues them for
 * registration.  You need to call also gwy_module_register_types() some time
 * in the future to finish the registration.
 *
 * Returns: The number of shared libraries that were found and loaded as either
 *          modules of module libraries.
 **/
guint
gwy_module_load_directory(const gchar *path,
                          GwyErrorList **errorlist)
{
    static const gchar attrs[] = (G_FILE_ATTRIBUTE_STANDARD_TYPE ","
                                  G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN ","
                                  G_FILE_ATTRIBUTE_STANDARD_IS_BACKUP ","
                                  G_FILE_ATTRIBUTE_STANDARD_NAME);

    g_return_val_if_fail(path, 0);
    GFile *file = g_file_new_for_path(path);
    GError *error = NULL;
    GFileEnumerator *enumerator = g_file_enumerate_children(file, attrs, 0,
                                                            NULL, &error);
    if (!enumerator) {
        gwy_error_list_propagate(errorlist, error);
        g_object_unref(file);
        return 0;
    }

    guint count = 0;
    for (GFileInfo *fileinfo = g_file_enumerator_next_file(enumerator, NULL,
                                                           &error);
         fileinfo;
         (void)g_object_unref(fileinfo),
         (fileinfo = g_file_enumerator_next_file(enumerator, NULL, &error))) {
        if (g_file_info_get_is_hidden(fileinfo)
            || g_file_info_get_is_backup(fileinfo))
            continue;

        GFileType filetype = g_file_info_get_file_type(fileinfo);
        if (filetype != G_FILE_TYPE_REGULAR
            && filetype != G_FILE_TYPE_SYMBOLIC_LINK)
            continue;

        const gchar *name = g_file_info_get_name(fileinfo);
        if (!g_str_has_suffix(name, "." G_MODULE_SUFFIX))
            continue;

        gchar *fullname = g_build_filename(path, name, NULL);
        if (queue_one_module_or_library(fullname, errorlist))
            count++;
        g_free(fullname);
    }

    if (error)
        gwy_error_list_propagate(errorlist, error);

    g_object_unref(enumerator);
    g_object_unref(file);
    return count;
}

/**
 * gwy_module_register_types:
 * @errorlist: (allow-none):
 *             Location to store possible errors.
 *             Errors from %GWY_MODULE_ERROR domain can occur.
 *
 * Registers extension types from all modules that were loaded but their types
 * were not registered yet.
 *
 * Returns: The number of new types registered.
 **/
guint
gwy_module_register_types(GwyErrorList **errorlist)
{
    GPtrArray *todo_list = gather_unregistered_modules();
    if (!todo_list)
        return 0;

    GHashTable *libraries_seen = g_hash_table_new(g_str_hash, g_str_equal);
    guint count = 0;
    for (guint i = 0; i < todo_list->len; i++) {
        ModuleInfo *modinfo = g_ptr_array_index(todo_list, i);
        const GwyModuleInfo *module_info = modinfo->module_info;
        for (guint j = 0; j < module_info->ntypes; j++) {
            GError *error = NULL;
            if (register_one_type(module_info->types + j,
                                  modinfo->qname,
                                  &error)) {
                count++;
            }
            if (error) {
                // TODO: log the error somewhere
                gwy_error_list_propagate(errorlist, error);
            }
        }

        if (modinfo->from_library) {
            const gchar *path = g_module_name(modinfo->mod);
            if (!g_hash_table_lookup(libraries_seen, path)) {
                g_module_make_resident(modinfo->mod);
                g_hash_table_insert(libraries_seen, (gpointer)path,
                                    GUINT_TO_POINTER(TRUE));
            }
        }
        else {
            g_module_make_resident(modinfo->mod);
        }
        modinfo->did_type_registration = TRUE;
    }

    g_hash_table_unref(libraries_seen);
    g_ptr_array_free(todo_list, TRUE);

    return count;
}

// For a module file.
// Thus *must* get non-NULL error.
static GQuark
queue_one_module(const gchar *filename,
                 GError **error)
{
    gchar *modname;
    GError *err = NULL;

    if (!(modname = module_name_from_path(filename, &err))) {
        log_module_failure(err, 0, filename, FALSE);
        g_propagate_error(error, err);
        return 0;
    }

    // XXX: Somewhere here we had code for avoiding attempt to load pygyw if
    // Python is not available.

    GQuark qname = g_quark_from_string(modname);
    g_free(modname);

    GModule *mod = open_module_file(filename, &err);
    if (!mod) {
        log_module_failure(err, qname, filename, FALSE);
        g_propagate_error(error, err);
        return 0;
    }

    gchar *symbol = g_strconcat(G_STRINGIFY(GWY_MODULE_INFO_SYMBOL), "_",
                                g_quark_to_string(qname), NULL);
    const GwyModuleInfo *module_info = find_module_info(mod, symbol, &err);
    g_free(symbol);

    if (!module_info || !queue_opened_module(mod, module_info, qname, &err)) {
        log_module_failure(err, qname, filename, FALSE);
        g_propagate_error(error, err);
        close_module(mod, filename);
        return 0;
    }

    return qname;
}

// The caller must do log_module_failure()!
static gboolean
queue_opened_module(GModule *mod,
                    const GwyModuleInfo *module_info,
                    GQuark qname,
                    GError **error)
{
    ModuleInfo *modinfo = NULL;
    if (!(modinfo = fill_module_info(module_info, qname, mod, FALSE, error)))
        return FALSE;

    if (!handle_overriding(modinfo, error)) {
        module_info_free(modinfo);
        return FALSE;
    }

    GHashTable *modules = ensure_module_table();
    gpointer key = GUINT_TO_POINTER(qname);
    g_hash_table_insert(modules, key, modinfo);

    return TRUE;
}

static guint
queue_module_library(const gchar *filename,
                     GwyErrorList **errorlist)
{
    GError *error = NULL;
    GModule *mod = open_module_file(filename, &error);
    if (!mod) {
        log_module_failure(error, 0, filename, TRUE);
        gwy_error_list_propagate(errorlist, error);
        return 0;
    }

    const gchar *symbol = G_STRINGIFY(GWY_MODULE_INFO_SYMBOL);
    const GwyModuleLibraryRecord *records = find_module_info(mod, symbol,
                                                             &error);
    if (!records) {
        log_module_failure(error, 0, filename, TRUE);
        gwy_error_list_propagate(errorlist, error);
        close_module(mod, filename);
        return 0;
    }

    return queue_opened_module_library(mod, records, errorlist);
}

static guint
queue_opened_module_library(GModule *mod,
                            const GwyModuleLibraryRecord *records,
                            GwyErrorList **errorlist)
{
    GHashTable *modules = ensure_module_table();
    GError *error = NULL;
    guint count = 0, i;

    for (i = 0; records[i].name; i++) {
        GQuark qname = g_quark_from_string(records[i].name);
        if (!module_name_is_valid(records[i].name, &error)) {
            log_module_failure(error, qname, g_module_name(mod), TRUE);
            gwy_error_list_propagate(errorlist, error);
            continue;
        }

        ModuleInfo *modinfo = NULL;
        if (!(modinfo = fill_module_info(records[i].info, qname, mod, TRUE,
                                         &error))) {
            log_module_failure(error, qname, g_module_name(mod), TRUE);
            gwy_error_list_propagate(errorlist, error);
            continue;
        }

        if (!handle_overriding(modinfo, &error)) {
            log_module_failure(error, qname, g_module_name(mod), TRUE);
            gwy_error_list_propagate(errorlist, error);
            module_info_free(modinfo);
            continue;
        }

        gpointer key = GUINT_TO_POINTER(qname);
        g_hash_table_insert(modules, key, modinfo);
        count++;
    }

    if (count)
        return count;

    if (!i) {
        g_set_error(&error, GWY_MODULE_ERROR, GWY_MODULE_ERROR_EMPTY_LIBRARY,
                    _("Module library ‘%s’ is empty."),
                    g_module_name(mod));
        log_module_failure(error, 0, g_module_name(mod), TRUE);
        gwy_error_list_propagate(errorlist, error);
    }
    // We should have at least error logged for this library one way or
    // another when we choose to fail.
    close_module(mod, NULL);

    return 0;
}

static guint
queue_one_module_or_library(const gchar *filename,
                            GwyErrorList **errorlist)
{
    GError *error = NULL;
    GModule *mod = open_module_file(filename, &error);
    if (!mod) {
        log_module_failure(error, 0, filename, TRUE);
        gwy_error_list_propagate(errorlist, error);
        return 0;
    }

    // First try opening the file as a module library.  Ignore the lookup error.
    const gchar *library_symbol = G_STRINGIFY(GWY_MODULE_INFO_SYMBOL);
    const GwyModuleLibraryRecord *records = find_module_info(mod,
                                                             library_symbol,
                                                             NULL);
    if (records)
        return queue_opened_module_library(mod, records, errorlist);

    // Then as a single module.
    gchar *modname;
    if (!(modname = module_name_from_path(filename, &error))) {
        log_module_failure(error, 0, filename, FALSE);
        gwy_error_list_propagate(errorlist, error);
        close_module(mod, filename);
        return 0;
    }

    GQuark qname = g_quark_from_string(modname);
    g_free(modname);

    gchar *symbol = g_strconcat(G_STRINGIFY(GWY_MODULE_INFO_SYMBOL), "_",
                                g_quark_to_string(qname), NULL);
    const GwyModuleInfo *module_info = find_module_info(mod, symbol, &error);
    g_free(symbol);

    if (!module_info || !queue_opened_module(mod, module_info, qname, &error)) {
        log_module_failure(error, qname, filename, FALSE);
        gwy_error_list_propagate(errorlist, error);
        close_module(mod, filename);
        return 0;
    }

    return 1;
}

static GModule*
open_module_file(const gchar *filename,
                 GError **error)
{
    GModule *mod = g_module_open(filename, G_MODULE_BIND_LAZY);
    if (mod)
        return mod;

    g_set_error(error, GWY_MODULE_ERROR, GWY_MODULE_ERROR_OPEN,
                // TRANSLATORS: Error message.
                "Cannot open module ‘%s’: %s",
                filename, g_module_error());
    return NULL;
}

static gconstpointer
find_module_info(GModule *mod,
                 const gchar *module_info_symbol,
                 GError **error)
{
    gpointer info;
    if (g_module_symbol(mod, module_info_symbol, &info) && info)
        return info;

    g_set_error(error, GWY_MODULE_ERROR, GWY_MODULE_ERROR_INFO,
                // TRANSLATORS: Error message.
                _("Cannot find module info ‘%s’ in ‘%s’."),
                module_info_symbol, g_module_name(mod));
    return NULL;
}

static void
close_module(GModule *mod, const gchar *filename)
{
    gchar *modfilename = NULL;

    // We cannot get the file name after we close the module, even
    // unsuccessfull, because it may be freed.
    if (!filename) {
        modfilename = g_strdup(g_module_name(mod));
        filename = modfilename;
    }

    if (g_module_close(mod)) {
        GWY_FREE(modfilename);
        return;
    }

    g_warning("Cannot close module ‘%s’: %s", filename, g_module_error());
    GWY_FREE(modfilename);
}

static gboolean
handle_overriding(ModuleInfo *modinfo,
                  GError **error)
{
    GHashTable *modules = ensure_module_table();
    gpointer key = GUINT_TO_POINTER(modinfo->qname);
    ModuleInfo *othermodinfo = NULL;

    if (!(othermodinfo = g_hash_table_lookup(modules, key)))
        return TRUE;

    if (othermodinfo->did_type_registration) {
        g_set_error(error, GWY_MODULE_ERROR, GWY_MODULE_ERROR_DUPLICATE_MODULE,
                    // TRANSLATORS: Error message.
                    _("Module ‘%s’ was already registered from ‘%s’ and "
                      "can no longer be overriden by ‘%s’."),
                    g_quark_to_string(modinfo->qname),
                    g_module_name(othermodinfo->mod),
                    g_module_name(modinfo->mod));
        return FALSE;
    }

    // Override the previous module, stealing its priority.
    GError *err = NULL;
    g_set_error(&err, GWY_MODULE_ERROR, GWY_MODULE_ERROR_OVERRIDEN,
                // TRANSLATORS: Error message.
                _("Module ‘%s’ loaded from ‘%s’ was overriden by ‘%s’."),
                g_quark_to_string(modinfo->qname),
                g_module_name(othermodinfo->mod),
                g_module_name(modinfo->mod));
    // This makes a copy of everything
    log_module_failure(err,
                       othermodinfo->qname, g_module_name(othermodinfo->mod),
                       othermodinfo->from_library);
    g_clear_error(&err);

    if (!othermodinfo->from_library) {
        g_module_close(othermodinfo->mod);
        othermodinfo->mod = NULL;
    }

    modinfo->priority = othermodinfo->priority;
    g_hash_table_remove(modules, key);

    return TRUE;
}

// @module_info can come from either a separate module or a library here.
static ModuleInfo*
fill_module_info(const GwyModuleInfo *module_info,
                 GQuark qname,
                 GModule *mod,
                 gboolean from_library,
                 GError **error)
{
    if (module_info->abi_version != GWY_MODULE_ABI_VERSION) {
        g_set_error(error, GWY_MODULE_ERROR, GWY_MODULE_ERROR_ABI,
                    // TRANSLATORS: Error message.
                    _("Module ABI version %u of ‘%s’ differs from %u."),
                    module_info->abi_version,
                    g_module_name(mod),
                    GWY_MODULE_ABI_VERSION);
        return NULL;
    }

    if (module_info->ntypes && !module_info->types) {
        g_set_error(error, GWY_MODULE_ERROR, GWY_MODULE_ERROR_TYPES,
                    // TRANSLATORS: Error message.
                    _("Module ‘%s’ declares non-zero number of types but it "
                      "does not provide any."),
                    g_module_name(mod));
        return NULL;
    }

    if (err_unset_metadata(error, module_info->author, "author",
                           mod)
        || err_unset_metadata(error, module_info->version, "version",
                              mod)
        || err_unset_metadata(error, module_info->copyright, "copyright",
                              mod)
        || err_unset_metadata(error, module_info->date, "date",
                              mod)
        || err_unset_metadata(error, module_info->description, "description",
                              mod))
        return NULL;

    ModuleInfo *modinfo = g_slice_new0(ModuleInfo);
    modinfo->module_info = module_info;
    modinfo->mod = mod;
    modinfo->qname = qname;
    modinfo->from_library = from_library;
    modinfo->priority = module_priority++;
    return modinfo;
}

static gboolean
err_unset_metadata(GError **error,
                   const gchar *metadata,
                   const gchar *name,
                   GModule *mod)
{
    if (metadata && *metadata)
        return FALSE;

    g_set_error(error, GWY_MODULE_ERROR, GWY_MODULE_ERROR_METADATA,
                _("Module ‘%s’ metadata does not define field ‘%s’."),
                g_module_name(mod), name);
    return TRUE;
}

static void
log_module_failure(GError *error,
                   GQuark qname,
                   const gchar *filename,
                   gboolean from_library)
{
    FailedModuleInfo info = {
        .error = g_error_copy(error),
        .path = g_strdup(filename),
        .qname = qname,
        .from_library = from_library,
    };

    GArray *modules = ensure_failed_module_table();
    g_array_append_val(modules, info);
}

static gchar*
module_name_from_path(const gchar *filename,
                      GError **error)
{
    gchar *modname = g_path_get_basename(filename);
    gchar *p;

    for (p = modname; *p; p++) {
        if (*p == '-')
            *p = '_';
        else
            *p = g_ascii_tolower(*p);
    }

    /* On normal platforms module names have an extension, but if it doesn't,
     * just get over it.  This can happen only with explicit gwy_module_load()
     * as gwy_module_load_directory() accepts only sane names. */
    if ((p = strchr(modname, '.')))
        *p = '\0';

    if (!module_name_is_valid(modname, error)) {
        g_free(modname);
        return NULL;
    }

    return modname;
}

static gboolean
module_name_is_valid(const gchar *name,
                     GError **error)
{
    if (gwy_ascii_strisident(name, "_", NULL))
        return TRUE;

    g_set_error(error, GWY_MODULE_ERROR, GWY_MODULE_ERROR_MODULE_NAME,
                // TRANSLATORS: Error message.
                _("Module name ‘%s’ is not a valid identifier."),
                name);
    return FALSE;
}

static GPtrArray*
gather_unregistered_modules(void)
{
    GHashTable *modules = ensure_module_table();
    GPtrArray *todo_list = g_ptr_array_new();
    GHashTableIter iter;
    gpointer key, value;

    g_hash_table_iter_init(&iter, modules);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        ModuleInfo *modinfo = (ModuleInfo*)value;
        if (modinfo->did_type_registration)
            continue;

        g_ptr_array_add(todo_list, modinfo);
    }

    if (!todo_list->len) {
        g_ptr_array_free(todo_list, TRUE);
        return NULL;
    }

    g_ptr_array_sort(todo_list, &compare_unregistered_modinfo);

    return todo_list;
}

static gint
compare_unregistered_modinfo(gconstpointer a,
                             gconstpointer b)
{
    const ModuleInfo **pmodinfoa = (const ModuleInfo**)a;
    const ModuleInfo **pmodinfob = (const ModuleInfo**)b;
    const ModuleInfo *modinfoa = *pmodinfoa, *modinfob = *pmodinfob;

    if (modinfoa->priority < modinfob->priority)
        return -1;
    if (modinfoa->priority > modinfob->priority)
        return 1;

    return 0;
}

static gboolean
register_one_type(const GwyModuleProvidedType *mptype,
                  GQuark module_qname,
                  GError **error)
{
    if (!mptype->name) {
        g_set_error(error, GWY_MODULE_ERROR, GWY_MODULE_ERROR_TYPE_NAME,
                    // TRANSLATORS: Error message.
                    _("NULL type name was supplied by module ‘%s’."),
                    g_quark_to_string(module_qname));
        return FALSE;
    }

    if (!mptype->get_type) {
        g_set_error(error, GWY_MODULE_ERROR, GWY_MODULE_ERROR_GET_TYPE,
                    // TRANSLATORS: Error message.
                    _("Module ‘%s’ supplied a NULL get-type function "
                      "for type ‘%s’."),
                    g_quark_to_string(module_qname), mptype->name);
        return FALSE;
    }

    if (!gwy_ascii_strisident(mptype->name, NULL, NULL)) {
        g_set_error(error, GWY_MODULE_ERROR, GWY_MODULE_ERROR_TYPE_NAME,
                    // TRANSLATORS: Error message.
                    _("Class name ‘%s’ in module ‘%s’ is not "
                      "a valid identifier."),
                    mptype->name, g_quark_to_string(module_qname));
        return FALSE;
    }

    if (!g_str_has_prefix(mptype->name, "GwyExt")
        || !g_ascii_isupper(mptype->name[7])) {
        g_set_error(error, GWY_MODULE_ERROR, GWY_MODULE_ERROR_TYPE_NAME,
                    // TRANSLATORS: Error message.
                    _("Class name ‘%s’ in module ‘%s’ does not have the form "
                      "GwyExtSomething."),
                    mptype->name, g_quark_to_string(module_qname));
        return FALSE;
    }

    GHashTable *types = ensure_type_table();
    TypeInfo *typeinfo = NULL;
    GQuark qname = g_quark_from_string(mptype->name);
    gpointer key = GUINT_TO_POINTER(qname);
    if ((typeinfo = g_hash_table_lookup(types, key))) {
        g_set_error(error, GWY_MODULE_ERROR, GWY_MODULE_ERROR_DUPLICATE_TYPE,
                    // TRANSLATORS: Error message.
                    _("Type ‘%s’, which module ‘%s’ is attempting to register, "
                      "has been already registered by module ‘%s’."),
                    mptype->name, g_quark_to_string(module_qname),
                    g_quark_to_string(typeinfo->module_qname));
        return FALSE;
    }

    // This should not happen. But...
    if (g_type_from_name(mptype->name)) {
        g_set_error(error, GWY_MODULE_ERROR, GWY_MODULE_ERROR_DUPLICATE_TYPE,
                    // TRANSLATORS: Error message.
                    _("Type ‘%s’, which module ‘%s’ is attempting to register, "
                     " has been already registered."),
                    g_quark_to_string(module_qname), mptype->name);
        return FALSE;
    }

    GType type = mptype->get_type();
    if (!type) {
        g_set_error(error, GWY_MODULE_ERROR, GWY_MODULE_ERROR_GET_TYPE,
                    // TRANSLATORS: Error message.
                    _("Module ‘%s’ get-type function for type ‘%s’ returned "
                      "NULL."),
                    g_quark_to_string(module_qname), mptype->name);
        return FALSE;
    }

    // They lied to us!  Treat it as non-fatal but stil feel deceived.
    if (!gwy_strequal(g_type_name(type), mptype->name)) {
        g_set_error(error, GWY_MODULE_ERROR, GWY_MODULE_ERROR_TYPE_NAME,
                    // TRANSLATORS: Error message.
                    _("Module ‘%s’ declared to provide the type ‘%s’, but "
                      "it registered the type ‘%s’."),
                    g_quark_to_string(module_qname), mptype->name,
                    g_type_name(type));
    }

    typeinfo = g_slice_new(TypeInfo);
    typeinfo->qname = qname;
    typeinfo->module_qname = module_qname;
    typeinfo->type = type;
    g_hash_table_insert(types, key, typeinfo);

    return TRUE;
}

static ModuleInfo*
lookup_module(const gchar *name,
              gboolean registered,
              gboolean queued)
{
    GQuark qname = g_quark_try_string(name);
    if (!qname)
        return NULL;

    GHashTable *modules = ensure_module_table();
    gpointer key = GUINT_TO_POINTER(qname);
    ModuleInfo *modinfo = g_hash_table_lookup(modules, key);
    if (!queued && !modinfo->did_type_registration)
        return NULL;
    if (!registered && modinfo->did_type_registration)
        return NULL;
    return modinfo;
}

static GHashTable*
ensure_module_table(void)
{
    static GHashTable *modules = NULL;

    if (G_LIKELY(modules))
        return modules;

    modules = g_hash_table_new_full(NULL, NULL, NULL, module_info_free);
    return modules;
}

static void
module_info_free(gpointer p)
{
    // GModules are apparently always kind-of-leaked.
    g_slice_free(ModuleInfo, p);
}

static GArray*
ensure_failed_module_table(void)
{
    static GArray *modules = NULL;

    if (G_LIKELY(modules))
        return modules;

    modules = g_array_new(FALSE, FALSE, sizeof(FailedModuleInfo));
    g_array_set_clear_func(modules, failed_module_info_free);
    return modules;
}

static void
failed_module_info_free(gpointer p)
{
    FailedModuleInfo *info = (FailedModuleInfo*)p;
    g_clear_error(&info->error);
    GWY_FREE(info->path);
    g_slice_free(FailedModuleInfo, info);
}

static GHashTable*
ensure_type_table(void)
{
    static GHashTable *types = NULL;

    if (G_LIKELY(types))
        return types;

    types = g_hash_table_new_full(NULL, NULL, NULL, type_info_free);
    return types;
}

static void
type_info_free(gpointer p)
{
    g_slice_free(TypeInfo, p);
}

/************************** Documentation ****************************/

/**
 * SECTION: module
 * @section_id: libgwyapp-module
 * @title: Module registration
 * @short_description: Loading and registration of modules
 *
 * Usually, modules are loaded and their extension types registered using
 * a single call to gwy_register_modules().  The details are described
 * below.
 *
 * <refsect2 id='libgwyapp-module-process'>
 * <title>Module registration process</title>
 * <para>Loading of modules supports overriding of system modules with
 * user-supplied modules.  Although modules are, in general, independent and it
 * is not possible to declare one module as depending on other, it is also
 * ensured system modules are loaded first.  A simple resolution mechanism is
 * used.</para>
 * <para>First, modules are gathered and dlopen()ed using functions such as
 * gwy_module_load(), gwy_module_load_library() or gwy_module_load_directory().
 * Since a module of the same name as one found earlier overrides the earlier
 * module, this should happen in the order from system modules to user
 * modules.  Function gwy_register_modules() ensures the required ordering,
 * however, it is something to keep in mind if you run the loading functions
 * manually. No extension class get-type functions are run at this
 * stage.</para>
 * <para>Extension classes are registered using gwy_module_register_types() in
 * the same order they were found, i.e. from lowest priority sources
 * (system modules) to highest priority sources (user modules).  However,
 * overriding takes place.  Classes of the overriding module module are
 * registered in place of the overriden module, i.e. they ‘cut the
 * queue’.</para>
 * <para>After class registration the modules become permanent and can no
 * longer be overriden.  So although it is possible to load another batch of
 * modules and call gwy_module_register_types() again, in the case of name
 * clash the new module fails and the already registered one prevails.</para>
 * </refsect2>
 * <refsect2 id='libgwyapp-module-initialisation'>
 * <title>Module initialisation/unloading</title>
 * <para>The module definition macro %GWY_DEFINE_MODULE provides a static data
 * structure describing the module.  Occasionally you may want to run some code
 * in order to construct the #GwyModuleInfo structure dynamically.  This can be
 * for instance the case of a file module which itself needs to query the
 * capabilities of some library.</para>
 * <para>The way to achieve this is define a GLib module initialisation
 * function <literal>g_module_check_init</literal><literal>()</literal>.
 * See #GModuleCheckInit for a description.
 * Such function can also return an error.  This enables modules to prevent
 * themselves from being loaded when they decide they cannot function
 * correctly.</para>
 * <para>For unloading, a GLib module unload function
 * <literal>g_module_unload</literal><literal>()</literal> can be provided.  It
 * is run when the module is unloaded, see #GModuleUnload.  Gwyddion does not
 * unload modules once they have been sucessfully registered by making them
 * resident with g_module_make_resident().  Therefore, the unloading might be a
 * concern only for failed and overriden modules.</para>
 * <para>Note modules requiring this functionality are not suitable for
 * inclusion into a module library.</para>
 * </refsect2>
 **/

/**
 * GWY_MODULE_ABI_VERSION:
 *
 * Gwyddion module ABI version.
 *
 * To be filled as @abi_version in #GwyModuleInfo.
 **/

/**
 * GWY_DEFINE_MODULE:
 * @mod_info: Pointer to the #GwyModuleInfo structure provided as the module
 *            info.  It is assumed to be valid during the entire lifetime of
 *            the module.
 * @name: Module name.
 *
 * Macro expanding to code necessary for registration of a module.
 *
 * The @name argument represents the module name.  It must match the file name
 * of the module with any operating system dependend exensions removed and
 * possible dashes converted to underscores and change of uppercase ASCII
 * letters to lowercase.  After this transformation, the file name must also
 * form a valid C identifier.  It is strongly recommended to avoid uppercase
 * letters completely in module file names.
 *
 * Example for a module called ‘awesome’ which would reside in file
 * "awesome.so", "awesome.dll", "awesome.dylib" or similar, depending on the
 * operating system.  In this example @module_types is a static array which is
 * sufficient for almost all modules:
 * |[
 * static const GwyModuleInfo module_info = {
 *    GWY_MODULE_ABI_VERSION,
 *    G_N_ELEMENTS(module_types),
 *    "This module is simply awesome.",
 *    "J. R. Hacker",
 *    "0.0.2alpha",
 *    "J. R. Hacker",
 *    "2015",
 *    module_types,
 * };
 *
 * GWY_DEFINE_MODULE(&module_info, awesome);
 * ]|
 **/

/**
 * GWY_DEFINE_MODULE_LIBRARY:
 * @mod_info_list: Array of #GwyModuleLibraryRecord records, terminated with
 *                 a record with %NULL name ans %GWY_MODULE_INFO_SENTINEL info;
 *                 to be returned as the list of modules in the library.
 *
 * Macro expanding to code necessary for registration of a module library.
 *
 * Module library is used for consolidation of a large numer of shared
 * dynamically loaded libraries to one.  This mostly only makes sense within
 * Gwyddion itself, which may contain hundreds of modules.  Standalone modules
 * should use %GWY_DEFINE_MODULE.
 *
 * A possible use case is a module which acts as the proxy for a group of
 * external extensions or scripts written using arbitrary third-party tools.
 * Such ‘module’ then does not register itself as a module.  Instead, it
 * behaves as a module library and provides (constructs) module registration
 * function for each of the extensions.
 *
 * If modules are consolidated into a module library, preprocessor macro
 * <literal>GWY_MODULE_BUILDING_LIBRARY</literal> should be defined to indicate
 * this.  Invididual module query functions defined with %GWY_DEFINE_MODULE are
 * then linked as internal to the library.  Only one library-level symbol,
 * defined with %GWY_DEFINE_MODULE_LIBRARY, is exported.
 **/

/**
 * GwyModuleInfo:
 * @abi_version: Gwyddion module ABI version, should be always
 *               #GWY_MODULE_ABI_VERSION.
 * @ntypes: Number of registered types, i.e. number of elements in @types.
 * @description: Description of the module purpose.
 * @author: Module author(s).
 * @version: Module version.
 * @copyright: Who has copyright on this module.
 * @date: Date (year).
 * @types: Array of @ntypes items describing the extension classes the module
 *         provides.
 *
 * Module information provided to GWY_DEFINE_MODULE().
 **/

/**
 * GwyModuleError:
 * @GWY_MODULE_ERROR_MODULE_NAME: Module name is not given or it is not a valid
 *                                identifier.
 * @GWY_MODULE_ERROR_DUPLICATE_MODULE: Module name clashes with an already
 *                                     registered module.
 * @GWY_MODULE_ERROR_EMPTY_LIBRARY: Module library does not contain any
 *                                  modules.  This may not be considered a
 *                                  hard error, anyway, such library is
 *                                  unloaded and reported as failed.
 * @GWY_MODULE_ERROR_OVERRIDEN: The module was overriden by a higher-priority
 *                              source.
 * @GWY_MODULE_ERROR_OPEN: Opening the module with dlopen() (or a similar
 *                         mechanism) failed.  This includes cases when the
 *                         module provides an initialisation function and this
 *                         function returns an error.
 * @GWY_MODULE_ERROR_INFO: Module info (or module library info) was not found
 *                         in the file or it was %NULL.
 * @GWY_MODULE_ERROR_ABI: Module ABI version mismatch.
 * @GWY_MODULE_ERROR_TYPES: Module provided inconsistent information about
 *                          extension types.
 * @GWY_MODULE_ERROR_METADATA: Module metadata (author, copyright, ...) are
 *                             missing or invalid.
 * @GWY_MODULE_ERROR_DUPLICATE_TYPE: Module attempted to register types that
 *                                   already exist.
 * @GWY_MODULE_ERROR_TYPE_NAME: Extension type name is not valid.
 * @GWY_MODULE_ERROR_GET_TYPE: Extension type get-type functions is not valid.
 *
 * Error codes for module loading and extension type registration.
 **/

/**
 * GwyGetTypeFunc:
 *
 * Type of get-type function for extension types.
 *
 * The function signature is the same as for all other functions providing a
 * #GType within GLib.
 *
 * Returns: The type.
 **/

/**
 * GwyModuleProvidedType:
 * @name: Type name in CamelCase, e.g. "GwyExtHappyUnicorn".
 * @get_type: Function to obtain the #GType for the type.
 *
 * Record defining one extension type provided by a module.
 **/

/**
 * GwyModuleLibraryRecord:
 * @name: Module name.  Exactly the same string as the identifier provided to
 *        %GWY_DEFINE_MODULE of this module.
 * @info: Module information.
 *
 * Type of module record in a module library.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
