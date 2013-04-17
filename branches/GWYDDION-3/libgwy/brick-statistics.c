/*
 *  $Id$
 *  Copyright (C) 2011-2013 David Nečas (Yeti).
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

static void summarize_lines(const gdouble *bbase,
                            guint bxres,
                            guint byres,
                            gdouble *f,
                            const gdouble *g,
                            guint width,
                            guint height,
                            guint depth,
                            gint colstep,
                            gint rowstep,
                            gint levelstep,
                            GwyBrickLineSummary quantity);
/*
 * Going sequentially through the brick, where we have to move in the field
 * when moving to the next column/row/level in the brick?
 *
 *  coldim  rowdim  |  colstep    rowstep        levelstep
 * -----------------------------------------------------------
 *    x       y     |     1     fxres-width    -fxres*height
 *    x       z     |     1       -width           fxres
 *    y       z     |     0          1          fxres-height
 *    y       x     |   fxres   1-fxres*width     -height
 *    z       x     |   fxres   -fxres*width         1
 *    z       y     |     0         fxres      1-fxres*height
 */

/**
 * gwy_brick_summarize_lines:
 * @brick: A data brick.
 * @bpart: (allow-none):
 *         Part of @brick to summarize.  Passing %NULL means the entire brick.
 * @target: A two-dimensional data field where the result will be placed.
 *          Its dimensions may match either @brick planes or @bpart.  In the
 *          former case the placement of result is determined by @bpart; in the
 *          latter case the result fills the entire @target.
 * @coldim: Dimension (axis) in @brick which will form columns in the summary
 *          field.
 * @rowdim: Dimension (axis) in @brick which will form rows in the summary
 *          field.
 * @quantity: The summary characteristics to calculate for each line.
 *
 * Characterises each line in a brick with a statistical quantity.
 *
 * See gwy_brick_extract_plane() for a discussion of the dimension arguments.
 **/
