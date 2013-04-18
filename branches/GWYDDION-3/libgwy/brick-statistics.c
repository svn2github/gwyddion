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
#include "libgwy/field-transform.h"
#include "libgwy/brick.h"
#include "libgwy/brick-arithmetic.h"
#include "libgwy/brick-statistics.h"
#include "libgwy/object-internal.h"
#include "libgwy/field-internal.h"
#include "libgwy/brick-internal.h"

static void    summarize_lines(const gdouble *bbase,
                               guint bxres,
                               guint byres,
                               gdouble *f,
                               gdouble *g,
                               guint width,
                               guint height,
                               guint depth,
                               gint rowstep,
                               gint levelstep,
                               GwyBrickLineSummary quantity);
static gdouble summarize_line (const gdouble *b,
                               guint avgsize,
                               GwyBrickLineSummary quantity);

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
    g_return_if_fail(quantity <= GWY_BRICK_LINE_NRMS);

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
    guint fcol, frow;
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

    /*
     * No dimension combination needs to lead to a bad memory access pattern.
     * We have two basic situations:
     * (a) Neither @coldim nor @rowdim is GWY_DIMEN_X.  Then x is the
     *     summarised dimension and we can simply summarise one brick line
     *     completely (accessing data sequentially), store the resulting value
     *     to field and move to another line.
     * (b) One of @coldim and @rowdim is GWY_DIMEN_X.  The we can always make x
     *     dimension of brick aligned with x dimension of field (we create a
     *     temporary field to achieve this).  Summarising one brick line thus
     *     means accessing sequentially in both brick and field.  Finally, we
     *     may need to perform a transposition of the field but this is done
     *     only once.
     */
    gint rowstep, levelstep;
    if (leveldim == GWY_DIMEN_X) {
        if (coldim == GWY_DIMEN_Y && rowdim == GWY_DIMEN_Z) {
            rowstep = 1;
            levelstep = target->xres - height;
        }
        else if (coldim == GWY_DIMEN_Z && rowdim == GWY_DIMEN_Y) {
            rowstep = target->xres;
            levelstep = 1 - (gint)target->xres*(gint)height;
        }
        else {
            g_assert_not_reached();
        }

        gdouble *f = target->data + frow*target->xres + fcol;
        for (guint l = 0; l < depth; l++, f += levelstep) {
            for (guint i = 0; i < height; i++, f += rowstep) {
                *f = summarize_line(bbase + (l*brick->yres + i)*brick->xres,
                                    avgsize, quantity);
            }
        }
    }
    else {
        GwyDimenType otherdim = coldim + rowdim - GWY_DIMEN_X;
        guint othersize = xsize + ysize - width;
        GwyField *tmp = gwy_field_new_sized(width, othersize, FALSE);
        GwyField *tmp2 = NULL;
        if (quantity == GWY_BRICK_LINE_RANGE
            || quantity == GWY_BRICK_LINE_RMS
            || quantity == GWY_BRICK_LINE_NRMS)
            tmp2 = gwy_field_new_alike(tmp, FALSE);

        if (otherdim == GWY_DIMEN_Y) {
            rowstep = 0;
            levelstep = -(gint)width*(gint)othersize;
        }
        else if (otherdim == GWY_DIMEN_Z) {
            rowstep = -(gint)width;
            levelstep = width;
        }
        else {
            g_assert_not_reached();
        }

        if (quantity == GWY_BRICK_LINE_MINIMUM) {
            gwy_field_fill_full(tmp, G_MAXDOUBLE);
            summarize_lines(bbase, brick->xres, brick->yres,
                            tmp->data, NULL,
                            width, height, depth, rowstep, levelstep,
                            quantity);
        }
        else if (quantity == GWY_BRICK_LINE_MAXIMUM) {
            gwy_field_fill_full(tmp, -G_MAXDOUBLE);
            summarize_lines(bbase, brick->xres, brick->yres,
                            tmp->data, NULL,
                            width, height, depth, rowstep, levelstep,
                            quantity);
        }
        else if (quantity == GWY_BRICK_LINE_RANGE) {
            gwy_field_fill_full(tmp, -G_MAXDOUBLE);
            gwy_field_fill_full(tmp2, G_MAXDOUBLE);
            summarize_lines(bbase, brick->xres, brick->yres,
                            tmp->data, tmp2->data,
                            width, height, depth, rowstep, levelstep,
                            quantity);
            gwy_field_add_field(tmp2, NULL, tmp, 0, 0, -1.0);
        }
        else if (quantity == GWY_BRICK_LINE_MEAN) {
            gwy_field_clear_full(tmp);
            summarize_lines(bbase, brick->xres, brick->yres,
                            tmp->data, NULL,
                            width, height, depth, rowstep, levelstep,
                            quantity);
            gwy_field_multiply(tmp, NULL, NULL, GWY_MASK_IGNORE, 1.0/avgsize);
        }
        else if (quantity == GWY_BRICK_LINE_RMS) {
            gwy_field_clear_full(tmp2);
            summarize_lines(bbase, brick->xres, brick->yres,
                            tmp2->data, NULL,
                            width, height, depth, rowstep, levelstep,
                            GWY_BRICK_LINE_MEAN);
            gwy_field_multiply(tmp2, NULL, NULL, GWY_MASK_IGNORE, 1.0/avgsize);
            gwy_field_clear_full(tmp);
            summarize_lines(bbase, brick->xres, brick->yres,
                            tmp->data, tmp2->data,
                            width, height, depth, rowstep, levelstep,
                            quantity);
            gwy_field_multiply(tmp, NULL, NULL, GWY_MASK_IGNORE, 1.0/avgsize);
            gwy_field_sqrt(tmp, NULL, NULL, GWY_MASK_IGNORE);
        }
        else if (quantity == GWY_BRICK_LINE_NRMS) {
            gwy_field_clear_full(tmp);
            if (avgsize > 1) {
                GwyFieldPart fpart = {
                    from[GWY_DIMEN_X], from[otherdim],
                    size[GWY_DIMEN_X], size[otherdim]
                };
                gwy_brick_extract_plane(brick, tmp2, &fpart,
                                        GWY_DIMEN_X, otherdim,
                                        from[leveldim], FALSE);
                summarize_lines(bbase, brick->xres, brick->yres,
                                tmp->data, tmp2->data,
                                width, height, depth, rowstep, levelstep,
                                quantity);
                gwy_field_multiply(tmp, NULL, NULL, GWY_MASK_IGNORE,
                                   1.0/(avgsize - 1.0));
                gwy_field_sqrt(tmp, NULL, NULL, GWY_MASK_IGNORE);
            }
        }

        if (coldim == GWY_DIMEN_X)
            gwy_field_copy(tmp, NULL, target, fcol, frow);
        else
            gwy_field_copy_congruent(tmp, NULL, target, fcol, frow,
                                     GWY_PLANE_MIRROR_DIAGONALLY);
        g_object_unref(tmp);
        GWY_OBJECT_UNREF(tmp2);
    }

    _gwy_assign_unit(&target->priv->xunit, brick->priv->xunit);
    _gwy_assign_unit(&target->priv->yunit, brick->priv->yunit);
    _gwy_assign_unit(&target->priv->zunit, brick->priv->wunit);
    gwy_field_invalidate(target);
}

