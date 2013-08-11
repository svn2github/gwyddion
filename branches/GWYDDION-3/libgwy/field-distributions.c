/*
 *  $Id$
 *  Copyright (C) 2009-2013 David Nečas (Yeti).
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
// Including before fftw3.h ensures fftw_complex is C99 ‘double complex’.
#include <complex.h>
#include <fftw3.h>
#include "libgwy/macros.h"
#include "libgwy/math.h"
#include "libgwy/fft.h"
#include "libgwy/line-arithmetic.h"
#include "libgwy/line-distributions.h"
#include "libgwy/field-statistics.h"
#include "libgwy/field-distributions.h"
#include "libgwy/mask-field-grains.h"
#include "libgwy/math-internal.h"
#include "libgwy/object-internal.h"
#include "libgwy/curve-internal.h"
#include "libgwy/mask-field-internal.h"
#include "libgwy/field-internal.h"

typedef enum {
    CF_ACF,
    CF_HHCF,
} CFType;

typedef struct {
    guint n;              // Number of quarter-pixels considered.
    guint n_in_range;     // Number of quarter-pixels within range.
    gboolean analyse : 1;
    gboolean count : 1;
    gdouble min;
    gdouble max;
    gdouble left_sum;     // Starting value for cumulative distributions.
    GwyLine *dist;
    // Slope-only
    gdouble dx;
    gdouble dy;
    gdouble cos_alpha;
    gdouble sin_alpha;
} DistributionData;

static inline guint
dist_points_for_n_points(guint n)
{
    return gwy_round(3.49*cbrt(n));
}

static void
sanitise_range(gdouble *min,
               gdouble *max)
{
    if (*max > *min)
        return;
    if (*max) {
        gdouble d = fabs(*max);
        *max += 0.1*d;
        *min -= 0.1*d;
        return;
    }
    *min = -1.0;
    *max = 1.0;
}

static void
value_dist_cont1(gdouble z1, gdouble z2, gdouble z3,
                 guint w,
                 DistributionData *ddata)
{
    GWY_ORDER(gdouble, z1, z2);
    if (z2 > z3) {
        GWY_SWAP(gdouble, z2, z3);
        GWY_ORDER(gdouble, z1, z2);
    }

    if (ddata->analyse) {
        if (z1 < ddata->min)
            ddata->min = z1;
        if (z3 > ddata->max)
            ddata->max = z3;
        ddata->n_in_range += w;
        ddata->n += w;
    }
    else if (ddata->count) {
        // FIXME: We could estimate how much of the contribution actually falls
        // within the range not just whether anything falls there at all.
        if (gwy_math_intersecting(z1, z3, ddata->min, ddata->max))
            ddata->n_in_range += w;
        ddata->n += w;
    }
    else
        ddata->left_sum += gwy_line_add_dist_trapezoidal(ddata->dist,
                                                         z1, z2, z2, z3, w);
}

static void
value_dist_cont(gdouble z1, gdouble z2, gdouble z3, gdouble z4,
                guint w1, guint w2, guint w3, guint w4,
                gpointer user_data)
{
    DistributionData *ddata = (DistributionData*)user_data;

    gdouble zc = 0.25*(z1 + z2 + z3 + z4);

    if (w1 || w2) {
        gdouble z12 = 0.5*(z1 + z2);
        value_dist_cont1(w1 ? z1 : z12, w2 ? z2 : z12, zc, w1 + w2, ddata);
    }
    if (w2 || w3) {
        gdouble z23 = 0.5*(z2 + z3);
        value_dist_cont1(w2 ? z2 : z23, w3 ? z3 : z23, zc, w2 + w3, ddata);
    }
    if (w3 || w4) {
        gdouble z34 = 0.5*(z3 + z4);
        value_dist_cont1(w3 ? z3 : z34, w4 ? z4 : z34, zc, w3 + w4, ddata);
    }
    if (w4 || w1) {
        gdouble z41 = 0.5*(z4 + z1);
        value_dist_cont1(w4 ? z4 : z41, w1 ? z1 : z41, zc, w4 + w1, ddata);
    }
}

static void
field_value_dist_cont(const GwyField *field,
                      const GwyFieldPart *fpart,
                      const GwyMaskField *mask,
                      GwyMaskingType masking,
                      guint npoints,
                      DistributionData *ddata)
{
    // Run analyse (find range and count) or count (count in range).  If both
    // is given by caller, this serves as a somewhat inefficient masked pixel
    // counting method.
    gwy_field_process_quarters(field, fpart, mask, masking, FALSE,
                               value_dist_cont, ddata);

    if (!npoints)
        npoints = dist_points_for_n_points(ddata->n_in_range/4.0);
    if (!npoints)
        return;

    sanitise_range(&ddata->min, &ddata->max);

    ddata->dist = gwy_line_new_sized(npoints, TRUE);
    ddata->dist->off = ddata->min;
    ddata->dist->real = ddata->max - ddata->min;

    ddata->analyse = ddata->count = FALSE;
    gwy_field_process_quarters(field, fpart, mask, masking, FALSE,
                               value_dist_cont, ddata);
}

static void
value_dist_discr_analyse(const GwyField *field,
                         const GwyFieldPart *fpart,
                         const GwyMaskField *mask,
                         GwyMaskingType masking,
                         guint maskcol, guint maskrow,
                         guint width, guint height,
                         DistributionData *ddata)
{
    guint n;

    if (ddata->analyse) {
        GwyFieldPart rect = { maskcol, maskrow, width, height };
        n = gwy_mask_field_part_count_masking(mask, &rect, masking);

        gwy_field_min_max(field, fpart, mask, masking,
                          &ddata->min, &ddata->max);
        ddata->n_in_range = ddata->n = n;
    }
    else {
        guint nabove, nbelow;
        n = gwy_field_count_above_below(field, fpart, mask, masking,
                                        ddata->max, ddata->min, TRUE,
                                        &nabove, &nbelow);
        ddata->n = n;
        ddata->n_in_range = n - (nabove + nbelow);
    }
}

static void
value_dist_discr_process(const GwyField *field,
                         const GwyMaskField *mask,
                         GwyMaskingType masking,
                         guint col, guint row,
                         guint width, guint height,
                         guint maskcol, guint maskrow,
                         DistributionData *ddata)
{
    GwyLine *line = ddata->dist;
    const gdouble *base = field->data + row*field->xres + col;
    gdouble min = ddata->min, max = ddata->max;
    guint npoints = line->res;
    gdouble q = npoints/(max - min);

    if (masking == GWY_MASK_IGNORE) {
        for (guint i = 0; i < height; i++) {
            const gdouble *d = base + i*field->xres;
            for (guint j = width; j; j--, d++) {
                if (*d < min)
                    ddata->left_sum++;
                else if (*d <= max) {
                    guint k = (guint)((*d - min)*q);
                    // Fix rounding errors.
                    if (G_UNLIKELY(k >= npoints))
                        line->data[npoints-1] += 1;
                    else
                        line->data[k] += 1;
                }
            }
        }
    }
    else {
        const gboolean invert = (masking == GWY_MASK_EXCLUDE);
        for (guint i = 0; i < height; i++) {
            const gdouble *d = base + i*field->xres;
            GwyMaskIter iter;
            gwy_mask_field_iter_init(mask, iter, maskcol, maskrow + i);
            for (guint j = width; j; j--, d++) {
                if (!gwy_mask_iter_get(iter) == invert) {
                    if (*d < min)
                        ddata->left_sum++;
                    else if (*d <= max) {
                        guint k = (guint)((*d - min)*q);
                        // Fix rounding errors.
                        if (G_UNLIKELY(k >= npoints))
                            line->data[npoints-1] += 1;
                        else
                            line->data[k] += 1;
                    }
                }
                gwy_mask_iter_next(iter);
            }
        }
    }
}

static void
field_value_dist_discr(const GwyField *field,
                       const GwyFieldPart *fpart,
                       const GwyMaskField *mask,
                       GwyMaskingType masking,
                       guint npoints,
                       DistributionData *ddata)
{
    guint col, row, width, height, maskcol, maskrow;
    if (!gwy_field_check_mask(field, fpart, mask, &masking,
                              &col, &row, &width, &height, &maskcol, &maskrow))
        return;

    value_dist_discr_analyse(field, fpart, mask, masking,
                             maskcol, maskrow, width, height,
                             ddata);

    if (!npoints)
        npoints = dist_points_for_n_points(ddata->n_in_range);
    if (!npoints)
        return;

    sanitise_range(&ddata->min, &ddata->max);

    ddata->dist = gwy_line_new_sized(npoints, TRUE);
    ddata->dist->off = ddata->min;
    ddata->dist->real = ddata->max - ddata->min;

    value_dist_discr_process(field, mask, masking,
                             col, row, width, height, maskcol, maskrow,
                             ddata);
}

/**
 * gwy_field_value_dist:
 * @field: A two-dimensional data field.
 * @fpart: (allow-none):
 *         Area in @field to process.  Pass %NULL to process entire @field.
 * @mask: (allow-none):
 *        Mask specifying which values to take into account/exclude, or %NULL.
 * @masking: Masking mode to use (has any effect only with non-%NULL @mask).
 * @cumulative: %TRUE to calculate cumulative distribution, %FALSE to calculate
 *              density.
 * @continuous: %TRUE to calculate the distribution of triangularly
 *              interpolated surface, %FALSE to calculate plain histogram of
 *              discrete values.
 * @npoints: Distribution resolution, i.e. the number of histogram bins.
 *           Pass zero to choose a suitable resolution automatically.
 * @min: Minimum value of the range to calculate the distribution in.
 * @max: Maximum value of the range to calculate the distribution in.
 *
 * Calculates the distribution of values in a field.
 *
 * Pass @max <= @min to calculate the distribution in the full data range
 * (with masking still considered).
 *
 * Returns: (transfer full):
 *          A new one-dimensional data line with the value distribution.
 **/
GwyLine*
gwy_field_value_dist(const GwyField *field,
                     const GwyFieldPart *fpart,
                     const GwyMaskField *mask,
                     GwyMaskingType masking,
                     gboolean cumulative,
                     gboolean continuous,
                     guint npoints,
                     gdouble min, gdouble max)
{
    gboolean explicit_range = min < max;
    DistributionData ddata = {
        0, 0,
        !explicit_range, TRUE,
        explicit_range ? min : G_MAXDOUBLE,
        explicit_range ? max : -G_MAXDOUBLE,
        0.0, NULL,
        NAN, NAN, NAN, NAN,    // Make any (mis)use of these evident.
    };

    if (continuous)
        field_value_dist_cont(field, fpart, mask, masking, npoints, &ddata);
    else
        field_value_dist_discr(field, fpart, mask, masking, npoints, &ddata);

    GwyLine *line = ddata.dist;

    if (line) {
        if (cumulative) {
            gwy_line_accumulate(line, TRUE);
            gwy_line_add(line, ddata.left_sum);
            gwy_line_multiply(line, 1.0/ddata.n);
        }
        else
            gwy_line_multiply(line, 1.0/(gwy_line_dx(line)*ddata.n));
    }
    else
        line = gwy_line_new();

    gwy_unit_assign(gwy_line_get_xunit(line), gwy_field_get_zunit(field));
    if (!cumulative)
        gwy_unit_power(gwy_line_get_yunit(line),
                       gwy_field_get_zunit(field), -1);

    return line;
}

static void
slope_dist_cont1(gdouble z1, gdouble z2, gdouble z3, gdouble z4,
                 gdouble w,
                 DistributionData *ddata)
{
    gdouble c0 = (z2 - z1)*ddata->cos_alpha/ddata->dx,
            c1 = (z3 - z4)*ddata->cos_alpha/ddata->dx,
            s0 = (z4 - z1)*ddata->sin_alpha/ddata->dy,
            s1 = (z3 - z2)*ddata->sin_alpha/ddata->dy;
    // Directional derivatives at the corners.
    gdouble v1 = c0 + s0, v2 = c0 + s1, v3 = c1 + s1, v4 = c1 + s0;

    // FIXME: Only four orders are possible:
    // (a) v1 ≤ v2 ≤ v3 ≤ v4
    // (b) v4 ≤ v3 ≤ v3 ≤ v1
    // (c) v1 ≤ v3 ≤ v2 ≤ v4
    // (d) v4 ≤ v2 ≤ v3 ≤ v1
    // We can differentiate between (a,c) and (b,d) by comparing v1 and v4.
    // Within each pair the order is then uniquely given by the angle.
    GWY_ORDER(gdouble, v1, v2);
    GWY_ORDER(gdouble, v3, v4);
    GWY_ORDER(gdouble, v1, v3);
    GWY_ORDER(gdouble, v2, v4);
    GWY_ORDER(gdouble, v2, v3);

    if (ddata->analyse) {
        if (v1 < ddata->min)
            ddata->min = v1;
        if (v4 > ddata->max)
            ddata->max = v4;
        ddata->n_in_range += w;
        ddata->n += w;
    }
    else if (ddata->count) {
        // FIXME: We could estimate how much of the contribution actually falls
        // within the range not just whether anything falls there at all.
        if (gwy_math_intersecting(v1, v4, ddata->min, ddata->max))
            ddata->n_in_range += w;
        ddata->n += w;
    }
    else
        ddata->left_sum += gwy_line_add_dist_trapezoidal(ddata->dist,
                                                         v1, v2, v3, v4, w);
}

