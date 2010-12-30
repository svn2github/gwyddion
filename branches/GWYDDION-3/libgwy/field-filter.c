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

#include <string.h>
#include "libgwy/macros.h"
#include "libgwy/math.h"
#include "libgwy/field-filter.h"
#include "libgwy/mask-field.h"
#include "libgwy/line-arithmetic.h"
#include "libgwy/line-statistics.h"
#include "libgwy/math-internal.h"
#include "libgwy/field-arithmetic.h"
#include "libgwy/field-internal.h"

static void
gwy_part_row_convolve3(GwyField *field,
                       const gdouble *kdata,
                       guint col, guint row,
                       guint width, guint height)
{
    if (width == 1) {
        gdouble s = kdata[0] + kdata[1] + kdata[2];
        gwy_field_multiply(field, &((GwyRectangle){col, row, width, height}),
                           NULL, GWY_MASK_IGNORE, s);
        return;
    }

    guint xres = field->xres;
    for (guint i = 0; i < height; i++) {
        gdouble *drow = field->data + (row + i)*xres + col;
        gdouble buf[3];
        buf[0] = (kdata[2]*(col ? *(drow - 1) : drow[1])
                  + kdata[1]*drow[0]
                  + kdata[0]*drow[1]);
        buf[1] = kdata[2]*drow[0] + kdata[1]*drow[1];
        buf[2] = kdata[2]*drow[1];
        for (guint j = 0; j < width; j++) {
            guint k = j + 2;
            if (G_UNLIKELY(col + k >= xres))
                k = 2*xres-1 - k;
            gdouble v = drow[k];
            drow[j] = buf[0];
            buf[0] = buf[1] + kdata[0]*v;
            buf[1] = buf[2] + kdata[1]*v;
            buf[2] = kdata[2]*v;
        }
    }
}

/**
 * gwy_field_row_convolve:
 * @field: A two-dimensional data field.
 * @rectangle: Area in @field to process.  Pass %NULL to process entire @field.
 * @kernel: Kenrel to convolve @field with.
 *
 * Convolve a field with a horizontal kernel.
 *
 * The kernel must have an odd number of pixels.  The convolution is then
 * performed with the kernel centred on the respective field pixels.
 * If you want to perform convolution with an even-sized kernel pad it with a
 * zero on the left or right side, depending on whether you prefer the result
 * to be shifted left or right.
 *
 * Data outside the field are represented using mirroring.  Data outside the
 * rectangle but still inside the field are taken from the field, i.e. the
 * result is influenced by data outside the rectangle.
 **/
void
gwy_field_row_convolve(GwyField *field,
                       const GwyRectangle *rectangle,
                       const GwyLine *kernel)
{
    guint col, row, width, height;
    if (!_gwy_field_check_rectangle(field, rectangle,
                                    &col, &row, &width, &height))
        return;

    g_return_if_fail(GWY_IS_LINE(kernel));
    g_return_if_fail(kernel->res % 2 == 1);

    guint kres, mres, k0, i, j, k, pos;
    const gdouble *kdata;
    gdouble *buf, *drow;
    gdouble d;

    kres = kernel->res;
    kdata = kernel->data;
    mres = 2*width;
    k0 = (kres/2 + 1)*mres;
    buf = g_new(gdouble, kres);

    for (i = 0; i < height; i++) {
        drow = field->data + (row + i)*field->xres + col;
        /* Initialize with a triangluar sums, mirror-extend */
        gwy_clear(buf, kres);
        for (j = 0; j < kres; j++) {
            k = (j + k0 - kres/2) % mres;
            d = drow[k < width ? k : mres-1 - k];
            for (k = 0; k <= j; k++)
                buf[k] += kdata[j - k]*d;
        }
        pos = 0;
        /* Middle part and tail with mirror extension again, we do some
         * O(1/2*k^2) of useless work here by not separating the tail */
        for (j = 0; j < width; j++) {
            drow[j] = buf[pos];
            buf[pos] = 0.0;
            pos = (pos + 1) % kres;
            k = (j + k0 + kres - kres/2) % mres;
            d = drow[G_LIKELY(k < width) ? k : mres-1 - k];
            for (k = pos; k < kres; k++)
                buf[k] += kdata[kres-1 - (k - pos)]*d;
            for (k = 0; k < pos; k++)
                buf[k] += kdata[pos-1 - k]*d;
        }
    }

    g_free(buf);
}

