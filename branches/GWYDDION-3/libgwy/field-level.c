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

#include "libgwy/macros.h"
#include "libgwy/math.h"
#include "libgwy/field-level.h"
#include "libgwy/libgwy-aliases.h"
#include "libgwy/processing-internal.h"

typedef struct {
    const gdouble *p;
    guint width;
    guint height;
    const gdouble *base;
    guint xres;
    guint yres;
    // Only with masking
    const GwyMaskField *mask;
    GwyMaskFieldIter iter;
    guint maskcol;
    guint maskrow;
    gboolean mode;
} PlaneFitData;

static gboolean
plane_fit(guint id,
          gdouble *fvalues,
          gdouble *value,
          gpointer user_data)
{
    PlaneFitData *data = (PlaneFitData*)user_data;
    guint i = id/data->width;
    guint j = id % data->width;
    if (j == 0)
        data->p = data->base + i*data->xres;
    gdouble x = 2*j/(data->width - 1.0) - 1.0;
    gdouble y = 2*i/(data->height - 1.0) - 1.0;
    fvalues[0] = 1.0;
    fvalues[1] = x;
    fvalues[2] = y;
    *value = *data->p;
    data->p++;
    return TRUE;
}

static gboolean
plane_fit_mask(guint id,
               gdouble *fvalues,
               gdouble *value,
               gpointer user_data)
{
    PlaneFitData *data = (PlaneFitData*)user_data;
    guint i = id/data->width;
    guint j = id % data->width;
    if (j == 0) {
        data->p = data->base + i*data->xres;
        gwy_mask_field_iter_init(data->mask, data->iter,
                                 data->maskcol, data->maskrow+i);
    }
    gboolean ok = (!!gwy_mask_field_iter_get(data->iter) == data->mode);
    if (ok) {
        gdouble x = 2*j/(data->width - 1.0) - 1.0;
        gdouble y = 2*i/(data->height - 1.0) - 1.0;
        fvalues[0] = 1.0;
        fvalues[1] = x;
        fvalues[2] = y;
        *value = *data->p;
    }
    gwy_mask_field_iter_next(data->iter);
    data->p++;
    return ok;
}

/**
 * gwy_field_part_fit_plane:
 * @field: A two-dimensional data field.
 * @mask: Mask specifying which values to take into account/exclude, or %NULL.
 * @masking: Masking mode to use (has any effect only with non-%NULL @mask).
 * @col: Column index of the upper-left corner of the rectangle.
 * @row: Row index of the upper-left corner of the rectangle.
 * @width: Rectangle width (number of columns).  It should be at least 2.
 * @height: Rectangle height (number of rows).  It should be at least 2.
 * @a: Location to store the constant coefficient, or %NULL.
 * @bx: Location to store the x coefficient, or %NULL.
 * @by: Location to store the y coefficient, or %NULL.
 *
 * Fits a plane through a rectangular part of a field.
 *
 * The coefficients follow the same convention as in gwy_field_area_fit_poly(),
 * i.e. they correspond the lateral coordinate ranges of the entire field
 * normalized to interval [-1,1].  If you want them expressed for integer row
 * and column, you can use the following transformation:
 * |[
 * apix = a - bx - by;
 * bcol = 2*bx/(field->xres - 1);
 * brow = 2*by/(field->yres - 1);
 * ]|
 * If you wish to simply use the coefficients for levelling, note that
 * gwy_field_subtract_plane() also uses the same coordinate convention.
 *
 * Returns: %TRUE if the plane was fitted, %FALSE if the there were too few
 *          points to fit a plane or there were no points with at least two
 *          different x or y coordinates.  The coefficients @a, @bx and @by are
 *          set in all cases, on failure, they are set to zeroes.
 **/
