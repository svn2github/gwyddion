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

// The test case declarations are separated from testlibgwy.h to avoid
// recompilation of everything when a new test is added.
#include "testlibgwy.h"
#include "test-list.h"
#include <stdlib.h>
#include <locale.h>
#include <glib/gstdio.h>

#ifdef HAVE_VALGRIND
#include <valgrind/valgrind.h>
#else
#define RUNNING_ON_VALGRIND 0
#endif

static void
remove_testdata(void)
{
    GFile *testdir = g_file_new_for_path(TEST_DATA_DIR);
    GFileEnumerator *enumerator
        = g_file_enumerate_children(testdir,
                                    G_FILE_ATTRIBUTE_STANDARD_TYPE ","
                                    G_FILE_ATTRIBUTE_STANDARD_NAME ",",
                                    G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                    NULL, NULL);
    if (enumerator) {
        GFileInfo *fileinfo;
        while ((fileinfo = g_file_enumerator_next_file(enumerator,
                                                       NULL, NULL))) {
            gchar *path = g_build_filename(TEST_DATA_DIR,
                                           g_file_info_get_name(fileinfo),
                                           NULL);
            if (g_file_info_get_file_type(fileinfo) != G_FILE_TYPE_REGULAR)
                g_printerr("Cannot remove %s: not a regular file.\n", path);
            else
                g_unlink(path);
            g_free(path);
            g_object_unref(fileinfo);
        }
        g_object_unref(enumerator);
    }
    g_rmdir(TEST_DATA_DIR);
}

/***************************************************************************
 *
 * Main
 *
 ***************************************************************************/

int
main(int argc, char *argv[])
{
    setenv("LC_NUMERIC", "C", TRUE);
    setlocale(LC_NUMERIC, "C");
    if (RUNNING_ON_VALGRIND) {
        setenv("G_SLICE", "always-malloc", TRUE);
        g_mem_gc_friendly = TRUE;
    }

    g_test_init(&argc, &argv, NULL);
    g_type_init();

#include "test-list.c"

    remove_testdata();
    g_mkdir(TEST_DATA_DIR, 0700);
    gint status = g_test_run();
    remove_testdata();

    // Good on Valgrind, but do it consistently always.
    fftw_forget_wisdom();

    return status;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