static void
gwy_field_col_convolve3(GwyField *field,
                        const gdouble *kdata,
                        guint col, guint row,
                        guint width, guint height)
{
    if (height == 1) {
        gdouble s = kdata[0] + kdata[1] + kdata[2];
        gwy_field_multiply(field, &((GwyRectangle){col, row, width, height}),
                           NULL, GWY_MASK_IGNORE, s);
        return;
    }

    guint xres = field->xres, yres = field->yres;
    gdouble *buf = g_new(gdouble, 3*width);
    gdouble *drow = field->data + row*xres + col;
    for (guint j = 0; j < width; j++) {
        buf[3*j] = (kdata[2]*(row ? *(drow - xres) : drow[j + xres])
                    + kdata[1]*drow[j]
                    + kdata[0]*drow[j + xres]);
        buf[3*j + 1] = kdata[2]*drow[j] + kdata[1]*drow[xres];
        buf[3*j + 2] = kdata[2]*drow[xres];
    }
    for (guint i = 0; i < height; i++) {
        drow = field->data + (row + i)*xres + col;
        guint k = i + 2;
        if (G_UNLIKELY(row + k >= yres))
            k = 2*yres-1 - k;
        gdouble *drow2 = field->data + (row + k)*xres + col;
        for (guint j = 0; j < width; j++) {
            gdouble v = drow2[j];
            drow[j] = buf[3*j];
            buf[3*j] = buf[3*j + 1] + kdata[0]*v;
            buf[3*j + 1] = buf[3*j + 2] + kdata[1]*v;
            buf[3*j + 2] = kdata[2]*v;
        }
    }

    g_free(buf);
    gwy_field_invalidate(field);
}

/**
 * gwy_field_col_convolve:
 * @field: A two-dimensional data field.
 * @rectangle: Area in @field to process.  Pass %NULL to process entire @field.
 * @kernel: Kenrel to convolve @field with.
 *
 * Convolve a rectangular part of a data field with a vertical kernel.
 *
 * The kernel must have an odd number of pixels.  The convolution is then
 * performed with the kernel centred on the respective field pixels.
 * If you want to perform convolution with an even-sized kernel pad it with a
 * zero on the left or right side, depending on whether you prefer the result
 * to be shifted left or right.
 *
 * Data outside the field are represented using mirroring.  Data outside the
 * rectangle but still inside the field are taken from the field, i.e. the
 * result is influenced by data outside the rectangle.
 **/
