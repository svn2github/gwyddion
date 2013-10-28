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
#include "libgwy/macros.h"
#include "libgwy/strfuncs.h"
#include "libgwy/object-utils.h"
#include "libgwyapp/module.h"

#define MODULE_INFO_SYMBOL "__gwy_module_info"

// 1. Find modules.  Create hash table of modules, set module prioritity and
//    order within the priority group.
//    Ignore modules we have already seen using @modules.

// FIXME: We should log ALL module registration failures separately, including
// from gwy_module_provide_type() and similar functions which may fail
// non-fatally (overriden class).  This should be available for later
// inspection.

typedef struct {
    const GwyModuleInfo *module_info;
    gchar *path;
    GQuark qname;
    GQuark library_qpath;
    gulong group_priority;
    gulong module_priority;
    gboolean did_type_registration;
} ExtModuleInfo;

typedef struct {
    GQuark qname;
    GQuark module_qname;
    GType type;
} ExtTypeInfo;

static GQuark       queue_one_module             (const gchar *filename,
                                                  GError **error);
static gboolean     queue_one_module_with_name   (const gchar *filename,
                                                  GQuark qname,
                                                  GError **error);
static gboolean     err_unset_metadata           (GError **error,
                                                  const gchar *metadata,
                                                  const gchar *name,
                                                  const gchar *filename);
static void         log_module_failure           (GError *error,
                                                  const gchar *modname,
                                                  const gchar *filename);
static gchar*       module_name_from_path        (const gchar *filename,
                                                  GError **error);
static GPtrArray*   gather_unregistered_modules  (void);
static gint         compare_unregistered_modinfo (gconstpointer a,
                                                  gconstpointer b);
static gboolean     register_one_type            (const GwyModuleProvidedType *mptype,
                                                  GQuark module_qname,
                                                  GError **error);
static GHashTable*  ensure_module_table          (void);
static void         module_registration_info_free(gpointer p);
static GHashTable*  ensure_type_table            (void);
static void         type_registration_info_free  (gpointer p);
static const gchar* module_source_string         (const ExtModuleInfo *modinfo);

static guint group_priority = 0;
static guint module_priority = 0;

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
 * @errorlist: 
 *
 * .
 *
 * Returns: 
 **/
guint
gwy_register_modules(GwyErrorList **errorlist)
{
    return 0;
}

/**
 * gwy_module_load:
 * @filename: Absolute file name of the module file to load.
 * @error: Location to store possible error.
 *
 * .
 *
 * Returns: 
 **/
