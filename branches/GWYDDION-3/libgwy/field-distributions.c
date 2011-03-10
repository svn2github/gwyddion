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
#include "libgwy/field-statistics.h"
#include "libgwy/field-distributions.h"
#include "libgwy/math-internal.h"
#include "libgwy/field-internal.h"

typedef struct {
    guint n;              // Number of quarter-pixels considered.
    guint n_in_range;     // Number of quarter-pixels within range.
    gboolean analyse : 1;
    gboolean count : 1;
    gdouble dx;
    gdouble dy;
    gdouble min;
    gdouble max;
    GwyLine *dist;
} DistributionData;

static void
sanitize_range(gdouble *min,
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
 * Returns: A new one-dimensional data line with the value distribution.
 **/
GwyLine*
gwy_field_value_dist(const GwyField *field,
                     const GwyFieldPart *fpart,
                     const GwyMaskField *mask,
                     GwyMaskingType masking,
                     gboolean cumulative,
                     guint npoints,
                     gdouble min, gdouble max)
{
    guint col, row, width, height, maskcol, maskrow;
    GwyLine *line = NULL;
    if (!_gwy_field_check_mask(field, fpart, mask, &masking,
                               &col, &row, &width, &height, &maskcol, &maskrow))
        goto fail;

    guint n;
    if (min < max) {
        // We know the range but have to figure out how many pixels we have.
        guint nabove, nbelow;
        n = gwy_field_count_above_below(field, fpart, mask, masking,
                                        max, min, TRUE, &nabove, &nbelow);
        n -= nabove + nbelow;
    }
    else {
        // We know the number of pixels but have to figure out the range.
        GwyFieldPart rect = { maskcol, maskrow, width, height };
        if (masking == GWY_MASK_INCLUDE)
            n = gwy_mask_field_part_count(mask, &rect, TRUE);
        else if (masking == GWY_MASK_EXCLUDE)
            n = gwy_mask_field_part_count(mask, &rect, FALSE);
        else
            n = width*height;

        gwy_field_min_max(field, fpart, mask, masking, &min, &max);
        sanitize_range(&min, &max);
    }
    if (!npoints) {
        npoints = gwy_round(3.49*cbrt(n));
        npoints = MAX(npoints, 1);
    }
    line = gwy_line_new_sized(npoints, TRUE);
    if (!n)
        goto fail;

    const gdouble *base = field->data + row*field->xres + col;
    gdouble q = (max - min)*npoints;
    guint ndata = 0;

    if (masking == GWY_MASK_IGNORE) {
        for (guint i = 0; i < height; i++) {
            const gdouble *d = base + i*field->xres;
            for (guint j = width; j; j--, d++) {
                guint k = (guint)((*d - min)/q);
                // Fix rounding errors.
                if (G_UNLIKELY(k >= npoints))
                    line->data[npoints-1] += 1;
                else
                    line->data[k] += 1;
            }
        }
        ndata = width*height;
    }
    else {
        const gboolean invert = (masking == GWY_MASK_EXCLUDE);
        for (guint i = 0; i < height; i++) {
            const gdouble *d = base + i*field->xres;
            GwyMaskIter iter;
            gwy_mask_field_iter_init(mask, iter, maskcol, maskrow + i);
            for (guint j = width; j; j--, d++) {
                if (!gwy_mask_iter_get(iter) == invert) {
                    guint k = (guint)((*d - min)/q);
                    // Fix rounding errors.
                    if (G_UNLIKELY(k >= npoints))
                        line->data[npoints-1] += 1;
                    else
                        line->data[k] += 1;
                    ndata++;
                }
                gwy_mask_iter_next(iter);
            }
        }
    }

    line->off = min;
    line->real = max - min;
    if (cumulative) {
        gwy_line_accumulate(line, TRUE);
        gwy_line_multiply(line, 1.0/line->data[npoints-1]);
    }
    else
        gwy_line_multiply(line, npoints/(max - min)/ndata);

fail:
    if (!line)
        line = gwy_line_new();

    gwy_unit_assign(gwy_line_get_unit_x(line), gwy_field_get_unit_z(field));
    if (!cumulative)
        gwy_unit_power(gwy_line_get_unit_y(line),
                       gwy_field_get_unit_z(field), -1);

    return line;
}

/**
 * update_uniform_dist:
 * @line: A data line containing the distribution, its physical dimensions are
 *        ignored, the bins are determined by @min and @max.
 * @min: Minimum of the first bin.
 * @max: Maximum of the last bin.
 * @from: Left endpoint of the contribution.
 * @to: Right endpoint of the contribution.
 * @weight: Total weight of the contribution to add.
 *
 * Adds a uniform contribution to a distribution.
 *
 * The contribution is uniform in [@from, @to] with integral equal to @weight.
 * If part of the contribution lies outside [@min, @max] it will be missing in
 * @line.
 **/
static void
update_uniform_dist(GwyLine *line, gdouble min, gdouble max,
                    gdouble from, gdouble to, gdouble weight)
{
    guint n = line->res;
    gdouble binsize = n/(max - min);
    gdouble binfrom = (from - min)/binsize, binto = (to - min)/binsize;

    if (binfrom > n || binto < 0.0)
        return;

    gboolean leftext = binfrom < 0.0, rightext = binto >= n;
    guint ifrom = leftext ? 0 : floor(binfrom);
    guint ito = rightext ? n : floor(binto);
    gdouble len = to - from;
    gdouble wbin = binsize*weight/len;

    guint i = ifrom;
    if (!leftext) {
        if (!rightext && ito == ifrom) {
            // Entire distribution is contained in bin @i.
            line->data[ifrom] += weight;
            return;
        }
        // Distribution starts in bin @i.
        gdouble xlen = (i*binsize - from)/len;
        line->data[i] += weight*xlen;
        i++;
    }

    // Note if @rightext is TRUE then @ito points after the last element
    // but if @rightext is FALSE then @ito points to the last element.
    while (i < ito) {
        // Open-ended contribution to bin @i.
        line->data[i] += wbin;
        i++;
    }

    if (!rightext) {
        // Distribution ends in bin @i.
        gdouble xlen = (to - i*binsize)/len;
        line->data[i] += weight*xlen;
    }
}


static void
update_left_triangular_dist(GwyLine *line, gdouble min, gdouble max,
                            gdouble from, gdouble to, gdouble weight)
{
    guint n = line->res;
    gdouble binsize = n/(max - min);
    gdouble binfrom = (from - min)/binsize, binto = (to - min)/binsize;

    if (binfrom > n || binto < 0.0)
        return;

    gboolean leftext = binfrom < 0.0, rightext = binto >= n;
    guint ifrom = leftext ? 0 : floor(binfrom);
    guint ito = rightext ? n : floor(binto);
    gdouble len = to - from;
    gdouble wbin = binsize*weight/(len*len);

    guint i = ifrom;
    if (!leftext) {
        if (!rightext && ito == ifrom) {
            // Entire distribution is contained in bin @i.
            line->data[ifrom] += weight;
            return;
        }
        // Distribution starts in bin @i.
        gdouble xlen = (i*binsize - from)/len;
        line->data[i] += weight*xlen*xlen;
        i++;
    }

    // Note if @rightext is TRUE then @ito points after the last element
    // but if @rightext is FALSE then @ito points to the last element.
    while (i < ito) {
        // Open-ended contribution to bin @i.
        line->data[i] += wbin*((2.0*i + 1.0)*binsize - from);
        i++;
    }

    if (!rightext) {
        // Distribution ends in bin @i.
        gdouble xlen = (to - i*binsize)/len;
        line->data[i] += weight*(2.0 - xlen)*xlen;
    }
}

static void
update_right_triangular_dist(GwyLine *line, gdouble min, gdouble max,
                             gdouble from, gdouble to, gdouble weight)
{
    guint n = line->res;
    gdouble binsize = n/(max - min);
    gdouble binfrom = (from - min)/binsize, binto = (to - min)/binsize;

    if (binfrom > n || binto < 0.0)
        return;

    gboolean leftext = binfrom < 0.0, rightext = binto >= n;
    guint ifrom = leftext ? 0 : floor(binfrom);
    guint ito = rightext ? n : floor(binto);
    gdouble len = to - from;
    gdouble wbin = binsize*weight/(len*len);

    guint i = ifrom;
    if (!leftext) {
        if (!rightext && ito == ifrom) {
            // Entire distribution is contained in bin @i.
            line->data[ifrom] += weight;
            return;
        }
        // Distribution starts in bin @i.
        gdouble xlen = (i*binsize - from)/len;
        line->data[i] += weight*(2.0 - xlen)*xlen;
        i++;
    }

    // Note if @rightext is TRUE then @ito points after the last element
    // but if @rightext is FALSE then @ito points to the last element.
    while (i < ito) {
        // Open-ended contribution to bin @i.
        line->data[i] += wbin*(to - (2.0*i + 1.0)*binsize);
        i++;
    }

    if (!rightext) {
        // Distribution ends in bin @i.
        gdouble xlen = (to - i*binsize)/len;
        line->data[i] += weight*xlen*xlen;
    }
}

static void
update_delta_dist(GwyLine *line, gdouble min, gdouble max,
                  gdouble value, gdouble weight)
{
    guint n = line->res;
    gdouble binsize = n/(max - min);
    gdouble binvalue = (value - min)/binsize;
    guint ivalue;

    if (binvalue > n || binvalue < 0.0)
        return;

    if (binvalue == n)
        ivalue = n-1;
    else
        ivalue = floor(binvalue);

    line->data[ivalue] += weight;
}

static inline void
update_uniform(DistributionData *ddata,
               gdouble v1, gdouble v2, guint w)
{
    gdouble vmin = MIN(v1, v2), vmax = MAX(v1, v2);

    if (ddata->analyse) {
        if (vmin < ddata->min)
            ddata->min = vmin;
        if (vmax > ddata->max)
            ddata->max = vmax;
        ddata->n_in_range += w;
        ddata->n += w;
    }
    else if (ddata->count) {
        // FIXME: We could count how much of the contribution actually falls
        // within the range not just whether anything of it falls there.
        if (MAX(vmax, ddata->max) - MIN(vmax, ddata->min)
            < vmax - vmin + ddata->max - ddata->min)
            ddata->n_in_range += w;
        ddata->n += w;
    }
    else
        update_uniform_dist(ddata->dist, ddata->min, ddata->max, vmin, vmax, w);
}

static inline void
update_delta(DistributionData *ddata, gdouble v, guint w)
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
        update_delta_dist(ddata->dist, ddata->min, ddata->max, v, w);
}