void
gwy_brick_summarize_lines(const GwyBrick *brick,
                          const GwyBrickPart *bpart,
                          GwyField *target,
                          GwyDimenType coldim, GwyDimenType rowdim,
                          GwyBrickLineSummary quantity)
{
    g_return_if_fail(coldim <= GWY_DIMEN_Z);
    g_return_if_fail(rowdim <= GWY_DIMEN_Z);
    g_return_if_fail(coldim != rowdim);
    g_return_if_fail(quantity <= GWY_BRICK_LINE_RMS);

    guint col, row, level, width, height, depth;
    if (!gwy_brick_check_part(brick, bpart,
                              &col, &row, &level, &width, &height, &depth))
        return;

    g_return_if_fail(GWY_IS_FIELD(target));
    GwyDimenType leveldim = (GWY_DIMEN_X + GWY_DIMEN_Y + GWY_DIMEN_Z
                             - coldim - rowdim);
    const guint res[] = { brick->xres, brick->yres, brick->zres };
    const guint size[] = { width, height, depth };
    const guint from[] = { col, row, level };
    guint xres = res[coldim], yres = res[rowdim];
    guint xsize = size[coldim], ysize = size[rowdim], avgsize = size[leveldim];
    guint fcol, frow, fwidth, fheight;
    if (target->xres == xsize && target->yres == ysize)
        fcol = frow = 0;
    else if (target->xres == xres && target->yres == yres) {
        fcol = from[coldim];
        frow = from[rowdim];
    }
    else {
        g_critical("Target field dimensions match neither source brick nor the "
                   "part.");
        return;
    }

    const gdouble *bbase = brick->data + (level*brick->yres + row)*brick->xres
                           + col;
    guint fstart = frow*target->xres + fcol;
    GwyFieldPart fpart = { fcol, frow, xsize, ysize };
    gint colstep, rowstep, levelstep;
    if (coldim == GWY_DIMEN_X && rowdim == GWY_DIMEN_Y) {
        colstep = 1;
        rowstep = target->xres - width;
        levelstep = -(gint)target->xres*(gint)height;
    }
    else if (coldim == GWY_DIMEN_X && rowdim == GWY_DIMEN_Z) {
        colstep = 1;
        rowstep = -(gint)width;
        levelstep = target->xres;
    }
    else if (coldim == GWY_DIMEN_Y && rowdim == GWY_DIMEN_Z) {
        colstep = 0;
        rowstep = 1;
        levelstep = target->xres - height;
    }
    else if (coldim == GWY_DIMEN_Y && rowdim == GWY_DIMEN_X) {
        colstep = target->xres;
        rowstep = 1 - (gint)target->xres*(gint)width;
        levelstep = -(gint)height;
    }
    else if (coldim == GWY_DIMEN_Z && rowdim == GWY_DIMEN_X) {
        colstep = target->xres;
        rowstep = -(gint)target->xres*(gint)width;
        levelstep = 1;
    }
    else if (coldim == GWY_DIMEN_Z && rowdim == GWY_DIMEN_Y) {
        colstep = 0;
        rowstep = target->xres;
        levelstep = 1 - (gint)target->xres*(gint)height;
    }
    else {
        g_assert_not_reached();
    }

    if (quantity == GWY_BRICK_LINE_MINIMUM)
    if (quantity == GWY_BRICK_LINE_MAXIMUM || quantity == GWY_BRICK_LINE_RANGE)
        gwy_field_fill(target, &fpart, NULL, GWY_MASK_IGNORE, -G_MAXDOUBLE);
    if (quantity == GWY_BRICK_LINE_MEAN || quantity == GWY_BRICK_LINE_RMS)
        gwy_field_clear(target, &fpart, NULL, GWY_MASK_IGNORE);

    if (quantity == GWY_BRICK_LINE_MINIMUM) {
        gwy_field_fill(target, &fpart, NULL, GWY_MASK_IGNORE, G_MAXDOUBLE);
        summarize_lines(bbase, brick->xres, brick->yres,
                        target->data + fstart, NULL,
                        width, height, depth, colstep, rowstep, levelstep,
                        quantity);
    }
    else if (quantity == GWY_BRICK_LINE_MAXIMUM) {
        gwy_field_fill(target, &fpart, NULL, GWY_MASK_IGNORE, -G_MAXDOUBLE);
        summarize_lines(bbase, brick->xres, brick->yres,
                        target->data + fstart, NULL,
                        width, height, depth, colstep, rowstep, levelstep,
                        quantity);
    }
    else if (quantity == GWY_BRICK_LINE_RANGE) {
        GwyField *tmp = gwy_field_new_alike(target, FALSE);
        gwy_field_fill(tmp, &fpart, NULL, GWY_MASK_IGNORE, G_MAXDOUBLE);
        summarize_lines(bbase, brick->xres, brick->yres,
                        tmp->data + fstart, NULL,
                        width, height, depth, colstep, rowstep, levelstep,
                        GWY_BRICK_LINE_MAXIMUM);
        gwy_field_fill(target, &fpart, NULL, GWY_MASK_IGNORE, -G_MAXDOUBLE);
        summarize_lines(bbase, brick->xres, brick->yres,
                        target->data + fstart, NULL,
                        width, height, depth, colstep, rowstep, levelstep,
                        GWY_BRICK_LINE_MINIMUM);
        gwy_field_add_field(tmp, &fpart, target, fpart.col, fpart.row, -1.0);
        g_object_unref(tmp);
    }
    else if (quantity == GWY_BRICK_LINE_MEAN) {
        gwy_field_clear(target, &fpart, NULL, GWY_MASK_IGNORE);
        summarize_lines(bbase, brick->xres, brick->yres,
                        target->data + fstart, NULL,
                        width, height, depth, colstep, rowstep, levelstep,
                        quantity);
        gwy_field_multiply(target, &fpart, NULL, GWY_MASK_IGNORE, 1.0/avgsize);
    }
    else if (quantity == GWY_BRICK_LINE_RMS) {
        GwyField *tmp = gwy_field_new_alike(target, FALSE);
        gwy_field_clear(tmp, &fpart, NULL, GWY_MASK_IGNORE);
        summarize_lines(bbase, brick->xres, brick->yres,
                        tmp->data + fstart, NULL,
                        width, height, depth, colstep, rowstep, levelstep,
                        GWY_BRICK_LINE_MEAN);
        gwy_field_multiply(tmp, &fpart, NULL, GWY_MASK_IGNORE, 1.0/avgsize);
        gwy_field_clear(target, &fpart, NULL, GWY_MASK_IGNORE);
        summarize_lines(bbase, brick->xres, brick->yres,
                        target->data + fstart, tmp->data + fstart,
                        width, height, depth, colstep, rowstep, levelstep,
                        GWY_BRICK_LINE_RMS);
        g_object_unref(tmp);
        gwy_field_multiply(target, &fpart, NULL, GWY_MASK_IGNORE, 1.0/avgsize);
        gwy_field_sqrt(target, &fpart, NULL, GWY_MASK_IGNORE);
    }

    _gwy_assign_unit(&target->priv->xunit, brick->priv->xunit);
    _gwy_assign_unit(&target->priv->yunit, brick->priv->yunit);
    _gwy_assign_unit(&target->priv->zunit, brick->priv->wunit);
    gwy_field_invalidate(target);
}

static void
summarize_lines(const gdouble *bbase, guint bxres, guint byres,
                gdouble *f, const gdouble *g,
                guint width, guint height, guint depth,
                gint colstep, gint rowstep, gint levelstep,
                GwyBrickLineSummary quantity)
{
    // Stepping @g in the outer cycles is nonsense but harmless for
    // single-field quantities.  We never dereference it.
    for (guint l = 0; l < depth; l++, f += levelstep, g += levelstep) {
        for (guint i = 0; i < height; i++, f += rowstep, g += rowstep) {
            const gdouble *b = bbase + (l*byres + i)*bxres;
            if (quantity == GWY_BRICK_LINE_MINIMUM) {
                for (guint j = width; j; j--, b++, f += colstep) {
                    if (*b < *f)
                        *f = *b;
                }
            }
            else if (quantity == GWY_BRICK_LINE_MAXIMUM) {
                for (guint j = width; j; j--, b++, f += colstep) {
                    if (*b > *f)
                        *f = *b;
                }
            }
            else if (quantity == GWY_BRICK_LINE_MEAN) {
                for (guint j = width; j; j--, b++, f += colstep)
                    *f += *b;
            }
            else if (quantity == GWY_BRICK_LINE_RMS) {
                for (guint j = width; j; j--, b++, f += colstep, g += colstep)
                    *f += (*b - *g)*(*b - *g);
            }
            else {
                g_return_if_reached();
            }
        }
    }
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
 * @GWY_BRICK_LINE_RANGE: Difference between maximum and minimum values.
 * @GWY_BRICK_LINE_MEAN: Mean value.
 * @GWY_BRICK_LINE_RMS: Root mean square value.
 *
 * Summary quantities characterising individual brick lines.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
