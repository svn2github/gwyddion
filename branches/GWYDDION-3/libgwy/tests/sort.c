/*
 *  $Id$
 *  Copyright (C) 2009 David Nečas (Yeti).
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
 * Math sorting
 *
 ***************************************************************************/

/* Return %TRUE if @array is ordered. */
static gboolean
test_sort_is_strictly_ordered(const gdouble *array, gsize n)
{
    gsize i;

    for (i = 1; i < n; i++, array++) {
        if (array[0] >= array[1])
            return FALSE;
    }
    return TRUE;
}

/* Return %TRUE if @array is ordered and its items correspond to @orig_array
 * items with permutations given by @index_array. */
static gboolean
test_sort_is_ordered_with_index(const gdouble *array, const guint *index_array,
                                const gdouble *orig_array, gsize n)
{
    gsize i;

    for (i = 0; i < n; i++) {
        if (index_array[i] >= n)
            return FALSE;
        if (array[i] != orig_array[index_array[i]])
            return FALSE;
    }
    return TRUE;
}

void
test_math_sort(void)
{
    gsize nmin = 0, nmax = 65536;

    if (g_test_quick())
        nmax = 8192;

    gdouble *array = g_new(gdouble, nmax);
    gdouble *orig_array = g_new(gdouble, nmax);
    guint *index_array = g_new(guint, nmax);
    for (gsize n = nmin; n < nmax; n = 7*n/6 + 1) {
        for (gsize i = 0; i < n; i++)
            orig_array[i] = sin(n/G_SQRT2 + 1.618*i);

        memcpy(array, orig_array, n*sizeof(gdouble));
        gwy_math_sort(array, NULL, n);
        g_assert(test_sort_is_strictly_ordered(array, n));

        memcpy(array, orig_array, n*sizeof(gdouble));
        for (gsize i = 0; i < n; i++)
            index_array[i] = i;
        gwy_math_sort(array, index_array, n);
        g_assert(test_sort_is_strictly_ordered(array, n));
        g_assert(test_sort_is_ordered_with_index(array, index_array,
                                                 orig_array, n));
    }
    g_free(index_array);
    g_free(orig_array);
    g_free(array);
}

void
test_math_median(void)
{
    guint nmax = 1000;
    gdouble *data = g_new(gdouble, nmax);
    GRand *rng = g_rand_new_with_seed(42);

    for (guint n = 1; n < nmax; n++) {
        for (guint i = 0; i < n; i++)
            data[i] = i;
        for (guint i = 0; i < n; i++) {
            guint jj1 = g_rand_int_range(rng, 0, n);
            guint jj2 = g_rand_int_range(rng, 0, n);
            GWY_SWAP(gdouble, data[jj1], data[jj2]);
        }
        gdouble med = gwy_math_median(data, n);
        g_assert_cmpfloat(med, ==, (n/2));
    }
    g_rand_free(rng);
    g_free(data);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
