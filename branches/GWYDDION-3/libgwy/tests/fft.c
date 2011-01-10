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

#include "testlibgwy.h"

#ifdef HAVE_VALGRIND
#include <valgrind/valgrind.h>
#else
#define RUNNING_ON_VALGRIND 0
#endif

/***************************************************************************
 *
 * FFT
 *
 ***************************************************************************/

static gboolean
is_nice_even(guint n)
{
    static const guint primes[] = { 2, 3, 5, 7 };

    if (n % 2)
        return FALSE;

    for (guint j = 0; j < G_N_ELEMENTS(primes); j++) {
        while (n % primes[j] == 0)
            n /= primes[j];
    }

    return n == 1 || n == 11 || n == 13;
}

void
test_fft_nice_size(void)
{
    guint nmax = 10000;
    guint maxnicenum = 1;

    for (guint n = 1; n < nmax; n++) {
        guint nicenum = gwy_fft_nice_transform_size(n);
        // The result must be nice.
        g_assert(is_nice_even(nicenum));
        // If n is already nice the result must be n itself.
        if (is_nice_even(n)) {
            g_assert_cmpuint(nicenum, ==, n);
        }
        // The sequence must be non-decreasing, descrasing numbers would mean
        // that something fishy is going on here.
        g_assert_cmpuint(nicenum, >=, maxnicenum);
        if (nicenum > maxnicenum)
            maxnicenum = nicenum;
    }
}

void
test_fft_load_wisdom(void)
{
    gwy_fft_load_wisdom();
}

void
test_fft_save_wisdom(void)
{
    // Saving wisdom found when running under valgrind is not a good idea.
    // Avoid it.
    if (!RUNNING_ON_VALGRIND)
        gwy_fft_save_wisdom();
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
