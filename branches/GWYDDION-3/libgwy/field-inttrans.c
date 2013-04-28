/*
 *  $Id$
 *  Copyright (C) 2013 David Nečas (Yeti).
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
#include "libgwy/field-arithmetic.h"
#include "libgwy/field-level.h"
#include "libgwy/field-inttrans.h"
#include "libgwy/math-internal.h"
#include "libgwy/object-internal.h"
#include "libgwy/field-internal.h"

#define CBIT GWY_FIELD_CBIT

static void humanize_in_place    (GwyField *field);
static void humanize_clear_cached(GwyField *field);
static void humanize_xoffset     (GwyField *field);
static void humanize_yoffset     (GwyField *field);
static void dehumanize_xoffset   (GwyField *field);
static void dehumanize_yoffset   (GwyField *field);
static void complete_fft_real    (GwyField *field);
static void complete_fft_imag    (GwyField *field);
static void fftize_xdim          (GwyField *fftfield,
                                  const GwyField *field);
static void fftize_ydim          (GwyField *fftfield,
                                  const GwyField *field);
static void copy_xdim            (GwyField *fftfield,
                                  const GwyField *field);
static void copy_ydim            (GwyField *fftfield,
                                  const GwyField *field);

static GwyField*
init_fft_field(const GwyField *src,
               GwyField *field)
{
    if (field) {
        g_object_ref(field);
        gwy_field_set_size(field, src->xres, src->yres, FALSE);
        return field;
    }
    return gwy_field_new_sized(src->xres, src->yres, FALSE);
}

/**
 * gwy_field_row_fft:
 * @field: A two-dimensional data field.
 * @reout: (allow-none):
 *         Target data field for the real part of the transform.
 *         It will be resized to match @field if necessary.
 * @imout: (allow-none):
 *         Target data field for the imaginary part of the transform.
 *         It will be resized to match @field if necessary.
 * @windowing: Windowing type to use.
 * @preserverms: %TRUE to preserve RMS value.  More precisely, for each row,
 *               the sum of squares of the output will be the same as the
 *               sum of squares of the input after levelling.  If %FALSE is
 *               passed the Fourier coefficients will be, in general, diminshed
 *               due to windowing.
 * @level: Row levelling to apply, pass 0 for no row levelling.  See
 *         gwy_field_level_rows().
 *
 * Performs real-to-complex FFT of rows of a two-dimensional data field.
 *
 * This is a high-level method suitable in cases when the FFT output is to be
 * analysed or displayed.  For modifications in frequency domain and
 * transforming back, gwy_field_row_fft_raw() is usually more suitable.
 *
 * Zero frequency will be in the top left corner; use
 * gwy_field_row_fft_humanize() if you want it in the centre.
 **/