static void
summarize_lines(const gdouble *bbase, guint bxres, guint byres,
                gdouble *f, gdouble *g,
                guint width, guint height, guint depth,
                gint rowstep, gint levelstep,
                GwyBrickLineSummary quantity)
{
    // Stepping @g in the outer cycles is nonsense but harmless for
    // single-field quantities.  We never dereference it.
    for (guint l = 0; l < depth; l++, f += levelstep, g += levelstep) {
        for (guint i = 0; i < height; i++, f += rowstep, g += rowstep) {
            const gdouble *b = bbase + (l*byres + i)*bxres;
            if (quantity == GWY_BRICK_LINE_MINIMUM) {
                for (guint j = width; j; j--, b++, f++) {
                    if (*b < *f)
                        *f = *b;
                }
            }
            else if (quantity == GWY_BRICK_LINE_MAXIMUM) {
                for (guint j = width; j; j--, b++, f++) {
                    if (*b > *f)
                        *f = *b;
                }
            }
            else if (quantity == GWY_BRICK_LINE_RANGE) {
                for (guint j = width; j; j--, b++, f++, g++) {
                    if (*b > *f)
                        *f = *b;
                    if (*b < *g)
                        *g = *b;
                }
            }
            else if (quantity == GWY_BRICK_LINE_MEAN) {
                for (guint j = width; j; j--, b++, f++)
                    *f += *b;
            }
            else if (quantity == GWY_BRICK_LINE_RMS) {
                for (guint j = width; j; j--, b++, f++, g++)
                    *f += (*b - *g)*(*b - *g);
            }
            else if (quantity == GWY_BRICK_LINE_NRMS) {
                for (guint j = width; j; j--, b++, f++, g++) {
                    *f += (*b - *g)*(*b - *g);
                    *g = *b;
                }
            }
            else {
                g_return_if_reached();
            }
        }
    }
}

static gdouble
summarize_line(const gdouble *b,
               guint avgsize,
               GwyBrickLineSummary quantity)
{
    if (quantity == GWY_BRICK_LINE_MINIMUM) {
        gdouble v = G_MAXDOUBLE;
        for (guint j = avgsize; j; j--, b++) {
            if (*b < v)
                v = *b;
        }
        return v;
    }

    if (quantity == GWY_BRICK_LINE_MAXIMUM) {
        gdouble v = -G_MAXDOUBLE;
        for (guint j = avgsize; j; j--, b++) {
            if (*b > v)
                v = *b;
        }
        return v;
    }

    if (quantity == GWY_BRICK_LINE_RANGE) {
        gdouble vm = G_MAXDOUBLE, vM = -G_MAXDOUBLE;
        for (guint j = avgsize; j; j--, b++) {
            if (*b < vm)
                vm = *b;
            if (*b > vM)
                vM = *b;
        }
        return vM - vm;
    }

    if (quantity == GWY_BRICK_LINE_MEAN) {
        gdouble v = 0.0;
        for (guint j = avgsize; j; j--, b++)
            v += *b;
        return v/avgsize;
    }

    if (quantity == GWY_BRICK_LINE_RMS) {
        const gdouble *bb = b;
        gdouble v = 0.0;
        for (guint j = avgsize; j; j--, b++)
            v += *b;
        v /= avgsize;
        b = bb;
        gdouble s = 0.0;
        for (guint j = avgsize; j; j--, b++)
            s += (*b - v)*(*b - v);
        return sqrt(s/avgsize);
    }

    if (quantity == GWY_BRICK_LINE_NRMS) {
        if (avgsize < 2)
            return 0.0;

        gdouble prev = *(b++), v = 0.0;
        for (guint j = avgsize-1; j; j--, b++) {
            v += (*b - prev)*(*b - prev);
            prev = *b;
        }
        return sqrt(v/(avgsize - 1.0));
    }

    g_assert_not_reached();
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
 * @GWY_BRICK_LINE_RMS: Root mean square value of differences from mean.
 * @GWY_BRICK_LINE_NRMS: Root mean square value of neighbour differences.
 *
 * Summary quantities characterising individual brick lines.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