static void
slope_dist_cont(gdouble z1, gdouble z2, gdouble z3, gdouble z4,
                guint w1, guint w2, guint w3, guint w4,
                gpointer user_data)
{
    DistributionData *ddata = (DistributionData*)user_data;

    // If entire area is covered we can process it at once.
    if (w1 && w2 && w3 && w4) {
        slope_dist_cont1(z1, z2, z3, z4, w1 + w2 + w3 + w4, ddata);
        return;
    }

    // Otherwise individual quarter-pixels need to be processed.
    gdouble z12 = 0.5*(z1 + z2), z23 = 0.5*(z2 + z3),
            z34 = 0.5*(z3 + z4), z41 = 0.5*(z4 + z1),
            zc = 0.5*(z12 + z23);

    if (w1)
        slope_dist_cont1(z1, z12, zc, z41, w1, ddata);
    if (w2)
        slope_dist_cont1(z12, z2, z23, zc, w1, ddata);
    if (w3)
        slope_dist_cont1(zc, z23, z3, z34, w1, ddata);
    if (w4)
        slope_dist_cont1(z41, zc, z34, z4, w1, ddata);
}

static void
slope_dist_discr1(gdouble v,
                  gdouble w,
                  DistributionData *ddata)
{
    if (ddata->analyse) {
        if (v < ddata->min)
            ddata->min = v;
        if (v > ddata->max)
            ddata->max = v;
        ddata->n_in_range += w;
        ddata->n += w;
    }
    else if (ddata->count) {
        if (v >= ddata->min && v <= ddata->max)
            ddata->n_in_range += w;
        ddata->n += w;
    }
    else
        ddata->left_sum += gwy_line_add_dist_delta(ddata->dist, v, w);
}

static void
slope_dist_discr(gdouble z1, gdouble z2, gdouble z3, gdouble z4,
                 guint w1, guint w2, guint w3, guint w4,
                 gpointer user_data)
{
    DistributionData *ddata = (DistributionData*)user_data;

    gdouble c0 = (z2 - z1)*ddata->cos_alpha/ddata->dx,
            c1 = (z3 - z4)*ddata->cos_alpha/ddata->dx,
            s0 = (z4 - z1)*ddata->sin_alpha/ddata->dy,
            s1 = (z3 - z2)*ddata->sin_alpha/ddata->dy;
    // Directional derivatives at the corners.
    gdouble v1 = c0 + s0, v2 = c0 + s1, v3 = c1 + s1, v4 = c1 + s0;

    if (w1)
        slope_dist_discr1(v1, w1, ddata);
    if (w2)
        slope_dist_discr1(v2, w2, ddata);
    if (w3)
        slope_dist_discr1(v3, w3, ddata);
    if (w4)
        slope_dist_discr1(v4, w4, ddata);
}

/**
 * gwy_field_slope_dist:
 * @field: A two-dimensional data field.
 * @fpart: (allow-none):
 *         Area in @field to process.  Pass %NULL to process entire @field.
 * @mask: (allow-none):
 *        Mask specifying which values to take into account/exclude, or %NULL.
 * @masking: Masking mode to use (has any effect only with non-%NULL @mask).
 * @angle: Direction in which to compute the derivatives.  Zero corresponds to
 *         positive row direction, π/2 corresponds to positive column
 *         direction, etc.  The angle is measured in the real space, not
 *         pixel-wise; this is important in case of non-square pixels.
 * @cumulative: %TRUE to calculate cumulative distribution, %FALSE to calculate
 *              density.
 * @continuous: %TRUE to calculate the distribution of linearly
 *              interpolated surface, %FALSE to calculate plain histogram of
 *              discrete values.
 * @npoints: Distribution resolution, i.e. the number of histogram bins.
 *           Pass zero to choose a suitable resolution automatically.
 * @min: Minimum value of the range to calculate the distribution in.
 * @max: Maximum value of the range to calculate the distribution in.
 *
 * Calculates the distribution of slopes in a field.
 *
 * For angles that are not multiples of π/2, slope distribution is meaningful
 * only if @x and @y units of @field match.
 *
 * Slopes are calculated as horizontal or vertical derivatives of the value,
 * i.e. dz/dx or dz/dy.
 *
 * Pass @max <= @min to calculate the distribution in the full data range
 * (with masking still considered).
 *
 * Returns: (transfer full):
 *          A new one-dimensional data line with the slope distribution.
 **/
GwyLine*
gwy_field_slope_dist(const GwyField *field,
                     const GwyFieldPart *fpart,
                     const GwyMaskField *mask,
                     GwyMaskingType masking,
                     gdouble angle,
                     gboolean cumulative,
                     gboolean continuous,
                     guint npoints,
                     gdouble min, gdouble max)
{
    guint col, row, width, height, maskcol, maskrow;
    GwyLine *line = NULL;
    if (!gwy_field_check_mask(field, fpart, mask, &masking,
                              &col, &row, &width, &height, &maskcol, &maskrow))
        goto fail;

    gboolean explicit_range = min < max;
    GwyFieldQuartersFunc func;
    func = continuous ? slope_dist_cont : slope_dist_discr;

    DistributionData ddata = {
        0, 0,
        !explicit_range, TRUE,
        explicit_range ? min : G_MAXDOUBLE,
        explicit_range ? max : -G_MAXDOUBLE,
        0.0, NULL,
        gwy_field_dx(field), gwy_field_dy(field),
        cos(angle), sin(angle),
    };

    // Run analyse (find range and count) or count (count in range).  If both
    // is given by caller, this serves as a somewhat inefficient masked pixel
    // counting method.
    gwy_field_process_quarters(field, fpart, mask, masking, FALSE,
                               func, &ddata);

    if (!npoints)
        npoints = dist_points_for_n_points(ddata.n_in_range/4.0);
    if (!npoints)
        goto fail;

    sanitise_range(&ddata.min, &ddata.max);

    line = ddata.dist = gwy_line_new_sized(npoints, TRUE);
    line->off = ddata.min;
    line->real = ddata.max - ddata.min;

    ddata.analyse = ddata.count = FALSE;
    gwy_field_process_quarters(field, fpart, mask, masking, FALSE,
                               func, &ddata);

    if (cumulative) {
        gwy_line_accumulate(line, TRUE);
        gwy_line_add(line, ddata.left_sum);
        gwy_line_multiply(line, 1.0/ddata.n);
    }
    else
        gwy_line_multiply(line, 1.0/(gwy_line_dx(line)*ddata.n));

fail:
    if (!line)
        line = gwy_line_new();

    if (gwy_unit_equal(field->priv->xunit, field->priv->yunit)
        || fabs(ddata.sin_alpha) < 1e-14) {
        gwy_unit_divide(gwy_line_get_xunit(line),
                        field->priv->zunit, field->priv->xunit);
        if (!cumulative)
            gwy_unit_power(gwy_line_get_yunit(line), field->priv->xunit, -1);
    }
    else if (fabs(ddata.cos_alpha) < 1e-14) {
        gwy_unit_divide(gwy_line_get_xunit(line),
                        field->priv->zunit, field->priv->yunit);
        if (!cumulative)
            gwy_unit_power(gwy_line_get_yunit(line), field->priv->yunit, -1);
    }
    else {
        g_warning("X and Y units of field do not match.");
        // Do not set any units then.
    }

    return line;
}

static void
row_assign_mask(const GwyMaskField *mask,
                gsize col,
                gsize row,
                gsize width,
                gboolean invert,
                gdouble *out)
{
    GwyMaskIter iter;

    gwy_mask_field_iter_init(mask, iter, col, row);
    for (gsize j = width; j; j--, out++) {
        *out = (!gwy_mask_iter_get(iter) == invert);
        gwy_mask_iter_next(iter);
    }
}

static void
row_accumulate(gdouble *accum,
               const gdouble *data,
               gsize size)
{
    for (gsize j = size; j; j--, accum++, data++)
        *accum += *data;
}

// FFTW calculates unnormalised DFT so we divide the result of the first
// transformation with (1/√size)² = 1/size and keep the second transfrom as-is
// to obtain exactly g_k.

static void
row_divide_nonzero(const gdouble *numerator,
                   const gdouble *denominator,
                   gdouble *out,
                   gsize size)
{
    for (guint j = 0; j < size; j++)
        out[j] = denominator[j] ? numerator[j]/denominator[j] : 0.0;
}

static void
row_accum_cnorm(gdouble *accum,
                const gwycomplex *fftc,
                gsize size,
                gdouble q)
{
    q /= size;

    gdouble *out = accum, *out2 = accum + (size-1);
    gdouble re = creal(*fftc), im = cimag(*fftc), v = q*(re*re + im*im);
    *out += v;
    out++, fftc++;
    for (guint j = (size + 1)/2 - 1; j; j--, fftc++, out++, out2--) {
        re = creal(*fftc);
        im = cimag(*fftc);
        v = q*(re*re + im*im);
        *out += v;
        *out2 += v;
    }
    if (size % 2 == 0) {
        re = creal(*fftc);
        im = cimag(*fftc);
        v = q*(re*re + im*im);
        *out += v;
    }
}

static void
row2_assign_cnorm(gdouble *out,
                  gdouble *out2,
                  const gwycomplex *fftc,
                  gsize size,
                  gdouble q)
{
    q /= size;

    gdouble re = creal(*fftc), im = cimag(*fftc), v = q*(re*re + im*im);
    *out = v;
    out++, fftc++;
    for (guint j = (size + 1)/2 - 1; j; j--, fftc++, out++, out2--) {
        re = creal(*fftc);
        im = cimag(*fftc);
        v = q*(re*re + im*im);
        *out = v;
        *out2 = v;
    }
    if (size % 2 == 0) {
        re = creal(*fftc);
        im = cimag(*fftc);
        v = q*(re*re + im*im);
        *out = v;
    }
}

static void
row_extfft_accum_cnorm(fftw_plan plan,
                       gdouble *fftr,
                       gdouble *accum,
                       gwycomplex *fftc,
                       gsize size,
                       gsize width,
                       gdouble q)
{
    gwy_clear(fftr + width, size - width);
    fftw_execute(plan);
    row_accum_cnorm(accum, fftc, size, q);
}

// Calculate the product A*B+AB*, equal to 2*(Re A Re B + Im A Im B), of two
// R2HC outputs (the result is added to @out including the redundant even
// terms).
static void
row_accum_cprod(const gwycomplex *fftca,
                const gwycomplex *fftcb,
                gdouble *out,
                gsize size,
                gdouble q)
{
    q *= 2.0/size;

    gdouble *out2 = out + size-1;
    gdouble rea = creal(*fftca), ima = cimag(*fftca),
            reb = creal(*fftcb), imb = cimag(*fftcb),
            v = q*(rea*reb + ima*imb);
    *out += v;
    out++, fftca++, fftcb++;
    for (guint j = (size + 1)/2 - 1; j; j--, out++, fftca++, fftcb++, out2--) {
        rea = creal(*fftca);
        ima = cimag(*fftca);
        reb = creal(*fftcb);
        imb = cimag(*fftcb);
        v = q*(rea*reb + ima*imb);
        *out += v;
        *out2 += v;
    }
    if (size % 2 == 0) {
        rea = creal(*fftca);
        ima = cimag(*fftca);
        reb = creal(*fftcb);
        imb = cimag(*fftcb);
        v = q*(rea*reb + ima*imb);
        *out += v;
    }
}

// Used in cases when we expect the imaginary part to be zero but do not want
// to bother with specialised DCT.
static void
row_extfft_extract_re(fftw_plan plan,
                      gdouble *fftr,
                      gdouble *out,
                      gwycomplex *fftc,
                      gsize size,
                      gsize width)
{
    gwy_assign(fftr, out, size);
    fftw_execute(plan);
    for (guint j = 0; j < width; j++)
        out[j] = creal(fftc[j]);
}

