/*
 *  $Id$
 *  Copyright (C) 2009-2011 David Nečas (Yeti).
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
#include "libgwy/math-internal.h"
#include "libgwy/field-internal.h"

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
                               value_dist_cont, &ddata);

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
                               value_dist_cont, &ddata);
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

        if (masking == GWY_MASK_INCLUDE)
            n = gwy_mask_field_part_count(mask, &rect, TRUE);
        else if (masking == GWY_MASK_EXCLUDE)
            n = gwy_mask_field_part_count(mask, &rect, FALSE);
        else
            n = width*height;

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
    gdouble q = (max - min)*npoints;

    if (masking == GWY_MASK_IGNORE) {
        for (guint i = 0; i < height; i++) {
            const gdouble *d = base + i*field->xres;
            for (guint j = width; j; j--, d++) {
                if (*d < min)
                    ddata->left_sum++;
                else if (*d <= max) {
                    guint k = (guint)((*d - min)/q);
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
                        guint k = (guint)((*d - min)/q);
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

    gwy_unit_assign(gwy_line_get_unit_x(line), gwy_field_get_unit_z(field));
    if (!cumulative)
        gwy_unit_power(gwy_line_get_unit_y(line),
                       gwy_field_get_unit_z(field), -1);

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

    gwy_unit_divide(gwy_line_get_unit_x(line),
                    gwy_field_get_unit_z(field), gwy_field_get_unit_xy(field));
    if (!cumulative)
        gwy_unit_power(gwy_line_get_unit_y(line),
                       gwy_field_get_unit_z(field), -1);

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
                gwycomplex *fftc,
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

// Used in cases when we expted the imaginary part to be zero but do not want
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

    gdouble a = sumsi/n;
    pdata = in;
    for (guint i = n; i; i--, pdata++, out++)
        *out = *pdata - a;
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
 *
 * Calculates the row-wise autocorrelation function of a field.
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
    gdouble *fftr = fftw_malloc(3*size*sizeof(gdouble));
    gdouble *accum_data = fftr + size;
    gdouble *accum_mask = fftr + 2*size;
    gwycomplex *fftc = fftw_malloc(cstride*sizeof(gwycomplex));
    guint nfullrows = 0, nemptyrows = 0;

    fftw_plan plan = fftw_plan_dft_r2c_1d(size, fftr, fftc,
                                          FFTW_DESTROY_INPUT
                                          | _gwy_fft_rigour());
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

    fftw_destroy_plan(plan);
    fftw_free(fftc);
    fftw_free(fftr);
    line->real = gwy_field_dx(field)*line->res;
    line->off = -0.5*gwy_line_dx(line);

fail:
    if (!line)
        line = gwy_line_new();

    gwy_unit_power(gwy_line_get_unit_x(line), gwy_field_get_unit_xy(field), -1);
    gwy_unit_power_multiply(gwy_line_get_unit_y(line),
                            gwy_field_get_unit_xy(field), 1,
                            gwy_field_get_unit_z(field), 2);
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
 * Calculates the row-wise power spectrum density function of a rectangular
 * part of a field.
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
    gdouble *fftr = fftw_malloc(4*size*sizeof(gdouble));
    gdouble *accum_data = fftr + 1*size;
    gdouble *accum_mask = fftr + 2*size;
    gdouble *window = fftr + 3*size;
    gwycomplex *fftc = fftw_malloc(cstride*sizeof(gwycomplex));
    guint nfullrows = 0, nemptyrows = 0;

    gwy_clear(accum_data, size);
    gwy_clear(accum_mask, size);

    gwy_fft_window_sample(window, width, windowing, 2);

    fftw_plan plan = fftw_plan_dft_r2c_1d(width, fftr, fftc,
                                          FFTW_DESTROY_INPUT
                                          | _gwy_fft_rigour());
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

    gwy_unit_power(gwy_line_get_unit_x(line), gwy_field_get_unit_xy(field), -1);
    gwy_unit_power_multiply(gwy_line_get_unit_y(line),
                            gwy_field_get_unit_xy(field), 1,
                            gwy_field_get_unit_z(field), 2);
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
 *
 * Calculates the row-wise height-height correlation function of a rectangular
 * part of a field.
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
    gdouble *fftr = fftw_malloc(4*size*sizeof(gdouble));
    gdouble *accum_data = fftr + size;
    gdouble *accum_mask = fftr + 2*size;
    gdouble *accum_v = fftr + 3*size;
    gwycomplex *fftc = fftw_malloc(2*cstride*sizeof(gwycomplex));
    gwycomplex *tmp = fftc + cstride;
    guint nfullrows = 0, nemptyrows = 0;
    gdouble *p;
    const gdouble *q;

    fftw_plan plan = fftw_plan_dft_r2c_1d(size, fftr, fftc,
                                          FFTW_DESTROY_INPUT
                                          | _gwy_fft_rigour());
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

    fftw_destroy_plan(plan);
    fftw_free(fftc);
    fftw_free(fftr);
    line->real = gwy_field_dx(field)*line->res;
    line->off = -0.5*gwy_line_dx(line);

fail:
    if (!line)
        line = gwy_line_new();

    gwy_unit_power(gwy_line_get_unit_x(line), gwy_field_get_unit_xy(field), -1);
    gwy_unit_power_multiply(gwy_line_get_unit_y(line),
                            gwy_field_get_unit_xy(field), 1,
                            gwy_field_get_unit_z(field), 2);
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
    guint n = width*height;

    if (masking == GWY_MASK_INCLUDE)
        n = gwy_mask_field_part_count(mask, &rect, TRUE);
    else if (masking == GWY_MASK_EXCLUDE)
        n = gwy_mask_field_part_count(mask, &rect, FALSE);

    if (!n)
        return NULL;

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
 *
 * Counts the number of edges between two pixels.
 *
 * An edge is counted if both pixels are counted according to the masking mode
 * @masking.
 *
 * Returns: The number of edges.
 **/
