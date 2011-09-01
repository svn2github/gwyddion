/*
 *  $Id$
 *  Copyright (C) 2009-2010 David Nečas (Yeti).
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

#include <string.h>
#include "libgwy/macros.h"
#include "libgwy/math.h"
#include "libgwy/line-arithmetic.h"
#include "libgwy/mask-field-arithmetic.h"
#include "libgwy/mask-field-grains.h"
#include "libgwy/field-arithmetic.h"
#include "libgwy/field-level.h"
#include "libgwy/object-internal.h"
#include "libgwy/line-internal.h"
#include "libgwy/field-internal.h"
#include "libgwy/mask-field-internal.h"

enum { NONE = G_MAXUINT };

typedef enum {
    UP,
    RIGHT,
    DOWN,
    LEFT,
    NDIRECTIONS
} LaplaceDirection;

typedef struct {
    const gdouble *p;
    guint width;
    guint height;
    const gdouble *base;
    guint xres;
    guint yres;
    // Only with masking
    const GwyMaskField *mask;
    GwyMaskIter iter;
    guint maskcol;
    guint maskrow;
    gboolean invert;
    // Only with general polynomial
    guint nterms;
    guint xmaxpower;
    guint ymaxpower;
    const guint *xpowers;
    const guint *ypowers;
    const gdouble *xp;
    const gdouble *yp;
} PolyFitData;

typedef struct {
    const gdouble *p;
    // Only with masking
    GwyMaskIter iter;
    gboolean invert;
    guint count;
    guint degree;
} RowFitData;

/*
 * @len: The number of blocks.  (The allocated size is usually a bit larger.)
 * @n: Start of each neighbour in @k and @w, size @len+1.
 * @k: Indices of neighbours for laplacian calculation, allocated size is a
 *     multiple of @len but blocks are variable-size, given by @n.  These
 *     indices refer to these arrays, not the original grid.
 * @w: Coefficient blocks (for calculation of the second derivative from
 *     neigbours), allocated size is a
 *     multiple of @len but blocks are variable-size, given by @n.
 * @z: Values of the points, size @len.
 * @rhs: Right-hand-sides of the points, size @len.
 * @f: The difference between the second derivative and the value, size @len.
 * @v: Conjugate-gradients auxiliary vector, size @len.
 * @t: Conjugate-gradients auxiliary vector, size @len.
 * @gindex: Where the point is placed in the original data, size @len.
 */
/* FIXME: Most of the coefficient sets in @w are repeated many times.  We could
 * consilidate them, replacing 3-5 doubles (typically) per iterator with one
 * integer.
 */
typedef struct {
    gsize len;
    gsize int_size;
    gsize float_size;
    guint *n;
    guint *k;
    gdouble *w;
    gdouble *z;
    gdouble *rhs;
    gdouble *f;
    gdouble *v;
    gdouble *t;
    guint *gindex;
} LaplaceIterators;

typedef struct {
    gboolean is_virtual : 1;
    gboolean is_boundary : 1;
    gboolean is_rhs : 1;

    guint bdist;     // Distance of the boundary line where ∂z/∂x = 0
    guint step;
    guint neighbour;
    guint neighbour2;

    gdouble rhs;     // Remember the exterior data used for rhs
    gdouble weight;  // Coefficient before (z_neighbour - z_0)
    gdouble weight2; // Coefficient before (z_neighbour2 - z_0)
} LaplaceNeighbour;

static gboolean
plane_fit(guint id,
          gdouble *fvalues,
          gdouble *value,
          gpointer user_data)
{
    PolyFitData *data = (PolyFitData*)user_data;
    guint i = id/data->width;
    guint j = id % data->width;
    if (j == 0)
        data->p = data->base + i*data->xres;
    gdouble x = 2*j/(data->xres - 1.0) - 1.0;
    gdouble y = 2*i/(data->yres - 1.0) - 1.0;
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
    PolyFitData *data = (PolyFitData*)user_data;
    guint i = id/data->width;
    guint j = id % data->width;
    if (j == 0) {
        data->p = data->base + i*data->xres;
        gwy_mask_field_iter_init(data->mask, data->iter,
                                 data->maskcol, data->maskrow+i);
    }
    gboolean ok = (!gwy_mask_iter_get(data->iter) == data->invert);
    if (ok) {
        gdouble x = 2*j/(data->xres - 1.0) - 1.0;
        gdouble y = 2*i/(data->yres - 1.0) - 1.0;
        fvalues[0] = 1.0;
        fvalues[1] = x;
        fvalues[2] = y;
        *value = *data->p;
    }
    gwy_mask_iter_next(data->iter);
    data->p++;
    return ok;
}

/**
 * gwy_field_fit_plane:
 * @field: A two-dimensional data field.
 * @fpart: (allow-none):
 *         Area in @field to process.  Pass %NULL to process entire @field.
 * @mask: (allow-none):
 *        Mask specifying which values to take into account/exclude, or %NULL.
 * @masking: Masking mode to use (has any effect only with non-%NULL @mask).
 * @a: Location to store the constant coefficient, or %NULL.
 * @bx: Location to store the x coefficient, or %NULL.
 * @by: Location to store the y coefficient, or %NULL.
 *
 * Fits a plane through a field.
 *
 * The coefficients correspond to coordinates normalised to [-1,1], see the
 * introduction for details.
 *
 * Returns: %TRUE if the plane was fitted, %FALSE if the there were too few
 *          points to fit a plane or there were no points with at least two
 *          different x or y coordinates.  The coefficients @a, @bx and @by are
 *          set in all cases, on failure, they are set to zeroes.
 **/