static void
row_extfft_symmetrise_re(fftw_plan plan,
                         gdouble *fftr,
                         gdouble *out,
                         gwycomplex *fftc,
                         gsize size)
{
    gwy_assign(fftr, out, size);
    fftw_execute(plan);

    gdouble *out2 = out + size-1;
    *out = creal(*fftc);
    out++, fftc++;
    for (gsize j = (size + 1)/2 - 1; j; j--, fftc++, out++, out2--)
        *out = *out2 = creal(*fftc);
    if (size % 2 == 0)
        *out = creal(*fftc);
}

// Calculate the complex absolute value of R2HC output items, excluding the
// redundant even terms.  So the size of @out must be @size/2 + 1.
static void
row_extract_cabs(const gwycomplex *in,
                 gdouble *out,
                 gsize width)
{
    *out = cabs(*in);
    for (gsize j = width; j; j--, out++, in++)
        *out = cabs(*in);
}

static void
row_accumulate_vk(const gdouble *data,
                  gdouble *v,
                  gsize size)
{
    const gdouble *data2 = data + (size-1);
    gdouble sum = 0.0;
    v += size-1;
    for (guint j = size; j; j--, data++, data2--, v--) {
        sum += (*data)*(*data) + (*data2)*(*data2);
        *v += sum;
    }
}

static void
row_copy_subtract(const gdouble *in,
                  gdouble *out,
                  guint n,
                  gdouble a)
{
    for (guint i = n; i; i--, in++, out++)
        *out = *in - a;
}

// Level a row of data by subtracting the mean value.
static void
row_level(const gdouble *in,
          gdouble *out,
          guint n)
{
    gdouble sumsi = 0.0;
    const gdouble *pdata = in;
    for (guint i = n; i; i--, pdata++)
        sumsi += *pdata;

    row_copy_subtract(in, out, n, sumsi/n);
}

// Level a row of data by subtracting the mean value of data under mask and
// clear (set to zero) all data not under mask.  Note how the zeroes nicely
// ensure that the subsequent functions Just Work(TM) and don't need to know we
// use masking at all.
static guint
row_level_mask(const gdouble *in,
               gdouble *out,
               guint n,
               GwyMaskIter iter0,
               gboolean invert)
{
    GwyMaskIter iter = iter0;
    gdouble sumsi = 0.0;
    const gdouble *pdata = in;
    guint nd = 0;
    for (guint i = n; i; i--, pdata++) {
        if (!gwy_mask_iter_get(iter) == invert) {
            sumsi += *pdata;
            nd++;
        }
        gwy_mask_iter_next(iter);
    }

    // This can be division by zero but in that case we never use the value.
    gdouble a = sumsi/nd;
    pdata = in;
    iter = iter0;
    for (guint i = n; i; i--, pdata++, out++) {
        *out = (!gwy_mask_iter_get(iter) == invert) ? *pdata - a : 0.0;
        gwy_mask_iter_next(iter);
    }
    return nd;
}

/* Window a row using a sampled windowing function. */
static void
row_window(gdouble *data, const gdouble *window, guint n)
{
    for (guint i = n; i; i--, data++, window++)
        *data *= *window;
}

/* Level and count the number of valid data in a row */
static guint
row_level_and_count(const gdouble *in,
                    gdouble *out,
                    guint width,
                    const GwyMaskField *mask,
                    GwyMaskingType masking,
                    guint maskcol,
                    guint maskrow,
                    guint level)
{
    if (masking == GWY_MASK_IGNORE) {
        if (level)
            row_level(in, out, width);
        else
            gwy_assign(out, in, width);
        return width;
    }

    GwyMaskIter iter;
    gwy_mask_field_iter_init(mask, iter, maskcol, maskrow);
    gboolean invert = (masking == GWY_MASK_EXCLUDE);
    if (level)
        return row_level_mask(in, out, width, iter, invert);

    guint count = 0;
    for (guint i = width; i; i--, in++, out++) {
        if (!gwy_mask_iter_get(iter) == invert) {
           *out = *in;
           count++;
        }
        else
            *out = 0.0;
        gwy_mask_iter_next(iter);
    }
    return count;
}

static void
set_cf_units(const GwyField *field,
             GwyLine *line,
             GwyLine *weights)
{
    // XXX: This assumes line is newly allocated and does not ever need to
    // clear previously set units.
    if (field->priv->xunit)
        gwy_unit_assign(gwy_line_get_xunit(line), field->priv->xunit);
    gwy_unit_power(gwy_line_get_yunit(line), field->priv->zunit, 2);
    if (!weights)
        return;

    gwy_unit_assign(gwy_line_get_xunit(weights), gwy_line_get_xunit(line));
    gwy_unit_clear(gwy_line_get_yunit(weights));
}

/**
 * gwy_field_row_acf:
 * @field: A two-dimensional data field.
 * @fpart: (allow-none):
 *         Area in @field to process.  Pass %NULL to process entire @field.
 * @mask: (allow-none):
 *        Mask specifying which values to take into account/exclude, or %NULL.
 * @masking: Masking mode to use (has any effect only with non-%NULL @mask).
 * @level: The first polynomial degree to keep in the rows, lower degrees than
 *         @level are subtracted.  Note only values 0 (no levelling) and 1
 *         (subtract the mean value of each row) are available at present.  For
 *         SPM data, you usually wish to pass 1.
 * @weights: (allow-none):
 *           Line to store the denominators to (or %NULL).  It will be resized
 *           to match the returned line.  The denominators are integers equal
 *           to the number of terms that contributed to each value.  They are
 *           suitable as fitting weight if the ACF is fitted.
 *
 * Calculates the row-wise autocorrelation function (ACF) of a field.
 *
 * The calculated ACF has the natural number of points, i.e. @width.
 *
 * Masking is performed by omitting all terms that contain excluded pixels.
 * Since different rows contain different numbers of pixels, the resulting
 * ACF values are calculated as a weighted sums where weight of each row's
 * contribution is proportional to the number of contributing terms.  In other
 * words, the weighting is fair: each contributing pixel has the same influence
 * on the result.
 *
 * Returns: (transfer full):
 *          A new one-dimensional data line with the ACF.
 **/
GwyLine*
gwy_field_row_acf(const GwyField *field,
                  const GwyFieldPart *fpart,
                  const GwyMaskField *mask,
                  GwyMaskingType masking,
                  guint level,
                  GwyLine *weights)
{
    guint col, row, width, height, maskcol, maskrow;
    GwyLine *line = NULL;
    if (!gwy_field_check_mask(field, fpart, mask, &masking,
                              &col, &row, &width, &height, &maskcol, &maskrow))
        goto fail;
    if (weights && !GWY_IS_LINE(weights)) {
        g_critical("weights is not a GwyLine");
        weights = NULL;
    }

    if (level > 1) {
        g_warning("Levelling degree %u is not supported, changing to 1.",
                  level);
        level = 1;
    }

    // Transform size must be at least twice the data size for zero padding.
    // An even size is necessary due to alignment constraints in FFTW.
    // Using this size for all buffers is a bit excessive but safe.
    line = gwy_line_new_sized(width, TRUE);
    gsize size = gwy_fft_nice_transform_size((width + 1)/2*4);
    // The innermost (contiguous) dimension of R2C the complex output is
    // slightly larger than the real input.  Note @cstride is measured in
    // gwycomplex, multiply it by 2 for doubles.
    gsize cstride = size/2 + 1;
    const gdouble *base = field->data + row*field->xres + col;
    const gboolean invert = (masking == GWY_MASK_EXCLUDE);
    gdouble *fftr = fftw_alloc_real(3*size);
    gdouble *accum_data = fftr + size;
    gdouble *accum_mask = fftr + 2*size;
    gwycomplex *fftc = fftw_alloc_complex(cstride);
    guint nfullrows = 0, nemptyrows = 0;

    fftw_plan plan = fftw_plan_dft_r2c_1d(size, fftr, fftc,
                                          FFTW_DESTROY_INPUT
                                          | _gwy_fft_rigour());
    g_assert(plan);
    gwy_clear(accum_data, size);
    gwy_clear(accum_mask, size);

    // Gather squared Fourier coefficients for all rows
    for (guint i = 0; i < height; i++) {
        guint count = row_level_and_count(base + i*field->xres, fftr, width,
                                          mask, masking, maskcol, maskrow + i,
                                          level);
        if (!count) {
            nemptyrows++;
            continue;
        }

        // Calculate and gather squared Fourier coefficients of the data.
        row_extfft_accum_cnorm(plan, fftr, accum_data, fftc, size, width, 1.0);

        if (count == width) {
            nfullrows++;
            continue;
        }

        // Calculate and gather squared Fourier coefficients of the mask.
        row_assign_mask(mask, maskcol, maskrow + i, width, invert, fftr);
        row_extfft_accum_cnorm(plan, fftr, accum_mask, fftc, size, width, 1.0);
    }

    // Numerator of G_k, i.e. FFT of squared data Fourier coefficients.
    row_extfft_extract_re(plan, fftr, accum_data, fftc, size, width);

    // Denominator of G_k, i.e. FFT of squared mask Fourier coefficients.
    // Don't perform the FFT if there were no partial rows.
    if (nfullrows + nemptyrows < height)
        row_extfft_extract_re(plan, fftr, accum_mask, fftc, size, width);

    for (guint j = 0; j < width; j++) {
        // Denominators must be rounded to integers because they are integers
        // and this permits to detect zeroes in the denominator.
        accum_mask[j] = gwy_round(accum_mask[j]) + nfullrows*(width - j);
    }
    row_divide_nonzero(accum_data, accum_mask, line->data, line->res);

    line->real = gwy_field_dx(field)*line->res;
    line->off = -0.5*gwy_line_dx(line);

    if (weights) {
        gwy_line_set_size(weights, line->res, FALSE);
        gwy_line_set_real(weights, line->real);
        gwy_line_set_offset(weights, line->off);
        gwy_assign(weights->data, accum_mask, weights->res);
    }

    fftw_destroy_plan(plan);
    fftw_free(fftc);
    fftw_free(fftr);

fail:
    if (!line)
        line = gwy_line_new();

    set_cf_units(field, line, weights);
    return line;
}

// @accum_{data,mask} are just working buffers (they are called this for
// consistency with normal ACF).
static void
grain_row_acf(const GwyField *field,
              guint col, guint row,
              guint width, guint height,
              const GwyMaskField *mask,
              GwyMaskingType masking,
              guint level,
              gdouble *fftr,
              gwycomplex *fftc,
              gdouble *accum_data, gdouble *accum_mask)
{
    const gdouble *base = field->data + row*field->xres + col;
    gboolean invert = (masking == GWY_MASK_EXCLUDE);
    gsize size = gwy_fft_nice_transform_size((width + 1)/2*4);
    guint nfullrows = 0, nemptyrows = 0;

    fftw_plan plan = fftw_plan_dft_r2c_1d(size, fftr, fftc,
                                          FFTW_DESTROY_INPUT
                                          | _gwy_fft_rigour());
    g_assert(plan);
    gwy_clear(accum_data, size);
    gwy_clear(accum_mask, size);

    // Gather squared Fourier coefficients for all rows
    for (guint i = 0; i < height; i++) {
        guint count = row_level_and_count(base + i*field->xres, fftr, width,
                                          mask, masking, 0, i, level);
        if (!count) {
            nemptyrows++;
            continue;
        }

        // Calculate and gather squared Fourier coefficients of the data.
        row_extfft_accum_cnorm(plan, fftr, accum_data, fftc, size, width, 1.0);

        if (count == width) {
            nfullrows++;
            continue;
        }

        // Calculate and gather squared Fourier coefficients of the mask.
        row_assign_mask(mask, 0, i, width, invert, fftr);
        row_extfft_accum_cnorm(plan, fftr, accum_mask, fftc, size, width, 1.0);
    }

    // Numerator of G_k, i.e. FFT of squared data Fourier coefficients.
    row_extfft_extract_re(plan, fftr, accum_data, fftc, size, width);

    // Denominator of G_k, i.e. FFT of squared mask Fourier coefficients.
    // Don't perform the FFT if there were no partial rows.
    if (nfullrows + nemptyrows < height)
        row_extfft_extract_re(plan, fftr, accum_mask, fftc, size, width);

    for (guint j = 0; j < width; j++) {
        // Denominators must be rounded to integers because they are integers
        // and this permits to detect zeroes in the denominator.
        accum_mask[j] = gwy_round(accum_mask[j]) + nfullrows*(width - j);
    }

    fftw_destroy_plan(plan);
}