const GwyModuleInfo*
gwy_module_load(const gchar *filename,
                GError **error)
{
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
 *
 * .
 *
 * Returns: 
 **/
guint
gwy_module_load_library(const gchar *filename,
                        GwyErrorList **errorlist)
{

    // TODO
    return 0;
}

/**
 * gwy_module_load_directory:
 * @path:
 * @errorlist: (allow-none):
 *             Location to store possible errors.
 *
 * .
 *
 * Returns: 
 **/
guint
gwy_module_load_directory(const gchar *path,
                          GwyErrorList **errorlist)
{

    // TODO
    return 0;
}

/**
 * gwy_module_register_types:
 * @errorlist: (allow-none):
 *             Location to store possible errors.
 *
 * Registers extension types from all modules that were loaded but their types
 * were not registered yet.
 *
 * Returns: The number of modules processed.
 **/
guint
gwy_module_register_types(GwyErrorList **errorlist)
{
    GPtrArray *todo_list = gather_unregistered_modules();
    if (!todo_list)
        return 0;

    for (guint i = 0; i < todo_list->len; i++) {
        ExtModuleInfo *modinfo = g_ptr_array_index(todo_list, i);
        const GwyModuleInfo *module_info = modinfo->module_info;
        for (guint j = 0; j < module_info->ntypes; j++) {
            GError *error = NULL;
            if (!register_one_type(module_info->types + j,
                                   modinfo->qname,
                                   &error)) {
                // TODO: log the error somewhere
                gwy_error_list_propagate(errorlist, error);
            }
        }
        modinfo->did_type_registration = TRUE;
    }

    guint count = todo_list->len;
    g_ptr_array_free(todo_list, TRUE);

    return count;
}

static GQuark
queue_one_module(const gchar *filename,
                 GError **error)
{
    GError *err = NULL;
    gchar *modname;

    if (!(modname = module_name_from_path(filename, &err))) {
        log_module_failure(err, "?", filename);
        g_propagate_error(error, err);
        return 0;
    }

    GQuark qname = g_quark_from_string(modname);
    g_free(modname);

    if (!queue_one_module_with_name(filename, qname, &err)) {
        log_module_failure(err, modname, filename);
        g_propagate_error(error, err);
        return 0;
    }

    return qname;
}

static gboolean
queue_one_module_with_name(const gchar *filename,
                           GQuark qname,
                           GError **error)
{
    GHashTable *modules = ensure_module_table();
    ExtModuleInfo *modinfo = NULL;
    gpointer key = GUINT_TO_POINTER(qname);

    if ((modinfo = g_hash_table_lookup(modules, key))) {
        g_set_error(error, GWY_MODULE_ERROR, GWY_MODULE_ERROR_DUPLICATE_MODULE,
                    // TRANSLATORS: Error message.
                    _("Module ‘%s’ was already registered from ‘%s’."),
                    g_quark_to_string(qname), module_source_string(modinfo));
        return FALSE;
    }

    // XXX: Somewhere here we had code for avoiding attempt to load pygyw if
    // Python is not available.

    GModule *mod = g_module_open(filename, G_MODULE_BIND_LAZY);
    if (!mod) {
        g_set_error(error, GWY_MODULE_ERROR, GWY_MODULE_ERROR_OPEN,
                    // TRANSLATORS: Error message.
                    "Cannot open module ‘%s’: %s", filename, g_module_error());
        return FALSE;
    }

    gchar *symbol = g_strconcat(MODULE_INFO_SYMBOL, "_",
                                g_quark_to_string(qname), NULL);
    const GwyModuleInfo *module_info = NULL;
    if (!g_module_symbol(mod, symbol, (gpointer)&module_info) || !module_info) {
        g_set_error(error, GWY_MODULE_ERROR, GWY_MODULE_ERROR_INFO,
                    // TRANSLATORS: Error message.
                    _("Cannot find module info ‘%s’ in ‘%s’."),
                    symbol, filename);
        goto fail;
    }

    if (module_info->abi_version != GWY_MODULE_ABI_VERSION) {
        g_set_error(error, GWY_MODULE_ERROR, GWY_MODULE_ERROR_ABI,
                    // TRANSLATORS: Error message.
                    _("Module ABI version %u of ‘%s’ differs from %u."),
                    module_info->abi_version, filename, GWY_MODULE_ABI_VERSION);
        goto fail;
    }

    if (module_info->ntypes && !module_info->types) {
        g_set_error(error, GWY_MODULE_ERROR, GWY_MODULE_ERROR_TYPES,
                    // TRANSLATORS: Error message.
                    _("Module ‘%s’ declares non-zero number of types but it "
                      "does not provide any."),
                    filename);
        goto fail;
    }

    if (err_unset_metadata(error, module_info->author, "author",
                           filename)
        || err_unset_metadata(error, module_info->version, "version",
                              filename)
        || err_unset_metadata(error, module_info->copyright, "copyright",
                              filename)
        || err_unset_metadata(error, module_info->date, "date",
                              filename)
        || err_unset_metadata(error, module_info->description, "description",
                              filename))
        goto fail;


    modinfo = g_slice_new0(ExtModuleInfo);
    modinfo->module_info = module_info;
    modinfo->path = g_strdup(filename);
    modinfo->qname = qname;
    modinfo->group_priority = group_priority;
    modinfo->module_priority = module_priority++;
    g_hash_table_insert(modules, key, modinfo);
    g_module_make_resident(mod);

    return TRUE;

fail:
    g_free(symbol);
    if (!g_module_close(mod)) {
        g_critical("Cannot unload module ‘%s’: %s", filename, g_module_error());
    }
    return FALSE;
}

static gboolean
err_unset_metadata(GError **error,
                   const gchar *metadata,
                   const gchar *name,
                   const gchar *filename)
{
    if (metadata && *metadata)
        return FALSE;

    g_set_error(error, GWY_MODULE_ERROR, GWY_MODULE_ERROR_METADATA,
                _("Module ‘%s’ metadata does not define field ‘%s’."),
                filename, name);
    return TRUE;
}

static void
log_module_failure(GError *error,
                   const gchar *modname,
                   const gchar *filename)
{
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

    if (!gwy_ascii_strisident(modname, "_", NULL)) {
        g_set_error(error, GWY_MODULE_ERROR, GWY_MODULE_ERROR_MODULE_NAME,
                    // TRANSLATORS: Error message.
                    _("Module name ‘%s’ is not a valid identifier."),
                    modname);
        g_free(modname);
        return NULL;
    }

    return modname;
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
        ExtModuleInfo *modinfo = (ExtModuleInfo*)value;
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
    const ExtModuleInfo **pmodinfoa = (const ExtModuleInfo**)a;
    const ExtModuleInfo **pmodinfob = (const ExtModuleInfo**)b;
    const ExtModuleInfo *modinfoa = *pmodinfoa, *modinfob = *pmodinfob;

    // Group priority is reversed, we start from system modules.  Overriding
    // has been already performed.
    if (modinfoa->group_priority > modinfob->group_priority)
        return -1;
    if (modinfoa->group_priority < modinfob->group_priority)
        return 1;

    // Within a group, register the modules as we found them.
    if (modinfoa->module_priority < modinfob->module_priority)
        return -1;
    if (modinfoa->module_priority > modinfob->module_priority)
        return 1;

    return 0;
}

static gboolean
register_one_type(const GwyModuleProvidedType *mptype,
                  GQuark module_qname,
                  GError **error)
{
    if (!mptype->get_type) {
        g_set_error(error, GWY_MODULE_ERROR, GWY_MODULE_ERROR_GET_TYPE,
                    // TRANSLATORS: Error message.
                    _("Module ‘%s’ supplied a NULL get-type function."),
                    g_quark_to_string(module_qname));
        return FALSE;
    }

    if (!mptype->name) {
        g_set_error(error, GWY_MODULE_ERROR, GWY_MODULE_ERROR_TYPE_NAME,
                    // TRANSLATORS: Error message.
                    _("NULL type name was supplied by module ‘%s’."),
                    g_quark_to_string(module_qname));
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
    ExtTypeInfo *typeinfo = NULL;
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

    typeinfo = g_slice_new(ExtTypeInfo);
    typeinfo->qname = qname;
    typeinfo->module_qname = module_qname;
    typeinfo->type = type;
    g_hash_table_insert(types, key, typeinfo);

    return TRUE;
}

static GHashTable*
ensure_module_table(void)
{
    static GHashTable *modules = NULL;

    if (G_LIKELY(modules))
        return modules;

    modules = g_hash_table_new_full(NULL, NULL,
                                    NULL, module_registration_info_free);
    return modules;
}

static void
module_registration_info_free(gpointer p)
{
    ExtModuleInfo *modinfo = (ExtModuleInfo*)p;
    GWY_FREE(modinfo->path);
    g_slice_free(ExtModuleInfo, modinfo);
}

static GHashTable*
ensure_type_table(void)
{
    static GHashTable *types = NULL;

    if (G_LIKELY(types))
        return types;

    types = g_hash_table_new_full(NULL, NULL,
                                  NULL, type_registration_info_free);
    return types;
}

static void
type_registration_info_free(gpointer p)
{
    g_slice_free(ExtTypeInfo, p);
}

static const gchar*
module_source_string(const ExtModuleInfo *modinfo)
{
    if (modinfo->library_qpath)
        return g_quark_to_string(modinfo->library_qpath);
    return modinfo->path;
}

/************************** Documentation ****************************/

/**
 * SECTION: module
 * @section_id: libgwyapp-module
 * @title: Module registration
 * @short_description: Loading and registration of modules
 *
 * Usually, modules are loaded and their extension types registered using
 * gwy_register_modules().
 *
 * Loading of modules supports overriding of system modules with user-supplied
 * modules.  A simple resolution mechanism is used.  All modules are found.
 * The search should start with the highest priority sources (user modules) and
 * continue to lowest priority modules (system modules).  Each call to
 * gwy_module_load(), gwy_module_load_library() or gwy_module_load_directory()
 * essentially creates a priority group.  Multiple modules of the same name
 * within one group lead to undefined behaviour, i.e. an arbitrary one may be
 * loaded and the other rejected.  In the case of multiple occurences of module
 * of the same name, the first seen source has the highest priority and
 * overrides the module seen later.
 *
 * Extension classes are then actually registered using
 * gwy_module_register_types() in the order they would be seen if the
 * registration started from lowest priority sources (system modules).
 * However, overriding takes place.
 *
 * Occasionally you may want to run some code in order to construct the
 * #GwyModuleInfo structure dynamically.  This can be for instance the case of
 * a file module which itself needs to query the capabilities of some library.
 * The way to achieve this is define GLib a module initialisation function.
 * See #GModuleCheckInit for a description.  Such function can also return an
 * error which is a way modules can prevent themselves from being loaded when
 * they decide they cannot function correctly.  It may or may not be useful to
 * also provide a GLib module unload function, for which see #GModuleUnload.
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
 * @mod_info_list: Array of module info structures, terminated with
 *                 %GWY_MODULE_INFO_SENTINEL, to be returned as the list of
 *                 modules in the library.
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
 * %GWY_MODULE_BUILDING_LIBRARY should be defined to indicate this.  Invididual
 * module query functions defined with %GWY_DEFINE_MODULE are then linked as
 * internal to the library.  Only one library-level symbol, defined with
 * %GWY_DEFINE_MODULE_LIBRARY, is exported.
 **/

/**
 * GwyModuleInfo:
 * @abi_version: Gwyddion module ABI version, should be always
 *               #GWY_MODULE_ABI_VERSION.
 * @register_func: Module registration function (the function run by Gwyddion
 *                 module system, actually registering particular module
 *                 features).
 * @blurb: Description of the module purpose.
 * @author: Module author(s).
 * @version: Module version.
 * @copyright: Who has copyright on this module.
 * @date: Date (year).
 *
 * Module information provided to GWY_DEFINE_MODULE().
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
