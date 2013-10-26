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
#include "libgwy/macros.h"
#include "libgwy/object-utils.h"
#include "libgwyapp/module.h"

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

/************************** Documentation ****************************/

/**
 * SECTION: module
 * @section_id: libgwyapp-module
 * @title: Module registration
 * @short_description: Loading and registration of modules
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
 * provides and, possibly, performs other tasks.
 *
 * Returns: Whether the registration succeeded.  When it returns %FALSE, the
 *          module loading is considered to fail.  The function must set
 *          the error in this case.  Failed modules are not unloaded.
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
