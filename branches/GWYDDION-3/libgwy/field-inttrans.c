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
#include "libgwy/field-level.h"
#include "libgwy/field-inttrans.h"
#include "libgwy/field-internal.h"
#include "libgwy/math-internal.h"

#define CBIT GWY_FIELD_CBIT

static void humanize_in_place(GwyField *field);
static void humanize_clear_cached(GwyField *field);

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
 * @preserverms: %TRUE to preserve RMS value.
 * @level: Row levelling to apply, pass 0 for no row levelling.  See
 *         gwy_field_level_rows().
 *
 * Performs FFT of rows of a two-dimensional data field.
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
    fftw_plan plan = fftw_plan_guru_split_dft_r2c(1, &trans_dims,
                                                  1, &repeat_dims,
                                                  myrein->data,
                                                  myreout->data,
                                                  myimout->data,
                                                  FFTW_DESTROY_INPUT
                                                  | _gwy_fft_rigour());
    gwy_field_copy_full(field, myrein);
    gwy_field_level_rows(myrein, level);
    gwy_field_fft_window(myrein, windowing, FALSE, TRUE);
    fftw_execute(plan);
    fftw_destroy_plan(plan);

    // TODO: distribute the FFT result which occupies only the first xres/2+1
    // values in each row.

    g_object_unref(myrein);
    g_object_unref(myimout);
    g_object_unref(myreout);

    // TODO: implement preserverms
    // FIXME: we may want two preserverms modes:
    // (a) plain preservation of the sum
    // (b) correct preservation of the integral
    // TODO: set dimensions and units of output fields
}

/**
 * gwy_field_row_fft_raw:
 * @rein: 
 * @imin: 
 * @reout: 
 * @imout: 
 * @direction: 
 * @preserveinput: 
 *
 * .
 **/
void
gwy_field_row_fft_raw(GwyField *rein,
                      GwyField *imin,
                      GwyField *reout,
                      GwyField *imout,
                      GwyTransformDirection direction,
                      gboolean preserveinput)
{

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
 * @preserveinput: 
 *
 * .
 **/
void
gwy_field_fft_raw(GwyField *rein,
                  GwyField *imin,
                  GwyField *reout,
                  GwyField *imout,
                  GwyTransformDirection direction,
                  gboolean preserveinput)
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

/**
 * SECTION: field-inttrans
 * @section_id: GwyField-inttrans
 * @title: GwyField integral transformations
 * @short_description: Integral transformations of fields (Fourier, wavelet)
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
