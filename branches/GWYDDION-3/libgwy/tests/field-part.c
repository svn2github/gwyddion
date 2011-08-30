/*
 *  $Id$
 *  Copyright (C) 2010 David Nečas (Yeti).
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
 * Field part
 *
 ***************************************************************************/

void
field_part_assert_equal(const GwyFieldPart *part,
                        const GwyFieldPart *refpart)
{
    g_assert((part && refpart) || (!part && !refpart));
    if (part) {
        g_assert_cmpuint(part->col, ==, refpart->col);
        g_assert_cmpuint(part->row, ==, refpart->row);
        g_assert_cmpuint(part->width, ==, refpart->width);
        g_assert_cmpuint(part->height, ==, refpart->height);
    }
}

void
test_field_part_boxed(void)
{
    GwyFieldPart fpart = { 1, 2, 3, 4 };
    GwyFieldPart *copy = serialize_boxed_and_back(&fpart,
                                                  GWY_TYPE_FIELD_PART);
    field_part_assert_equal(copy, &fpart);
    g_boxed_free(GWY_TYPE_FIELD_PART, copy);
}

void
test_field_part_intersect(void)
{
    enum { max_size = 40, niter = 500, n = 10000 };
    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        GwyFieldPart result = {
            g_rand_int_range(rng, 0, max_size),
            g_rand_int_range(rng, 0, max_size),
            g_rand_int_range(rng, 0, max_size),
            g_rand_int_range(rng, 0, max_size),
        };
        GwyFieldPart otherpart = {
            g_rand_int_range(rng, 0, max_size),
            g_rand_int_range(rng, 0, max_size),
            g_rand_int_range(rng, 0, max_size),
            g_rand_int_range(rng, 0, max_size),
        };
        GwyFieldPart fpart = result;
        gboolean intersecting = gwy_field_part_intersect(&result, &otherpart);

        for (guint k = 0; k < n; k++) {
            guint j = g_rand_int_range(rng, 0, 2*max_size);
            guint i = g_rand_int_range(rng, 0, 2*max_size);
            gboolean in_fpart = (j >= fpart.col
                                 && j - fpart.col < fpart.width
                                 && i >= fpart.row
                                 && i - fpart.row < fpart.height);
            gboolean in_otherpart = (j >= otherpart.col
                                     && j - otherpart.col < otherpart.width
                                     && i >= otherpart.row
                                     && i - otherpart.row < otherpart.height);
            gboolean in_result = (j >= result.col
                                  && j - result.col < result.width
                                  && i >= result.row
                                  && i - result.row < result.height);

            g_assert_cmpuint(in_result, ==, in_fpart && in_otherpart);
            g_assert(intersecting || !in_result);
        }
    }

    g_rand_free(rng);
}

void
test_field_part_union(void)
{
    enum { max_size = 40, niter = 500, n = 10000 };
    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        GwyFieldPart result = {
            g_rand_int_range(rng, 0, max_size),
            g_rand_int_range(rng, 0, max_size),
            g_rand_int_range(rng, 0, max_size),
            g_rand_int_range(rng, 0, max_size),
        };
        GwyFieldPart otherpart = {
            g_rand_int_range(rng, 0, max_size),
            g_rand_int_range(rng, 0, max_size),
            g_rand_int_range(rng, 0, max_size),
            g_rand_int_range(rng, 0, max_size),
        };
        GwyFieldPart fpart = result;
        gwy_field_part_union(&result, &otherpart);

        for (guint k = 0; k < n; k++) {
            guint j = g_rand_int_range(rng, 0, 2*max_size);
            guint i = g_rand_int_range(rng, 0, 2*max_size);
            gboolean in_x = ((j >= fpart.col
                              || j >= otherpart.col)
                             && (j < fpart.col + fpart.width
                                 || j < otherpart.col + otherpart.width));
            gboolean in_y = ((i >= fpart.row
                              || i >= otherpart.row)
                             && (i < fpart.row + fpart.height
                                 || i < otherpart.row + otherpart.height));
            gboolean in_result = (j >= result.col
                                  && j - result.col < result.width
                                  && i >= result.row
                                  && i - result.row < result.height);

            g_assert_cmpuint(in_result, ==, in_x && in_y);
        }
    }

    g_rand_free(rng);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