gboolean
gwy_field_part_fit_plane(const GwyField *field,
                         const GwyMaskField *mask,
                         GwyMaskingType masking,
                         guint col, guint row, guint width, guint height,
                         gdouble *a, gdouble *bx, gdouble *by)
{
    guint maskcol, maskrow;
    if (!_gwy_field_check_mask(field, mask, &masking,
                               col, row, width, height, &maskcol, &maskrow)
        || width < 2 || height < 2)
        goto fail;

    PlaneFitData data = {
        .width = width,
        .height = height,
        .base = field->data + row*field->xres + col,
        .xres = field->xres,
        .yres = field->yres,
        .mask = mask,
        .maskcol = maskcol,
        .maskrow = maskrow,
    };
    gdouble params[3];
    gboolean ok;
    if (masking == GWY_MASK_IGNORE) {
        ok = gwy_linear_fit(plane_fit, width*height, params, 3, NULL,
                            &data);
    }
    else {
        data.mode = (masking == GWY_MASK_INCLUDE);
        ok = gwy_linear_fit(plane_fit_mask, width*height, params, 3, NULL,
                            &data);
    }
    if (!ok)
        goto fail;

    GWY_MAYBE_SET(a, params[0]);
    GWY_MAYBE_SET(bx, params[1]);
    GWY_MAYBE_SET(by, params[2]);
    return TRUE;

fail:
    GWY_MAYBE_SET(a, 0.0);
    GWY_MAYBE_SET(bx, 0.0);
    GWY_MAYBE_SET(by, 0.0);
    return FALSE;
}

/**
 * gwy_field_part_inclination:
 * @field: A two-dimensional data field.
 * @mask: Mask specifying which values to take into account/exclude, or %NULL.
 * @masking: Masking mode to use (has any effect only with non-%NULL @mask).
 * @col: Column index of the upper-left corner of the rectangle.
 * @row: Row index of the upper-left corner of the rectangle.
 * @width: Rectangle width (number of columns).  It should be at least 2.
 * @height: Rectangle height (number of rows).  It should be at least 2.
 * @damping: Damping factor determining the range of deviations of local normal
 *           vectors from (0,0,1) that non-negligibly contribute to the result.
 *           It must be positive, a reasonable value may be e.g. 20.
 * @bx: Location to store the x coefficient, or %NULL.
 * @by: Location to store the y coefficient, or %NULL.
 *
 * Fits a plane through a rectangular part of a field by straighenting up
 * facets.
 *
 * The coefficients follow the same convention as in
 * gwy_field_area_fit_plane() and gwy_field_area_fit_poly() though they are
 * calculated differently.  Instead of fitting a plane throguh the points,
 * local factes are determined from 2×2 areas and averaged, leading to a mean
 * normal to the surface.
 *
 * If @damping was zero (which is not permitted), the normal would correspond
 * to the mean normal to the mean plane and the coefficients found by this
 * method would be equal to those calculated by gwy_field_area_fit_plane().
 *
 * Positive values of @damping suppress large slopes by weighting the normals
 * by a Gaussian function of the slope magnitude.  This means edges and noise
 * do not influence the mean normal much and it is effectively calculated
 * only from relatively flat areas of the surface.
 *
 * Since this method is non-linear, it is often used iteratively until it
 * converges.  If there are several competing directions of normals of flat
 * areas, one of them usually wins and the final normal corresponds
 * approximately to the orientation of these facets.
 *
 * Returns: %TRUE if the plane was fitted, %FALSE in degenerate cases when the
 *          plane is impossible to find. The coefficients @bx and @by are
 *          set in all cases, on failure, they are set to zeroes.
 **/