static void
slope_horiz_cont(gdouble z1, gdouble z2, gdouble z3, gdouble z4,
                 guint w1, guint w2, guint w3, guint w4,
                 gpointer user_data)
{
    DistributionData *ddata = (DistributionData*)user_data;

    if (w1 || w2) {
        gdouble zds = (z2 - z1)/ddata->dx;
        gdouble zdc = zds + 0.5*(z3 - z4)/ddata->dx;
        update_uniform(ddata, zds, zdc, w1 + w2);
    }

    if (w3 || w4) {
        gdouble zds = (z3 - z4)/ddata->dx;
        gdouble zdc = zds + 0.5*(z2 - z1)/ddata->dx;
        update_uniform(ddata, zds, zdc, w3 + w4);
    }
}

static void
slope_vert_cont(gdouble z1, gdouble z2, gdouble z3, gdouble z4,
                guint w1, guint w2, guint w3, guint w4,
                gpointer user_data)
{
    DistributionData *ddata = (DistributionData*)user_data;

    if (w1 || w4) {
        gdouble zds = (z4 - z1)/ddata->dy;
        gdouble zdc = zds + 0.5*(z3 - z2)/ddata->dy;
        update_uniform(ddata, zds, zdc, w1 + w4);
    }

    if (w2 || w3) {
        gdouble zds = (z3 - z2)/ddata->dy;
        gdouble zdc = zds + 0.5*(z4 - z1)/ddata->dy;
        update_uniform(ddata, zds, zdc, w2 + w3);
    }
}