/**
 * gwy_field_grain_row_acf:
 * @field: A two-dimensional data field.
 * @mask: A two-dimensional mask field defining the areas to analyse.
 * @grain_id: The id number of the grain to analyse, from 1 to @ngrains (see
 *            gwy_mask_field_grain_numbers()).  Passing 0 means to analyse the
 *            empty space outside grains as a whole and thus not much different
 *            from gwy_field_row_acf() with %GWY_MASK_EXCLUDE masking type.
 *            Passing %G_MAXUINT means to calculate ACF of the entire masked
 *            area but by constructing it from single-grains ACFs which is
 *            different from gwy_field_row_acf().
 * @level: The first polynomial degree to keep in the rows, lower degrees than
 *         @level are subtracted.  Note only values 0 (no levelling) and 1
 *         (subtract the mean value of each row) are available at present.  For
 *         SPM data, you usually wish to pass 1.
 * @weights: (allow-none):
 *           Line to store the denominators to (or %NULL).  It will be resized
 *           to match the returned line.  The denominators are integers equal
 *           to the number of terms that contributed to each value.  They are
 *           suitable as fitting weight if the ACF is fitted.
 *
 * Calculates the row-wise autocorrelation function (ACF) of a field.
 *
 * The calculated ACF has the natural number of points, i.e. the width of
 * the widest grain analysed.
 *
 * See gwy_field_row_acf() for calculation details.
 *
 * This function works with entire fields.  A single grain in the complete mask
 * can correspond to several disjoint grains or no grain in a part of the mask.
 * Therefore, to pass a meaningful @grain_id, you need to explicitly extract
 * the mask part and number grains in it anyway.
 *
 * Returns: (transfer full):
 *          A new one-dimensional data line with the ACF.
 **/
GwyLine*
gwy_field_grain_row_acf(const GwyField *field,
                        const GwyMaskField *mask,
                        guint grain_id,
                        guint level,
                        GwyLine *weights)
{
    g_return_val_if_fail(GWY_IS_MASK_FIELD(mask), NULL);
    g_return_val_if_fail(GWY_IS_FIELD(field), NULL);
    g_return_val_if_fail(mask->xres == field->xres, NULL);
    g_return_val_if_fail(mask->yres == field->yres, NULL);
    if (weights && !GWY_IS_LINE(weights)) {
        g_critical("weights is not a GwyLine");
        weights = NULL;
    }

    if (level > 1) {
        g_warning("Levelling degree %u is not supported, changing to 1.",
                  level);
        level = 1;
    }

    guint ngrains = gwy_mask_field_n_grains(mask);
    const GwyFieldPart *bboxes = gwy_mask_field_grain_bounding_boxes(mask);
    g_return_val_if_fail(grain_id <= ngrains || grain_id == G_MAXUINT, NULL);

    guint gfrom = (grain_id == G_MAXUINT) ? 1 : grain_id;
    guint gto = (grain_id == G_MAXUINT) ? ngrains : grain_id;
    guint width = 0;

    for (grain_id = gfrom; grain_id <= gto; grain_id++) {
        const GwyFieldPart *bbox = bboxes + grain_id;
        if (bbox->width > width)
            width = bbox->width;
    }

    // Transform size must be at least twice the data size for zero padding.
    // An even size is necessary due to alignment constraints in FFTW.
    // Using this size for all buffers is a bit excessive but safe.
    GwyLine *line = gwy_line_new_sized(width, TRUE);
    gsize size = gwy_fft_nice_transform_size((width + 1)/2*4);
    // The innermost (contiguous) dimension of R2C the complex output is
    // slightly larger than the real input.  Note @cstride is measured in
    // gwycomplex, multiply it by 2 for doubles.
    gsize cstride = size/2 + 1;
    gdouble *fftr = fftw_alloc_real(3*size + 2*width);
    gdouble *accum_data = fftr + size;
    gdouble *accum_mask = fftr + 2*size;
    gdouble *total_data = fftr + 3*size;
    gdouble *total_mask = fftr + 3*size + width;
    gwycomplex *fftc = fftw_alloc_complex(cstride);

    gwy_clear(total_data, width);
    gwy_clear(total_mask, width);

    GwyMaskField *grainmask = gwy_mask_field_new();
    for (grain_id = gfrom; grain_id <= gto; grain_id++) {
        GwyMaskingType masking = grain_id ? GWY_MASK_INCLUDE : GWY_MASK_EXCLUDE;
        const GwyFieldPart *bbox = bboxes + grain_id;

        gwy_mask_field_extract_grain(mask, grainmask, grain_id, 0);
        grain_row_acf(field, bbox->col, bbox->row, bbox->width, bbox->height,
                      grainmask, masking, level,
                      fftr, fftc, accum_data, accum_mask);
        row_accumulate(total_data, accum_data, bbox->width);
        row_accumulate(total_mask, accum_mask, bbox->width);
    }
    g_object_unref(grainmask);

    row_divide_nonzero(total_data, total_mask, line->data, line->res);

    line->real = gwy_field_dx(field)*line->res;
    line->off = -0.5*gwy_line_dx(line);

    if (weights) {
        gwy_line_set_size(weights, line->res, FALSE);
        gwy_line_set_real(weights, line->real);
        gwy_line_set_offset(weights, line->off);
        gwy_assign(weights->data, total_mask, weights->res);
    }

    fftw_free(fftc);
    fftw_free(fftr);

    set_cf_units(field, line, weights);
    return line;
}

/**
 * gwy_field_row_psdf:
 * @field: A two-dimensional data field.
 * @fpart: (allow-none):
 *         Area in @field to process.  Pass %NULL to process entire @field.
 * @mask: (allow-none):
 *        Mask specifying which values to take into account/exclude, or %NULL.
 * @masking: Masking mode to use (has any effect only with non-%NULL @mask).
 * @windowing: Windowing type to use.
 * @level: The first polynomial degree to keep in the rows, lower degrees than
 *         @level are subtracted.  Note only values 0 (no levelling) and 1
 *         (subtract the mean value of each row) are available at present.  For
 *         SPM data, you usually wish to pass 1.
 *
 * Calculates the row-wise power spectrum density function (PSDF) of a
 * rectangular part of a field.
 *
 * The calculated PSDF has the natural number of points that follows from DFT,
 * i.e. @width/2+1.
 *
 * The reduction of the total energy by windowing is compensated by multiplying
 * the PSDF to make its sum of squares equal to the input data sum of squares.
 *
 * Masking is performed by omitting all terms that contain excluded pixels.
 * Since different rows contain different numbers of pixels, the resulting
 * PSDF is calculated as a weighted sum where each row's weight is proportional
 * to the number of contributing pixels.  In other words, the weighting is
 * fair: each contributing pixel has the same influence on the result.
 *
 * Returns: (transfer full):
 *          A new one-dimensional data line with the PSDF.
 **/
GwyLine*
gwy_field_row_psdf(const GwyField *field,
                   const GwyFieldPart *fpart,
                   const GwyMaskField *mask,
                   GwyMaskingType masking,
                   GwyWindowingType windowing,
                   guint level)
{
    guint col, row, width, height, maskcol, maskrow;
    GwyLine *line = NULL;
    if (!gwy_field_check_mask(field, fpart, mask, &masking,
                              &col, &row, &width, &height, &maskcol, &maskrow))
        goto fail;

    if (level > 1) {
        g_warning("Levelling degree %u is not supported, changing to 1.",
                  level);
        level = 1;
    }

    // The innermost (contiguous) dimension of R2C the complex output is
    // slightly larger than the real input.  Note @cstride is measured in
    // gwycomplex, multiply it by 2 for doubles.
    gsize cstride = width/2 + 1;
    // An even size is necessary due to alignment constraints in FFTW.
    // Using this size for all buffers is a bit excessive but safe.
    line = gwy_line_new_sized(cstride, TRUE);
    gsize size = (width + 3)/4*4;
    const gdouble *base = field->data + row*field->xres + col;
    const gboolean invert = (masking == GWY_MASK_EXCLUDE);
    gdouble *fftr = fftw_alloc_real(4*size);
    gdouble *accum_data = fftr + 1*size;
    gdouble *accum_mask = fftr + 2*size;
    gdouble *window = fftr + 3*size;
    gwycomplex *fftc = fftw_alloc_complex(cstride);
    guint nfullrows = 0, nemptyrows = 0;

    gwy_clear(accum_data, size);
    gwy_clear(accum_mask, size);

    gwy_fft_window_sample(window, width, windowing, 2);

    fftw_plan plan = fftw_plan_dft_r2c_1d(width, fftr, fftc,
                                          FFTW_DESTROY_INPUT
                                          | _gwy_fft_rigour());
    g_assert(plan);
    for (guint i = 0; i < height; i++) {
        guint count = row_level_and_count(base + i*field->xres, fftr, width,
                                          mask, masking, maskcol, maskrow + i,
                                          level);
        if (!count) {
            nemptyrows++;
            continue;
        }

        // Calculate and gather squared Fourier coefficients of the data.
        row_window(fftr, window, width);
        row_extfft_accum_cnorm(plan, fftr, accum_data, fftc, width, width, 1.0);

        if (count == width) {
            nfullrows++;
            continue;
        }

        // Calculate and gather squared Fourier coefficients of the mask.
        row_assign_mask(mask, maskcol, maskrow + i, width, invert, fftr);
        row_extfft_accum_cnorm(plan, fftr, accum_mask, fftc, width, width, 1.0);
    }

    // Numerator of A_k, i.e. FFT of squared data Fourier coefficients.
    row_extfft_symmetrise_re(plan, fftr, accum_data, fftc, width);

    // Denominator of A_k, i.e. FFT of squared mask Fourier coefficients.
    // Don't perform the FFT if there were no partial rows.
    if (nfullrows + nemptyrows < height)
        row_extfft_symmetrise_re(plan, fftr, accum_mask, fftc, width);

    for (guint j = 0; j < width; j++) {
        // Denominators must be rounded to integers because they are integers
        // and this permits to detect zeroes in the denominator.
        accum_mask[j] = gwy_round(accum_mask[j]) + nfullrows*width;
    }
    row_divide_nonzero(accum_data, accum_mask, fftr, width);

    // The transform is the other way round – for complex numbers.  Since it
    // is in fact a DCT here we don't care and run it as a forward transform.
    fftw_execute(plan);
    row_extract_cabs(fftc, line->data, line->res);

    fftw_destroy_plan(plan);
    fftw_free(fftc);
    fftw_free(fftr);

    gwy_line_multiply(line, gwy_field_dx(field)/(2*G_PI));
    line->real = G_PI/gwy_field_dx(field);
    line->off = -0.5*gwy_line_dx(line);

fail:
    if (!line)
        line = gwy_line_new();

    gwy_unit_power(gwy_line_get_xunit(line), gwy_field_get_xunit(field), -1);
    gwy_unit_power_multiply(gwy_line_get_yunit(line),
                            gwy_field_get_xunit(field), 1,
                            gwy_field_get_zunit(field), 2);
    return line;
}

/**
 * gwy_field_row_hhcf:
 * @field: A two-dimensional data field.
 * @fpart: (allow-none):
 *         Area in @field to process.  Pass %NULL to process entire @field.
 * @mask: (allow-none):
 *        Mask specifying which values to take into account/exclude, or %NULL.
 * @masking: Masking mode to use (has any effect only with non-%NULL @mask).
 * @level: The first polynomial degree to keep in the rows, lower degrees than
 *         @level are subtracted.  Note only values 0 (no levelling) and 1
 *         (subtract the mean value of each row) are available at present.
 *         There is no difference for HHCF.
 * @weights: (allow-none):
 *           Line to store the denominators to (or %NULL).  It will be resized
 *           to match the returned line.  The denominators are integers equal
 *           to the number of terms that contributed to each value.  They are
 *           suitable as fitting weight if the ACF is fitted.
 *
 * Calculates the row-wise height-height correlation function (HHCF) of a
 * rectangular part of a field.
 *
 * The calculated HHCF has the natural number of points, i.e. @width.
 *
 * Masking is performed by omitting all terms that contain excluded pixels.
 * Since different rows contain different numbers of pixels, the resulting
 * HHCF values are calculated as a weighted sums where weight of each row's
 * contribution is proportional to the number of contributing terms.  In other
 * words, the weighting is fair: each contributing pixel has the same influence
 * on the result.
 *
 * Returns: (transfer full):
 *          A new one-dimensional data line with the HHCF.
 **/