static guint
count_edges(const GwyMaskField *mask,
            GwyMaskingType masking,
            guint maskcol, guint maskrow,
            guint width, guint height)
{
    if (masking == GWY_MASK_IGNORE)
        return 2*width*height - width - height;

    g_assert(mask);

    gboolean invert = (masking == GWY_MASK_EXCLUDE);
    guint nedges = 0;

    for (guint i = 0; i < height-1; i++) {
        GwyMaskIter iter1, iter2;
        gwy_mask_field_iter_init(mask, iter1, maskcol, maskrow + i);
        gwy_mask_field_iter_init(mask, iter2, maskcol, maskrow + i+1);

        gboolean curr = !gwy_mask_iter_get(iter1);
        gboolean lower = !gwy_mask_iter_get(iter2);
        if (curr == invert && lower == invert)
            nedges++;

        for (guint j = width-1; j; j--) {
            gboolean right = curr;
            gwy_mask_iter_next(iter1);
            gwy_mask_iter_next(iter2);
            curr = !gwy_mask_iter_get(iter1);
            lower = !gwy_mask_iter_get(iter2);
            if (curr == invert && right == invert)
                nedges++;
            if (curr == invert && lower == invert)
                nedges++;
        }
    }

    GwyMaskIter iter;
    gwy_mask_field_iter_init(mask, iter, maskcol, maskrow + height-1);
    gboolean curr = !gwy_mask_iter_get(iter);
    for (guint j = width-1; j; j--) {
        gboolean right = curr;
        gwy_mask_iter_next(iter);
        curr = !gwy_mask_iter_get(iter);
        if (curr == invert && right == invert)
            nedges++;
    }

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
            for (guint j = width-1; j; j--, frow++) {
                add_to_min_max_dist(mindist, maxdist, *frow, *(frow + 1));
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

        for (guint j = width-1; j; j--, frow++) {
            gboolean right = curr;
            gwy_mask_iter_next(iter1);
            gwy_mask_iter_next(iter2);
            curr = !gwy_mask_iter_get(iter1);
            lower = !gwy_mask_iter_get(iter2);
            if (curr == invert && right == invert)
                add_to_min_max_dist(mindist, maxdist, *frow, *(frow + 1));
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
minkowski_surface(const GwyField *field,
                  guint col, guint row,
                  guint width, guint height,
                  const GwyMaskField *mask,
                  GwyMaskingType masking,
                  guint maskcol, guint maskrow,
                  guint npoints,
                  gdouble min, gdouble max)
{
    GwyLine *line = NULL;
    guint nedges = count_edges(mask, masking, maskcol, maskrow, width, height);

    if (!nedges)
        return NULL;

    // FIXME: For npoints, it would be more useful to count only edges in
    // range, not all edges.  However, the total number if needed for
    // normalization.
    if (!npoints)
        npoints = dist_points_for_n_points(nedges);

    line = gwy_line_new_sized(npoints, TRUE);
    line->real = max - min;
    line->off = min;
    GwyLine *maxdist = line, *mindist = gwy_line_duplicate(maxdist);
    calculate_min_max_dist(field, col, row, width, height,
                           mask, masking, maskcol, maskrow,
                           mindist, maxdist);
    // The non-cumulative distributions are already prepared pixel-centered so
    // use plain summing here.
    gwy_line_accumulate(mindist, FALSE);
    gwy_line_accumulate(maxdist, FALSE);
    gwy_line_add_line(maxdist, NULL, mindist, 0, -1.0);
    g_object_unref(maxdist);
    gwy_line_multiply(maxdist, 1.0/nedges);

    return line;
}

/**
 * fill_one_grain:
 * @data: Arbitrary integer data.  Grain is formed by values equal to the
 *        value at (@col, @row).
 * @xres: The number of columns in @data.
 * @yres: The number of rows in @data.
 * @col: Column inside a grain.
 * @row: Row inside a grain.
 * @visited: An array of size @xres×@yres that contain zeroes in empty space
 *           and yet unvisited grains.  Current grain will be filled with
 *           @grain_no.
 * @grain_no: Value to fill current grain with.
 * @listv: A working buffer of size at least @xres×@yres/2 + 2, its content is
 *         owerwritten.
 * @listh: A working buffer of size at least @xres×@yres/2 + 2, its content is
 *         owerwritten.
 *
 * Internal function to fill/number a one grain.
 *
 * The @visited, @listv, and @listh buffers are recyclable between calls so
 * they don't have to be allocated and freed for each grain, speeding up
 * sequential grain processing.  Generally, this function itself does not
 * allocate or free any memory.
 *
 * Returns: The number of pixels in the grain.
 **/
static guint
fill_one_grain(const guint *data,
               guint xres, guint yres,
               guint col, guint row,
               guint *visited,
               guint grain_no,
               guint *listv, guint *listh)
{
    g_return_val_if_fail(grain_no, 0);
    guint initial = row*xres + col;
    guint look_for = data[initial];
    guint count = 1;

    // check for a single point
    visited[initial] = grain_no;
    if ((!col || data[initial - 1] != look_for)
        && (!row || data[initial - xres] != look_for)
        && (col + 1 == xres || data[initial + 1] != look_for)
        && (row + 1 == yres || data[initial + xres] != look_for)) {

        return count;
    }

    guint nv = 2, nh = 2, n = xres*yres;
    listv[0] = listv[1] = initial;
    listh[0] = listh[1] = initial;

    while (nv) {
        // go through vertical lines and expand them horizontally
        for (guint i = 0; i < nv; i += 2) {
            for (guint p = listv[i]; p <= listv[i + 1]; p += xres) {
                guint start, stop, j;

                // scan left
                start = p - 1;
                stop = (p/xres)*xres;
                for (j = start; j >= stop; j--) {
                    if (visited[j] || data[j] != look_for)
                        break;
                    visited[j] = grain_no;
                    count++;
                }
                if (j < start) {
                    listh[nh++] = j + 1;
                    listh[nh++] = start;
                }

                // scan right
                start = p + 1;
                stop = (p/xres + 1)*xres;
                for (j = start; j < stop; j++) {
                    if (visited[j] || data[j] != look_for)
                        break;
                    visited[j] = grain_no;
                    count++;
                }
                if (j > start) {
                    listh[nh++] = start;
                    listh[nh++] = j - 1;
                }
            }
        }
        nv = 0;

        // go through horizontal lines and expand them vertically
        for (guint i = 0; i < nh; i += 2) {
            for (guint p = listh[i]; p <= listh[i + 1]; p++) {
                gint start, stop, j;

                // scan up
                start = p - xres;
                stop = p % xres;
                for (j = start; j >= stop; j -= xres) {
                    if (visited[j] || data[j] != look_for)
                        break;
                    visited[j] = grain_no;
                    count++;
                }
                if (j < start) {
                    listv[nv++] = j + xres;
                    listv[nv++] = start;
                }

                // scan down
                start = p + xres;
                stop = p % xres + n;
                for (j = start; j < stop; j += xres) {
                    if (visited[j] || data[j] != look_for)
                        break;
                    visited[j] = grain_no;
                    count++;
                }
                if (j > start) {
                    listv[nv++] = start;
                    listv[nv++] = j - xres;
                }
            }
        }
        nh = 0;
    }

    return count;
}

/* Calculate discrete heights.
 *
 * There are @npoints buckes, the 0th collects everything below 1/2, the rest
 * is pixel-sized, heights above @npoints-1/2 are the uncounted remainder. This
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
    gdouble q = (npoints - 1.0)/(max - min);

    for (guint i = 0; i < height; i++) {
        const gdouble *drow = field->data + (i + row)*field->xres + col;
        guint *hrow = heights + i*width;

        for (guint j = width; j; j--, drow++, hrow++) {
            gdouble x = floor((white ? (*drow - min) : (max - *drow))*q + 0.5);
            x = CLAMP(x, 0.0, npoints-1);
            *hrow = (guint)x;
        }
    }

    return heights;
}

/*
 * Group pixels of the same discrete height.  nh then holds indices in
 * hindex where each height starts.
 * TODO: support masking (must fix cycles that go over pixels to check mask)
 */
static void
group_by_height(const guint *heights,
                guint npoints, guint n,
                guint *nh, guint *hindex)
{
    // Make nh[i] the start of the block of discrete height i in hindex[].
    for (guint i = 0; i < n; i++)
        nh[heights[i]]++;
    for (guint i = 1; i < npoints; i++)
        nh[i] += nh[i-1];
    for (guint i = npoints-1; i; i--)
        nh[i] = nh[i-1];
    nh[0] = 0;

    // Fill the blocks in hindex[] with indices of points with the
    // corresponding discrete height.
    for (guint i = 0; i < n; i++)
        hindex[nh[heights[i]]++] = i;
    for (guint i = npoints-1; i; i--)
        nh[i] = nh[i-1];
    nh[0] = 0;
    // To avoid special-cases, append the index of end of the array.
    nh[npoints] = n;
}

/* Merge grains i and j in map with full resolution */
static inline void
resolve_grain_map(guint *m, guint i, guint j)
{
    guint ii, jj;

    // Find what i and j fully resolve to.
    for (ii = i; m[ii] != ii; ii = m[ii])
        ;
    for (jj = j; m[jj] != jj; jj = m[jj])
        ;
    guint k = MIN(ii, jj);

    // Turn partial resultions to full.
    for (ii = m[i]; m[ii] != ii; ii = m[ii]) {
        m[i] = k;
        i = ii;
    }
    m[ii] = k;
    for (jj = m[j]; m[jj] != jj; jj = m[jj]) {
        m[j] = k;
        j = jj;
    }
    m[jj] = k;
}

/**
 * gwy_data_field_area_grains_tgnd_range:
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
    GwyLine *line = gwy_line_new_sized(npoints, FALSE);

    GwyFieldPart rect = { maskcol, maskrow, width, height };
    guint n = width*height;
    if (masking == GWY_MASK_INCLUDE)
        n = gwy_mask_field_part_count(mask, &rect, TRUE);
    else if (masking == GWY_MASK_EXCLUDE)
        n = gwy_mask_field_part_count(mask, &rect, FALSE);

    n = MAX(n, 1);

    guint *heights = discretise_heights(field, col, row, width, height,
                                        mask, masking, maskcol, maskrow,
                                        npoints, min, max, white);
    guint *nh = g_new0(guint, npoints);
    guint *hindex = g_new(guint, width*height);
    group_by_height(heights, npoints, width*height, nh, hindex);

    guint *grains = g_new0(guint, n);
    guint *listv = g_new(guint, n/2 + 2);
    guint *listh = g_new(guint, n/2 + 2);

    guint *m = g_new(guint, 1), *mm = m;
    guint msize = 0;

    // Main iteration
    guint last_grain_no = 0;
    for (guint h = 0; h < npoints; h++) {
        // Mark new subgrains corresponding just to height @h.
        guint grain_no = last_grain_no;
        //gwy_debug("Height %d, number of old grains: %d", h, grain_no);
        for (guint i = nh[h]; i < nh[h+1]; i++) {
            guint j = hindex[i];
            if (!grains[j]) {
                grain_no++;
                fill_one_grain(heights, width, height,
                               j % width, j/width,
                               grains, grain_no, listv, listh);
            }
        }
        //gwy_debug("new subgrains: %d", grain_no-last_grain_no);

        if (grain_no == last_grain_no) {
            //gwy_debug("skipping empty height level");
            line->data[h] = h ? line->data[h-1] : 0;
            continue;
        }

        // Initialize grains number maps for merge scan
        if (grain_no+1 > msize) {
            g_free(m);
            m = g_new(guint, 2*(grain_no+1));
            mm = m + grain_no+1;
        }
        for (guint i = 0; i <= grain_no; i++) {
            m[i] = i;
            mm[i] = 0;
        }

        /* Find grains that touch each other for merge.
         *
         * Previously existing grains that did not touch don't touch now
         * either.  So we are only interested in neighbours of pixels of new
         * subgrains. */
        for (guint i = nh[h]; i < nh[h+1]; i++) {
            guint j = hindex[i];
            // Left
            if (j % width && grains[j-1]
                && m[grains[j]] != m[grains[j-1]])
                resolve_grain_map(m, grains[j], grains[j-1]);
            // Right
            if ((j+1) % width && grains[j+1]
                && m[grains[j]] != m[grains[j+1]])
                resolve_grain_map(m, grains[j], grains[j+1]);
            // Up
            if (j/width && grains[j-width]
                && m[grains[j]] != m[grains[j-width]])
                resolve_grain_map(m, grains[j], grains[j-width]);
            // Down
            if (j/width < height-1 && grains[j+width]
                && m[grains[j]] != m[grains[j+width]])
                resolve_grain_map(m, grains[j], grains[j+width]);
        }

        // Resolve remianing grain number links in m
        for (guint i = 1; i <= grain_no; i++)
            m[i] = m[m[i]];

        // Compactify grain numbers
        guint k = 0;
        for (guint i = 1; i <= grain_no; i++) {
            if (!mm[m[i]]) {
                k++;
                mm[m[i]] = k;
            }
            m[i] = mm[m[i]];
        }

#ifdef DEBUG
        for (guint i = 0; i <= grain_no; i++)
            g_printerr("%d[%d] ", m[i], i);
        g_printerr("\n");
#endif

        /* Renumber grains (we make use of the fact m[0] = 0).
         *
         * This is the only place where we have to scan the entire region.
         * Since grain numbers usually vary wildly and globally, we probably
         * can't avoid it. */
        for (guint i = 0; i < n; i++)
            grains[i] = m[grains[i]];

        // The number of grains for this h
        line->data[h] = k;
        last_grain_no = k;
    }

    g_free(m);
    g_free(listv);
    g_free(listh);
    g_free(grains);
    g_free(hindex);
    g_free(nh);
    g_free(heights);

    return line;
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
    GwyFieldPart rect = { maskcol, maskrow, width, height };
    guint n = width*height;
    if (masking == GWY_MASK_INCLUDE)
        n = gwy_mask_field_part_count(mask, &rect, TRUE);
    else if (masking == GWY_MASK_EXCLUDE)
        n = gwy_mask_field_part_count(mask, &rect, FALSE);

    if (!n)
        return NULL;

    GwyLine *whitedist = grain_number_dist(field, col, row, width, height,
                                           mask, masking, maskcol, maskrow,
                                           TRUE, npoints, min, max);
    GwyLine *blackdist = grain_number_dist(field, col, row, width, height,
                                           mask, masking, maskcol, maskrow,
                                           FALSE, npoints, min, max);
    gwy_line_add_line(whitedist, NULL, blackdist, 0, -1.0);
    gwy_line_multiply(whitedist, 1.0/n);
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
 * Note at present masking is implemented only for the volume and surface
 * functionals %GWY_MINKOWSKI_VOLUME and %GWY_MINKOWSKI_SURFACE.
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

    gboolean explicit_range = min < max;
    if (!explicit_range)
        gwy_field_min_max(field, fpart, mask, masking, &min, &max);
    sanitise_range(&min, &max);

    // Cannot determine npoints here, it depends on the functional.
    if (type == GWY_MINKOWSKI_VOLUME) {
        line = minkowski_volume(field, col, row, width, height,
                                mask, masking, maskcol, maskrow,
                                npoints, min, max);
    }
    else if (type == GWY_MINKOWSKI_SURFACE) {
        line = minkowski_surface(field, col, row, width, height,
                                 mask, masking, maskcol, maskrow,
                                 npoints, min, max);
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

    gwy_unit_assign(gwy_line_get_unit_x(line), gwy_field_get_unit_z(field));

    return line;
}

/**
 * SECTION: field-distributions
 * @section_id: GwyField-distributions
 * @title: GwyField distributions
 * @short_description: One-dimensional distributions and functionals of fields
 *
 * Statistical distribution densities are normalised so that their integral,
 * that can also be calculated as gwy_line_mean(line)*line->real, is unity.
 * Cumulative distribution values then always line in the interval [0,1].
 **/

/**
 * GwyMinkowskiFunctionalType:
 * @GWY_MINKOWSKI_VOLUME: Fraction of ‘white’ pixels from the total
 *                        number of pixels.
 * @GWY_MINKOWSKI_SURFACE: Fraction of ‘black–white’ pixel edges from the
 *                         total number of edges.
 * @GWY_MINKOWSKI_CONNECTIVITY: Difference between the numbers of ‘white’ and
 *                              ‘black’ connected areas (grains) divided by
 *                              the total number of pixels.
 *
 * Types of Minkowski functionals.
 *
 * Each quantity is a function of threshold; pixels above this threshold are
 * considered ‘white’, pixels below ‘black’.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