static void
slope_horiz_disrc(gdouble z1, gdouble z2, gdouble z3, gdouble z4,
                  guint w1, guint w2, guint w3, guint w4,
                  gpointer user_data)
{
    DistributionData *ddata = (DistributionData*)user_data;

    if (w1 || w2)
        update_delta(ddata, (z2 - z1)/ddata->dx, w1 + w2);

    if (w3 || w4)
        update_delta(ddata, (z3 - z4)/ddata->dx, w3 + w4);
}

static void
slope_vert_discr(gdouble z1, gdouble z2, gdouble z3, gdouble z4,
                 guint w1, guint w2, guint w3, guint w4,
                 gpointer user_data)
{
    DistributionData *ddata = (DistributionData*)user_data;

    if (w1 || w4)
        update_delta(ddata, (z4 - z1)/ddata->dx, w1 + w4);

    if (w2 || w3)
        update_delta(ddata, (z3 - z2)/ddata->dx, w2 + w3);
}

/**
 * gwy_field_slope_dist:
 * @field: A two-dimensional data field.
 * @fpart: (allow-none):
 *         Area in @field to process.  Pass %NULL to process entire @field.
 * @mask: (allow-none):
 *        Mask specifying which values to take into account/exclude, or %NULL.
 * @masking: Masking mode to use (has any effect only with non-%NULL @mask).
 * @orientation: Orientation in which to compute the derivatives.
 * @cumulative: %TRUE to calculate cumulative distribution, %FALSE to calculate
 *              density.
 * @continuous: %TRUE to calculate the distribution of linearly interpolated
 *              surface, %FALSE to use plain histogram of discrete values.
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
 * Returns: A new one-dimensional data line with the slope distribution.
 **/
