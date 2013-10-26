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

// FIXME: We should log ALL module registration failures separately, including
// from gwy_module_provide_type() and similar functions which may fail
// non-fatally (overriden class).  This should be available for later
// inspection.

// FIXME: If a module or module library does not make ANY successful type
// registration call consider it failed and remove it too.  Like Gwyddion 2.

static gboolean check_type_name   (const gchar *name,
                                   GError **error);
static void     note_down_one_type(const gchar *name,
                                   GwyGetTypeFunc gettype);

static GHashTable *modules = NULL;
static GString *current_module = NULL;

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

#if 0
static const GwyModuleInfo*
gwy_module_do_register_module(const gchar *filename,
                              GHashTable *mods,
                              GError **error)
{
    GModule *mod;
    gboolean ok;
    _GwyModuleInfoInternal *iinfo;
    GwyModuleInfo *mod_info = NULL;
    GwyModuleQueryFunc query;
    gchar *modname, *s;
    GError *err = NULL;

    s = g_path_get_basename(filename);
    modname = g_ascii_strdown(s, -1);
    g_free(s);
    /* FIXME: On normal platforms module names have an extension, but if
     * it doesn't, just get over it.  This can happen only with explicit
     * gwy_module_register_module() as gwy_load_modules_in_dir() accepts
     * only sane names. */
    if ((s = strchr(modname, '.')))
        *s = '\0';
    if (!*modname) {
        g_set_error(&err, GWY_MODULE_ERROR, GWY_MODULE_ERROR_NAME,
                    "Module name is empty");
        gwy_module_register_fail(err, error, modname, filename);
        g_free(modname);
        return NULL;
    }

    if (!gwy_strisident(modname, "_-", NULL))
        g_warning("Module name `%s' is not a valid identifier. "
                  "It may be rejected in future.", modname);

    if (g_hash_table_lookup(mods, modname)) {
        g_set_error(&err, GWY_MODULE_ERROR, GWY_MODULE_ERROR_DUPLICATE,
                    "Module was already registered");
        gwy_module_register_fail(err, error, modname, filename);
        g_free(modname);
        return NULL;
    }

    if (gwy_strequal(modname, "pygwy") && !check_python_availability()) {
        g_set_error(&err, GWY_MODULE_ERROR, GWY_MODULE_ERROR_OPEN,
                    "Avoiding to register pygwy if Python is unavailable.");
        gwy_module_register_fail(err, error, modname, filename);
        g_free(modname);
        return NULL;
    }

    gwy_debug("Trying to load module `%s' from file `%s'.", modname, filename);
    mod = g_module_open(filename, G_MODULE_BIND_LAZY);

    if (!mod) {
        g_set_error(&err, GWY_MODULE_ERROR, GWY_MODULE_ERROR_OPEN,
                    "Cannot open module: %s", g_module_error());
        gwy_module_register_fail(err, error, modname, filename);
        g_free(modname);
        return NULL;
    }
    gwy_debug("Module loaded successfully as `%s'.", g_module_name(mod));

    /* Sanity checks on the module before registration is attempted. */
    ok = TRUE;
    currenly_registered_module = modname;
    if (!g_module_symbol(mod, "_gwy_module_query", (gpointer)&query)
        || !query) {
        g_set_error(&err, GWY_MODULE_ERROR, GWY_MODULE_ERROR_QUERY,
                    "Module contains no query function");
        gwy_module_register_fail(err, error, modname, filename);
        ok = FALSE;
    }

    if (ok) {
        mod_info = query();
        if (!mod_info) {
            g_set_error(&err, GWY_MODULE_ERROR, GWY_MODULE_ERROR_INFO,
                        "Module info is NULL");
            gwy_module_register_fail(err, error, modname, filename);
            ok = FALSE;
        }
    }

    if (ok) {
        ok = mod_info->abi_version == GWY_MODULE_ABI_VERSION;
        if (!ok) {
            g_set_error(&err, GWY_MODULE_ERROR, GWY_MODULE_ERROR_ABI,
                        "Module ABI version %d differs from %d",
                        mod_info->abi_version, GWY_MODULE_ABI_VERSION);
            gwy_module_register_fail(err, error, modname, filename);
        }
    }

    if (ok) {
        ok = mod_info->register_func
                && mod_info->blurb && *mod_info->blurb
                && mod_info->author && *mod_info->author
                && mod_info->version && *mod_info->version
                && mod_info->copyright && *mod_info->copyright
                && mod_info->date && *mod_info->date;
        if (!ok) {
            g_set_error(&err, GWY_MODULE_ERROR, GWY_MODULE_ERROR_ABI,
                        "Module info has missing/invalid fields");
            gwy_module_register_fail(err, error, modname, filename);
        }
    }

    if (ok) {
        iinfo = g_new(_GwyModuleInfoInternal, 1);
        iinfo->mod_info = mod_info;
        iinfo->name = modname;
        iinfo->file = g_strdup(filename);
        iinfo->loaded = TRUE;
        iinfo->funcs = NULL;
        g_hash_table_insert(mods, (gpointer)iinfo->name, iinfo);
        if (!(ok = mod_info->register_func())) {
            g_set_error(&err, GWY_MODULE_ERROR, GWY_MODULE_ERROR_REGISTER,
                        "Module feature registration failed");
            gwy_module_register_fail(err, error, modname, filename);
        }
        if (ok && !iinfo->funcs) {
            g_set_error(&err, GWY_MODULE_ERROR, GWY_MODULE_ERROR_REGISTER,
                        "Module did not register any function");
            gwy_module_register_fail(err, error, modname, filename);
            ok = FALSE;
        }

        if (ok)
            gwy_module_pedantic_check(iinfo);
        else {
            gwy_module_get_rid_of(iinfo->name);
            modname = NULL;
        }
    }

    if (ok) {
        gwy_debug("Making module `%s' resident.", filename);
        g_module_make_resident(mod);
    }
    else {
        if (!g_module_close(mod))
            g_critical("Cannot unload module `%s': %s",
                       filename, g_module_error());
        g_free(modname);
    }
    currenly_registered_module = NULL;

    return ok ? mod_info : NULL;
}
#endif