GwyLine*
gwy_field_row_hhcf(const GwyField *field,
                   const GwyFieldPart *fpart,
                   const GwyMaskField *mask,
                   GwyMaskingType masking,
                   guint level,
                   GwyLine *weights)
{
    guint col, row, width, height, maskcol, maskrow;
    GwyLine *line = NULL;
    if (!gwy_field_check_mask(field, fpart, mask, &masking,
                              &col, &row, &width, &height, &maskcol, &maskrow))
        goto fail;
    if (weights && !GWY_IS_LINE(weights)) {
        g_critical("weights is not a GwyLine");
        weights = NULL;
    }

    if (level > 1) {
        g_warning("Levelling degree %u is not supported, changing to 1.",
                  level);
        level = 1;
    }

    // Transform size must be at least twice the data size for zero padding.
    // An even size is necessary due to alignment constraints in FFTW.
    // Using this size for all buffers is a bit excessive but safe.
    line = gwy_line_new_sized(width, TRUE);
    gsize size = gwy_fft_nice_transform_size((width + 1)/2*4);
    // The innermost (contiguous) dimension of R2C the complex output is
    // slightly larger than the real input.  Note @cstride is measured in
    // gwycomplex, multiply it by 2 for doubles.
    gsize cstride = size/2 + 1;
    const gdouble *base = field->data + row*field->xres + col;
    const gboolean invert = (masking == GWY_MASK_EXCLUDE);
    gdouble *fftr = fftw_alloc_real(4*size);
    gdouble *accum_data = fftr + size;
    gdouble *accum_mask = fftr + 2*size;
    gdouble *accum_v = fftr + 3*size;
    gwycomplex *fftc = fftw_alloc_complex(2*cstride);
    gwycomplex *tmp = fftc + cstride;
    guint nfullrows = 0, nemptyrows = 0;
    gdouble *p;
    const gdouble *q;

    fftw_plan plan = fftw_plan_dft_r2c_1d(size, fftr, fftc,
                                          FFTW_DESTROY_INPUT
                                          | _gwy_fft_rigour());
    g_assert(plan);
    gwy_clear(accum_data, size);
    gwy_clear(accum_mask, size);
    gwy_clear(accum_v, size);

    // Gather V_ν-2|Z_ν|² for all rows, except that for full rows we actually
    // gather just -2|Z_ν|² because v_k can be calculated without DFT.
    for (guint i = 0; i < height; i++) {
        guint count = row_level_and_count(base + i*field->xres, fftr, width,
                                          mask, masking, maskcol, maskrow + i,
                                          level);
        if (!count) {
            nemptyrows++;
            continue;
        }

        // Calculate v_k before FFT destroys the input levelled/filtered data.
        if (count == width)
            row_accumulate_vk(fftr, accum_v, width);
        else {
            // For partial rows, we will need the data later to calculate FFT
            // of their squares.  Save them to the line that conveniently has
            // the right size.
            gwy_assign(line->data, fftr, width);
        }

        // Calculate and gather -2 times squared Fourier coefficients.
        row_extfft_accum_cnorm(plan, fftr, accum_data, fftc, size, width, -2.0);

        if (count == width) {
            nfullrows++;
            continue;
        }

        // First calculate U_ν (Fourier cofficients of squared data).  Save
        // them to tmp.
        q = line->data;
        p = fftr;
        for (guint j = width; j; j--, p++, q++)
            *p = (*q)*(*q);
        gwy_clear(fftr + width, size - width);
        fftw_execute(plan);
        gwy_assign(tmp, fftc, cstride);

        // Mask.  We need the intermediate result C_ν to combine it with U_ν.
        row_assign_mask(mask, maskcol, maskrow + i, width, invert, fftr);
        gwy_clear(fftr + width, size - width);
        fftw_execute(plan);

        // Accumulate V_ν (calculated from C_ν and U_ν) to accum_data.
        row_accum_cprod(tmp, fftc, accum_data, size, 1.0);

        // And accumulate squared mask Fourier coeffs |C_ν|².
        row_accum_cnorm(accum_mask, fftc, size, 1.0);
    }

    // Numerator of H_k, excluding non-DFT data in v_k.
    row_extfft_extract_re(plan, fftr, accum_data, fftc, size, width);
    // Combine it with v_k to get the full numerator in accum_data.
    row_accumulate(accum_data, accum_v, width);

    // Denominator of H_k, i.e. FFT of squared mask Fourier coefficients.
    // Don't perform the FFT if there were no partial rows.
    if (nfullrows + nemptyrows < height)
        row_extfft_extract_re(plan, fftr, accum_mask, fftc, size, width);

    for (guint j = 0; j < width; j++) {
        // Denominators must be rounded to integers because they are integers
        // and this permits to detect zeroes in the denominator.
        accum_mask[j] = gwy_round(accum_mask[j]) + nfullrows*(width - j);
    }
    row_divide_nonzero(accum_data, accum_mask, line->data, line->res);

    line->real = gwy_field_dx(field)*line->res;
    line->off = -0.5*gwy_line_dx(line);

    if (weights) {
        gwy_line_set_size(weights, line->res, FALSE);
        gwy_line_set_real(weights, line->real);
        gwy_line_set_offset(weights, line->off);
        gwy_assign(weights->data, accum_mask, weights->res);
    }

    fftw_destroy_plan(plan);
    fftw_free(fftc);
    fftw_free(fftr);

fail:
    if (!line)
        line = gwy_line_new();

    set_cf_units(field, line, weights);
    return line;
}

static inline void
add_to_dist(GwyLine *dist, gdouble z)
{
    gdouble x = (z - dist->off)/dist->real*dist->res - 0.5;
    if (x <= 0.0)
        dist->data[0] += 1.0;
    else if (x <= dist->res-1)
        dist->data[(guint)ceil(x)] += 1.0;
}

/* Recalculate area excess based on second-order expansion to the true one,
 * assuming the distribution is exponential. */
static inline gdouble
asg_correction(gdouble ex)
{
    if (ex < 1e-3)
        return ex*(1.0 - ex*(1.0 - 3.0*ex*(1.0 - 5.0*ex*(1.0 - 7.0*ex*(1.0 - 9.0*ex*(1.0 - 11.0*ex))))));

    return sqrt(0.5*G_PI*ex) * exp(0.5/ex) * erfc(sqrt(0.5/ex));
}

/**
 * gwy_field_row_asg:
 * @field: A two-dimensional data field.
 * @fpart: (allow-none):
 *         Area in @field to process.  Pass %NULL to process entire @field.
 * @mask: (allow-none):
 *        Mask specifying which values to take into account/exclude, or %NULL.
 * @masking: Masking mode to use (has any effect only with non-%NULL @mask).
 * @level: The first polynomial degree to keep in the rows, lower degrees than
 *         @level are subtracted.  Note only values 0 (no levelling) and 1
 *         (subtract the mean value of each row) are available at present.
 *         There is no difference for ASG.
 *
 * Calculates the row-wise area scale graph (ASG) of a rectangular part of a
 * field.
 *
 * The calculated ASG has the natural number of points, i.e. @width-1.
 *
 * The ASG represents the apparent area excess (ratio of surface and projected
 * area minus one) observed at given length scale.  The quantity calculated by
 * this function serves a similar purpose as ASME B46.1 area scale graph but
 * is defined differently, based on the HHCF.  See gwy_field_row_hhcf()
 * for details of its calculation.
 *
 * Returns: (transfer full):
 *          A new one-dimensional data line with the ASG.
 **/
GwyLine*
gwy_field_row_asg(const GwyField *field,
                  const GwyFieldPart *fpart,
                  const GwyMaskField *mask,
                  GwyMaskingType masking,
                  guint level)
{
    GwyLine *hhcf = gwy_field_row_hhcf(field, fpart, mask, masking, level,
                                       NULL);
    GwyLine *line = NULL;

    if (hhcf->res < 2)
        goto fail;

    line = gwy_line_new_sized(hhcf->res - 1, FALSE);
    gdouble dx = gwy_line_dx(hhcf);
    gwy_line_set_real(line, dx*line->res);
    gwy_line_set_offset(line, 0.5*dx);

    for (guint i = 0; i < line->res; i++) {
        gdouble t = (i + 0.5)*dx + line->off;
        line->data[i] = asg_correction(hhcf->data[i+1]/(t*t));
    }

fail:
    g_object_unref(hhcf);

    if (!line)
        line = gwy_line_new();

    if (field->priv->xunit)
        gwy_unit_assign(gwy_line_get_xunit(line), field->priv->xunit);

    return line;
}

static GwyLine*
minkowski_volume(const GwyField *field,
                 guint col, guint row,
                 guint width, guint height,
                 const GwyMaskField *mask,
                 GwyMaskingType masking,
                 guint maskcol, guint maskrow,
                 guint npoints,
                 gdouble min, gdouble max)
{
    GwyFieldPart rect = { maskcol, maskrow, width, height };
    guint n = gwy_mask_field_part_count_masking(mask, &rect, masking);

    if (!n)
        return NULL;

    if (!(min < max))
        gwy_field_min_max(field, &(GwyFieldPart){ col, row, width, height },
                          mask, masking, &min, &max);
    sanitise_range(&min, &max);

    if (!npoints)
        npoints = dist_points_for_n_points(n);

    GwyLine *dist = gwy_line_new_sized(npoints, TRUE);
    dist->real = max - min;
    dist->off = min;

    if (masking != GWY_MASK_IGNORE) {
        gboolean invert = (masking == GWY_MASK_EXCLUDE);
        for (guint i = 0; i < height; i++) {
            const gdouble *drow = field->data + (i + row)*field->xres + col;
            GwyMaskIter iter;
            gwy_mask_field_iter_init(mask, iter, maskcol, maskrow + i);
            for (guint j = width; j; j--, drow++) {
                if (!gwy_mask_iter_get(iter) == invert)
                    add_to_dist(dist, *drow);
                gwy_mask_iter_next(iter);
            }
        }
    }
    else {
        for (guint i = 0; i < height; i++) {
            const gdouble *drow = field->data + (i + row)*field->xres + col;
            for (guint j = width; j; j--, drow++)
                add_to_dist(dist, *drow);
        }
    }

    // The non-cumulative distributions are already prepared pixel-centered so
    // use plain summing here.
    gwy_line_accumulate(dist, FALSE);
    for (guint k = 0; k < dist->res; k++)
        dist->data[k] = 1.0 - dist->data[k]/n;

    return dist;
}

/**
 * count_edges:
 * @mask: A mask field.
 * @masking: Masking mode.
 * @col: Column index.
 * @row: Row index.
 * @width: Part width (number of column).
 * @height: Part height (number of rows).
 * @min: Location to store the minimum.
 * @max: Location to store the maximum.
 *
 * Counts the number of edges between two pixels.
 *
 * An edge is counted if both pixels are counted according to the masking mode
 * @masking.
 *
 * Since only edges that lie between two counted pixels contribute the minimum
 * and maximum is also calculated edge-wise.  This essentially means that
 * they are calculated ignoring single-pixel grains as all other masked values
 * have some neighbour.
 *
 * Returns: The number of edges.
 **/