void
gwy_field_row_fft(const GwyField *field,
                  GwyField *reout,
                  GwyField *imout,
                  GwyWindowingType windowing,
                  gboolean preserverms,
                  guint level)
{
    g_return_if_fail(GWY_IS_FIELD(field));
    g_return_if_fail(!reout || GWY_IS_FIELD(reout));
    g_return_if_fail(!imout || GWY_IS_FIELD(imout));
    g_return_if_fail(windowing <= GWY_WINDOWING_KAISER25);
    g_return_if_fail(level <= 3);
    if (!reout && !imout)
        return;

    guint xres = field->xres, yres = field->yres;
    GwyField *myreout = init_fft_field(field, reout);
    GwyField *myimout = init_fft_field(field, imout);
    GwyField *myrein = gwy_field_duplicate(field);

    fftw_iodim trans_dims = { xres, 1, 1 };
    fftw_iodim repeat_dims = { yres, xres, xres };
    guint flags = FFTW_DESTROY_INPUT | _gwy_fft_rigour();
    fftw_plan plan = fftw_plan_guru_split_dft_r2c(1, &trans_dims,
                                                  1, &repeat_dims,
                                                  myrein->data,
                                                  myreout->data, myimout->data,
                                                  flags);
    gwy_field_copy_full(field, myrein);
    gwy_field_level_rows(myrein, level);
    gdouble *sqsum = NULL;
    if (preserverms) {
        sqsum = g_new0(gdouble, yres);
        for (guint i = 0; i < yres; i++) {
            const gdouble *d = myrein->data + i*xres;
            gdouble s = 0.0;
            for (guint j = xres; j; j--, d++)
                s += (*d)*(*d);
            sqsum[i] = s;
        }
    }
    gwy_field_fft_window(myrein, windowing, FALSE, TRUE);
    fftw_execute(plan);
    fftw_destroy_plan(plan);
    g_object_unref(myrein);

    if (preserverms || reout)
        complete_fft_real(myreout);
    if (preserverms || imout)
        complete_fft_imag(myimout);

    if (preserverms) {
        for (guint i = 0; i < yres; i++) {
            gdouble *re = myreout->data + i*xres;
            gdouble *im = myimout->data + i*xres;
            gdouble s = 0.0;
            if (sqsum[i]) {
                for (guint j = xres; j; j--, re++, im++)
                    s += (*re)*(*re) + (*im)*(*im);
                // s == 0.0 should not really happen since we took the sqsum
                // after levelling, but...
                if (s)
                    s = sqrt(sqsum[i]/s);
            }
            re = myreout->data + i*xres;
            im = myimout->data + i*xres;
            for (guint j = xres; j; j--, re++, im++) {
                *re *= s;
                *im *= s;
            }
        }
        g_free(sqsum);
    }
    else {
        gdouble q = 1.0/sqrt(xres);
        if (reout)
            gwy_field_multiply(reout, NULL, NULL, GWY_MASK_IGNORE, q);
        if (imout)
            gwy_field_multiply(imout, NULL, NULL, GWY_MASK_IGNORE, q);
    }

    g_object_unref(myimout);
    g_object_unref(myreout);

    if (reout) {
        fftize_xdim(reout, field);
        copy_ydim(reout, field);
        gwy_field_invalidate(reout);
    }
    if (imout) {
        fftize_xdim(imout, field);
        copy_ydim(imout, field);
        gwy_field_invalidate(imout);
    }
}

/**
 * gwy_field_row_fft_raw:
 * @rein: (allow-none):
 *        Real input two-dimensional data field.
 * @imin: (allow-none):
 *        Imaginary input two-dimensional data field.
 * @reout: (allow-none):
 *         Target data field for the real part of the transform.
 *         It will be resized to match @rein if necessary.
 * @imout: (allow-none):
 *         Target data field for the imaginary part of the transform.
 *         It will be resized to match @rein if necessary.
 * @direction: Transformation direction.
 *
 * Performs raw FFT of rows of a two-dimensional data field.
 *
 * This is a low-level method suitable in cases when you intend to transform
 * the data back and forth.  Function gwy_field_row_fft() is usually more
 * suitable for analysis and display.
 *
 * Zero frequency will be in the top left corner; use
 * gwy_field_row_fft_humanize() if you want it in the centre.
 *
 * Some input or output fields may be %NULL if you are not interested in the
 * corresponding part or want to perform a real-to-complex (or
 * imaginary-to-complex for that matter) transform.  To be precise, either or
 * both of @reout and @imout can be %NULL.  The function reduces to no-op if
 * both are %NULL.  If they are not both %NULL at least one of @rein and @imin
 * must be given.  If both input fields are given they must be compatible.
 *
 * All fields must be different objects.
 **/