GwyLine*
gwy_field_slope_dist(const GwyField *field,
                     const GwyFieldPart *fpart,
                     const GwyMaskField *mask,
                     GwyMaskingType masking,
                     GwyOrientation orientation,
                     gboolean cumulative,
                     gboolean continuous,
                     guint npoints,
                     gdouble min, gdouble max)
{
    guint col, row, width, height, maskcol, maskrow;
    GwyLine *line = NULL;
    if (!_gwy_field_check_mask(field, fpart, mask, &masking,
                               &col, &row, &width, &height, &maskcol, &maskrow))
        goto fail;

    GwyFieldQuartersFunc func;
    if (continuous)
        func = (orientation == GWY_ORIENTATION_HORIZONTAL
                ? slope_horiz_cont
                : slope_vert_cont);
    else
        func = (orientation == GWY_ORIENTATION_HORIZONTAL
                ? slope_horiz_disrc
                : slope_vert_discr);

    DistributionData ddata = {
        0, 0,
        min > max, TRUE,
        gwy_field_dx(field), gwy_field_dy(field),
        G_MAXDOUBLE, -G_MAXDOUBLE,
        NULL,
    };

    // Run analyse (find range and count) or count (count in range).  If both
    // is given, this serves as a somewhat inefficient masked pixel counting
    // method.
    gwy_field_process_quarters(field, fpart, mask, masking, func, &ddata);
    g_assert(ddata.n % 4 == 0);

    if (!npoints)
        npoints = gwy_round(3.49*cbrt(ddata.n_in_range/4));
    if (!npoints)
        goto fail;

    if (min <= max) {
        ddata.min = min;
        ddata.max = max;
    }

    line = ddata.dist = gwy_line_new_sized(npoints, TRUE);
    sanitize_range(&ddata.min, &ddata.max);
    line->off = ddata.min;
    line->real = ddata.max - ddata.min;

    ddata.analyse = ddata.count = FALSE;
    gwy_field_process_quarters(field, fpart, mask, masking, func, &ddata);

    if (cumulative) {
        gwy_line_accumulate(line, TRUE);
        gwy_line_multiply(line, 1.0/ddata.n);
    }
    else
        gwy_line_multiply(line, npoints/(max - min)/ddata.n);

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
 * Returns: A new one-dimensional data line with the ACF.
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
    if (!_gwy_field_check_mask(field, fpart, mask, &masking,
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
 * Returns: A new one-dimensional data line with the PSDF.
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
    if (!_gwy_field_check_mask(field, fpart, mask, &masking,
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
 * Returns: A new one-dimensional data line with the HHCF.
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
    if (!_gwy_field_check_mask(field, fpart, mask, &masking,
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
 * GwyOrientation:
 * @GWY_ORIENTATION_HORIZONTAL: Horizontal orientation.
 * @GWY_ORIENTATION_VERTICAL: Vertical orientation.
 *
 * Orientation type.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
