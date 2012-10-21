/*
 *  $Id$
 *  Copyright (C) 2009,2012 David Nečas (Yeti).
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
 * Error lists
 *
 ***************************************************************************/

void
assert_error_list(GwyErrorList *error_list,
                  GwyErrorList *expected_errors)
{
    //dump_error_list(error_list);
    g_assert_cmpuint(g_slist_length(error_list),
                     ==,
                     g_slist_length(expected_errors));
    guint n = g_slist_length(expected_errors);
    for (guint i = 0; i < n; i++) {
        GError *expected_error = g_slist_nth_data(expected_errors, i);
        GError *error = g_slist_nth_data(error_list, i);
        g_assert_error(error, expected_error->domain, expected_error->code);
        g_assert_cmpstr(error->message, ==, expected_error->message);
    }
}

void
test_error_list(void)
{
    GwyErrorList *errlist = NULL;
    GError *err;

    /* A simple error. */
    gwy_error_list_add(&errlist, GWY_PACK_ERROR, GWY_PACK_ERROR_FORMAT,
                       "Just testing...");
    g_assert_cmpuint(g_slist_length(errlist), ==, 1);

    /* Several errors. */
    gwy_error_list_add(&errlist, GWY_PACK_ERROR, GWY_PACK_ERROR_FORMAT,
                       "Just testing %d...", 2);
    g_assert_cmpuint(g_slist_length(errlist), ==, 2);
    /* The latter must be on top. */
    err = errlist->data;
    g_assert_cmpuint(err->domain, ==, GWY_PACK_ERROR);
    g_assert_cmpuint(err->code, ==, GWY_PACK_ERROR_FORMAT);
    g_assert_cmpstr(err->message, ==, "Just testing 2...");

    /* Destruction. */
    gwy_error_list_clear(&errlist);
    g_assert_cmpuint(g_slist_length(errlist), ==, 0);

    /* Propagation */
    gwy_error_list_propagate(&errlist, NULL);
    g_assert_cmpuint(g_slist_length(errlist), ==, 0);
    err = g_error_new(GWY_PACK_ERROR, GWY_PACK_ERROR_FORMAT,
                      "Just testing 3...");
    gwy_error_list_propagate(&errlist, err);
    g_assert_cmpuint(g_slist_length(errlist), ==, 1);
    gwy_error_list_clear(&errlist);

    /* Ignoring errors. */
    gwy_error_list_add(NULL, GWY_PACK_ERROR, GWY_PACK_ERROR_FORMAT,
                       "Ignored error");
    err = g_error_new(GWY_PACK_ERROR, GWY_PACK_ERROR_FORMAT,
                      "Ignored error");
    gwy_error_list_propagate(NULL, err);
    gwy_error_list_clear(NULL);
}

void
dump_error_list(GwyErrorList *error_list)
{
    guint i = 0;
    for (GSList *l = error_list; l; l = g_slist_next(l), i++) {
        g_printerr("error%u: %s\n", i, ((GError*)l->data)->message);
    }
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