void
gwy_field_row_fft_raw(const GwyField *rein,
                      const GwyField *imin,
                      GwyField *reout,
                      GwyField *imout,
                      GwyTransformDirection direction)
{
    g_return_if_fail(!rein || GWY_IS_FIELD(rein));
    g_return_if_fail(!imin || GWY_IS_FIELD(imin));
    g_return_if_fail(!reout || GWY_IS_FIELD(reout));
    g_return_if_fail(!imout || GWY_IS_FIELD(imout));
    g_return_if_fail(!reout || (reout != rein && reout != imin));
    g_return_if_fail(!imout || (imout != rein && imout != imin));
    g_return_if_fail(direction == GWY_TRANSFORM_FORWARD
                     || direction == GWY_TRANSFORM_BACKWARD);

    // Require at least some input if any output is wanted.
    if (!reout && !imout)
        return;
    g_return_if_fail(rein || imin);
    g_return_if_fail(reout != imout);

    const GwyField *in = rein ? rein : imin;
    guint xres = in->xres, yres = in->yres;
    GwyField *myreout = init_fft_field(in, reout);
    GwyField *myimout = init_fft_field(in, imout);

    fftw_iodim trans_dims = { xres, 1, 1 };
    fftw_iodim repeat_dims = { yres, xres, xres };
    guint flags = FFTW_PRESERVE_INPUT | _gwy_fft_rigour();
    if (!rein || !imin) {
        // R2C transform, possibly with some output fixup.
        fftw_plan plan = fftw_plan_guru_split_dft_r2c(1, &trans_dims,
                                                      1, &repeat_dims,
                                                      in->data,
                                                      myreout->data,
                                                      myimout->data,
                                                      flags);
        fftw_execute(plan);
        fftw_destroy_plan(plan);
        complete_fft_real(myreout);
        complete_fft_imag(myimout);

        if (in == rein && direction == GWY_TRANSFORM_BACKWARD) {
            gdouble *im = myimout->data;
            for (guint k = xres*yres; k; k--, im++)
                *im = -(*im);
        }
        else if (in == imin && direction == GWY_TRANSFORM_FORWARD) {
            gdouble *re = myreout->data, *im = myimout->data;
            for (guint k = xres*yres; k; k--, re++, im++) {
                GWY_SWAP(gdouble, *re, *im);
                *re = -(*re);
            }
        }
        else if (in == imin && direction == GWY_TRANSFORM_FORWARD) {
            gdouble *re = myreout->data, *im = myimout->data;
            for (guint k = xres*yres; k; k--, re++, im++)
                GWY_SWAP(gdouble, *re, *im);
        }
    }
    else {
        GwyFieldCompatFlags compat = (GWY_FIELD_COMPAT_RES
                                      | GWY_FIELD_COMPAT_REAL
                                      | GWY_FIELD_COMPAT_UNITS);
        g_return_if_fail(gwy_field_is_incompatible(rein, imin, compat));

        gdouble *reindata = rein->data, *imindata = imin->data,
                *reoutdata = myreout->data, *imoutdata = myimout->data;
        if (direction == GWY_TRANSFORM_BACKWARD) {
            GWY_SWAP(gdouble*, reindata, imindata);
            GWY_SWAP(gdouble*, reoutdata, imoutdata);
        }
        fftw_plan plan = fftw_plan_guru_split_dft(1, &trans_dims,
                                                  1, &repeat_dims,
                                                  reindata, imindata,
                                                  reoutdata, imoutdata,
                                                  flags);
        fftw_execute(plan);
        fftw_destroy_plan(plan);
    }

    g_object_unref(myimout);
    g_object_unref(myreout);

    gdouble q = 1.0/sqrt(xres);

    if (reout) {
        fftize_xdim(reout, in);
        copy_ydim(reout, in);
        gwy_field_invalidate(reout);
        gwy_field_multiply(reout, NULL, NULL, GWY_MASK_IGNORE, q);
    }
    if (imout) {
        fftize_xdim(imout, in);
        copy_ydim(imout, in);
        gwy_field_invalidate(imout);
        gwy_field_multiply(imout, NULL, NULL, GWY_MASK_IGNORE, q);
    }
}

/**
 * gwy_field_fft:
 * @field: 
 * @reout: 
 * @imout: 
 * @windowing: 
 * @preserverms: 
 * @level: 
 *
 * .
 **/
void
gwy_field_fft(const GwyField *field,
              GwyField *reout,
              GwyField *imout,
              GwyWindowingType windowing,
              gboolean preserverms,
              gint level)
{

}