static guint
count_edges(const GwyField *field,
            guint col, guint row,
            guint width, guint height,
            const GwyMaskField *mask,
            GwyMaskingType masking,
            guint maskcol, guint maskrow,
            gdouble *min, gdouble *max)
{
    if (masking == GWY_MASK_IGNORE) {
        gwy_field_min_max(field, &(GwyFieldPart){ col, row, width, height },
                          NULL, GWY_MASK_IGNORE, min, max);
        return 2*width*height - width - height;
    }

    g_assert(mask);

    gboolean invert = (masking == GWY_MASK_EXCLUDE);
    guint xres = field->xres;
    guint nedges = 0;

    gdouble min1 = G_MAXDOUBLE, max1 = -G_MAXDOUBLE;
    const gdouble *base = field->data + row*xres + col;
    for (guint i = 0; i < height-1; i++) {
        GwyMaskIter iter1, iter2;
        gwy_mask_field_iter_init(mask, iter1, maskcol, maskrow + i);
        gwy_mask_field_iter_init(mask, iter2, maskcol, maskrow + i+1);

        gboolean curr = !gwy_mask_iter_get(iter1);
        gboolean lower = !gwy_mask_iter_get(iter2);
        if (curr == invert && lower == invert) {
            gdouble z1 = base[i*xres], z2 = base[(i + 1)*xres];
            GWY_ORDER(gdouble, z1, z2);
            if (z1 < min1)
                min1 = z1;
            if (z2 > max1)
                max1 = z2;
            nedges++;
        }

        for (guint j = 1; j < width; j++) {
            gboolean right = curr;
            gwy_mask_iter_next(iter1);
            gwy_mask_iter_next(iter2);
            curr = !gwy_mask_iter_get(iter1);
            lower = !gwy_mask_iter_get(iter2);
            if (curr == invert && right == invert) {
                gdouble z1 = base[i*xres + j-1], z2 = base[i*xres + j];
                GWY_ORDER(gdouble, z1, z2);
                if (z1 < min1)
                    min1 = z1;
                if (z2 > max1)
                    max1 = z2;
                nedges++;
            }
            if (curr == invert && lower == invert) {
                gdouble z1 = base[i*xres + j], z2 = base[(i + 1)*xres + j];
                GWY_ORDER(gdouble, z1, z2);
                if (z1 < min1)
                    min1 = z1;
                if (z2 > max1)
                    max1 = z2;
                nedges++;
            }
        }
    }

    GwyMaskIter iter;
    gwy_mask_field_iter_init(mask, iter, maskcol, maskrow + height-1);
    gboolean curr = !gwy_mask_iter_get(iter);
    for (guint j = 1; j < width; j++) {
        gboolean right = curr;
        gwy_mask_iter_next(iter);
        curr = !gwy_mask_iter_get(iter);
        if (curr == invert && right == invert) {
            gdouble z1 = base[(height-1)*xres + j-1],
                    z2 = base[(height-1)*xres + j];
            GWY_ORDER(gdouble, z1, z2);
            if (z1 < min1)
                min1 = z1;
            if (z2 > max1)
                max1 = z2;
            nedges++;
        }
    }

    *min = min1;
    *max = max1;

    return nedges;
}

static inline void
add_to_min_max_dist(GwyLine *mindist, GwyLine *maxdist,
                    gdouble z1, gdouble z2)
{
    GWY_ORDER(gdouble, z1, z2);
    add_to_dist(mindist, z1);
    add_to_dist(maxdist, z2);
}

static void
calculate_min_max_dist(const GwyField *field,
                       guint col, guint row,
                       guint width, guint height,
                       const GwyMaskField *mask,
                       GwyMaskingType masking,
                       guint maskcol, guint maskrow,
                       GwyLine *mindist, GwyLine *maxdist)
{
    guint xres = field->xres;

    if (masking == GWY_MASK_IGNORE) {
        for (guint i = 0; i < height-1; i++) {
            const gdouble *frow = field->data + (row + i)*xres + col;
            add_to_min_max_dist(mindist, maxdist, *frow, *(frow + xres));
            for (guint j = width-1; j; j--) {
                add_to_min_max_dist(mindist, maxdist, *frow, *(frow + 1));
                frow++;
                add_to_min_max_dist(mindist, maxdist, *frow, *(frow + xres));
            }
        }
        const gdouble *frow = field->data + (row + height-1)*xres + col;
        for (guint j = width-1; j; j--, frow++)
            add_to_min_max_dist(mindist, maxdist, *frow, *(frow + 1));

        return;
    }

    g_assert(mask);

    gboolean invert = (masking == GWY_MASK_EXCLUDE);

    for (guint i = 0; i < height-1; i++) {
        const gdouble *frow = field->data + (row + i)*xres + col;
        GwyMaskIter iter1, iter2;
        gwy_mask_field_iter_init(mask, iter1, maskcol, maskrow + i);
        gwy_mask_field_iter_init(mask, iter2, maskcol, maskrow + i+1);

        gboolean curr = !gwy_mask_iter_get(iter1);
        gboolean lower = !gwy_mask_iter_get(iter2);
        if (curr == invert && lower == invert)
            add_to_min_max_dist(mindist, maxdist, *frow, *(frow + xres));

        for (guint j = width-1; j; j--) {
            gboolean right = curr;
            gwy_mask_iter_next(iter1);
            gwy_mask_iter_next(iter2);
            curr = !gwy_mask_iter_get(iter1);
            lower = !gwy_mask_iter_get(iter2);
            if (curr == invert && right == invert)
                add_to_min_max_dist(mindist, maxdist, *frow, *(frow + 1));
            frow++;
            if (curr == invert && lower == invert)
                add_to_min_max_dist(mindist, maxdist, *frow, *(frow + xres));
        }
    }

    const gdouble *frow = field->data + (row + height-1)*xres + col;
    GwyMaskIter iter;
    gwy_mask_field_iter_init(mask, iter, maskcol, maskrow + height-1);
    gboolean curr = !gwy_mask_iter_get(iter);
    for (guint j = width-1; j; j--, frow++) {
        gboolean right = curr;
        gwy_mask_iter_next(iter);
        curr = !gwy_mask_iter_get(iter);
        if (curr == invert && right == invert)
            add_to_min_max_dist(mindist, maxdist, *frow, *(frow + 1));
    }
}

/* The calculation is based on expressing the functional as the difference of
 * cumulative distributions of min(z1, z2) and max(z1, z2) where (z1, z2) are
 * couples of pixels with a common edge. */
static GwyLine*
minkowski_boundary(const GwyField *field,
                   guint col, guint row,
                   guint width, guint height,
                   const GwyMaskField *mask,
                   GwyMaskingType masking,
                   guint maskcol, guint maskrow,
                   guint npoints,
                   gdouble min, gdouble max)
{
    gdouble min1, max1;
    guint nedges = count_edges(field, col, row, width, height,
                               mask, masking, maskcol, maskrow,
                               &min1, &max1);

    if (!nedges)
        return NULL;

    // FIXME: For npoints, it would be more useful to count only edges in
    // range, not all edges.  However, the total number if needed for
    // normalization.
    if (!npoints)
        npoints = dist_points_for_n_points(nedges);

    if (!(min < max)) {
        min = min1;
        max = max1;
    }
    sanitise_range(&min, &max);

    GwyLine *line = gwy_line_new_sized(npoints, TRUE);
    line->real = max - min;
    line->off = min;
    GwyLine *mindist = line, *maxdist = gwy_line_duplicate(mindist);
    calculate_min_max_dist(field, col, row, width, height,
                           mask, masking, maskcol, maskrow,
                           mindist, maxdist);
    // The non-cumulative distributions are already prepared pixel-centered so
    // use plain summing here.
    gwy_line_accumulate(mindist, FALSE);
    gwy_line_accumulate(maxdist, FALSE);
    gwy_line_add_line(maxdist, NULL, mindist, 0, -1.0);
    g_object_unref(maxdist);
    gwy_line_multiply(mindist, 1.0/nedges);

    return line;
}

/* Calculate discrete heights.
 *
 * There are @npoints+1 buckes, the 0th collects everything below 1/2, the
 * rest is pixel-sized, the last collects evrything above @npoints-1/2 . This
 * makes the distribution pixel-centered and symmetrical for white and black
 * cases, as necessary.
 */
static guint*
discretise_heights(const GwyField *field,
                   guint col, guint row,
                   guint width, guint height,
                   const GwyMaskField *mask,
                   GwyMaskingType masking,
                   guint maskcol, guint maskrow,
                   guint npoints,
                   gdouble min, gdouble max,
                   gboolean white)
{
    guint *heights = g_new(guint, width*height);
    gdouble q = npoints/(max - min);
    gboolean invert = (masking == GWY_MASK_EXCLUDE);

    for (guint i = 0; i < height; i++) {
        const gdouble *drow = field->data + (i + row)*field->xres + col;
        guint *hrow = heights + i*width;

        if (masking == GWY_MASK_IGNORE) {
            for (guint j = width; j; j--, drow++, hrow++) {
                gdouble x = white ? (max - *drow) : (*drow - min);
                x = ceil(x*q - 0.5);
                x = CLAMP(x, 0.0, npoints);
                *hrow = (guint)x;
            }
        }
        else {
            GwyMaskIter iter;
            gwy_mask_field_iter_init(mask, iter, maskcol, maskrow + i);
            for (guint j = width; j; j--, drow++, hrow++) {
                if (!gwy_mask_iter_get(iter) == invert) {
                    gdouble x = white ? (max - *drow) : (*drow - min);
                    x = ceil(x*q - 0.5);
                    x = CLAMP(x, 0.0, npoints);
                    *hrow = (guint)x;
                }
                else {
                    *hrow = G_MAXUINT;
                }
                gwy_mask_iter_next(iter);
            }
        }
    }

    return heights;
}

/*
 * Group pixels of the same discrete height.  nh then holds indices in
 * hindex where each height starts.
 */
static void
group_by_height(const guint *heights,
                guint npoints, guint size,
                guint *nh, guint *hindex)
{
    // Make nh[i] the start of the block of discrete height i in hindex[].
    for (guint i = 0; i < size; i++) {
        guint h = heights[i];
        if (h != G_MAXUINT)
            nh[h]++;
    }
    for (guint i = 1; i <= npoints; i++)
        nh[i] += nh[i-1];
    for (guint i = npoints; i; i--)
        nh[i] = nh[i-1];
    nh[0] = 0;

    // Fill the blocks in hindex[] with indices of points with the
    // corresponding discrete height.
    for (guint i = 0; i < size; i++) {
        guint h = heights[i];
        if (h != G_MAXUINT)
            hindex[nh[h]++] = i;
    }
    for (guint i = npoints+1; i; i--)
        nh[i] = nh[i-1];
    nh[0] = 0;
}

static inline guint
uniq_array(guint *x, guint n)
{
    guint i = 1;
    while (i < n) {
        guint j;
        for (j = 0; j < i; j++) {
            if (x[i] == x[j])
                break;
        }

        if (j < i) {
            x[i] = x[--n];
        }
        else
            i++;
    }
    return n;
}

static guint
compress_grain_numbers(guint *grains, guint n, guint *m, guint ngrains)
{
    // Build a fully resolved renumbering.
    guint count = 1;
    for (guint i = 1; i < ngrains; i++) {
        if (m[i] == i)
            m[i] = count++;
        else
            m[i] = m[m[i]];
    }

    // Apply the renumbering to grains.
    for (guint k = n; k; k--, grains++)
        *grains = m[*grains];

    // Apply the renumbering to the map.  This is trivial because the result
    // must be the sequence 1..count-1.
    for (guint i = 1; i < count; i++)
        m[i] = i;

    return count;
}

/**
 * grain_number_dist:
 * @data_field: A data field.
 * @target_line: A data line to store the distribution to.  It will be
 *               resampled to the requested width.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 * @min: Minimum threshold value.
 * @max: Maximum threshold value.
 * @white: If %TRUE, hills are marked, otherwise valleys are marked.
 * @nstats: The number of samples to take on the distribution function.  If
 *          nonpositive, a suitable resolution is determined automatically.
 *
 * Calculates threshold grain number distribution in given height range.
 *
 * This is the number of grains for each of @nstats equidistant height
 * threshold levels.  For large @nstats this function is much faster than the
 * equivalent number of gwy_data_field_grains_mark_height() calls.
 **/