gboolean
gwy_field_part_inclination(const GwyField *field,
                           const GwyMaskField *mask,
                           GwyMaskingType masking,
                           guint col, guint row, guint width, guint height,
                           gdouble damping,
                           gdouble *bx,
                           gdouble *by)
{
    guint maskcol, maskrow;
    if (!_gwy_field_check_mask(field, mask, &masking,
                               col, row, width, height, &maskcol, &maskrow)
        || width < 2 || height < 2)
        goto fail;

    if (!(damping > 0.0)) {
        g_critical("Damping must be positive.");
        goto fail;
    }

    guint xres = field->xres;
    gdouble dx = gwy_field_dx(field), dy = gwy_field_dy(field);
    const gdouble *base = field->data + row*field->xres + col;

    // First determine the RMS of local slopes.
    // This gives us a scale we work on so the algorithm can adapt to both
    // surfaces with small and large ratio of heights and lateral dimensions.
    gdouble sigma2 = 0.0;
    guint n = 0;
    if (masking == GWY_MASK_IGNORE) {
        for (guint i = 0; i+1 < height; i++) {
            const gdouble *d1 = base + i*xres, *d2 = d1 + xres;
            for (guint j = width-1; j; j--, d1++, d2++) {
                gdouble vx = 0.5*(d1[1] + d2[1] - d1[0] - d2[0])/dx;
                gdouble vy = 0.5*(d2[0] + d2[1] - d1[0] - d1[1])/dy;
                sigma2 += vx*vx + vy*vy;
            }
        }
        n = width*height;
    }
    else {
        GwyMaskFieldIter iter1, iter2;
        const gboolean invert = (masking == GWY_MASK_EXCLUDE);
        for (guint i = 0; i+1 < height; i++) {
            const gdouble *d1 = base + i*xres, *d2 = d1 + xres;
            gwy_mask_field_iter_init(mask, iter1, maskcol, maskrow);
            gwy_mask_field_iter_init(mask, iter2, maskcol, maskrow+1);
            gboolean m2 = !!gwy_mask_field_iter_get(iter1) ^ invert;
            gboolean m4 = !!gwy_mask_field_iter_get(iter2) ^ invert;
            for (guint j = width-1; j; j--, d1++, d2++) {
                gboolean m1 = m2, m3 = m4;
                gwy_mask_field_iter_next(iter1);
                m2 = !!gwy_mask_field_iter_get(iter1) ^ invert;
                gwy_mask_field_iter_next(iter2);
                m4 = !!gwy_mask_field_iter_get(iter2) ^ invert;
                if (m1 & m2 & m3 & m4) {
                    gdouble vx = 0.5*(d1[1] + d2[1] - d1[0] - d2[0])/dx;
                    gdouble vy = 0.5*(d2[0] + d2[1] - d1[0] - d1[1])/dy;
                    sigma2 += vx*vx + vy*vy;
                    n++;
                }
            }
        }
    }

    if (n < 4)
        goto fail;

    sigma2 = sigma2/(n*damping);

    gdouble sumvx = 0.0, sumvy = 0.0, sumvz = 0.0;
    if (masking == GWY_MASK_IGNORE) {
        for (guint i = 0; i+1 < height; i++) {
            const gdouble *d1 = base + i*xres, *d2 = d1 + xres;
            for (guint j = width-1; j; j--, d1++, d2++) {
                gdouble vx = 0.5*(d1[1] + d2[1] - d1[0] - d2[0])/dx;
                gdouble vy = 0.5*(d2[0] + d2[1] - d1[0] - d1[1])/dy;
                gdouble q = exp((vx*vx + vy*vy)/sigma2);
                sumvx += vx/q;
                sumvy += vy/q;
                sumvz += 1.0/q;
            }
        }
    }
    else {
        GwyMaskFieldIter iter1, iter2;
        const gboolean invert = (masking == GWY_MASK_EXCLUDE);
        for (guint i = 0; i+1 < height; i++) {
            const gdouble *d1 = base + i*xres, *d2 = d1 + xres;
            gwy_mask_field_iter_init(mask, iter1, maskcol, maskrow);
            gwy_mask_field_iter_init(mask, iter2, maskcol, maskrow+1);
            gboolean m2 = !!gwy_mask_field_iter_get(iter1) ^ invert;
            gboolean m4 = !!gwy_mask_field_iter_get(iter2) ^ invert;
            for (guint j = width-1; j; j--, d1++, d2++) {
                gboolean m1 = m2, m3 = m4;
                gwy_mask_field_iter_next(iter1);
                m2 = !!gwy_mask_field_iter_get(iter1) ^ invert;
                gwy_mask_field_iter_next(iter2);
                m4 = !!gwy_mask_field_iter_get(iter2) ^ invert;
                if (m1 & m2 & m3 & m4) {
                    gdouble vx = 0.5*(d1[1] + d2[1] - d1[0] - d2[0])/dx;
                    gdouble vy = 0.5*(d2[0] + d2[1] - d1[0] - d1[1])/dy;
                    gdouble q = exp((vx*vx + vy*vy)/sigma2);
                    sumvx += vx/q;
                    sumvy += vy/q;
                    sumvz += 1.0/q;
                }
            }
        }
    }
    GWY_MAYBE_SET(bx, 0.5*sumvx/sumvz*field->xreal);
    GWY_MAYBE_SET(by, 0.5*sumvy/sumvz*field->yreal);
    return TRUE;

fail:
    GWY_MAYBE_SET(bx, 0.0);
    GWY_MAYBE_SET(by, 0.0);
    return FALSE;
}


#define __LIBGWY_FIELD_LEVEL_C__
#include "libgwy/libgwy-aliases.c"

/**
 * SECTION: field-level
 * @title: GwyField levelling
 * @short_description: Field levelling and background subtraction
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
