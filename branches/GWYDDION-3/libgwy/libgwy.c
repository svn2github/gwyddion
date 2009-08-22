/*
 *  $Id$
 *  Copyright (C) 2009 David Necas (Yeti).
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

#include "libgwy/libgwy.h"
#include "libgwy/libgwy-aliases.h"

static gpointer init_types(G_GNUC_UNUSED gpointer arg);

/**
 * gwy_type_init:
 *
 * Forces registration of all serializable libgwy types.
 *
 * It calls g_type_init() first to make sure GLib object system is initialized.
 *
 * Calling this function is not necessary to use libgwy, except perhaps if you
 * implement custom deserializers.  It is safe to call this function more than
 * once, subsequent calls are no-op.  It is safe to call this function from
 * more threads if GLib thread support is initialized.
 **/
void
gwy_type_init(void)
{
    static GOnce types_initialized = G_ONCE_INIT;
    g_once(&types_initialized, init_types, NULL);
}

static gpointer
init_types(G_GNUC_UNUSED gpointer arg)
{
    g_type_init();

    g_type_class_peek(GWY_TYPE_SERIALIZABLE);
    g_type_class_peek(GWY_TYPE_UNIT);
    g_type_class_peek(GWY_TYPE_CONTAINER);
    g_type_class_peek(GWY_TYPE_INVENTORY);

    return NULL;
}

/*
 * Finding directiores.
 *
 * We deliberately ignore XDG which unfortunately means ignoring most GLib
 * provided functions.  Rationale:
 * - Doing things the Unix way has preference to some piece of PDF.
 * - The difference between configuration and data is artificial.
 * - How do I get a search path for loadable modules from the Base Directory
 *   Specification?  And if I cannot, how am I supposed to be consistent then?
 * - It does not seem that openssh, bash or vim are going to follow some
 *   XDG_THINGS_IN_WEIRD_PLACES variables.
 *
 * 0. OVERVIEW
 * We have the following primary tasks:
 *
 * 1. Find self.  This means the directory this library was actually loaded
 * from.
 *
 * 2. Find the user configuration directory.  This is in home, or in other
 * place if we are on Win32 and other weird systems.
 *
 * 3. Find all directories where we should look for "foo".  These can be of
 * the following kinds:
 *
 *   (a) Related to the system directory, defined in point 1.
 *
 *   (b) Related to the user directory, defined in point 2.
 *
 *   (c) Arbitrary, unrelated to anything.
 *
 * 1. SEARCHING SELF
 * The following can be tried:
 * - environment variables GWYDDION_DATADIR, GWYDDION_LIBDIR,
 *   GWYDDION_LOCALE_DIR
 * - /proc/self/map (only on Linux)
 * - g_win32_get_package_installation_directory_of_module() (only on Win32)
 * - argv[0] and realpath, possibly together with PATH
 * - compile-time prefix
 *
 * These possibilities are tried in the following order and the first that
 * results in finding self is used.  The order is given by decreasing
 * reliability, except of course GWYDDION_ROOTDIR, which is not very reliable
 * but the user it always right.
 *
 * If none of them produces a match, fail.  This means that a fool-proof user
 * of libgwy must check whether gwy_data_directory(), gwy_lib_directory(),
 * gwy_locale_directory() return non-NULL.
 *
 * 2. SEARCHING USER DIRECTORY
 * The following can be tried:
 * - environment variable HOME
 * - g_get_home_dir()
 *
 * If neither leads to a suitable directory, fail.  An application should give
 * a warning if it find that gwy_user_directory() returns NULL and avoid
 * saving configuration.  It needs to check whether it actualy succeeded
 * anyway.
 *
 * 3. GATHERING ALL DIRECTORIES
 * The following can be tried:
 * - specific environment variables with modules, data, etc. paths -- not
 *   implemented (c)
 * - user directory (b)
 * - our installation directory (a)
 * - default system directories (such as /usr) (c)
 *
 * All are concatenated into one list in given order.  If some directory
 * happens to occur multiple times the first occurrence counts.
 */

#define __GWY_LIBGWY_C__
#include "libgwy/libgwy-aliases.c"

/**
 * SECTION: libgwy
 * @title: libgwy
 * @short_description: Library-level functions of libgwy
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