static GwyLine*
grain_number_dist(const GwyField *field,
                  guint col, guint row,
                  guint width, guint height,
                  const GwyMaskField *mask,
                  GwyMaskingType masking,
                  guint maskcol, guint maskrow,
                  gboolean white,
                  guint npoints,
                  gdouble min, gdouble max)
{
    GwyFieldPart rect = { maskcol, maskrow, width, height };
    guint n = gwy_mask_field_part_count_masking(mask, &rect, masking);

    if (!n)
        return NULL;

    if (!npoints)
        npoints = dist_points_for_n_points(n);

    GwyLine *line = gwy_line_new_sized(npoints, FALSE);
    line->real = max - min;
    line->off = min;

    guint *heights = discretise_heights(field, col, row, width, height,
                                        mask, masking, maskcol, maskrow,
                                        npoints, min, max, white);
    guint *nh = g_new0(guint, npoints+2);
    guint *hindex = g_new(guint, n);
    group_by_height(heights, npoints, width*height, nh, hindex);

    guint *grains = heights;     // No longer needed.
    gwy_clear(grains, width*height);
    guint *m = g_new(guint, n+1);
    m[0] = 0;

    // Main iteration
    guint ngrains = 1;
    for (guint h = 0; h < npoints; h++) {
        if (h && nh[h] == nh[h+1]) {
            line->data[h] = line->data[h-1];
            continue;
        }

        for (guint l = nh[h]; l < nh[h+1]; l++) {
            guint k = hindex[l], i = k/width, j = k % width;
            g_assert(!grains[k]);

            // Find grain numbers of neighbours, if any.
            guint neigh[4], nn = 0;
            if (i && grains[k-width])
                neigh[nn++] = grains[k-width];
            if (j && grains[k-1])
                neigh[nn++] = grains[k-1];
            if (j < width-1 && grains[k+1])
                neigh[nn++] = grains[k+1];
            if (i < height-1 && grains[k+width])
                neigh[nn++] = grains[k+width];

            if (nn) {
                // Merge all grains that touch this pixel to one.
                nn = uniq_array(neigh, nn);
                for (guint p = 1; p < nn; p++)
                    resolve_grain_map(m, neigh[p-1], neigh[p]);
                guint ming = m[neigh[0]];
                for (guint p = 1; p < nn; p++) {
                    if (m[neigh[p]] < ming)
                        ming = m[neigh[p]];
                }
                // And this is also the number the new pixel gets.
                grains[k] = ming;
            }
            else {
                // A new grain not touching anything gets a new number.
                g_assert(ngrains <= n);
                m[ngrains] = ngrains;
                grains[k] = ngrains++;
                continue;
            }

        }

        // Resolve remaining grain number links in the map.  This nicely works
        // because we resolve downwards and go from the lowest number.
        guint count = 0;
        for (guint i = 1; i < ngrains; i++) {
            m[i] = m[m[i]];
            if (m[i] == i)
                count++;
        }

        line->data[h] = (gdouble)count/n;

        if (7*count/5 + 4 < ngrains)
            ngrains = compress_grain_numbers(grains, width*height, m, ngrains);

#if 0
        g_printerr("GRAINS %u :: %u\n", h, count);
        for (guint i = 0; i < height; i++) {
            for (guint j = 0; j < width; j++) {
                if (grains[i*width + j])
                    g_printerr("%02u", grains[i*width + j]);
                else
                    g_printerr("..");
                g_printerr("%c", j == width-1 ? '\n' : ' ');
            }
        }
        g_printerr("MAPPED %u\n", h);
        for (guint i = 0; i < height; i++) {
            for (guint j = 0; j < width; j++) {
                if (grains[i*width + j])
                    g_printerr("%02u", m[grains[i*width + j]]);
                else
                    g_printerr("..");
                g_printerr("%c", j == width-1 ? '\n' : ' ');
            }
        }
#endif
    }

    if (white) {
        for (guint j = 0; j < npoints/2; j++)
            GWY_SWAP(gdouble, line->data[j], line->data[npoints-1 - j]);
    }

    g_free(m);
    g_free(hindex);
    g_free(nh);
    g_free(heights);

    return line;
}

static GwyLine*
minkowski_ngrains(const GwyField *field,
                  guint col, guint row,
                  guint width, guint height,
                  const GwyMaskField *mask,
                  GwyMaskingType masking,
                  guint maskcol, guint maskrow,
                  gboolean white,
                  guint npoints,
                  gdouble min, gdouble max)
{
    if (!(min < max))
        gwy_field_min_max(field, &(GwyFieldPart){ col, row, width, height },
                          mask, masking, &min, &max);
    sanitise_range(&min, &max);

    return grain_number_dist(field, col, row, width, height,
                             mask, masking, maskcol, maskrow,
                             white, npoints, min, max);
}

static GwyLine*
minkowski_connectivity(const GwyField *field,
                       guint col, guint row,
                       guint width, guint height,
                       const GwyMaskField *mask,
                       GwyMaskingType masking,
                       guint maskcol, guint maskrow,
                       guint npoints,
                       gdouble min, gdouble max)
{
    if (!(min < max))
        gwy_field_min_max(field, &(GwyFieldPart){ col, row, width, height },
                          mask, masking, &min, &max);
    sanitise_range(&min, &max);

    GwyLine *whitedist = grain_number_dist(field, col, row, width, height,
                                           mask, masking, maskcol, maskrow,
                                           TRUE, npoints, min, max);
    if (!whitedist)
        return NULL;

    GwyLine *blackdist = grain_number_dist(field, col, row, width, height,
                                           mask, masking, maskcol, maskrow,
                                           FALSE, npoints, min, max);
    gwy_line_add_line(blackdist, NULL, whitedist, 0, -1.0);
    g_object_unref(blackdist);

    return whitedist;
}

/**
 * gwy_field_minkowski:
 * @field: A two-dimensional data field.
 * @fpart: (allow-none):
 *         Area in @field to process.  Pass %NULL to process entire @field.
 * @mask: (allow-none):
 *        Mask specifying which values to take into account/exclude, or %NULL.
 * @masking: Masking mode to use (has any effect only with non-%NULL @mask).
 * @type: Type of functional to calculate.
 * @npoints: Resolution, i.e. the number of returned line points.
 *           Pass zero to choose a suitable resolution automatically.
 * @min: Minimum value of the range to calculate the distribution in.
 * @max: Maximum value of the range to calculate the distribution in.
 *
 * Calculates given Minkowski functional of values in a field.
 *
 * Pass @max <= @min to calculate the functional in the full data range
 * (with masking still considered).
 *
 * Note at present masking is implemented only for the volume and boundary
 * functionals %GWY_MINKOWSKI_VOLUME and %GWY_MINKOWSKI_BOUNDARY.
 *
 * Returns: (transfer full):
 *          A new one-dimensional data line with the requested functional.
 **/
GwyLine*
gwy_field_minkowski(const GwyField *field,
                    const GwyFieldPart *fpart,
                    const GwyMaskField *mask,
                    GwyMaskingType masking,
                    GwyMinkowskiFunctionalType type,
                    guint npoints,
                    gdouble min, gdouble max)
{
    GwyLine *line = NULL;

    guint col, row, width, height, maskcol, maskrow;
    if (!gwy_field_check_mask(field, fpart, mask, &masking,
                              &col, &row, &width, &height, &maskcol, &maskrow))
        goto fail;

    // Cannot determine npoints here, it depends on the functional.
    if (type == GWY_MINKOWSKI_VOLUME) {
        line = minkowski_volume(field, col, row, width, height,
                                mask, masking, maskcol, maskrow,
                                npoints, min, max);
    }
    else if (type == GWY_MINKOWSKI_BOUNDARY) {
        line = minkowski_boundary(field, col, row, width, height,
                                  mask, masking, maskcol, maskrow,
                                  npoints, min, max);
    }
    else if (type == GWY_MINKOWSKI_BLACK) {
        line = minkowski_ngrains(field, col, row, width, height,
                                 mask, masking, maskcol, maskrow,
                                 FALSE, npoints, min, max);
    }
    else if (type == GWY_MINKOWSKI_WHITE) {
        line = minkowski_ngrains(field, col, row, width, height,
                                 mask, masking, maskcol, maskrow,
                                 TRUE, npoints, min, max);
    }
    else if (type == GWY_MINKOWSKI_CONNECTIVITY) {
        line = minkowski_connectivity(field, col, row, width, height,
                                      mask, masking, maskcol, maskrow,
                                      npoints, min, max);
    }
    else {
        g_critical("Unknown Minkowski functional type %u.", type);
    }

fail:
    if (!line)
        line = gwy_line_new();

    gwy_unit_assign(gwy_line_get_xunit(line), gwy_field_get_zunit(field));

    return line;
}

static void
hhcf_running_sums(GwyField *rsum,
                  const gdouble *data, guint stride, guint width, guint height,
                  gdouble *buf)
{
    guint xres = rsum->xres, yres = rsum->yres;
    guint xrange = xres/2, yrange = yres/2;
    gdouble *rsdata = rsum->data;
    g_assert(xres == 2*xrange+1);
    g_assert(yres == 2*yrange+1);

    // Same-sign HHCF arguments, sums growing along the major diagonal.
    gwy_clear(buf, width);
    for (guint i = 0; i < height; i++) {
        const gdouble *row1 = data + i*stride;
        const gdouble *row2 = data + (height-1 - i)*stride + width-1;
        gdouble s = 0.0;
        for (guint j = 0; j < width; j++, row1++, row2--) {
            gdouble z1 = *row1, z2 = *row2;
            buf[j] += z1*z1 + z2*z2;
            s += buf[j];
            guint ii = height-1 - i, jj = width-1 - j;
            if (ii <= yrange && jj <= xrange) {
                rsdata[(yrange + ii)*xres + xrange + jj] = s;
                rsdata[(yrange - ii)*xres + xrange - jj] = s;
            }
        }
    }

    // Opposite-sign HHCF arguments, sums growing along the minor diagonal.
    gwy_clear(buf, width);
    for (guint i = 0; i < height; i++) {
        const gdouble *row1 = data + (height-1 - i)*stride;
        const gdouble *row2 = data + i*stride + width-1;
        gdouble s = 0.0;
        for (guint j = 0; j < width; j++, row1++, row2--) {
            gdouble z1 = *row1, z2 = *row2;
            buf[j] += z1*z1 + z2*z2;
            s += buf[j];
            guint ii = height-1 - i, jj = width-1 - j;
            if (ii <= yrange && jj <= xrange) {
                rsdata[(yrange + ii)*xres + xrange - jj] = s;
                rsdata[(yrange - ii)*xres + xrange + jj] = s;
            }
        }
    }
}

static GwyField*
gwy_field_cf(const GwyField *field,
             const GwyFieldPart *fpart,
             guint xrange, guint yrange,
             guint level,
             CFType type)
{
    guint col, row, width, height;
    if (!gwy_field_check_part(field, fpart, &col, &row, &width, &height))
        return gwy_field_new();

    if (level > 1) {
        g_warning("Levelling degree %u is not supported, changing to 1.",
                  level);
        level = 1;
    }
    if (xrange >= width) {
        g_warning("CF x range is not smaller than width, fixing it.");
        xrange = width-1;
    }
    if (yrange >= height) {
        g_warning("CF y range is not smaller than height, fixing it.");
        yrange = height-1;
    }

    guint xsize = gwy_fft_nice_transform_size(width + xrange);
    guint ysize = gwy_fft_nice_transform_size(height + yrange);
    // The innermost (contiguous) dimension of R2C the complex output is
    // slightly larger than the real input.  Note @cstride is measured in
    // gwycomplex, multiply it by 2 for doubles.
    gsize cxsize = xsize/2 + 1;
    gdouble *fftr = fftw_alloc_real(xsize*ysize);
    gwycomplex *fftc = fftw_alloc_complex(cxsize*ysize);
    fftw_plan plan = fftw_plan_dft_r2c_2d(ysize, xsize, fftr, fftc,
                                          FFTW_DESTROY_INPUT
                                          | _gwy_fft_rigour());
    g_assert(plan);

    guint xres = field->xres;
    if (level == 1) {
        gdouble mean = gwy_field_mean(field, fpart, NULL, GWY_MASK_IGNORE);
        for (guint i = 0; i < height; i++) {
            const gdouble *drow = field->data + (i + row)*xres + col;
            gdouble *rrow = fftr + i*xsize;
            row_copy_subtract(drow, rrow, width, mean);
            gwy_clear(rrow + width, xsize - width);
        }
    }
    else {
        for (guint i = 0; i < height; i++) {
            const gdouble *drow = field->data + (i + row)*xres + col;
            gdouble *rrow = fftr + i*xsize;
            gwy_assign(rrow, drow, width);
            gwy_clear(rrow + width, xsize - width);
        }
    }

    GwyField *cf = gwy_field_new_sized(2*xrange + 1, 2*yrange + 1, TRUE);
    guint txres = cf->xres, tyres = cf->yres;

    if (type == CF_HHCF) {
        if (yrange)
            hhcf_running_sums(cf, fftr, xsize, width, height,
                              fftr + xsize*height);
        else {
            // If yrange=0 we cannot use the lower (to be zeroed) part of fftr
            // as a temporary scratch space because there is no lower part.
            gdouble *workspace = g_new(gdouble, width);
            hhcf_running_sums(cf, fftr, xsize, width, height, workspace);
            g_free(workspace);
        }
    }

    gwy_clear(fftr + xsize*height, xsize*(ysize - height));

    fftw_execute(plan);

    row2_assign_cnorm(fftr, fftr + xsize-1, fftc, xsize, 1.0/ysize);
    for (guint i = 1; i < ysize; i++) {
        const gwycomplex *crow = fftc + i*cxsize;
        gdouble *rrow = fftr + i*xsize,
                *rrow2 = fftr + (ysize - i)*xsize + xsize-1;
        row2_assign_cnorm(rrow, rrow2, crow, xsize, 1.0/ysize);
    }

    fftw_execute(plan);
    fftw_destroy_plan(plan);
    fftw_free(fftr);

    for (guint i = 0; i <= yrange; i++) {
        const gwycomplex *crow = fftc + i*cxsize;
        gdouble *frow = cf->data + (yrange + i)*txres + xrange;
        gdouble *brow = cf->data + (yrange - i)*txres + xrange;
        if (type == CF_ACF) {
            for (guint j = 0; j <= xrange; j++, crow++, frow++, brow--) {
                gdouble v = creal(*crow);
                *frow = *brow = v/(height - i)/(width - j);
            }
        }
        else if (type == CF_HHCF) {
            guint from = i ? 0 : 1;  // Don't process the central point twice.
            for (guint j = from; j <= xrange; j++, crow++, frow++, brow--) {
                gdouble v = creal(*crow);
                *frow = (*frow - 2.0*v)/(height - i)/(width - j);
                *brow = (*brow - 2.0*v)/(height - i)/(width - j);
            }
        }
    }
    for (guint i = 1; i <= yrange; i++) {
        const gwycomplex *crow = fftc + (ysize - i)*cxsize + 1;
        gdouble *frow = cf->data + (yrange - i)*txres + (xrange+1);
        gdouble *brow = cf->data + (yrange + i)*txres + (xrange-1);
        if (type == CF_ACF) {
            for (guint j = 1; j <= xrange; j++, crow++, frow++, brow--) {
                gdouble v = creal(*crow);
                *frow = *brow = v/(height - i)/(width - j);
            }
        }
        else if (type == CF_HHCF) {
            for (guint j = 1; j <= xrange; j++, crow++, frow++, brow--) {
                gdouble v = creal(*crow);
                *frow = (*frow - 2.0*v)/(height - i)/(width - j);
                *brow = (*brow - 2.0*v)/(height - i)/(width - j);
            }
        }
    }
    fftw_free(fftc);

    gdouble dx = gwy_field_dx(field), dy = gwy_field_dy(field);
    gwy_field_set_xreal(cf, dx*txres);
    gwy_field_set_yreal(cf, dy*tyres);
    gwy_field_set_xoffset(cf, -0.5*cf->xreal);
    gwy_field_set_yoffset(cf, -0.5*cf->yreal);

    _gwy_assign_unit(&cf->priv->xunit, field->priv->xunit);
    _gwy_assign_unit(&cf->priv->yunit, field->priv->yunit);
    gwy_unit_power(gwy_field_get_zunit(cf), field->priv->zunit, 2);

    return cf;
}