gboolean
gwy_field_fit_plane(const GwyField *field,
                    const GwyFieldPart *fpart,
                    const GwyMaskField *mask,
                    GwyMaskingType masking,
                    gdouble *a, gdouble *bx, gdouble *by)
{
    guint col, row, width, height, maskcol, maskrow;
    if (!gwy_field_check_mask(field, fpart, mask, &masking,
                              &col, &row, &width, &height, &maskcol, &maskrow)
        || width < 2 || height < 2)
        goto fail;

    PolyFitData data = {
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
        data.invert = (masking == GWY_MASK_EXCLUDE);
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
 * gwy_field_subtract_plane:
 * @field: A two-dimensional data field.
 * @a: Constant coefficient.
 * @bx: X-coefficient.
 * @by: Y-coefficient.
 *
 * Subtracts a plane from a field.
 *
 * The coefficients correspond to coordinates normalised to [-1,1], see the
 * introduction for details.
 **/
void
gwy_field_subtract_plane(GwyField *field,
                         gdouble a, gdouble bx, gdouble by)
{
    g_return_if_fail(GWY_IS_FIELD(field));
    a -= bx + by;
    bx *= (field->xres > 1) ? 2.0/(field->xres - 1) : 0.0;
    by *= (field->yres > 1) ? 2.0/(field->yres - 1) : 0.0;
    gdouble *d = field->data;
    for (guint i = 0; i < field->yres; i++) {
        for (guint j = 0; j < field->xres; j++, d++)
            *d -= a + j*bx + i*by;
    }
    gwy_field_invalidate(field);
}

/**
 * gwy_field_inclination:
 * @field: A two-dimensional data field.
 * @fpart: (allow-none):
 *         Area in @field to process.  Pass %NULL to process entire @field.
 * @mask: (allow-none):
 *        Mask specifying which values to take into account/exclude, or %NULL.
 * @masking: Masking mode to use (has any effect only with non-%NULL @mask).
 * @damping: Damping factor determining the range of deviations of local normal
 *           vectors from (0,0,1) that non-negligibly contribute to the result.
 *           It must be positive, a reasonable value may be e.g. 20.
 * @bx: Location to store the x coefficient, or %NULL.
 * @by: Location to store the y coefficient, or %NULL.
 *
 * Fits a plane through a field by straighenting up facets.
 *
 * The coefficients correspond to coordinates normalised to [-1,1], see the
 * introduction for details.
 *
 * Although @bx and @by have the same meaning as in gwy_field_fit_plane()
 * they are calculated differently.  Instead of fitting a plane through the
 * points, local factes are determined from 2×2 areas and averaged, leading to
 * a mean normal to the surface.
 *
 * If @damping was zero (which is not permitted), the normal would correspond
 * to the mean normal to the mean plane and the coefficients found by this
 * method would be equal to those calculated by gwy_field_fit_plane().
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
gwy_field_inclination(const GwyField *field,
                      const GwyFieldPart *fpart,
                      const GwyMaskField *mask,
                      GwyMaskingType masking,
                      gdouble damping,
                      gdouble *bx,
                      gdouble *by)
{
    guint col, row, width, height, maskcol, maskrow;
    if (!gwy_field_check_mask(field, fpart, mask, &masking,
                              &col, &row, &width, &height, &maskcol, &maskrow)
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
        GwyMaskIter iter1, iter2;
        const gboolean invert = (masking == GWY_MASK_EXCLUDE);
        for (guint i = 0; i+1 < height; i++) {
            const gdouble *d1 = base + i*xres, *d2 = d1 + xres;
            gwy_mask_field_iter_init(mask, iter1, maskcol, maskrow);
            gwy_mask_field_iter_init(mask, iter2, maskcol, maskrow+1);
            gboolean m2 = !gwy_mask_iter_get(iter1) == invert;
            gboolean m4 = !gwy_mask_iter_get(iter2) == invert;
            for (guint j = width-1; j; j--, d1++, d2++) {
                gboolean m1 = m2, m3 = m4;
                gwy_mask_iter_next(iter1);
                m2 = !gwy_mask_iter_get(iter1) == invert;
                gwy_mask_iter_next(iter2);
                m4 = !gwy_mask_iter_get(iter2) == invert;
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
        GwyMaskIter iter1, iter2;
        const gboolean invert = (masking == GWY_MASK_EXCLUDE);
        for (guint i = 0; i+1 < height; i++) {
            const gdouble *d1 = base + i*xres, *d2 = d1 + xres;
            gwy_mask_field_iter_init(mask, iter1, maskcol, maskrow);
            gwy_mask_field_iter_init(mask, iter2, maskcol, maskrow+1);
            gboolean m2 = !gwy_mask_iter_get(iter1) == invert;
            gboolean m4 = !gwy_mask_iter_get(iter2) == invert;
            for (guint j = width-1; j; j--, d1++, d2++) {
                gboolean m1 = m2, m3 = m4;
                gwy_mask_iter_next(iter1);
                m2 = !gwy_mask_iter_get(iter1) == invert;
                gwy_mask_iter_next(iter2);
                m4 = !gwy_mask_iter_get(iter2) == invert;
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

static gboolean
poly_fit(guint id,
         gdouble *fvalues,
         gdouble *value,
         gpointer user_data)
{
    PolyFitData *data = (PolyFitData*)user_data;
    guint i = id/data->width;
    guint j = id % data->width;
    if (j == 0)
        data->p = data->base + i*data->xres;
    const gdouble *xp = data->xp + j*(data->xmaxpower + 1);
    const gdouble *yp = data->yp + i*(data->ymaxpower + 1);
    for (guint k = 0; k < data->nterms; k++)
        fvalues[k] = xp[data->xpowers[k]] * yp[data->ypowers[k]];
    *value = *data->p;
    data->p++;
    return TRUE;
}

static gboolean
poly_fit_mask(guint id,
              gdouble *fvalues,
              gdouble *value,
              gpointer user_data)
{
    PolyFitData *data = (PolyFitData*)user_data;
    guint i = id/data->width;
    guint j = id % data->width;
    if (j == 0) {
        data->p = data->base + i*data->xres;
        gwy_mask_field_iter_init(data->mask, data->iter,
                                 data->maskcol, data->maskrow+i);
    }
    gboolean ok = (!gwy_mask_iter_get(data->iter) == data->invert);
    if (ok) {
        const gdouble *xp = data->xp + j*(data->xmaxpower + 1);
        const gdouble *yp = data->yp + i*(data->ymaxpower + 1);
        for (guint k = 0; k < data->nterms; k++)
            fvalues[k] = xp[data->xpowers[k]] * yp[data->ypowers[k]];
        *value = *data->p;
    }
    gwy_mask_iter_next(data->iter);
    data->p++;
    return ok;
}

static gdouble*
enumerate_powers(const guint *powers, guint nterms,
                 guint first, guint len, guint dim,
                 guint *pmaxpower)
{
    guint maxpower = powers[0];
    for (guint k = 1; k < nterms; k++) {
        if (powers[k] > maxpower)
            maxpower = powers[k];
    }
    *pmaxpower = maxpower;

    gdouble *powertable = g_new(gdouble, (maxpower + 1)*len);
    gdouble *p = powertable;
    for (guint i = 0; i < len; i++) {
        gdouble t = 2*(i + first)/(dim - 1.0) - 1.0;
        gdouble tp = 1.0;
        for (guint j = 0; j < maxpower; j++, p++) {
            *p = tp;
            tp *= t;
        }
        *p = tp;
        p++;
    }

    return powertable;
}

/**
 * gwy_field_fit_poly:
 * @field: A two-dimensional data field.
 * @fpart: (allow-none):
 *         Area in @field to process.  Pass %NULL to process entire @field.
 * @mask: (allow-none):
 *        Mask specifying which values to take into account/exclude, or %NULL.
 * @masking: Masking mode to use (has any effect only with non-%NULL @mask).
 * @xpowers: (array length=nterms):
 *           Array of length @nterms containing the powers of @x to fit.
 * @ypowers: (array length=nterms):
 *           Array of length @nterms containing the powers of @y to fit.
 * @nterms: Number of polynomial terms, i.e. the length of @xpowers,
 *          @ypowers and @coeffs.
 * @coeffs: (out) (array length=nterms):
 *          Array of length @nterms to store the individual term coefficients
 *          to.
 *
 * Fits a general polynomial through a field.
 *
 * The coefficients correspond to coordinates normalised to [-1,1], see the
 * introduction for details.
 *
 * The arrays @xpowers and @ypowers define the individual terms to fit.  For
 * instance the polynomial
 * @a + @b<subscript>x</subscript>@x + @b<subscript>y</subscript>@y + @c @x²
 * contains terms with the following (@x,@y) powers: (0,0), (1,0), (0,1) and
 * (2,0).  Therefore, to fit this polynomial and obtain
 * [@a, @b<subscript>x</subscript>, @b<subscript>y</subscript>, @c] in @coeffs,
 * the arrays should be set to
 * @xpowers = [0, 1, 0, 2] and @ypowers = [0, 0, 1, 0].
 *
 * Returns: %TRUE if the polynomial was fitted, %FALSE in degenerate cases,
 *          i.e. when points do not uniquely determine the polynomial with the
 *          requested terms.  For high polynomial degrees %FALSE can also be
 *          returned because the terms are no longer numerically independent.
 **/
gboolean
gwy_field_fit_poly(const GwyField *field,
                   const GwyFieldPart *fpart,
                   const GwyMaskField *mask,
                   GwyMaskingType masking,
                   const guint *xpowers, const guint *ypowers,
                   guint nterms,
                   gdouble *coeffs)
{
    if (!nterms || !coeffs)
        return TRUE;

    guint col, row, width, height, maskcol, maskrow;
    if (!gwy_field_check_mask(field, fpart, mask, &masking,
                              &col, &row, &width, &height, &maskcol, &maskrow))
        goto fail;

    g_return_val_if_fail(xpowers && ypowers, FALSE);

    guint xmaxpower, ymaxpower;
    gdouble *xp = enumerate_powers(xpowers, nterms, col, width, field->xres,
                                   &xmaxpower);
    gdouble *yp = enumerate_powers(ypowers, nterms, row, height, field->yres,
                                   &ymaxpower);

    PolyFitData data = {
        .width = width,
        .height = height,
        .base = field->data + row*field->xres + col,
        .xres = field->xres,
        .yres = field->yres,
        .mask = mask,
        .maskcol = maskcol,
        .maskrow = maskrow,
        .nterms = nterms,
        .xmaxpower = xmaxpower,
        .ymaxpower = ymaxpower,
        .xpowers = xpowers,
        .ypowers = ypowers,
        .xp = xp,
        .yp = yp,
    };
    gboolean ok;
    if (masking == GWY_MASK_IGNORE) {
        ok = gwy_linear_fit(poly_fit, width*height, coeffs, nterms, NULL,
                            &data);
    }
    else {
        data.invert = (masking == GWY_MASK_EXCLUDE);
        ok = gwy_linear_fit(poly_fit_mask, width*height, coeffs, nterms, NULL,
                            &data);
    }
    g_free(xp);
    g_free(yp);
    if (!ok)
        goto fail;

    return TRUE;

fail:
    gwy_clear(coeffs, nterms);
    return FALSE;
}

/**
 * gwy_field_subtract_poly:
 * @field: A two-dimensional data field.
 * @xpowers: (array length=nterms):
 *           Array of length @nterms containing the powers of @x to subtract.
 * @ypowers: (array length=nterms):
 *           Array of length @nterms containing the powers of @y to subtract.
 * @nterms: Number of polynomial terms, i.e. the length of @xpowers,
 *          @ypowers and @coeffs.
 * @coeffs: (array length=nterms):
 *          Array of length @nterms with the individual term coefficients.
 *
 * Subtracts a general polynomial from a field.
 *
 * The coefficients correspond to coordinates normalised to [-1,1], see the
 * introduction for details.  The meaning of @xpowers and @ypowers is described
 * in detail in gwy_field_fit_poly().
 **/
void
gwy_field_subtract_poly(GwyField *field,
                        const guint *xpowers, const guint *ypowers,
                        guint nterms,
                        const gdouble *coeffs)
{
    if (!nterms)
        return;
    g_return_if_fail(GWY_IS_FIELD(field));
    g_return_if_fail(xpowers && ypowers);
    g_return_if_fail(coeffs);

    guint xmaxpower, ymaxpower;
    gdouble *xp = enumerate_powers(xpowers, nterms,
                                   0, field->xres, field->xres, &xmaxpower);
    gdouble *yp = enumerate_powers(ypowers, nterms,
                                   0, field->yres, field->yres, &ymaxpower);
    gdouble *d = field->data;
    for (guint i = 0; i < field->yres; i++) {
        const gdouble *y = yp + i*(ymaxpower + 1);
        for (guint j = 0; j < field->xres; j++, d++) {
            const gdouble *x = xp + j*(xmaxpower + 1);
            gdouble s = 0.0;
            for (guint k = 0; k < nterms; k++)
                s += coeffs[k] * x[xpowers[k]] * y[ypowers[k]];
            *d -= s;
        }
    }
    g_free(xp);
    g_free(yp);
    gwy_field_invalidate(field);
}

/**
 * gwy_field_shift_rows:
 * @field: A two-dimensional data field.
 * @shifts: Line with the shifts.  Its resolution must match either the
 *          y-resolution of @field.  Physical properties of @shifts,
 *          such as length, offset and units, are ignored.
 *
 * Shifts values in rows of a field by specified values.
 *
 * The shifts are absolute, i.e. values in each row is simply shifted by
 * the corresponding number in @shifts.  If you have relative shifts, i.e.
 * always with respect to the previous row, you can use gwy_line_accumulate()
 * to transform them to absolute shifts first.
 **/
void
gwy_field_shift_rows(GwyField *field,
                     const GwyLine *shifts)
{
    g_return_if_fail(GWY_IS_FIELD(field));
    g_return_if_fail(GWY_IS_LINE(shifts));
    g_return_if_fail(shifts->res == field->yres);
    for (guint i = 0; i < field->yres; i++) {
        gdouble s = shifts->data[i];
        if (s) {
            gdouble *d = field->data + i*field->xres;
            for (guint j = field->xres; j; j--, d++)
                *d -= s;
        }
    }
    gwy_field_invalidate(field);
}

// These functions already calculate *corrective* shifts, not direct ones.
static gboolean
fit_row_mean(const GwyField *field,
             guint row,
             const GwyMaskField *mask,
             GwyMaskingType masking,
             guint maskrow,
             guint min_freedom,
             gdouble *value)
{
    const gdouble *d = field->data + row*field->xres;
    gdouble sum = 0.0;
    guint count = 0;
    if (masking == GWY_MASK_IGNORE) {
        for (guint j = field->xres; j; j--, d++)
            sum += *d;
        count = field->xres;
    }
    else {
        const gboolean invert = (masking == GWY_MASK_EXCLUDE);
        GwyMaskIter iter;
        gwy_mask_field_iter_init(mask, iter, 0, maskrow);
        for (guint j = field->xres; j; j--, d++) {
            if (!gwy_mask_iter_get(iter) == invert) {
                sum += *d;
                count++;
            }
            gwy_mask_iter_next(iter);
        }
    }
    if (count >= 1 + min_freedom) {
        *value = sum/count;
        return TRUE;
    }
    return FALSE;
}

static guint
fit_row_median(const GwyField *field,
               guint row,
               const GwyMaskField *mask,
               GwyMaskingType masking,
               guint maskrow,
               guint min_freedom,
               gdouble *buffer,
               gdouble *value)
{
    const gdouble *d = field->data + row*field->xres;
    guint count;
    if (masking == GWY_MASK_IGNORE) {
        gwy_assign(buffer, d, field->xres);
        count = field->xres;
    }
    else {
        gdouble *p = buffer;
        const gboolean invert = (masking == GWY_MASK_EXCLUDE);
        GwyMaskIter iter;
        gwy_mask_field_iter_init(mask, iter, 0, maskrow);
        for (guint j = field->xres; j; j--, d++) {
            if (!gwy_mask_iter_get(iter) == invert)
                *(p++) = *d;
            gwy_mask_iter_next(iter);
        }
        count = p - buffer;
    }
    if (count >= 1 + min_freedom) {
        *value = gwy_math_median(buffer, count);
        return TRUE;
    }
    return FALSE;
}

static void
fit_row_mean_diff(const GwyField *field,
                  guint row,
                  const GwyMaskField *mask,
                  GwyMaskingType masking,
                  guint maskrow,
                  guint min_freedom,
                  gdouble *shift)
{
    const gdouble *d1 = field->data + row*field->xres;
    const gdouble *d2 = d1 + field->xres;
    gdouble sum = 0.0;
    guint count = 0;
    if (masking == GWY_MASK_IGNORE) {
        for (guint j = field->xres; j; j--, d1++, d2++)
            sum += *d1 - *d2;
        count = field->xres;
    }
    else {
        const gboolean invert = (masking == GWY_MASK_EXCLUDE);
        GwyMaskIter iter1, iter2;
        gwy_mask_field_iter_init(mask, iter1, 0, maskrow);
        gwy_mask_field_iter_init(mask, iter2, 0, maskrow+1);
        for (guint j = field->xres; j; j--, d1++, d2++) {
            if (!gwy_mask_iter_get(iter1) == invert
                && !gwy_mask_iter_get(iter2) == invert) {
                sum += *d1 - *d2;
                count++;
            }
            gwy_mask_iter_next(iter1);
            gwy_mask_iter_next(iter2);
        }
    }
    *shift = (count >= 1 + min_freedom) ? sum/count : 0.0;
}

static void
fit_row_median_diff(const GwyField *field,
                    guint row,
                    const GwyMaskField *mask,
                    GwyMaskingType masking,
                    guint maskrow,
                    guint min_freedom,
                    gdouble *buffer,
                    gdouble *shift)
{
    const gdouble *d1 = field->data + row*field->xres;
    const gdouble *d2 = d1 + field->xres;
    gdouble *p = buffer;
    if (masking == GWY_MASK_IGNORE) {
        for (guint j = field->xres; j; j--, d1++, d2++)
            *(p++) = *d1 - *d2;
    }
    else {
        const gboolean invert = (masking == GWY_MASK_EXCLUDE);
        GwyMaskIter iter1, iter2;
        gwy_mask_field_iter_init(mask, iter1, 0, maskrow);
        gwy_mask_field_iter_init(mask, iter2, 0, maskrow+1);
        for (guint j = field->xres; j; j--, d1++, d2++) {
            if (!gwy_mask_iter_get(iter1) == invert
                && !gwy_mask_iter_get(iter2) == invert)
                *(p++) = *d1 - *d2;
            gwy_mask_iter_next(iter1);
            gwy_mask_iter_next(iter2);
        }
    }
    guint count = p - buffer;
    *shift = (count >= 1 + min_freedom) ? gwy_math_median(buffer, count) : 0.0;
}

// Transform a line containing absolute values for row to line containing
// corrective differences where both rows are good and 0s where we cannot
// calculate the difference because either row is bad.
static void
find_shifts_of_good_rows(GwyLine *shifts,
                         GwyMaskIter iter)
{
    gboolean curr = gwy_mask_iter_get(iter);
    gdouble *d = shifts->data;
    for (guint i = shifts->res-1; i; i--, d++) {
        gboolean prev = curr;
        gwy_mask_iter_next(iter);
        curr = gwy_mask_iter_get(iter);
        if (curr && prev)
            *d -= *(d+1);
        else
            *d = 0.0;
    }
    d = shifts->data + shifts->res-2;
    for (guint i = shifts->res-1; i; i--, d--)
        *(d+1) = *d;
    shifts->data[0] = 0.0;
}

/**
 * gwy_field_find_row_shifts:
 * @field: A two-dimensional data field.
 * @mask: (allow-none):
 *        Mask specifying which values to take into account/exclude, or %NULL.
 * @masking: Masking mode to use (has any effect only with non-%NULL @mask).
 * @method: Row shift finding method.
 * @min_freedom: Minimum number of points in a row, above the number required
 *               by the specific method, required to shift it.  Rows that do
 *               not contain at least this number of free points due to masking
 *               are not shifted relatively to their neighbours.
 *
 * Finds shifts of field rows relative to their neighbour rows.
 *
 * Note this method calculates relative shifts, although gwy_field_shift_rows()
 * takes absolute shifts.
 *
 * Returns: (transfer full):
 *          A newly created one-dimensional data line with the same dimension
 *          as the field vertical size, containing the relative shifts.
 **/
GwyLine*
gwy_field_find_row_shifts(const GwyField *field,
                          const GwyMaskField *mask,
                          GwyMaskingType masking,
                          GwyRowShiftMethod method,
                          guint min_freedom)
{
    guint col, row, width, height, maskcol, maskrow;
    if (!gwy_field_check_mask(field, NULL, mask, &masking,
                              &col, &row, &width, &height, &maskcol, &maskrow))
        return NULL;

    GwyLine *shifts = gwy_line_new_sized(field->yres, FALSE);
    if (field->yres < 2) {
        gwy_line_clear(shifts, NULL);
        goto fail;
    }

    gsize bufsize = field->xres * sizeof(gdouble);
    gsize masksize = ((field->yres + 0x1f) >> 5) * sizeof(guint32);

    if (method == GWY_ROW_SHIFT_MEAN) {
        // XXX: This is a fake one-dimensional mask
        guint32 *goodrows = g_slice_alloc0(masksize);
        GwyMaskIter iter = { goodrows, FIRST_BIT };
        for (guint i = 0; i < field->yres; i++) {
            if (fit_row_mean(field, i, mask, masking, maskrow, min_freedom,
                             shifts->data + i))
                gwy_mask_iter_set(iter, TRUE);
            gwy_mask_iter_next(iter);
        }
        iter = (GwyMaskIter){ goodrows, FIRST_BIT };
        find_shifts_of_good_rows(shifts, iter);
        g_slice_free1(masksize, goodrows);
    }
    else if (method == GWY_ROW_SHIFT_MEDIAN) {
        // XXX: This is a fake one-dimensional mask
        guint32 *goodrows = g_slice_alloc0(masksize);
        gdouble *buffer = g_slice_alloc(bufsize);
        GwyMaskIter iter = { goodrows, FIRST_BIT };
        for (guint i = 0; i < field->yres; i++) {
            if (fit_row_median(field, i, mask, masking, maskrow, min_freedom,
                               buffer, shifts->data + i))
                gwy_mask_iter_set(iter, TRUE);
            gwy_mask_iter_next(iter);
        }
        iter = (GwyMaskIter){ goodrows, FIRST_BIT };
        find_shifts_of_good_rows(shifts, iter);
        g_slice_free1(masksize, goodrows);
        g_slice_free1(bufsize, buffer);
    }
    else if (method == GWY_ROW_SHIFT_MEAN_DIFF) {
        shifts->data[0] = 0.0;
        for (guint i = 0; i < field->yres-1; i++)
            fit_row_mean_diff(field, i, mask, masking, maskrow, min_freedom,
                              shifts->data + i+1);
    }
    else if (method == GWY_ROW_SHIFT_MEDIAN_DIFF) {
        shifts->data[0] = 0.0;
        gdouble *buffer = g_slice_alloc(bufsize);
        for (guint i = 0; i < field->yres-1; i++)
            fit_row_median_diff(field, i, mask, masking, maskrow, min_freedom,
                                buffer, shifts->data + i+1);
        g_slice_free1(bufsize, buffer);
    }
    else {
        g_critical("Unknown row shift method %u", method);
        gwy_line_clear(shifts, NULL);
    }

fail:
    shifts->res = field->yres;
    shifts->off = field->yoff;
    ASSIGN_UNITS(shifts->priv->unit_x, field->priv->unit_xy);
    ASSIGN_UNITS(shifts->priv->unit_y, field->priv->unit_z);

    return shifts;
}

static gboolean
promote(const guint *levels, guint *buffer,
        guint xres, guint yres,
        guint level, guint step)
{
    guint nx = (xres + step-1)/step, ny = (yres + step-1)/step;
    guint vstep = xres*step;
    gboolean ok = FALSE;

    if (nx < 3 || ny < 3)
        return ok;

    for (guint i = 0; i < ny; i++) {
        for (guint j = 0; j < nx; j++) {
            guint k = (i*xres + j)*step;

            if (levels[k] == level
                && (!i || levels[k-vstep] == level)
                && (!j || levels[k-step] == level)
                && (j == nx-1 || levels[k+step] == level)
                && (i == ny-1 || levels[k+vstep] == level)) {
                buffer[k] = level+1;
                ok = TRUE;
            }
        }
    }

    return ok;
}

static void
demote(const guint *levels, guint *buffer,
       guint xres, guint yres,
       guint level, guint step)
{
    guint nx = (xres + step-1)/step, ny = (yres + step-1)/step;
    guint vstep = xres*step;

    if (nx < 3 || ny < 3)
        return;

    for (guint i = 1; i < ny-1; i++) {
        for (guint j = 1; j < nx-1; j++) {
            guint k = (i*xres + j)*step;

            if (levels[k] == level
                && (levels[k-vstep-step] == level-1
                    || levels[k-vstep] == level-1
                    || levels[k-vstep+step] == level-1
                    || levels[k-step] == level-1
                    || levels[k+step] == level-1
                    || levels[k+vstep-step] == level-1
                    || levels[k+vstep] == level-1
                    || levels[k+vstep+step] == level-1)) {
                if (buffer[k-vstep-step] > level)
                    buffer[k-vstep-step] = level;
                if (buffer[k-vstep] > level)
                    buffer[k-vstep] = level;
                if (buffer[k-vstep+step] > level)
                    buffer[k-vstep+step] = level;
                if (buffer[k-step] > level)
                    buffer[k-step] = level;
                if (buffer[k+step] > level)
                    buffer[k+step] = level;
                if (buffer[k+vstep-step] > level)
                    buffer[k+vstep-step] = level;
                if (buffer[k+vstep] > level)
                    buffer[k+vstep] = level;
                if (buffer[k+vstep+step] > level)
                    buffer[k+vstep+step] = level;
            }
        }
    }
}

static gboolean
reduce(const guint *levels, guint *buffer,
       guint xres, guint yres,
       guint level, guint step)
{
    g_return_val_if_fail(step % 2 == 0, FALSE);

    guint nx = (xres + step-1)/step, ny = (yres + step-1)/step;
    guint halfstep = step/2;
    guint vstep = xres*step, vhalfstep = xres*halfstep;
    gboolean ok = FALSE;
    gboolean right = (nx - 1)*step + halfstep < xres;
    gboolean down = (ny - 1)*step + halfstep < yres;

    if (nx < 3 || ny < 3)
        return ok;

    for (guint i = 0; i < ny; i++) {
        for (guint j = 0; j < nx; j++) {
            guint k = (i*xres + j)*step;

            if (levels[k] == level
                && (!i || !j || levels[k-vstep-step] >= level)
                && (!i || levels[k-vstep] == level)
                && (!i || j == nx-1 || levels[k-vstep+step] >= level)
                && (!j || levels[k-step] == level)
                && (j == nx-1 || levels[k+step] == level)
                && (i == ny-1 || !j || levels[k+vstep-step] >= level)
                && (i == ny-1 || levels[k+vstep] == level)
                && (i == ny-1 || j == nx-1 || levels[k+vstep+step] >= level)) {
                buffer[k] = level+1;
                if (i && j)
                    buffer[k-vhalfstep-halfstep] = NONE;
                if (i)
                    buffer[k-vhalfstep] = NONE;
                if (i && (right || j < nx-1))
                    buffer[k-vhalfstep+halfstep] = NONE;
                if (j)
                    buffer[k-halfstep] = NONE;
                if (right || j < nx-1)
                    buffer[k+halfstep] = NONE;
                if ((down || i < ny-1) && j)
                    buffer[k+vhalfstep-halfstep] = NONE;
                if (down || i < ny-1)
                    buffer[k+vhalfstep] = NONE;
                if ((down || i < ny-1) && (right || j < nx-1))
                    buffer[k+vhalfstep+halfstep] = NONE;
                ok = TRUE;
            }
        }
    }

    return ok;
}

static void
remove_spikes(guint *levels,
              guint xres, guint yres,
              guint level, guint step)
{
    guint nx = (xres + step-1)/step, ny = (yres + step-1)/step;

    if (nx < 3 || ny < 3)
        return;

    for (guint i = 1; i < ny-1; i++) {
        for (guint j = 1; j < nx-1; j++) {
            guint k = (i*xres + j)*step;

            if (levels[k] == level) {
                guint su = (levels[k-xres*step] == NONE),
                      sd = (levels[k+xres*step] == NONE),
                      sl = (levels[k-step] == NONE),
                      sr = (levels[k+step] == NONE);

                if ((su & sd & ~sl & ~sr) || (~su & ~sd & sl & sr))
                    levels[k] = NONE;
            }
        }
    }
}

static guint
build_levels(guint *levels, guint *buffer,
             guint xres, guint yres)
{
    guint step = 1, level = 0;

    gwy_assign(buffer, levels, xres*yres);
    for (;;) {
        // Promote odd levels to one-greater even levels if they do not touch
        // lower-values levels.
        level++;
        if (!promote(levels, buffer, xres, yres, level, step))
            break;

        // Ensure a dense representation near the boundary.
        if (level == 1) {
            gwy_assign(levels, buffer, xres*yres);
            demote(levels, buffer, xres, yres, level, step);
        }

        gwy_assign(levels, buffer, xres*yres);
        // Clear the space around even levels and promote them to one-greater
        // odd levels if the do not touch lower levels.
        level++;
        step *= 2;
        if (!reduce(levels, buffer, xres, yres, level, step))
            break;

        // Remove even levels that would have to be interpolated from two
        // opposide sides (they appear when both sides of it are promoted but
        // not the point itself).
        if (level > 1)
            remove_spikes(buffer, xres, yres, level, step/2);

        gwy_assign(levels, buffer, xres*yres);
    }

    return level;
}

static guint
count_grid_points(const guint *levels,
                  guint xres, guint yres)
{
    guint npoints = 0;

    for (guint k = 0; k < xres*yres; k++) {
        if (levels[k] && levels[k] != NONE)
            npoints++;
    }

    return npoints;
}

static void
build_grid_index(const guint *levels,
                 guint xres, guint yres,
                 guint *gindex,
                 guint *revindex)
{
    guint n = 0;

    for (guint k = 0; k < xres*yres; k++) {
        if (levels[k] && levels[k] != NONE) {
            revindex[k] = n;
            gindex[n++] = k;
        }
        else
            revindex[k] = NONE;
    }
}

static void
laplace_iterators_setup(LaplaceIterators *iterators,
                        guint maxneighbours)
{
    gsize len = iterators->len;

    iterators->k = iterators->n + (len + 2);
    iterators->gindex = iterators->k + maxneighbours*len;

    iterators->rhs = iterators->z + len;
    iterators->f = iterators->rhs + len;
    iterators->v = iterators->f + len;
    iterators->t = iterators->v + len;
    iterators->w = iterators->t + len;
}

static void
laplace_iterators_resize(LaplaceIterators *iterators,
                         guint len,
                         guint maxneighbours)
{
    gsize int_size = (maxneighbours + 2)*len + 2;
    gsize float_size = (maxneighbours + 5)*len;

    iterators->len = len;

    if (int_size > iterators->int_size) {
        if (G_UNLIKELY(iterators->int_size))
            g_warning("Laplace iterators need to be enlarged (int).");
        GWY_FREE(iterators->n);
        iterators->n = g_new0(guint, int_size);
        iterators->int_size = int_size;
    }
    else
        gwy_clear(iterators->n, int_size);

    if (float_size > iterators->float_size) {
        if (G_UNLIKELY(iterators->float_size))
            g_warning("Laplace iterators need to be enlarged (float).");
        GWY_FREE(iterators->z);
        iterators->z = g_new0(gdouble, float_size);
        iterators->float_size = float_size;
    }
    else
        gwy_clear(iterators->z, float_size);

    laplace_iterators_setup(iterators, maxneighbours);
}

static LaplaceIterators*
laplace_iterators_new(guint len, guint maxneighbours)
{
    LaplaceIterators *iterators = g_slice_new0(LaplaceIterators);
    laplace_iterators_resize(iterators, len, maxneighbours);
    return iterators;
}

static void
laplace_iterators_free(LaplaceIterators *iterators)
{
    g_free(iterators->z);
    g_free(iterators->n);
    g_slice_free(LaplaceIterators, iterators);
}

static void
analyse_neighbour_direction(const guint *levels,
                            const gdouble *data,
                            gint xres, gint yres,
                            gint xstep, gint ystep,
                            gint j, gint i,
                            const guint *revindex,
                            LaplaceNeighbour *nd)
{
    gint ineigh, jneigh, step;
    guint kk;

    gwy_clear(nd, 1);
    step = MAX(ABS(xstep), ABS(ystep));
    ineigh = i + ystep;
    jneigh = j + xstep;

    // 1 Primary neighbour.
    // 1.a Neumann boundary.
    // The upper and left boundaries are always aligned.
    if (ineigh < 0) {
        g_assert(i == 0);
        nd->is_boundary = TRUE;
        nd->step = step;
        return;
    }
    if (jneigh < 0) {
        g_assert(j == 0);
        nd->is_boundary = TRUE;
        nd->step = step;
        return;
    }
    // The other boundaries can be unaligned.
    if (ineigh >= yres) {
        nd->is_boundary = TRUE;
        nd->bdist = yres-1 - i;
        nd->step = step;
        return;
    }
    if (jneigh >= xres) {
        nd->is_boundary = TRUE;
        nd->bdist = xres-1 - j;
        nd->step = step;
        return;
    }

    kk = ineigh*xres + jneigh;

    // 1.b Dirichlet boundary.
    if (!levels[kk]) {
        g_assert(step == 1);
        nd->is_rhs = TRUE;
        nd->step = step;
        nd->rhs = data[kk];
        return;
    }

    // 1.c Interior.
    if (levels[kk] != NONE) {
        nd->neighbour = revindex[kk];
        nd->step = step;
        return;
    }

    // 2 Secondary neighbour.
    ineigh = i + 2*ystep;
    jneigh = j + 2*xstep;

    // 2.a Neumann boundary.
    // The upper and left boundaries are always aligned.
    if (ineigh < 0) {
        g_assert(i == 0);
        nd->is_boundary = TRUE;
        nd->step = 2*step;
        return;
    }
    if (jneigh < 0) {
        g_assert(j == 0);
        nd->is_boundary = TRUE;
        nd->step = 2*step;
        return;
    }
    // The other boundaries can be unaligned.
    if (ineigh >= yres) {
        nd->is_boundary = TRUE;
        nd->bdist = yres-1 - i;
        nd->step = 2*step;
        return;
    }
    if (jneigh >= xres) {
        nd->is_boundary = TRUE;
        nd->bdist = xres-1 - j;
        nd->step = 2*step;
        return;
    }

    kk = ineigh*xres + jneigh;
    g_assert(levels[kk]);    // Dirichlet boundary is always at step one.

    // 2.b Interior.
    if (levels[kk] != NONE) {
        nd->neighbour = revindex[kk];
        nd->step = 2*step;
        return;
    }

    // 3 Virtual neighbour.
    gint xorthostep = xstep ? 0 : ABS(ystep),
         yorthostep = ystep ? 0 : ABS(xstep);
    ineigh = i + 2*ystep - yorthostep;
    jneigh = j + 2*xstep - xorthostep;
    // The upper and left boundaries are always aligned.
    g_assert(ineigh >= 0);
    g_assert(jneigh >= 0);
    kk = ineigh*xres + jneigh;

    nd->is_virtual = TRUE;
    nd->neighbour = revindex[kk];
    nd->step = 2*step;  // The long distance; the short is always half of that.

    ineigh = i + 2*ystep + yorthostep;
    jneigh = j + 2*xstep + xorthostep;
    g_assert(ineigh < yres || jneigh < xres);
    if (ineigh < yres && jneigh < xres) {
        kk = ineigh*xres + jneigh;
        nd->neighbour2 = revindex[kk];
        g_assert(nd->neighbour2 != NONE);
    }
    else {
        nd->is_boundary = TRUE;
        if (ineigh >= yres)
            nd->bdist = yres-1 - i;
        else
            nd->bdist = xres-1 - j;
    }
}

static void
calculate_weights(LaplaceNeighbour *nd)
{
    LaplaceDirection virtual_dir = NDIRECTIONS;

    // At most one virtual direction.
    for (guint j = 0; j < NDIRECTIONS; j++) {
        if (nd[j].is_virtual) {
            g_assert(virtual_dir == NDIRECTIONS);
            virtual_dir = j;
        }
    }

    // No virtual, no mixing of z_xx and z_yy.
    if (virtual_dir == NDIRECTIONS) {
        for (guint j = 0; j < NDIRECTIONS; j++) {
            guint jj = (j + 2) % NDIRECTIONS;

            if (nd[j].is_boundary)
                continue;

            gdouble s = nd[j].step;
            gdouble xs = nd[jj].is_boundary ? 2*nd[jj].bdist : nd[jj].step;
            nd[j].weight = 2.0/(s + xs)/s;
        }
        return;
    }

    // Virtual.
    guint i = virtual_dir,
          iright = (i + 1) % NDIRECTIONS,
          ii = (i + 2) % NDIRECTIONS,
          ileft = (i + 3) % NDIRECTIONS;
    guint boundary_dir = NDIRECTIONS;
    gboolean virtual_is_boundary = nd[i].is_boundary;

    // At most one boundary direction, except the boundary direction itself.
    for (guint j = 0; j < NDIRECTIONS; j++) {
        if (j != virtual_dir && nd[j].is_boundary) {
            g_assert(boundary_dir == NDIRECTIONS);
            boundary_dir = j;
        }
    }
    g_assert(!virtual_is_boundary || (boundary_dir == iright
                                      || boundary_dir == ileft));
    if (boundary_dir == NDIRECTIONS) {
        gdouble s = nd[i].step, ss = nd[ii].step;
        gdouble sleft = nd[ileft].step, sright = nd[iright].step;
        gdouble w = 1.0 - 0.25*s/(s + ss);
        nd[i].weight = nd[i].weight2 = 1.0/(s + ss)/s;
        nd[ii].weight = 2.0/(s + ss)/ss;
        nd[ileft].weight = 2.0*w/(sleft + sright)/sleft;
        nd[iright].weight = 2.0*w/(sleft + sright)/sright;
    }
    else if (boundary_dir == ii) {
        gdouble s = nd[i].step;
        gdouble sleft = nd[ileft].step, sright = nd[iright].step;
        gdouble b = nd[boundary_dir].bdist;
        gdouble w = 1.0 - 0.25*s/(s + 2*b);
        nd[i].weight = nd[i].weight2 = 1.0/(s + 2*b)/s;
        nd[ileft].weight = 2.0*w/(sleft + sright)/sleft;
        nd[iright].weight = 2.0*w/(sleft + sright)/sright;
    }
    else {
        guint irem = (boundary_dir + 2) % NDIRECTIONS;
        gdouble s = nd[i].step, ss = nd[ii].step;
        gdouble srem = nd[irem].step;
        gdouble b = nd[boundary_dir].bdist;
        gdouble w = 1.0 - 0.25*(s + 4*b)/(s + ss);
        nd[i].weight = 2.0/(s + ss)/s;
        nd[ii].weight = 2.0/(s + ss)/ss;
        nd[irem].weight = 2.0*w/(srem + 2*b)/srem;
    }
}

static void
build_iterator(LaplaceNeighbour *nd,
               LaplaceIterators *iterators,
               guint ipt,
               gdouble *nrhs,
               gdouble *rhssum)
{
    guint npt = 0;
    gdouble ws = 0.0, rs = 0.0;

    // Figure out how many neighbours we have and sum the weights.
    for (guint i = 0; i < NDIRECTIONS; i++) {
        if (nd[i].weight) {
            ws += nd[i].weight;
            if (nd[i].is_rhs) {
                g_assert(!nd[i].is_virtual);
                g_assert(!nd[i].is_boundary);
                rs += nd[i].rhs;
                *nrhs += nd[i].weight;
            }
            else
                npt++;

            if (nd[i].weight2) {
                g_assert(nd[i].is_virtual);
                ws += nd[i].weight2;
                npt++;
            }
        }
    }
    g_assert(npt > 0 && npt <= 5);

    guint start = iterators->n[ipt];
    iterators->n[ipt+1] = start + npt;
    if (rs) {
        *rhssum += rs;
        iterators->rhs[ipt] = rs/ws;
    }

    // Create the iterators.
    gdouble *iter_w = iterators->w + start;
    guint *iter_k = iterators->k + start;
    for (guint i = 0; i < NDIRECTIONS; i++) {
        if (!nd[i].is_rhs && nd[i].weight) {
            *(iter_w++) = nd[i].weight/ws;
            *(iter_k++) = nd[i].neighbour;
            if (nd[i].weight2) {
                *(iter_w++) = nd[i].weight2/ws;
                *(iter_k++) = nd[i].neighbour2;
            }
        }
    }

    // Sort the segments by k.
    iter_w = iterators->w + start;
    iter_k = iterators->k + start;
    gboolean sorted = FALSE;
    do {
        sorted = TRUE;
        for (guint i = 1; i < npt; i++) {
            if (iter_k[i-1] > iter_k[i]) {
                GWY_SWAP(guint, iter_k[i-1], iter_k[i]);
                GWY_SWAP(gdouble, iter_w[i-1], iter_w[i]);
                sorted = FALSE;
            }
        }
    } while (!sorted);
}

static void
build_sparse_iterators(LaplaceIterators *iterators,
                       guint *revindex,
                       const guint *levels,
                       const gdouble *data,
                       guint xres, guint yres)
{
    guint len = count_grid_points(levels, xres, yres);

    laplace_iterators_resize(iterators, len, 5);
    build_grid_index(levels, xres, yres, iterators->gindex, revindex);

    const guint *gindex = iterators->gindex;
    gdouble rhssum = 0.0, nrhs = 0.0;
    LaplaceNeighbour nd[NDIRECTIONS];

    for (guint ipt = 0; ipt < len; ipt++) {
        guint k = gindex[ipt], i = k/xres, j = k % xres;
        gint step = 1 << ((levels[k] - 1)/2);

        analyse_neighbour_direction(levels, data, xres, yres,
                                    0, -step, j, i, revindex, nd + UP);
        analyse_neighbour_direction(levels, data, xres, yres,
                                    step, 0, j, i, revindex, nd + RIGHT);
        analyse_neighbour_direction(levels, data, xres, yres,
                                    0, step, j, i, revindex, nd + DOWN);
        analyse_neighbour_direction(levels, data, xres, yres,
                                    -step, 0, j, i, revindex, nd + LEFT);

        calculate_weights(nd);
        build_iterator(nd, iterators, ipt, &nrhs, &rhssum);
    }

    // Initialise with the mean value of right hand sides, including
    // multiplicity.
    rhssum /= nrhs;
    for (guint ipt = 0; ipt < len; ipt++)
        iterators->z[ipt] = rhssum;
}

static void
build_dense_iterators(LaplaceIterators *iterators,
                      guint *revindex,
                      const guint *levels,
                      const gdouble *data,
                      guint xres, guint yres)
{
    guint len = count_grid_points(levels, xres, yres);

    laplace_iterators_resize(iterators, len, 4);
    build_grid_index(levels, xres, yres, iterators->gindex, revindex);

    const guint *gindex = iterators->gindex;
    for (guint ipt = 0; ipt < len; ipt++) {
        guint k = gindex[ipt], i = k/xres, j = k % xres, ws = 0, n = 0;
        gdouble rs = 0.0;
        guint *iter_k = iterators->k + iterators->n[ipt];
        gdouble *iter_w = iterators->w + iterators->n[ipt];

        if (i) {
            guint kk = k-xres;
            ws++;
            if (levels[kk]) {
                *(iter_k++) = revindex[kk];
                n++;
            }
            else
                rs += data[kk];
        }
        if (j) {
            guint kk = k-1;
            ws++;
            if (levels[kk]) {
                *(iter_k++) = revindex[kk];
                n++;
            }
            else
                rs += data[kk];
        }
        if (j < xres-1) {
            guint kk = k+1;
            ws++;
            if (levels[kk]) {
                *(iter_k++) = revindex[kk];
                n++;
            }
            else
                rs += data[kk];
        }
        if (i < yres-1) {
            guint kk = k+xres;
            ws++;
            if (levels[kk]) {
                *(iter_k++) = revindex[kk];
                n++;
            }
            else
                rs += data[kk];
        }

        iterators->z[ipt] = data[k];
        iterators->rhs[ipt] = rs/ws;
        iterators->n[ipt+1] = iterators->n[ipt] + n;
        while (n--)
            *(iter_w++) = 1.0/ws;
    }
}

static void
calculate_f(LaplaceIterators *iterators)
{
    const guint *n = iterators->n, *k = iterators->k;
    const gdouble *z = iterators->z, *iz = iterators->z,
                  *w = iterators->w, *rhs = iterators->rhs;
    gdouble *f = iterators->f;

    gdouble maxerr = 0.0;

    for (guint ipt = iterators->len; ipt; ipt--, n++, z++, rhs++, f++) {
        gdouble lhs = 0.0;
        for (guint l = *(n + 1) - *n; l; l--, k++, w++)
            lhs += iz[*k]*(*w);
        *f = (*z - lhs) - *rhs;
        maxerr = fmax(maxerr, fabs(*f));
    }
}

static void
iterate_simple(LaplaceIterators *iterators)
{
    const gdouble *f = iterators->f;
    gdouble *z = iterators->z;

    for (guint ipt = iterators->len; ipt; ipt--, z++, f++)
        *z -= 0.8*(*f);
}

static void
matrix_multiply(LaplaceIterators *iterators,
                const gdouble *v,
                gdouble *r)
{
    const guint *n = iterators->n, *k = iterators->k;
    const gdouble *w = iterators->w, *iv = v;

    for (guint ipt = iterators->len; ipt; ipt--, n++, v++, r++) {
        gdouble s = 0.0;

        for (guint l = *(n + 1) - *n; l; l--, k++, w++)
            s += iv[*k]*(*w);

        *r = *v - s;
    }
}

static gboolean
iterate_conj_grad(LaplaceIterators *iterators)
{
    // Temporary quantities: t = A.v, S = v.t, φ = v.f
    matrix_multiply(iterators, iterators->v, iterators->t);

    gdouble S = 0.0, phi = 0.0;
    gdouble *v = iterators->v, *t = iterators->t, *f = iterators->f;
    for (guint ipt = iterators->len; ipt; ipt--, v++, t++, f++) {
        S += (*v)*(*t);
        phi += (*v)*(*f);
    }

    if (S < 1e-16)
        return TRUE;

    // New value and f = A.z-b
    gdouble phiS = phi/S;
    gdouble *z = iterators->z;
    v = iterators->v;
    f = iterators->f;
    t = iterators->t;
    for (guint ipt = iterators->len; ipt; ipt--, z++, v++, f++, t++) {
        *z -= phiS*(*v);
        *f -= phiS*(*t);
    }

    // New v
    phi = 0.0;
    f = iterators->f;
    t = iterators->t;
    for (guint ipt = iterators->len; ipt; ipt--, t++, f++)
        phi += (*t)*(*f);

    phiS = phi/S;
    v = iterators->v;
    f = iterators->f;
    for (guint ipt = iterators->len; ipt; ipt--, v++, f++)
        *v = *f - phiS*(*v);

    return FALSE;
}

static void
move_result_to_data(const LaplaceIterators *iterators,
                    gdouble *data)
{
    for (guint ipt = 0; ipt < iterators->len; ipt++)
        data[iterators->gindex[ipt]] = iterators->z[ipt];
}

static void
interpolate(guint *levels,
            gdouble *data,
            guint xres, guint yres,
            guint step)
{
    guint nx = (xres + step-1)/step, ny = (yres + step-1)/step,
          vstep = xres*step;

    if (nx < 3 || ny < 3)
        return;

    // Six-point interpolation
    for (guint i = 0; i < ny; i++) {
        if (i % 2 == 0) {
            // Interpolated point horizontally in between two other points.
            for (guint j = 1; j < nx; j += 2) {
                guint k = (i*xres + j)*step;
                if (levels[k] != NONE)
                    continue;

                if (i >= 2 && i < ny-2 && j < nx-1) {
                    data[k] = (0.375*(data[k-step] + data[k+step])
                               + 0.0625*(data[k-2*vstep-step]
                                         + data[k-2*vstep+step]
                                         + data[k+2*vstep-step]
                                         + data[k+2*vstep+step]));
                    levels[k] = (levels[k-step] + levels[k+step])/2;
                }
                else if (j < nx-1 && i < ny-2) {
                    // Upper boundary is aligned.
                    data[k] = (0.375*(data[k-step] + data[k+step])
                               + 0.125*(data[k+2*vstep-step]
                                        + data[k+2*vstep+step]));
                    levels[k] = (levels[k-step] + levels[k+step])/2;
                }
                else if (j < nx-1 && i >= 2) {
                    // Lower boundary can be unaligned.
                    guint bdist = yres-1 - i*step;
                    guint a = 4*bdist + 3*step, b = step, d = 8*(bdist + step);
                    data[k] = (a*(data[k-step] + data[k+step])
                               + b*(data[k-2*vstep-step]
                                    + data[k-2*vstep+step]))/d;
                    levels[k] = (levels[k-step] + levels[k+step])/2;
                }
                else if (i >= 2 && i < ny-2) {
                    // Right boundary can be unaligned.
                    guint bdist = xres-1 - j*step;
                    guint a = 6*step - 4*bdist, b = 2*bdist + step, d = 8*step;
                    data[k] = (a*data[k-step]
                               + b*(data[k-2*vstep-step]
                                    + data[k+2*vstep-step]))/d;
                    levels[k] = levels[k-step];
                }
                else if (i < ny-2) {
                    // Upper boundary is aligned, right boundary can be
                    // unaligned.
                    guint bdist = xres-1 - j*step;
                    guint a = 3*step - 2*bdist, b = 2*bdist + step, d = 4*step;
                    data[k] = (a*data[k-step] + b*data[k-step+2*vstep])/d;
                    levels[k] = levels[k-step];
                }
                else if (i >= 2) {
                    // Lower and right boundaries can be both unaligned.
                    guint xbdist = xres-1 - j*step;
                    guint ybdist = yres-1 - i*step;
                    guint a = 3*step + 4*ybdist - 2*xbdist, b = 2*xbdist + step;
                    data[k] = (a*data[k-step] + b*data[k-2*vstep])/(a + b);
                    levels[k] = levels[k-step];
                }
                else {
                    g_assert_not_reached();
                }
            }
        }
        else {
            // Interpolated point vertically in between two other points.
            for (guint j = 0; j < nx; j += 2) {
                guint k = (i*xres + j)*step;
                if (levels[k] != NONE)
                    continue;

                if (j >= 2 && j < nx-2 && i < ny-1) {
                    data[k] = (0.375*(data[k-vstep] + data[k+vstep])
                               + 0.0625*(data[k-vstep-2*step]
                                         + data[k-vstep+2*step]
                                         + data[k+vstep-2*step]
                                         + data[k+vstep+2*step]));
                    levels[k] = (levels[k-vstep] + levels[k+vstep])/2;
                }
                else if (j < nx-2 && i < ny-1) {
                    // Left boundary is aligned.
                    data[k] = (0.375*(data[k-vstep] + data[k+vstep])
                               + 0.125*(data[k-vstep+2*step]
                                        + data[k+vstep+2*step]));
                    levels[k] = (levels[k-vstep] + levels[k+vstep])/2;
                }
                else if (j >= 2 && i < ny-1) {
                    // Right boundary can be unaligned.
                    guint bdist = xres-1 - j*step;
                    guint a = 4*bdist + 3*step, b = step, d = 8*(bdist + step);
                    data[k] = (a*(data[k-vstep] + data[k+vstep])
                               + b*(data[k-vstep-2*step]
                                    + data[k+vstep-2*step]))/d;
                    levels[k] = (levels[k-vstep] + levels[k+vstep])/2;
                }
                else if (j >= 2 && j < nx-2) {
                    // Lower boundary can be unaligned.
                    guint bdist = yres-1 - i*step;
                    guint a = 6*step - 4*bdist, b = 2*bdist + step, d = 8*step;
                    data[k] = (a*data[k-vstep]
                               + b*(data[k-vstep-2*step]
                                    + data[k-vstep+2*step]))/d;
                    levels[k] = levels[k-vstep];
                }
                else if (j < nx-2) {
                    // Left boundary is aligned, lower boundary can be
                    // unaligned.
                    guint bdist = yres-1 - i*step;
                    guint a = 3*step - 2*bdist, b = 2*bdist + step, d = 4*step;
                    data[k] = (a*data[k-vstep] + b*data[k+2*step-vstep])/d;
                    levels[k] = levels[k-vstep];
                }
                else if (j >= 2) {
                    // Lower and right boundaries can be both unaligned.
                    guint xbdist = xres-1 - j*step;
                    guint ybdist = yres-1 - i*step;
                    guint a = 3*step + 4*xbdist - 2*ybdist, b = 2*ybdist + step;
                    data[k] = (a*data[k-vstep] + b*data[k-2*step])/(a + b);
                    levels[k] = levels[k-vstep];
                }
                else {
                    g_assert_not_reached();
                }
            }
        }
    }

    // Four-point interpolation
    for (guint i = 1; i < ny; i += 2) {
        for (guint j = 1; j < nx; j += 2) {
            guint k = (i*xres + j)*step;
            if (levels[k] != NONE)
                continue;

            if (i < ny-1 && j < nx-1) {
                data[k] = 0.25*(data[k-vstep] + data[k+vstep]
                                + data[k-step] + data[k+step]);
                levels[k] = (levels[k-vstep] + levels[k+vstep]
                             + levels[k-step] + levels[k+step])/4;
            }
            else if (i < ny-1) {
                // Right boundary can be unaligned.
                guint bdist = xres-1 - j*step;
                guint a = 2*bdist + step, b = 2*step, d = 4*(bdist + step);
                data[k] = (a*(data[k-vstep] + data[k+vstep])
                           + b*data[k-step])/d;
                levels[k] = (levels[k-vstep] + levels[k+vstep])/2;
            }
            else if (j < nx-1) {
                // Lower boundary can be unaligned.
                guint bdist = yres-1 - i*step;
                guint a = 2*bdist + step, b = 2*step, d = 4*(bdist + step);
                data[k] = (a*(data[k-step] + data[k+step])
                           + b*data[k-vstep])/d;
                levels[k] = (levels[k-step] + levels[k+step])/2;
            }
            else {
                // Right and lower boundary can be unaligned both.
                guint xbdist = xres-1 - j*step;
                guint ybdist = yres-1 - i*step;
                guint a = 2*ybdist + step, b = 2*xbdist + step;
                data[k] = (a*data[k-step] + b*data[k-vstep])/(a + b);
                levels[k] = (levels[k-step] + levels[k-vstep])/2;
            }
        }
    }
}

static void
reconstruct(guint *levels,
            gdouble *data,
            guint xres, guint yres,
            guint level)
{
    guint step = 1 << ((level - 1)/2);
    while (step) {
        interpolate(levels, data, xres, yres, step);
        step /= 2;
    }
}

static void
init_data_simple(gdouble *data, guint *levels,
                 guint xres, guint yres)
{
    for (guint k = 0; k < xres*yres; k++)
        levels[k] = !!levels[k];

    guint level = 1;
    gboolean finished = FALSE;
    while (!finished) {
        finished = TRUE;
        for (guint i = 0; i < yres; i++) {
            for (guint j = 0; j < xres; j++) {
                guint k = i*xres + j, n = 0;
                gdouble s = 0;

                if (levels[k] != level)
                    continue;

                if (i && levels[k-xres] < level) {
                    s += data[k-xres];
                    n++;
                }
                if (j && levels[k-1] < level) {
                    s += data[k-1];
                    n++;
                }
                if (j+1 < xres && levels[k+1] < level) {
                    s += data[k+1];
                    n++;
                }
                if (i+1 < yres && levels[k+xres] < level) {
                    s += data[k+xres];
                    n++;
                }

                if (n) {
                    data[k] = s/n;
                }
                else {
                    levels[k] = level+1;
                    finished = FALSE;
                }
            }
        }
        level++;
    }
}

static void
laplace_sparse(LaplaceIterators *iterators,
               guint *revindex,
               gdouble *data, guint *levels, guint xres, guint yres,
               guint nconjgrad, guint nsimple)
{
    // Revindex is filled later, use is as a temporary xres*yres-sized buffer.
    guint maxlevel = build_levels(levels, revindex, xres, yres);
    if (maxlevel < 3) {
        // If the grain is nowhere thick just init the interior using boundary
        // conditions and continue with dense iteration.  Note for single-pixel
        // grains init_data_simple() already produces the solution.
        init_data_simple(data, levels, xres, yres);
        return;
    }

    build_sparse_iterators(iterators, revindex, levels, data, xres, yres);
    calculate_f(iterators),
    gwy_assign(iterators->v, iterators->f, iterators->len);
    gboolean finished = FALSE;
    for (guint iter = 0; iter < nconjgrad; iter++) {
        if ((finished = iterate_conj_grad(iterators)))
            break;
    }
    if (!finished) {
        for (guint iter = 0; iter < nsimple; iter++) {
            calculate_f(iterators);
            iterate_simple(iterators);
        }
    }
    move_result_to_data(iterators, data);
    reconstruct(levels, data, xres, yres, maxlevel);
}

static void
laplace_dense(LaplaceIterators *iterators,
              guint *revindex,
              gdouble *data, guint *levels, guint xres, guint yres,
              guint nconjgrad, guint nsimple)
{
    build_dense_iterators(iterators, revindex, levels, data, xres, yres);
    calculate_f(iterators);
    gwy_assign(iterators->v, iterators->f, iterators->len);
    gboolean finished = FALSE;
    for (guint iter = 0; iter < nconjgrad; iter++) {
        if ((finished = iterate_conj_grad(iterators)))
            break;
    }
    if (!finished) {
        for (guint iter = 0; iter < nsimple; iter++) {
            calculate_f(iterators);
            iterate_simple(iterators);
        }
    }
    move_result_to_data(iterators, data);
}

// Extract grain data from full-sized @grains and @data to workspace-sized
// @levels and @z.
static void
extract_grain(const guint *grains,
              const gdouble *data,
              guint xres,
              const GwyFieldPart *fpart,
              guint grain_id,
              guint *levels,
              gdouble *z)
{
    for (guint i = 0; i < fpart->height; i++) {
        gwy_assign(z + i*fpart->width,
                   data + (i + fpart->row)*xres + fpart->col,
                   fpart->width);

        const guint *grow = grains + (i + fpart->row)*xres + fpart->col;
        guint *lrow = levels + i*fpart->width;
        for (guint j = fpart->width; j; j--, lrow++, grow++)
            *lrow = (*grow == grain_id);
    }
}

// Put interpolated grain data @z back to @data.
static void
insert_grain(const guint *grains,
             gdouble *data,
             guint xres,
             const GwyFieldPart *fpart,
             guint grain_id,
             const gdouble *z)
{
    for (guint i = 0; i < fpart->height; i++) {
        const gdouble *zrow = z + i*fpart->width;
        const guint *grow = grains + (i + fpart->row)*xres + fpart->col;
        gdouble *drow = data + (i + fpart->row)*xres + fpart->col;
        for (guint j = fpart->width; j; j--, zrow++, drow++, grow++) {
            if (*grow == grain_id)
                *drow = *zrow;
        }
    }
}

static void
enlarge_field_part(GwyFieldPart *fpart,
                   guint xres, guint yres)
{
    if (fpart->col) {
        fpart->col--;
        fpart->width++;
    }
    if (fpart->col + fpart->width < xres)
        fpart->width++;

    if (fpart->row) {
        fpart->row--;
        fpart->height++;
    }
    if (fpart->row + fpart->height < yres)
        fpart->height++;
}

/*
 * Find the largest
 * - grain size in the terms of pixels: this is the number of iterators for
 *   dense iteration
 * - grain size in the terms of extended bounding box (i.e. bounding box
 *   including one more line of pixels to each side, if possible): this is
 *   the size of levels, revindex and data arrays.
 */
static void
find_largest_sizes(guint xres, guint yres,
                   const GwyFieldPart *bboxes,
                   const guint *sizes,
                   guint gfrom, guint gto,
                   guint *size,
                   guint *bboxsize)
{
    *size = *bboxsize = 0;
    for (guint gno = gfrom; gno <= gto; gno++) {
        if (sizes[gno] > *size)
            *size = sizes[gno];

        GwyFieldPart bbox = bboxes[gno];
        enlarge_field_part(&bbox, xres, yres);
        guint bs = bbox.width * bbox.height;
        if (bs > *bboxsize)
            *bboxsize = bs;
    }
}

/**
 * gwy_field_laplace_solve:
 * @field: A two-dimensional data field.
 * @mask: A two-dimensional mask field defining the areas to interpolate.
 * @grain_id: The id number of the grain to replace with the solution of
 *           Laplace equation, from 1 to @ngrains (see
 *           gwy_mask_field_number_grains()).  Passing 0 means to replace the
 *           entire empty space outside grains while passing %G_MAXUINT means
 *           to replace the entire masked area.
 *
 * Replaces masked areas by the solution of Laplace equation.
 *
 * The boundary conditions on mask boundaries are Dirichlet with values given
 * by pixels on the outer boundary of the masked area.  Boundary conditions at
 * field edges are Neumann conditions ∂z/∂n=0 where n denotes the normal to the
 * edge.  If entire area of @field is to be replaced the problem is
 * underspecified; @field will be filled with zeroes.
 *
 * No precision control is provided; the result should be simply good enough
 * for image processing purposes with the typical local error of order 10⁻⁵ for
 * very large grains and possibly much smaller for small grains.
 **/
void
gwy_field_laplace_solve(GwyField *field,
                        GwyMaskField *mask,
                        guint grain_id)
{
    g_return_if_fail(GWY_IS_MASK_FIELD(mask));
    g_return_if_fail(GWY_IS_FIELD(field));
    g_return_if_fail(mask->xres == field->xres);
    g_return_if_fail(mask->yres == field->yres);

    // To fill the entire empty space we need to divide it to grains too so
    // work with the inverted mask.
    if (grain_id == 0) {
        mask = gwy_mask_field_duplicate(mask);
        gwy_mask_field_logical(mask, NULL, NULL, GWY_LOGICAL_NA);
        grain_id = G_MAXUINT;
    }
    else
        g_object_ref(mask);

    guint ngrains;
    const guint *grains = gwy_mask_field_number_grains(mask, &ngrains);
    g_return_if_fail(grain_id == G_MAXUINT || grain_id <= ngrains);

    const GwyFieldPart *bboxes = gwy_mask_field_grain_bounding_boxes(mask);
    const guint *sizes = gwy_mask_field_grain_sizes(mask);
    guint xres = field->xres, yres = field->yres;

    // The underspecified case.
    if (ngrains == 1 && sizes[1] == xres*yres) {
        gwy_field_clear_full(field);
        g_object_unref(mask);
        return;
    }

    guint gfrom = (grain_id == G_MAXUINT) ? 1 : grain_id;
    guint gto = (grain_id == G_MAXUINT) ? ngrains : grain_id;

    // Allocate everything at the maximum size to avoid reallocations.
    guint maxsize, maxbboxsize;
    find_largest_sizes(xres, yres, bboxes, sizes, gfrom, gto,
                       &maxsize, &maxbboxsize);

    guint *levels = g_new(guint, maxbboxsize);
    guint *revindex = g_new(guint, maxbboxsize);
    gdouble *z = g_new(gdouble, maxbboxsize);
    LaplaceIterators *iterators = laplace_iterators_new(maxsize, 5);

    for (grain_id = gfrom; grain_id <= gto; grain_id++) {
        GwyFieldPart bbox = bboxes[grain_id];
        enlarge_field_part(&bbox, xres, yres);
        extract_grain(grains, field->data, xres, &bbox, grain_id, levels, z);
        laplace_sparse(iterators, revindex, z, levels,
                       bbox.width, bbox.height, 45, 10);
        if (sizes[grain_id] > 1)
            laplace_dense(iterators, revindex, z, levels,
                          bbox.width, bbox.height, 30, 20);
        insert_grain(grains, field->data, xres, &bbox, grain_id, z);
    }

    laplace_iterators_free(iterators);
    g_free(z);
    g_free(levels);
    g_free(revindex);

    g_object_unref(mask);
    gwy_field_invalidate(field);
}

/**
 * SECTION: field-level
 * @section_id: GwyField-level
 * @title: GwyField levelling
 * @short_description: Field levelling and background subtraction
 *
 * All plane and polynomial fitting and subtracting methods use the following
 * convention for @x and @y coordinates (abscissas of the polynoms):  The
 * entire field range in either coordinate is transformed to interval [-1,1].
 * More precisely, the zeroth column/row corresponds to abscissa value of -1,
 * whereas the last column/row to 1.
 *
 * This permits to easily apply the same polynomial levelling to resampled
 * fields, however, the primary reason is improving the orthogonality of the
 * polynomial terms (which is rather poor) in the common case.
 *
 * Knowing the exact interpretation of the coefficients is not necessary to
 * just subtract the background as the fitting and subtracting functions follow
 * the same convention.  To obtain plane coefficients for pixel coordinates
 * (i.e. row and column) the following transformation can be used:
 * |[
 * ap = a - bx - by;
 * bcol = 2*bx/(field->xres - 1);
 * brow = 2*by/(field->yres - 1);
 * ]|
 **/

/**
 * GwyRowShiftMethod:
 * @GWY_ROW_SHIFT_MEAN: The difference of row mean values.
 * @GWY_ROW_SHIFT_MEDIAN: The difference of row median values.
 * @GWY_ROW_SHIFT_MEAN_DIFF: The mean value of row differences.
 * @GWY_ROW_SHIFT_MEDIAN_DIFF: The median value of row differences.
 *
 * Row shift finding methods.
 *
 * All methods are based on finding the shift necesary to align consecutive
 * rows.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
