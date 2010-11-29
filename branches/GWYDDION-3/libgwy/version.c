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

#include "config.h"
#include "libgwy/version.h"

/**
 * gwy_version_major:
 *
 * Gets the major version of libgwy.
 *
 * If the version is 2.99.7.20090604, this function returns 2.
 *
 * Returns: The major version.
 **/
gint
gwy_version_major(void)
{
    return GWY_VERSION_MAJOR;
}

/**
 * gwy_version_minor:
 *
 * Gets the minor version of libgwy.
 *
 * If the version is 2.99.7.20090604, this function returns 99.
 *
 * Returns: The minor version.
 **/
gint
gwy_version_minor(void)
{
    return GWY_VERSION_MINOR;
}

/**
 * gwy_version_string:
 *
 * Gets the full libgwy version as a string.
 *
 * If the version is 2.99.7.20090604, this function returns
 * <literal>"2.99.7.20090604"</literal>.
 *
 * This is the only method to get finer version information than major.minor.
 * However, only development versions use finer versioning than major.minor
 * therefore a module or app requiring such information is probably broken
 * anyway.  A meaningful use is to advertise the version of Gwyddion your app
 * runs with.
 *
 * Returns: The full version as a constant string.
 **/
const gchar*
gwy_version_string(void)
{
    return GWY_VERSION_STRING;
}


/**
 * SECTION: version
 * @title: version
 * @short_description: Version information
 *
 * Macros like %GWY_VERSION_MAJOR can be used for compile-time version checks,
 * that is they tell what version a module or app is being compiled or was
 * compiled with.
 *
 * On the other hand functions like gwy_version_major() can be used to run-time
 * version checks and they tell what version a module or app was linked or
 * is running with.
 **/

/**
 * GWY_VERSION_MAJOR:
 *
 * Expands to the major version of libgwy as a number.
 *
 * If the version is 2.99.7.20090604, this macro is defined as 2.
 **/

/**
 * GWY_VERSION_MINOR:
 *
 * Expands to the minor version of libgwy as a number.
 *
 * If the version is 2.99.7.20090604, this macro is defined as 99.
 **/

/**
 * GWY_VERSION_STRING:
 *
 * Expands to the full libgwy version as a string.
 *
 * If the version is 2.99.7.20090604, this macro is defined as
 * <literal>"2.99.7.20090604"</literal>.
 *
 * See gwy_version_string() for caveats.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