/**
 * gwy_field_acf:
 * @field: A two-dimensional data field.
 * @fpart: (allow-none):
 *         Area in @field to process.  Pass %NULL to process entire @field.
 * @xrange: Maximum horizontal shift (in pixels) to include in the result.
 *          It must not be larger than the field part width.
 * @yrange: Maximum vertical shift (in pixels) to include in the result.
 *          It must not be larger than the field part height.
 * @level: The first polynomial degree to keep in the rows, lower degrees than
 *         @level are subtracted.  Note only values 0 (no levelling) and 1
 *         (subtract the mean value of each row) are available at present.
 *
 * Calculated two-dimensional autocorrelation function (ACF) of a field.
 *
 * The returned field will have dimensions (2@xrange + 1)×(2@yrange + 1), with
 * the zero-shift autocorrelation function in the centre.  Since the
 * two-dimensional ACF has C₂ symmetry half of the output is redundant but it
 * is included for convenience.
 *
 * Returns: (transfer full):
 *          A new two-dimensional data field with the requested functional.
 **/
GwyField*
gwy_field_acf(const GwyField *field,
              const GwyFieldPart *fpart,
              guint xrange, guint yrange,
              guint level)
{
    return gwy_field_cf(field, fpart, xrange, yrange, level, CF_ACF);
}

/**
 * gwy_field_hhcf:
 * @field: A two-dimensional data field.
 * @fpart: (allow-none):
 *         Area in @field to process.  Pass %NULL to process entire @field.
 * @xrange: Maximum horizontal shift (in pixels) to include in the result.
 *          It must not be larger than the field part width.
 * @yrange: Maximum vertical shift (in pixels) to include in the result.
 *          It must not be larger than the field part height.
 * @level: The first polynomial degree to keep in the rows, lower degrees than
 *         @level are subtracted.  Note only values 0 (no levelling) and 1
 *         (subtract the mean value of each row) are available at present.
 *
 * Calculated two-dimensional height-height correlation function (HHCF) of a
 * field.
 *
 * The returned field will have dimensions (2@xrange + 1)×(2@yrange + 1), with
 * the zero-shift height-height correlation function in the centre.  Since the
 * two-dimensional HHCF has C₂ symmetry half of the output is redundant but it
 * is included for convenience.
 *
 * Returns: (transfer full):
 *          A new two-dimensional data field with the requested functional.
 **/
GwyField*
gwy_field_hhcf(const GwyField *field,
               const GwyFieldPart *fpart,
               guint xrange, guint yrange,
               guint level)
{
    return gwy_field_cf(field, fpart, xrange, yrange, level, CF_HHCF);
}

static void
gather_interpolated(gdouble *sums, gdouble *weights, gint npoints,
                    gdouble real, gdouble off,
                    gdouble x, gdouble y, gdouble z)
{
    gdouble l = (hypot(x, y) - off)/real*npoints;
    gint il = floor(l);
    il = CLAMP(il, 0, npoints-1);
    gdouble w1 = fabs(l - il - 0.5), w0 = 1.0 - w1;
    sums[il] += w0*z;
    weights[il] += w0;
    if (l > il + 0.5) {
        if (G_LIKELY(il < npoints-1))
            il++;
    }
    else {
        if (G_LIKELY(il))
            il--;
    }
    sums[il] += w1*z;
    weights[il] += w1;
}

/**
 * gwy_field_angular_average:
 * @field: A two-dimensional data field.
 *         Its @x and @y physical dimensions should have the same units.
 * @fpart: (allow-none):
 *         Area in @field to process.  Pass %NULL to process entire @field.
 * @mask: (allow-none):
 *        Mask specifying which values to take into account/exclude, or %NULL.
 * @masking: Masking mode to use (has any effect only with non-%NULL @mask).
 * @npoints: Resolution, i.e. the preferred number of returned curve points.
 *           Pass zero to choose a suitable resolution automatically.
 *
 * Averages angularly data in a two-dimensional data field.
 *
 * Angular averaging results in one-dimensional data in which the abscissa is
 * the distance from origin.  The averaging is carried out around the origin in
 * physical coordinates.  Field functions calculating the Fourier transform,
 * autocorrelation function and similar quantities usually arrange the data and
 * set offsets so that this method can be used directly on their output.
 *
 * It is <emphasis>not</emphasis> guaranteed that the returned curve will have
 * exactly @npoints points.  In fact, if the combination of field part
 * selection and masking leaves no actual data to average, the returned curve
 * will be empty regardless of @npoints.
 *
 * Returns: (transfer full):
 *          A new one-dimensional curve with angularly averaged field data.
 **/
GwyCurve*
gwy_field_angular_average(const GwyField *field,
                          const GwyFieldPart *fpart,
                          const GwyMaskField *mask,
                          GwyMaskingType masking,
                          guint npoints)
{
    guint col, row, width, height, maskcol, maskrow;
    GwyCurve *curve = NULL;

    if (!gwy_field_check_mask(field, fpart, mask, &masking,
                              &col, &row, &width, &height, &maskcol, &maskrow))
        goto fail;

    if (!gwy_unit_equal(field->priv->xunit, field->priv->yunit))
        g_warning("Angular averaging requires same lateral units.");

    // Figure out suitable uniform sampling.  Since we return a curve a
    // non-uniform sampling might be advantageous but I have no idea how to
    // find a suitable one in the presence of a mask.
    gdouble xoff = field->xoff, yoff = field->yoff,
            dx = gwy_field_dx(field), dy = gwy_field_dy(field);
    gdouble xl = (col + 0.5)*dx + xoff,
            xr = (col + width - 0.5)*dx + xoff,
            yu = (row + 0.5)*dy + yoff,
            yl = (row + height - 0.5)*dy + yoff;
    gdouble xm = (xl*xr > 0.0) ? fmin(fabs(xl), fabs(xr)) : 0.0;
    gdouble ym = (yu*yl > 0.0) ? fmin(fabs(yu), fabs(yl)) : 0.0;
    gdouble xM = fmax(fabs(xl), fabs(xr));
    gdouble yM = fmax(fabs(yu), fabs(yl));
    gdouble Lmin = hypot(xm, ym), Lmax = hypot(xM, yM);

    // Handle the silly case separately.
    if (npoints == 1) {
        gdouble mean = gwy_field_mean(field, fpart, mask, masking);
        if (isfinite(mean)) {
            curve = gwy_curve_new_sized(npoints);
            curve->data[0] = (GwyXY){ 0.5*(Lmin + Lmax), mean };
        }
        goto fail;
    }

    if (!npoints) {
        // This makes 1st item typically missing but it distinguishes nicely
        // radii 1, √2 and 2 in the 2nd, 3rd and 4th item.
        gdouble edl = 0.24*(gwy_field_dx(field) + gwy_field_dy(field));
        npoints = gwy_round((Lmax - Lmin)/edl + 1.0);
    }
    gdouble dl = (Lmax - Lmin)/(npoints - 1);

    guint xres = field->xres;
    gdouble *sums = g_new0(gdouble, npoints);
    gdouble *weights = g_new0(gdouble, npoints);
    gdouble off = Lmin - 0.5*dl;
    gdouble real = Lmax - Lmin + dl;

    const gdouble *base = field->data + row*xres + col;
    if (masking == GWY_MASK_IGNORE) {
        for (guint i = 0; i < height; i++) {
            const gdouble *d = base + i*xres;
            gdouble y = i*dy + yu;
            for (guint j = 0; j < width; j++, d++) {
                gdouble x = j*dx + xl;
                gather_interpolated(sums, weights, npoints, real, off,
                                    x, y, *d);
            }
        }
    }
    else {
        const gboolean invert = (masking == GWY_MASK_EXCLUDE);
        for (guint i = 0; i < height; i++) {
            gdouble y = i*dy + yu;
            const gdouble *d = base + i*xres;
            GwyMaskIter iter;
            gwy_mask_field_iter_init(mask, iter, maskcol, maskrow + i);
            for (guint j = 0; j < width; j++, d++) {
                if (!gwy_mask_iter_get(iter) == invert) {
                    gdouble x = j*dx + xl;
                    gather_interpolated(sums, weights, npoints, real, off,
                                        x, y, *d);
                }
                gwy_mask_iter_next(iter);
            }
        }
    }

    guint ngood = 0;
    for (guint i = 0; i < npoints; i++)
        ngood += (weights[i] > 1e-9);

    curve = gwy_curve_new_sized(ngood);
    for (guint i = 0, j = 0; i < npoints; i++) {
        if (weights[i] > 1e-9) {
            curve->data[j++] = (GwyXY){
                (i + 0.5)*dl + off, sums[i]/weights[i],
            };
        }
    }

    g_free(weights);
    g_free(sums);

fail:
    if (!curve)
        curve = gwy_curve_new();

    _gwy_assign_unit(&curve->priv->xunit, field->priv->xunit);
    _gwy_assign_unit(&curve->priv->yunit, field->priv->zunit);

    return curve;
}

/**
 * SECTION: field-distributions
 * @section_id: GwyField-distributions
 * @title: GwyField distributions
 * @short_description: One- and two-dimensional distributions and functionals of fields
 *
 * Statistical distribution densities are normalised so that their integral,
 * that can also be calculated as gwy_line_mean(line)*line->real, is unity.
 * Cumulative distribution values then always line in the interval [0,1].
 **/

/**
 * GwyMinkowskiFunctionalType:
 * @GWY_MINKOWSKI_VOLUME: Fraction of ‘white’ pixels from the total
 *                        number of pixels.
 * @GWY_MINKOWSKI_BOUNDARY: Fraction of ‘black–white’ pixel edges from the
 *                          total number of edges.
 * @GWY_MINKOWSKI_BLACK: The number of ‘black’ connected areas (grains) divided
 *                       by the total number of pixels.
 * @GWY_MINKOWSKI_WHITE: The number of ‘white’ connected areas (grains) divided
 *                       by the total number of pixels.
 * @GWY_MINKOWSKI_CONNECTIVITY: Difference between the numbers of ‘white’ and
 *                              ‘black’ connected areas (grains) divided by
 *                              the total number of pixels.
 *
 * Types of Minkowski functionals and related quantities.
 *
 * Each quantity is a function of threshold; pixels above this threshold are
 * considered ‘white’, pixels below ‘black’.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