/**
 * gwy_field_fft_raw:
 * @rein: 
 * @imin: 
 * @reout: 
 * @imout: 
 * @direction: 
 *
 * .
 **/
void
gwy_field_fft_raw(const GwyField *rein,
                  const GwyField *imin,
                  GwyField *reout,
                  GwyField *imout,
                  GwyTransformDirection direction)
{

}

/**
 * gwy_field_fft_window:
 * @field: A two-dimensional data field.
 * @windowing: Windowing method.
 * @columns: %TRUE to perform column-wise windowing.
 * @rows: %TRUE to perform row-wise windowing.
 *
 * Applies a FFT windowing type to a data field.
 **/
void
gwy_field_fft_window(GwyField *field,
                     GwyWindowingType windowing,
                     gboolean columns,
                     gboolean rows)
{
    g_return_if_fail(GWY_IS_FIELD(field));
    g_return_if_fail(windowing <= GWY_WINDOWING_KAISER25);

    guint xres = field->xres, yres = field->yres;
    guint size = MAX((columns ? yres : 0), (rows ? xres : 0));
    if (!size)
        return;

    gdouble *window = g_new(gdouble, size);
    if (rows) {
        gwy_fft_window_sample(window, xres, windowing, 0);
        for (guint i = 0; i < yres; i++) {
            const gdouble *w = window;
            gdouble *d = field->data + i*xres;
            for (guint j = xres; j; j--, d++, w++)
                *d *= *w;
        }
    }
    if (columns) {
        gwy_fft_window_sample(window, yres, windowing, 0);
        for (guint i = 0; i < yres; i++) {
            gdouble w = window[i];
            gdouble *d = field->data + i*xres;
            for (guint j = xres; j; j--, d++)
                *d *= w;
        }
    }

    g_free(window);
    gwy_field_invalidate(field);
}

/**
 * gwy_field_fft_humanize:
 * @field: A two-dimensional data field.
 *
 * Rearranges two-dimensional FFT output in a data field to a human-friendly
 * form.
 *
 * Top-left, top-right, bottom-left and bottom-right sub-rectangles are swapped
 * to obtain a humanized two-dimensional FFT output with the zero frequency in
 * the centre.
 *
 * More precisely, for even field dimensions the equally-sized blocks starting
 * with the Nyquist frequency and with the zero frequency (constant component)
 * will exchange places, leaving the zero frequency half a pixel right and down
 * from the centre.  For odd field dimensions, the block containing the zero
 * frequency is one item larger so the constant component will actually end up
 * in the exact centre.
 *
 * If both dimensions are even, this function is involutory and identical to
 * gwy_field_fft_dehumanize().  However, if any dimension is odd,
 * gwy_field_fft_humanize() and gwy_field_fft_dehumanize() are different,
 * therefore they must be paired properly.
 *
 * This method also changes the lateral offsets so that origin in real
 * coordinates corresponds to the centre of pixel with the constant Fourier
 * component (other methods behave similarly).
 **/
void
gwy_field_fft_humanize(GwyField *field)
{
    g_return_if_fail(GWY_IS_FIELD(field));
    guint xres = field->xres;
    guint yres = field->yres;
    if ((xres | yres) & 1) {
        guint jm = xres/2;
        guint im = yres/2;
        GwyField *buf = gwy_field_new_alike(field, FALSE);
        gwy_field_copy(field, &(GwyFieldPart){ 0, 0, xres-jm, yres-im },
                       buf, jm, im);
        gwy_field_copy(field, &(GwyFieldPart){ xres-jm, 0, jm, yres-im },
                       buf, 0, im);
        gwy_field_copy(field, &(GwyFieldPart){ 0, yres-im, xres-jm, im },
                       buf, jm, 0);
        gwy_field_copy(field, &(GwyFieldPart){ xres-jm, yres-im, jm, im },
                       buf, 0, 0);
        gwy_field_copy_full(buf, field);
        g_object_unref(buf);
    }
    else
        humanize_in_place(field);

    humanize_clear_cached(field);
    humanize_xoffset(field);
    humanize_yoffset(field);
}