/**
 * gwy_module_provide_type:
 * @name: Type name in GLib type system.
 * @gettype: Get-type function for this type.
 * @error: Location to store possible error.
 *
 * Provides one extension type from a module.
 *
 * The name must be of the form "GwyExtSomething" where "GwyExt" is the fixed
 * part and "Something" is the CamelCase part identifying the specific type.
 * Modules within one priority group must not attempt to register the same
 * type twice.
 *
 * Returns: %TRUE if the type name seems to be valid.
 **/
gboolean
gwy_module_provide_type(const gchar *name,
                        GwyGetTypeFunc gettype,
                        GError **error)
{
    g_return_val_if_fail(name, FALSE);
    g_return_val_if_fail(gettype, FALSE);
    if (!check_type_name(name, error))
        return FALSE;
    // TODO: Check whether we have already seen this type.
    // TODO: Do not permit attempts to register the same type multiple times
    //       within one priority group.
    note_down_one_type(name, gettype);
    return TRUE;
}

/**
 * gwy_module_provide:
 * @errorlist: (allow-none):
 *             Location to store possible errors.
 * @name: Name of the first extension type to register.
 * @...: Get-type function for the first type, followed by name of the second
 *       type and its get-type function, etc.  The argument list is terminated
 *       with a %NULL.
 *
 * Provides several extension types from a module.
 *
 * See gwy_module_provide_type() for details.
 *
 * Returns: The number of accepted types.
 **/
guint
gwy_module_provide(GwyErrorList **errorlist,
                   const gchar *name,
                   ...)
{
    if (!name)
        return 0;

    guint goodcount = 0;
    va_list ap;
    va_start(ap, name);
    do {
        GError *error = NULL;
        GwyGetTypeFunc gettype = va_arg(ap, GwyGetTypeFunc);
        g_assert(gettype);
        if (check_type_name(name, &error)) {
            goodcount++;
            note_down_one_type(name, gettype);
        }
        else {
            gwy_error_list_propagate(errorlist, error);
        }
        name = va_arg(ap, const gchar*);
    } while (name);

    va_end(ap);

    return goodcount;
}

/**
 * gwy_module_providev:
 * @types: (array length=ntypes):
 *         Array of extension types.
 * @ntypes: Number of items in @types.
 * @errorlist: (allow-none):
 *             Location to store possible errors.
 *
 * Provides several extension types given in an array from a module.
 *
 * Returns: The number of accepted types.
 **/
