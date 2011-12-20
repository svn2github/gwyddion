/*
 *  $Id$
 *  Copyright (C) 2010-2011 David Nečas (Yeti).
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

#include "testlibgwy.h"

/***************************************************************************
 *
 * Resources
 *
 ***************************************************************************/

void
resource_check_file(GwyResource *resource,
                    const gchar *filename)
{
    gchar *res_filename = NULL;
    g_object_get(resource, "file-name", &res_filename, NULL);
    GFile *gfile = g_file_new_for_path(filename);
    GFile *res_gfile = g_file_new_for_path(res_filename);
    g_assert(g_file_equal(gfile, res_gfile));
    g_object_unref(gfile);
    g_object_unref(res_gfile);
    g_free(res_filename);
}

void
test_resource_finalize(void)
{
    gwy_resources_finalize();
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