/**
 * gwy_field_fft_dehumanize:
 * @field: A two-dimensional data field.
 *
 * Rearranges two-dimensional FFT output in a field back from the
 * human-friendly form.
 *
 * Top-left, top-right, bottom-left and bottom-right sub-rectangles are swapped
 * to reshuffle a humanized two-dimensional FFT output back into the natural
 * positions.
 *
 * See gwy_field_fft_humanize() for details.
 **/
void
gwy_field_fft_dehumanize(GwyField *field)
{
    g_return_if_fail(GWY_IS_FIELD(field));
    guint xres = field->xres;
    guint yres = field->yres;
    if ((xres | yres) & 1) {
        guint jm = xres/2;
        guint im = yres/2;
        GwyField *buf = gwy_field_new_alike(field, FALSE);
        gwy_field_copy(field, &(GwyFieldPart){ 0, 0, jm, im },
                       buf, xres-jm, yres-im);
        gwy_field_copy(field, &(GwyFieldPart){ jm, 0, xres-jm, im },
                       buf, 0, yres-im);
        gwy_field_copy(field, &(GwyFieldPart){ 0, im, jm, yres-im},
                       buf, xres-jm, 0);
        gwy_field_copy(field, &(GwyFieldPart){ jm, im, xres-jm, yres-im},
                       buf, 0, 0);
        gwy_field_copy_full(buf, field);
        g_object_unref(buf);
    }
    else
        humanize_in_place(field);

    humanize_clear_cached(field);
    dehumanize_xoffset(field);
    dehumanize_yoffset(field);
}

/**
 * gwy_field_row_fft_humanize:
 * @field: A two-dimensional data field.
 *
 * Rearranges row-wise FFT output in a data field to a human-friendly form.
 *
 * Left and right halves are swapped to obtain a humanized row-wise FFT output
 * with the zero frequency in the centre column.
 *
 * See gwy_field_fft_humanize() for details.
 **/
void
gwy_field_row_fft_humanize(GwyField *field)
{
    g_return_if_fail(GWY_IS_FIELD(field));
    guint xres = field->xres;
    guint yres = field->yres;
    guint jm = xres/2;
    gdouble *buf = g_new(gdouble, xres-jm);
    gdouble *data = field->data;
    for (guint i = 0; i < yres; i++) {
        gdouble *row = data + i*xres;
        // The left part is the potentially larger.  This order ensures no
        // overlap troubles occur.
        gwy_assign(buf, row, xres-jm);
        gwy_assign(row, row + xres-jm, jm);
        gwy_assign(row + jm, buf, xres-jm);
    }
    g_free(buf);
    humanize_clear_cached(field);
    humanize_xoffset(field);
}

/**
 * gwy_field_row_fft_dehumanize:
 * @field: A two-dimensional data field.
 *
 * Rearranges row-wise FFT output in a field back from the human-friendly form.
 *
 * Left and right halves are swapped to reshuffle a humanized row-wise FFT
 * output back into the natural positions.
 *
 * See gwy_field_fft_humanize() for details.
 **/
void
gwy_field_row_fft_dehumanize(GwyField *field)
{
    g_return_if_fail(GWY_IS_FIELD(field));
    guint xres = field->xres;
    guint yres = field->yres;
    guint jm = xres/2;
    gdouble *buf = g_new(gdouble, xres-jm);
    gdouble *data = field->data;
    for (guint i = 0; i < yres; i++) {
        gdouble *row = data + i*xres;
        // The right part is the potentially larger.  This order ensures no
        // overlap troubles occur.
        gwy_assign(buf, row + jm, xres-jm);
        gwy_assign(row + xres-jm, row, jm);
        gwy_assign(row, buf, xres-jm);
    }
    g_free(buf);
    humanize_clear_cached(field);
    dehumanize_xoffset(field);
}

/*
 * (De)humanizes a data field with Fourier coefficients in-place.
 *
 * This method can be only used for even-sized data fields and then
 * it is an involutory operation.
 */
