/*
 *  $Id$
 *  Copyright (C) 2011 David Nečas (Yeti).
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

#include <math.h>
#include "libgwy/macros.h"
#include "libgwy/field-arithmetic.h"
#include "libgwy/brick.h"
#include "libgwy/brick-statistics.h"
#include "libgwy/object-internal.h"
#include "libgwy/field-internal.h"
#include "libgwy/brick-internal.h"

/**
 * gwy_brick_summarize_lines:
 * @brick: A data brick.
 * @bpart: (allow-none):
 *         Part of @brick to summarize.  Passing %NULL means the entire brick.
 * @target: A two-dimensional data field where the result will be placed.
 *          Its dimensions may match either @brick planes or @bpart.  In the
 *          former case the placement of result is determined by @bpart; in the
 *          latter case the result fills the entire @target.
 * @quantity: The summary characteristics to calculate for each line.
 *
 * Characterises each line in a brick with a statistical quantity.
 **/
void
gwy_brick_summarize_lines(const GwyBrick *brick,
                          const GwyBrickPart *bpart,
                          GwyField *target,
                          GwyBrickLineSummary quantity)
{
    guint col, row, level, width, height, depth;
    if (!gwy_brick_check_part(brick, bpart,
                              &col, &row, &level, &width, &height, &depth))
        return;

    g_return_if_fail(GWY_IS_FIELD(target));
    guint fcol, frow;
    if (target->xres == width && target->yres == height)
        fcol = frow = 0;
    else if (target->xres == brick->xres && target->yres == brick->yres) {
        fcol = col;
        frow = row;
    }
    else {
        g_critical("Target field dimensions match neither source brick nor the "
                   "part.");
        return;
    }

    const gdouble *bbase = brick->data + (level*brick->yres + row)*brick->xres
                           + col;
    gdouble *fbase = target->data + frow*target->xres + fcol;
    GwyFieldPart fpart = { fcol, frow, width, height };

    if (quantity == GWY_BRICK_LINE_MINIMUM) {
        gwy_field_fill(target, &fpart, NULL, GWY_MASK_IGNORE, G_MAXDOUBLE);
        for (guint l = 0; l < depth; l++) {
            for (guint i = 0; i < height; i++) {
                const gdouble *b = bbase + (l*brick->yres + i)*brick->xres;
                gdouble *f = fbase + i*target->xres;
                for (guint j = width; j; j--, b++, f++) {
                    if (*b < *f)
                        *f = *b;
                }
            }
        }
    }
    else if (quantity == GWY_BRICK_LINE_MAXIMUM) {
        gwy_field_fill(target, &fpart, NULL, GWY_MASK_IGNORE, -G_MAXDOUBLE);
        for (guint l = 0; l < depth; l++) {
            for (guint i = 0; i < height; i++) {
                const gdouble *b = bbase + (l*brick->yres + i)*brick->xres;
                gdouble *f = fbase + i*target->xres;
                for (guint j = width; j; j--, b++, f++) {
                    if (*b > *f)
                        *f = *b;
                }
            }
        }
    }
    else if (quantity == GWY_BRICK_LINE_MEAN
             || quantity == GWY_BRICK_LINE_RMS) {
        gwy_field_fill(target, &fpart, NULL, GWY_MASK_IGNORE, 0.0);
        for (guint l = 0; l < depth; l++) {
            for (guint i = 0; i < height; i++) {
                const gdouble *b = bbase + (l*brick->yres + i)*brick->xres;
                gdouble *f = fbase + i*target->xres;
                for (guint j = width; j; j--, b++, f++) {
                    *f += *b;
                }
            }
        }
        gwy_field_multiply(target, &fpart, NULL, GWY_MASK_IGNORE, 1.0/depth);
    }

    if (quantity == GWY_BRICK_LINE_RMS) {
        GwyField *tmp = gwy_field_new_sized(width, height, TRUE);
        for (guint l = 0; l < depth; l++) {
            for (guint i = 0; i < height; i++) {
                const gdouble *b = bbase + (l*brick->yres + i)*brick->xres;
                const gdouble *f = fbase + i*target->xres;
                gdouble *t = tmp->data + i*width;
                for (guint j = width; j; j--, b++, f++, t++) {
                    *t += (*b - *f)*(*b - *f);
                }
            }
        }
        for (guint i = 0; i < height; i++) {
            const gdouble *t = tmp->data + i*width;
            gdouble *f = fbase + i*target->xres;
            for (guint j = width; j; j--, f++, t++)
                *f = sqrt(*t/depth);
        }
        g_object_unref(tmp);
    }

    _gwy_assign_unit(&target->priv->unit_x, brick->priv->unit_x);
    _gwy_assign_unit(&target->priv->unit_y, brick->priv->unit_y);
    _gwy_assign_unit(&target->priv->unit_z, brick->priv->unit_w);
    gwy_field_invalidate(target);
}

/**
 * SECTION: brick-statistics
 * @section_id: GwyBrick-statistics
 * @title: GwyBrick statistics
 * @short_description: Statistical characteristics of bricks
 **/

/**
 * GwyBrickLineSummary:
 * @GWY_BRICK_LINE_MINIMUM: Minimum value.
 * @GWY_BRICK_LINE_MAXIMUM: Maximum value.
 * @GWY_BRICK_LINE_MEAN: Mean value.
 * @GWY_BRICK_LINE_RMS: Root mean square value.
 *
 * Summary quantities characterising individual brick lines.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