guint
gwy_module_providev(const GwyModuleProvidedType *types,
                    guint ntypes,
                    GwyErrorList **errorlist)
{
    if (!ntypes)
        return 0;

    g_return_val_if_fail(types, 0);
    for (guint i = 0; i < ntypes; i++) {
        const GwyModuleProvidedType *provtype = types + i;
        g_return_val_if_fail(provtype->name, 0);
        g_return_val_if_fail(provtype->get_type, 0);
    }

    guint goodcount = 0;
    for (guint i = 0; i < ntypes; i++) {
        const GwyModuleProvidedType *provtype = types + i;
        GError *error = NULL;
        if (check_type_name(provtype->name, &error)) {
            goodcount++;
            note_down_one_type(provtype->name, provtype->get_type);
        }
        else {
            gwy_error_list_propagate(errorlist, error);
        }
    }

    return goodcount;
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

    // TODO
    return NULL;
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
 *
 * .
 **/
void
gwy_module_register_types(void)
{

}

static gboolean
check_type_name(const gchar *name,
                GError **error)
{
    if (!gwy_ascii_strisident(name, NULL, NULL)) {
        g_set_error(error, GWY_MODULE_ERROR, GWY_MODULE_ERROR_NAME,
                    // TRANSLATORS: Error message.
                    _("Class name not a valid identifier."));
        return FALSE;
    }
    if (!g_str_has_prefix(name, "GwyExt") || !g_ascii_isupper(name[7])) {
        g_set_error(error, GWY_MODULE_ERROR, GWY_MODULE_ERROR_NAME,
                    // TRANSLATORS: Error message.
                    _("Class name does not have the form GwyExtSomething."));
        return FALSE;
    }

    return TRUE;
}

static void
note_down_one_type(const gchar *name,
                   GwyGetTypeFunc gettype)
{
}

/************************** Documentation ****************************/

/**
 * SECTION: module
 * @section_id: libgwyapp-module
 * @title: Module registration
 * @short_description: Loading and registration of modules
 *
 * Loading of modules supports overriding of system modules with user-supplied
 * modules.  A simple resolution mechanism is used.  Module registration
 * functions of all modules are run.  It should start with the highest priority
 * sources (user modules), and all extension classes are gathered.  Each call
 * to gwy_module_load(), gwy_module_load_library() or
 * gwy_module_load_directory() creates a priority group.
 *
 * In the case of multiply-defined classes the first seen source has the
 * highest priority and overrides the class seen later.  Extension classes are
 * then actually registered in the order they would be seen if the registration
 * started from lowest priority sources (system modules).  However, overriding
 * takes place.
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
 * @mod_info: The #GwyModuleInfo structure to be returned as the module info.
 * @name: Module name.
 *
 * Macro expanding to code necessary for registration of a module.
 *
 * The @name argument represents the module name. It must match the file name
 * of the module with any operating system dependend exensions removed and 
 * possible dashes converted to underscores.  After this transformation, the
 * module name must form a valid identifier.  It is strongly recommended to
 * avoid uppercase letters in the name.
 **/

/**
 * GWY_DEFINE_MODULE_LIBRARY:
 * @mod_info_list: Array of module query functions, terminated with %NULL,
 *                 to be returned as the list of modules in the library.
 *
 * Macro expanding to code necessary for registration of a module library.
 *
 * Module library is used for consolidation of a large numer of shared
 * dynamically loaded libraries to one.  This probably only makes sense within
 * Gwyddion itself, which may contain hundreds of modules.  Standalone modules
 * should use %GWY_DEFINE_MODULE.
 *
 * If modules are consolidated into a module library, preprocessor macro
 * %GWY_MODULE_BUILDING_LIBRARY should be defined to indicate this.  Invididual
 * module query functions are then linked as internal to the library, only one
 * library-level query function is exported.
 **/

/**
 * GwyModuleQueryFunc:
 *
 * Module query function type.
 *
 * The module query function is provided, under normal circumstances, by
 * macro %GWY_DEFINE_MODULE.
 *
 * Returns: The module information structure.
 **/

/**
 * GwyModuleRegisterFunc:
 *
 * Module registration function type.
 *
 * The module registration function registers extension classes the module
 * provides and, possibly, performs other tasks.  However, the execution of
 * this functions does not mean the classes provided by this module will be
 * actually registered as they can be overriden.
 *
 * If a module registration fails the function must set an error.  Failed
 * modules are unloaded and all classes they provided are ignored.
 *
 * Returns: Whether the module registration succeeded.  If it returns %FALSE,
 *          the module loading is considered to fail.
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