void
gwy_field_col_convolve(GwyField *field,
                       const GwyRectangle* rectangle,
                       const GwyLine *kernel)
{
    guint col, row, width, height;
    if (!_gwy_field_check_rectangle(field, rectangle,
                                    &col, &row, &width, &height))
        return;

    g_return_if_fail(GWY_IS_LINE(kernel));
    g_return_if_fail(kernel->res % 2 == 1);

    guint kres, xres, mres, k0, i, j, k, pos;
    const gdouble *kdata;
    gdouble *buf, *dcol;
    gdouble d;

    kres = kernel->res;
    kdata = kernel->data;
    xres = field->xres;
    mres = 2*height;
    k0 = (kres/2 + 1)*mres;
    buf = g_new(gdouble, kres);

    /* This looks like a bad memory access pattern.  And for small kernels it
     * indeed is (we should iterate row-wise and directly calculate the sums).
     * For large kernels this is mitigated by the maximum possible amount of
     * work done per a data field access. */
    for (j = 0; j < width; j++) {
        dcol = field->data + row*xres + (col + j);
        /* Initialize with a triangluar sums, mirror-extend */
        gwy_clear(buf, kres);
        for (i = 0; i < kres; i++) {
            k = (i + k0 - kres/2) % mres;
            d = dcol[k < height ? k*xres : (mres-1 - k)*xres];
            for (k = 0; k <= i; k++)
                buf[k] += kdata[i - k]*d;
        }
        pos = 0;
        /* Middle part and tail with mirror extension again, we do some
         * O(1/2*k^2) of useless work here by not separating the tail */
        for (i = 0; i < height; i++) {
            dcol[i*xres] = buf[pos];
            buf[pos] = 0.0;
            pos = (pos + 1) % kres;
            k = (i + k0 + kres - kres/2) % mres;
            d = dcol[G_LIKELY(k < height) ? k*xres : (mres-1 - k)*xres];
            for (k = pos; k < kres; k++)
                buf[k] += kdata[kres-1 - (k - pos)]*d;
            for (k = 0; k < pos; k++)
                buf[k] += kdata[pos-1 - k]*d;
        }
    }

    g_free(buf);
    gwy_field_invalidate(field);
}

static gboolean
make_gaussian_kernel(GwyLine *kernel,
                     gdouble sigma)
{
    // Smaller values lead to underflow even the first non-central term
    if (sigma < 0.026)
        return FALSE;

    // Exclude terms smaller than machine eps compared to the central term
    guint res = 2*(guint)ceil(8.0*sigma) + 1;
    // But do at least a 3-term filter so that they don't say we didn't try
    res = MAX(res, 3);
    gdouble center = (res - 1.0)/2.0;
    double s = 0.0;

    gwy_line_set_size(kernel, res, FALSE);
    for (guint k = 0; k < res; k++) {
        gdouble x = k - center;
        x /= sigma;
        s += kernel->data[k] = exp(-x*x/2.0);
    }
    gwy_line_multiply(kernel, 1.0/s);

    return TRUE;
}

/**
 * gwy_field_filter_gaussian:
 * @field: A two-dimensional data field.
 * @rectangle: Area in @field to process.  Pass %NULL to process entire @field.
 * @hsigma: Horizontal sigma parameter (square root of dispersion) of the
 *          Gaussian, in pixels.
 * @vsigma: Vertical sigma parameter (square root of dispersion) of the
 *          Gaussian, in pixels.
 *
 * Filters a rectangular part of a field with a Gaussian filter.
 *
 * The Gausian is normalised, i.e. the filter is sum-preserving.
 *
 * Pass the same @hsigma and @vsigma for a pixel-wise rotationally symmetrical
 * Gaussian which is usually wanted.  However, if the horizontal and vertical
 * physical pixel dimensions differ or filtering only in one direction is
 * required then passing different @hsigma and @vsigma can be useful.
 **/
void
gwy_field_filter_gaussian(GwyField *field,
                          const GwyRectangle* rectangle,
                          gdouble hsigma, gdouble vsigma)
{
    /* The field and rectangle are checked by the actual convolution funcs. */
    g_return_if_fail(hsigma >= 0.0 && vsigma >= 0.0);

    /* XXX: Expressing the convolution using a cyclic convolution of the kernel
     * and mirrored data, the kernel can be always reduced (projected) onto
     * size of the mirrored data, i.e. max 2*data_size. */
    GwyLine *kernel = gwy_line_new();
    gboolean done = FALSE;

    if (make_gaussian_kernel(kernel, hsigma)) {
        gwy_field_row_convolve(field, rectangle, kernel);
        if (vsigma == hsigma) {
            gwy_field_col_convolve(field, rectangle, kernel);
            done = TRUE;
        }
    }
    if (!done && make_gaussian_kernel(kernel, vsigma))
        gwy_field_col_convolve(field, rectangle, kernel);
    g_object_unref(kernel);
}

/**
 * SECTION: field-filter
 * @section_id: GwyField-filter
 * @title: GwyField filtering
 * @short_description: Field filtering
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