static void
humanize_in_place(GwyField *field)
{
    gdouble *data = field->data;
    guint xres = field->xres;
    guint yres = field->yres;
    guint im = yres/2;
    guint jm = xres/2;
    for (guint i = 0; i < im; i++) {
        for (guint j = 0; j < jm; j++) {
            GWY_SWAP(gdouble,
                     data[j + i*xres], data[(j + jm) + (i + im)*xres]);
            GWY_SWAP(gdouble,
                     data[j + (i + im)*xres], data[(j + jm) + i*xres]);
        }
    }
}

static void
humanize_clear_cached(GwyField *field)
{
    field->priv->cached &= (CBIT(MIN) | CBIT(MAX) | CBIT(AVG) | CBIT(RMS)
                            | CBIT(MSQ) | CBIT(MED));
}

static void
humanize_xoffset(GwyField *field)
{
    if (field->xres & 1)
        gwy_field_set_xoffset(field, -0.5*field->xreal);
    else
        gwy_field_set_xoffset(field, -0.5*(field->xreal + gwy_field_dx(field)));
}

static void
dehumanize_xoffset(GwyField *field)
{
    gwy_field_set_xoffset(field, -0.5*gwy_field_dx(field));
}

static void
humanize_yoffset(GwyField *field)
{
    if (field->yres & 1)
        gwy_field_set_yoffset(field, -0.5*field->xreal);
    else
        gwy_field_set_yoffset(field, -0.5*(field->xreal + gwy_field_dy(field)));
}

static void
dehumanize_yoffset(GwyField *field)
{
    gwy_field_set_yoffset(field, -0.5*gwy_field_dy(field));
}

static void
complete_fft_real(GwyField *field)
{
    guint xres = field->xres, yres = field->yres;
    guint len = (xres + 1)/2 - 1;
    for (guint i = 0; i < yres; i++) {
        const gdouble *re1 = field->data + (i*xres + 1);
        gdouble *re2 = field->data + (i*xres + xres-1);
        for (guint j = len; j; j--, re1++, re2++)
            *re2 = *re1;
    }
}

static void
complete_fft_imag(GwyField *field)
{
    guint xres = field->xres, yres = field->yres;
    guint len = (xres + 1)/2 - 1;
    for (guint i = 0; i < yres; i++) {
        const gdouble *im1 = field->data + (i*xres + 1);
        gdouble *im2 = field->data + (i*xres + xres-1);
        for (guint j = len; j; j--, im1++, im2++)
            *im2 = -(*im1);
    }
}

static void
fftize_xdim(GwyField *fftfield,
            const GwyField *field)
{
    gwy_field_set_xreal(fftfield, 2.0*G_PI/gwy_field_dx(field));
    dehumanize_xoffset(fftfield);
    gwy_unit_power(fftfield->priv->xunit, field->priv->xunit, -1);
}

static void
fftize_ydim(GwyField *fftfield,
            const GwyField *field)
{
    gwy_field_set_yreal(fftfield, 2.0*G_PI/gwy_field_dy(field));
    dehumanize_yoffset(fftfield);
    gwy_unit_power(fftfield->priv->yunit, field->priv->yunit, -1);
}

static void
copy_xdim(GwyField *fftfield,
          const GwyField *field)
{
    gwy_field_set_xreal(fftfield, field->xreal);
    gwy_field_set_xoffset(fftfield, field->xoff);
    _gwy_assign_unit(&fftfield->priv->xunit, field->priv->xunit);
}

static void
copy_ydim(GwyField *fftfield,
          const GwyField *field)
{
    gwy_field_set_yreal(fftfield, field->yreal);
    gwy_field_set_yoffset(fftfield, field->yoff);
    _gwy_assign_unit(&fftfield->priv->yunit, field->priv->yunit);
}

/**
 * SECTION: field-inttrans
 * @section_id: GwyField-inttrans
 * @title: GwyField integral transformations
 * @short_description: Integral transformations of fields (Fourier, wavelet)
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
