/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

#include "config.h"

#ifdef HAVE_FFTW3
#include <fftw3.h>
#endif

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/inttrans.h>
#include <libprocess/linestats.h>
#include <libprocess/simplefft.h>
#include <libprocess/cwt.h>

static void     gwy_data_line_fft_hum      (GwyTransformDirection direction,
                                            GwyDataLine *rsrc,
                                            GwyDataLine *isrc,
                                            GwyDataLine *rdest,
                                            GwyDataLine *idest,
                                            GwyInterpolationType interpolation);
static void     gwy_data_field_mult_wav          (GwyDataField *real_field,
                                                  GwyDataField *imag_field,
                                                  gdouble scale,
                                                  Gwy2DCWTWaveletType wtype);
static gdouble  edist                            (gint xc1, gint yc1,
                                                  gint xc2, gint yc2);

static void     flip_xy                     (GwyDataField *source, 
                                             GwyDataField *dest, 
                                             gboolean minor);

/*#ifdef HAVE_FFTW3*/
/* Good FFTW array sizes for extended and resampled arrays.
 *
 * Since extending and resampling always involves an O(N) part -- N being the
 * extended array size -- that may even be dominant, it isn't wise to use the
 * fastest possible FFT if it requires considerably larger array.  Following
 * numbers represent a reasonable compromise tested on a few platforms */
static const guint16 nice_fftw_num[] = {
       18,    20,    22,    24,    25,    27,    28,    30,    32,    33,
       35,    36,    40,    42,    44,    72,    75,    77,    81,    84,
       88,    90,    96,    98,    99,   100,   105,   108,   110,   112,
      120,   126,   128,   132,   135,   140,   144,   150,   154,   160,
      165,   168,   175,   176,   180,   189,   196,   198,   200,   210,
      216,   220,   224,   225,   231,   240,   243,   250,   252,   256,
      264,   270,   275,   280,   288,   294,   297,   300,   308,   315,
      320,   324,   330,   336,   343,   350,   352,   360,   375,   384,
      385,   396,   400,   405,   420,   432,   441,   448,   450,   462,
      480,   486,   490,   495,   500,   504,   512,   528,   540,   550,
      576,   588,   594,   600,   616,   630,   640,   648,   672,   675,
      686,   700,   704,   720,   729,   735,   750,   768,   770,   784,
      792,   800,   810,   825,   840,   864,   896,   900,   924,   960,
      972,   980,   990,  1000,  1008,  1024,  1050,  1056,  1080,  1100,
     1120,  1152,  1155,  1176,  1200,  1215,  1232,  1280,  1296,  1320,
     1344,  1350,  1400,  1408,  1440,  1470,  1536,  1568,  1575,  1584,
     1620,  1680,  1728,  1760,  1792,  1800,  1920,  2048,  2058,  2100,
     2112,  2160,  2240,  2250,  2304,  2310,  2352,  2400,  2430,  2464,
     2520,  2560,  2592,  2688,  2700,  2744,  2800,  2816,  2880,  3072,
     3136,  3200,  3240,  3360,  3456,  3520,  3584,  3600,  3645,  3840,
     4096,  4116,  4200,  4224,  4320,  4480,  4608,  4704,  4800,  4860,
     5120,  5184,  5376,  5400,  5488,  5632,  5760,  6144,  6272,  6400,
     6480,  6720,  6750,  6912,  7168,  7200,  7680,  7776,  7840,  8000,
     8064,  8192,  8232,  8400,  8448,  8640,  8960,  9216,  9408,  9600,
     9720,  9800, 10080, 10240, 10368, 10560, 10752, 10800, 10976, 11088,
    11520, 11760, 12288, 12544, 12600, 12800, 12960, 13200, 13440, 13824,
    14080, 14112, 14336, 14400, 15360, 15552, 15680, 16000, 16128, 16200,
    16384, 16464, 16632, 16800, 16875, 17280, 17600, 17920, 18432, 18480,
    19200, 19440, 19600, 19712, 19800, 20160, 20250, 20480, 20736, 21120,
    21504, 21600, 21870, 22050, 22176, 22400, 23040, 23100, 23328, 23520,
    23760, 24000, 24192, 24300, 24576, 24640, 24696, 24750, 25088, 25200,
    25344, 25600, 25920, 26400, 26880, 27000, 27648, 27720, 28160, 28672,
    28800, 29160, 29400, 29568, 30000, 30720, 31104, 31680, 32000, 32256,
    32400, 33000, 33264, 33600, 33792, 34560, 34992, 35000, 35200, 35280,
    35640, 35840, 36000, 36450, 36864, 36960, 37632, 37800, 38016, 38400,
    38880, 39424, 39600, 40000, 40320, 40500, 40960, 41472, 42000, 42240,
    42336, 42525, 43008, 43200, 43218, 43740, 43904, 44000, 44352, 44550,
    44800, 45000, 45056, 45360, 46080, 46656, 47520, 48000, 48384, 48600,
    49152, 49280, 49392, 49500, 50176, 50400, 50625, 50688, 51840, 52800,
    52920, 53760, 54000, 54432, 54675, 55296, 55440, 56250, 57600, 58320,
    58800, 59400, 60000, 60480, 60750, 61440, 61740, 62208, 63000, 63360,
    64800,
};

/* Indices of powers of 2 in nice_fftw_numbers[], starting from 2^4 */
static const guint nice_fftw_num_2n[] = {
    0, 8, 15, 32, 59, 96, 135, 167, 200, 231, 270, 331
};

/**
 * gwy_fft_find_nice_size:
 * @size: Array size.  Currently it must not be larger than a hard-coded
 *        maximum (64800) which should be good enough for all normal uses.
 *
 * Finds a nice-for-FFT array size.
 *
 * XXX XXX XXX Until the transition to FFTW is finished, it always returns
 * number nice for FFTW, ignoring simplefft.
 *
 * The `nice' means three properties are guaranteed: it is greater than or
 * equal to @size; it can be directly used with current FFT backend without
 * scaling (<link linkend="libgwyprocess-simplefft">simplefft</link> can only
 * handle powers of 2); and the transform is fast (this is important for FFTW
 * backend which can handle all array sizes, but performance may suffer).
 *
 * Returns: A nice FFT array size.
 **/
gint
gwy_fft_find_nice_size(gint size)
{
    gint x, p2;

    if (size <= 1 << 4)
        return size;
    g_return_val_if_fail(size <= nice_fftw_num[G_N_ELEMENTS(nice_fftw_num)-1],
                         size);
    for (x = size >> 4, p2 = 0; x; p2++, x = x >> 1)
        ;
    for (x = nice_fftw_num_2n[p2-1]; nice_fftw_num[x] < size; x++)
        ;
    return nice_fftw_num[x];
}
/*#endif*/  /* HAVE_FFTW3 */

/**
 * gwy_data_line_fft_hum:
 * @direction: FFT direction (1 or -1).
 * @rsrc: Real input.
 * @isrc: Imaginary input.
 * @rdest: Real output.
 * @idest: Imaginary output.
 * @interpolation: interpolation used
 *
 * Performs 1D FFT using the alogrithm ffthum (see simplefft.h).
 * Resamples data to closest 2^N and then resamples result back.
 * Resample data by yourself if you want further FFT processing as
 * resampling of the FFT spectrum can destroy some information in it.
 **/
#ifdef HAVE_FFTW3
static void
gwy_data_line_fft_simple(GwyTransformDirection direction,
                      GwyDataLine *rsrc, GwyDataLine *isrc,
                      GwyDataLine *rdest, GwyDataLine *idest,
                      GwyInterpolationType interpolation)
{
    gint newres, oldres, i;
    fftw_complex *in, *out;
    fftw_plan plan;
    gdouble q;

    newres = gwy_fft_find_nice_size(rsrc->res);
    oldres = rsrc->res;
    q = sqrt(newres);

    gwy_data_line_resample(rsrc, newres, interpolation);
    gwy_data_line_resample(isrc, newres, interpolation);
    gwy_data_line_resample(rdest, newres, GWY_INTERPOLATION_NONE);
    gwy_data_line_resample(idest, newres, GWY_INTERPOLATION_NONE);

    in = fftw_malloc(sizeof(fftw_complex) * newres);
    out = fftw_malloc(sizeof(fftw_complex) * newres);
    plan = fftw_plan_dft_1d(newres, in, out, direction, FFTW_MEASURE);
    for (i = 0; i < newres; i++) {
        in[i][0] = rsrc->data[i];
        in[i][1] = isrc->data[i];
    }
    fftw_execute(plan);
    for (i = 0; i < newres; i++) {
        rdest->data[i] = out[i][0]/q;
        idest->data[i] = out[i][1]/q;
    }
    fftw_destroy_plan(plan);
    fftw_free(in);
    fftw_free(out);

    /*FIXME interpolation can dramatically alter the spectrum. Do it preferably
     after all the processings*/
    gwy_data_line_resample(rsrc, oldres, interpolation);
    gwy_data_line_resample(isrc, oldres, interpolation);
    gwy_data_line_resample(rdest, oldres, interpolation);
    gwy_data_line_resample(idest, oldres, interpolation);
}
#else
static void
gwy_data_line_fft_simple(GwyTransformDirection direction,
                      GwyDataLine *rsrc, GwyDataLine *isrc,
                      GwyDataLine *rdest, GwyDataLine *idest,
                      GwyInterpolationType interpolation)
{
    gint newres, oldres;

    /*find the next power of two*/
    newres = gwy_data_field_get_fft_res(rsrc->res);
    oldres = rsrc->res;

    gwy_data_line_resample(rsrc, newres, interpolation);
    gwy_data_line_resample(isrc, newres, interpolation);
    gwy_data_line_resample(rdest, newres, GWY_INTERPOLATION_NONE);
    gwy_data_line_resample(idest, newres, GWY_INTERPOLATION_NONE);

    gwy_fft_simple(direction, rsrc->data, isrc->data, rdest->data, idest->data,
                newres);

    /*FIXME interpolation can dramatically alter the spectrum. Do it preferably
     after all the processings*/
    gwy_data_line_resample(rsrc, oldres, interpolation);
    gwy_data_line_resample(isrc, oldres, interpolation);
    gwy_data_line_resample(rdest, oldres, interpolation);
    gwy_data_line_resample(idest, oldres, interpolation);
}
#endif

/**
 * gwy_data_line_fft:
 * @rsrc: Real input data line.
 * @isrc: Imaginary input data line.
 * @rdest: Real output data line.
 * @idest: Imaginary output data line.
 * @windowing: Windowing mode.
 * @direction: FFT direction.
 * @interpolation: Interpolation type.
 * @preserverms: %TRUE to preserve RMS value while windowing.
 * @level: %TRUE to level line before computation.
 *
 * Performs Fast Fourier transform using a given algorithm.
 *
 * A windowing or data leveling can be applied if requested.
 **/
void
gwy_data_line_fft(GwyDataLine *rsrc, GwyDataLine *isrc,
                  GwyDataLine *rdest, GwyDataLine *idest,
                  GwyWindowingType windowing,
                  GwyTransformDirection direction,
                  GwyInterpolationType interpolation,
                  gboolean preserverms,
                  gboolean level)
{
    gint i, n;
    gdouble rmsa, rmsb;
    GwyDataLine *multra, *multia;
    gdouble coefs[4];

    g_return_if_fail(GWY_IS_DATA_LINE(rsrc));
    g_return_if_fail(GWY_IS_DATA_LINE(isrc));
    g_return_if_fail(GWY_IS_DATA_LINE(rdest));
    g_return_if_fail(GWY_IS_DATA_LINE(idest));

    gwy_debug("");
    if (isrc->res != rsrc->res) {
        gwy_data_line_resample(isrc, rsrc->res, GWY_INTERPOLATION_NONE);
        gwy_data_line_clear(isrc);
    }
    if (rdest->res != rsrc->res)
        gwy_data_line_resample(rdest, rsrc->res, GWY_INTERPOLATION_NONE);
    if (idest->res != rsrc->res)
        gwy_data_line_resample(idest, rsrc->res, GWY_INTERPOLATION_NONE);

    if (level == TRUE) {
        n = 1;
        gwy_data_line_fit_polynom(rsrc, n, coefs);
        gwy_data_line_subtract_polynom(rsrc, n, coefs);
        gwy_data_line_fit_polynom(isrc, n, coefs);
        gwy_data_line_subtract_polynom(isrc, n, coefs);
    }

    gwy_data_line_clear(rdest);
    gwy_data_line_clear(idest);


    if (preserverms == TRUE) {
        multra = gwy_data_line_duplicate(rsrc);
        multia = gwy_data_line_duplicate(isrc);

        rmsa = gwy_data_line_get_rms(multra);

        gwy_fft_window(multra->data, multra->res, windowing);
        gwy_fft_window(multia->data, multia->res, windowing);

        gwy_data_line_fft_simple(direction, multra, multia, rdest, idest,
                              interpolation);

        rmsb = 0;
        for (i = 0; i < multra->res/2; i++)
            rmsb += 2*(rdest->data[i]*rdest->data[i]
                       + idest->data[i]*idest->data[i])
                    /(rsrc->res*rsrc->res);
        rmsb = sqrt(rmsb);

        gwy_data_line_multiply(rdest, rmsa/rmsb);
        gwy_data_line_multiply(idest, rmsa/rmsb);
        g_object_unref(multra);
        g_object_unref(multia);
    }
    else {
        gwy_fft_window(rsrc->data, rsrc->res, windowing);
        gwy_fft_window(isrc->data, rsrc->res, windowing);

        gwy_data_line_fft_simple(direction, rsrc, isrc, rdest, idest,
                              interpolation);
    }
}

/**
 * gwy_data_field_2dfft:
 * @ra: Real input data field
 * @ia: Imaginary input data field
 * @rb: Real output data field
 * @ib: Imaginary output data field
 * @windowing: Windowing type.
 * @direction: FFT direction.
 * @interpolation: Interpolation type.
 * @preserverms: %TRUE to preserve RMS while windowing.
 * @level: %TRUE to level data before computation.
 *
 * Computes 2D Fast Fourier Transform.
 *
 * If requested a windowing and/or leveling is applied to preprocess data to
 * obtain reasonable results.
 **/
void
gwy_data_field_2dfft(GwyDataField *ra, GwyDataField *ia,
                     GwyDataField *rb, GwyDataField *ib,
                     GwyWindowingType windowing,
                     GwyTransformDirection direction,
                     GwyInterpolationType interpolation,
                     gboolean preserverms, gboolean level)
{
    GwyDataField *rh, *ih;

    rh = gwy_data_field_new_alike(ra, FALSE);
    ih = gwy_data_field_new_alike(ra, FALSE);

    gwy_data_field_xfft(ra, ia, rh, ih,
                        windowing, direction, interpolation,
                        preserverms, level);
    gwy_data_field_yfft(rh, ih, rb, ib,
                        windowing, direction, interpolation,
                        preserverms, level);

    g_object_unref(rh);
    g_object_unref(ih);

    gwy_data_field_invalidate(rb);
    gwy_data_field_invalidate(ib);
}

/**
 * gwy_data_field_2dfft_real:
 * @ra: Real input data field
 * @rb: Real output data field
 * @ib: Imaginary output data field
 * @windowing: Windowing type.
 * @direction: FFT direction.
 * @interpolation: Interpolation type.
 * @preserverms: %TRUE to preserve RMS while windowing.
 * @level: %TRUE to level data before computation.
 *
 * Computes 2D Fast Fourier Transform of real data.
 *
 * As the input is only real, the computation can be a somewhat faster
 * than gwy_data_field_2dfft().
 **/
void
gwy_data_field_2dfft_real(GwyDataField *ra, GwyDataField *rb,
                          GwyDataField *ib,
                          GwyWindowingType windowing,
                          GwyTransformDirection direction,
                          GwyInterpolationType interpolation,
                          gboolean preserverms, gboolean level)
{
    GwyDataField *rh, *ih;

    rh = gwy_data_field_new_alike(ra, FALSE);
    ih = gwy_data_field_new_alike(ra, FALSE);
    gwy_data_field_xfft_real(ra, rh, ih,
                             windowing, direction, interpolation,
                             preserverms, level);
    gwy_data_field_yfft(rh, ih, rb, ib,
                        windowing, direction, interpolation,
                        preserverms, level);

    g_object_unref(rh);
    g_object_unref(ih);

    gwy_data_field_invalidate(rb);
    gwy_data_field_invalidate(ib);
}


/**
 * gwy_data_field_get_fft_res:
 * @data_res: data resolution
 *
 * Finds the closest 2^N value.
 *
 * Returns: 2^N good for FFT.
 **/
gint
gwy_data_field_get_fft_res(gint data_res)
{
    return (gint)pow(2, ((gint)floor(log((gdouble)data_res)/log(2.0) + 0.5)));
}


/**
 * gwy_data_field_2dfft_humanize:
 * @data_field: A data field.
 *
 * Rearranges 2D FFT output to a human-friendly form.
 *
 * Top-left, top-right, bottom-left and bottom-right sub-squares are swapped
 * to obtain a humanized 2D FFT output with (0,0) in the center.
 **/
void
gwy_data_field_2dfft_humanize(GwyDataField *a)
{
    gint i, j, im, jm, xres;
    GwyDataField *b;

    b = gwy_data_field_duplicate(a);
    im = a->yres/2;
    jm = a->xres/2;
    xres = a->xres;
    for (i = 0; i < im; i++) {
        for (j = 0; j < jm; j++) {
            a->data[(j + jm) + (i + im)*xres] = b->data[j + i*xres];
            a->data[(j + jm) + i*xres] = b->data[j + (i + im)*xres];
            a->data[j + (i + im)*xres] = b->data[(j + jm) + i*xres];
            a->data[j + i*xres] = b->data[(j + jm) + (i + im)*xres];
        }
    }
    g_object_unref(b);
}


/**
 * gwy_data_field_xfft:
 * @ra: Real input data field
 * @ia: Imaginary input data field
 * @rb: Real output data field
 * @ib: Imaginary output data field
 * @windowing: Windowing type.
 * @direction: FFT direction.
 * @interpolation: Interpolation type.
 * @preserverms: %TRUE to preserve RMS while windowing.
 * @level: %TRUE to level data before computation.
 *
 * Transforms all rows in a data field with Fast Fourier Transform.
 *
 * If requested a windowing and/or leveling is applied to preprocess data to
 * obtain reasonable results.
 **/
void
gwy_data_field_xfft(GwyDataField *ra, GwyDataField *ia,
                    GwyDataField *rb, GwyDataField *ib,
                    GwyWindowingType windowing,
                    GwyTransformDirection direction,
                    GwyInterpolationType interpolation,
                    gboolean preserverms, gboolean level)
{
    gint k, newxres;
    GwyDataField *rin, *iin;
    const gdouble *in_rdata, *in_idata;
    gdouble *out_rdata, *out_idata;

    rin = gwy_data_field_duplicate(ra);
    iin = gwy_data_field_duplicate(ia);

    gwy_fft_window_datafield(rin, GTK_ORIENTATION_HORIZONTAL, windowing);
    gwy_fft_window_datafield(iin, GTK_ORIENTATION_HORIZONTAL, windowing);
    if (!rb) 
        rb = gwy_data_field_new_alike(ra, TRUE);
    else {
        gwy_data_field_resample(rb, rin->xres, rin->yres, GWY_INTERPOLATION_NONE);
        gwy_data_field_fill(rb, 0);
    }
    if (!ib) 
        ib = gwy_data_field_new_alike(ra, TRUE);
    else {
        gwy_data_field_resample(ib, rin->xres, rin->yres, GWY_INTERPOLATION_NONE);
        gwy_data_field_fill(ib, 0);
    }

     
    /*here starts the fft itself*/
    newxres = gwy_data_field_get_fft_res(gwy_data_field_get_xres(rin));
    gwy_data_field_resample(rin, newxres, rin->yres, GWY_INTERPOLATION_BILINEAR);
    gwy_data_field_resample(iin, newxres, rin->yres, GWY_INTERPOLATION_BILINEAR);
    gwy_data_field_resample(rb, newxres, rin->yres, GWY_INTERPOLATION_BILINEAR);
    gwy_data_field_resample(ib, newxres, rin->yres, GWY_INTERPOLATION_BILINEAR);
     
    in_rdata = gwy_data_field_get_data_const(rin);
    in_idata = gwy_data_field_get_data_const(iin);
    out_rdata = gwy_data_field_get_data(rb);
    out_idata = gwy_data_field_get_data(ib);
    for (k = 0; k < rin->yres; k++) {
        gwy_fft_simple(direction, 
                       in_rdata + k*rin->xres,
                       in_idata + k*rin->xres,
                       out_rdata + k*rin->xres,
                       out_idata + k*rin->xres,
                       newxres);
    }

    gwy_data_field_resample(rb, ra->xres, ra->yres, GWY_INTERPOLATION_BILINEAR);
    gwy_data_field_resample(ib, ra->xres, ra->yres, GWY_INTERPOLATION_BILINEAR);
    /*here ends the fft itself*/
    
    g_object_unref(rin);
    g_object_unref(iin);
    gwy_data_field_invalidate(rb);
    gwy_data_field_invalidate(ib);
}

/**
 * gwy_data_field_yfft:
 * @ra: Real input data field
 * @ia: Imaginary input data field
 * @rb: Real output data field
 * @ib: Imaginary output data field
 * @windowing: Windowing type.
 * @direction: FFT direction.
 * @interpolation: Interpolation type.
 * @preserverms: %TRUE to preserve RMS while windowing.
 * @level: %TRUE to level data before computation.
 *
 * Transforms all columns in a data field with Fast Fourier Transform.
 *
 * If requested a windowing and/or leveling is applied to preprocess data to
 * obtain reasonable results.
 **/
void
gwy_data_field_yfft(GwyDataField *ra, GwyDataField *ia,
                    GwyDataField *rb, GwyDataField *ib,
                    GwyWindowingType windowing,
                    GwyTransformDirection direction,
                    GwyInterpolationType interpolation,
                    gboolean preserverms, gboolean level)
{
    gint i, k, newyres;
    GwyDataField *rin, *iin;
    const gdouble *in_rdata, *in_idata;
    gdouble *out_rdata, *out_idata;
    gdouble *col_in_rdata, *col_in_idata, *col_out_rdata, *col_out_idata;

    rin = gwy_data_field_duplicate(ra);
    iin = gwy_data_field_duplicate(ia);

    gwy_fft_window_datafield(rin, GTK_ORIENTATION_HORIZONTAL, windowing);
    gwy_fft_window_datafield(iin, GTK_ORIENTATION_HORIZONTAL, windowing);
    if (!rb) 
        rb = gwy_data_field_new_alike(ra, TRUE);
    else {
        gwy_data_field_resample(rb, rin->xres, rin->yres, GWY_INTERPOLATION_NONE);
        gwy_data_field_fill(rb, 0);
    }
    if (!ib) 
        ib = gwy_data_field_new_alike(ra, TRUE);
    else {
        gwy_data_field_resample(ib, rin->xres, rin->yres, GWY_INTERPOLATION_NONE);
        gwy_data_field_fill(ib, 0);
    }

     
    /*here starts the fft itself*/
    newyres = gwy_data_field_get_fft_res(gwy_data_field_get_yres(rin));
    gwy_data_field_resample(rin, rin->xres, newyres, GWY_INTERPOLATION_BILINEAR);
    gwy_data_field_resample(iin, rin->xres, newyres, GWY_INTERPOLATION_BILINEAR);
    gwy_data_field_resample(rb, rin->xres, newyres, GWY_INTERPOLATION_BILINEAR);
    gwy_data_field_resample(ib, rin->xres, newyres, GWY_INTERPOLATION_BILINEAR);
     
    in_rdata = gwy_data_field_get_data_const(rin);
    in_idata = gwy_data_field_get_data_const(iin);
    out_rdata = gwy_data_field_get_data(rb);
    out_idata = gwy_data_field_get_data(ib);

    col_in_rdata = (gdouble *)g_malloc(newyres*sizeof(gdouble));
    col_in_idata = (gdouble *)g_malloc(newyres*sizeof(gdouble));
    col_out_rdata = (gdouble *)g_malloc(newyres*sizeof(gdouble));
    col_out_idata = (gdouble *)g_malloc(newyres*sizeof(gdouble));
    
    for (k = 0; k < rin->xres; k++) {
        for (i = 0; i < newyres; i++)
        {
            col_in_rdata[i] = in_rdata[i*rin->xres + k];
            col_in_idata[i] = in_idata[i*rin->xres + k];
        }
        gwy_fft_simple(direction, 
                       col_in_rdata,
                       col_in_idata,
                       col_out_rdata,
                       col_out_idata,
                       newyres);
        for (i = 0; i < newyres; i++)
        {
            out_rdata[i*rin->xres + k] = col_out_rdata[i];
            out_idata[i*rin->xres + k] = col_out_idata[i];
        }
     }

    g_free(col_in_rdata);
    g_free(col_in_idata);
    g_free(col_out_rdata);
    g_free(col_out_idata);

    gwy_data_field_resample(rb, ra->xres, ra->yres, GWY_INTERPOLATION_BILINEAR);
    gwy_data_field_resample(ib, ra->xres, ra->yres, GWY_INTERPOLATION_BILINEAR);
    /*here ends the fft itself*/
    
    g_object_unref(rin);
    g_object_unref(iin);
    gwy_data_field_invalidate(rb);
    gwy_data_field_invalidate(ib);
}

/**
 * gwy_data_field_xfft_real:
 * @ra: Real input data field
 * @rb: Real output data field
 * @ib: Imaginary output data field
 * @windowing: Windowing type.
 * @direction: FFT direction.
 * @interpolation: Interpolation type.
 * @preserverms: %TRUE to preserve RMS while windowing.
 * @level: %TRUE to level data before computation.
 *
 * Transforms all rows in a data real field with Fast Fourier Transform.
 *
 * As the input is only real, the computation can be a somewhat faster
 * than gwy_data_field_xfft().
 **/
void
gwy_data_field_xfft_real(GwyDataField *ra, GwyDataField *rb,
                         GwyDataField *ib,
                         GwyWindowingType windowing,
                         GwyTransformDirection direction,
                         GwyInterpolationType interpolation,
                         gboolean preserverms, gboolean level)
{
    gint k, j;
    GwyDataLine *rin, *iin, *rout, *iout, *rft1, *ift1, *rft2, *ift2;

    gwy_data_field_resample(rb, ra->xres, ra->yres, GWY_INTERPOLATION_NONE);
    gwy_data_field_resample(ib, ra->xres, ra->yres, GWY_INTERPOLATION_NONE);

    rin = gwy_data_line_new(ra->xres, ra->yreal, FALSE);
    rout = gwy_data_line_new(ra->xres, ra->yreal, FALSE);
    iin = gwy_data_line_new(ra->xres, ra->yreal, FALSE);
    iout = gwy_data_line_new(ra->xres, ra->yreal, FALSE);
    rft1 = gwy_data_line_new(ra->xres, ra->yreal, FALSE);
    ift1 = gwy_data_line_new(ra->xres, ra->yreal, FALSE);
    rft2 = gwy_data_line_new(ra->xres, ra->yreal, FALSE);
    ift2 = gwy_data_line_new(ra->xres, ra->yreal, FALSE);

    /*we compute allways two FFTs simultaneously*/
    for (k = 0; k < ra->yres; k++) {
        gwy_data_field_get_row(ra, rin, k);
        if (k < ra->yres-1)
            gwy_data_field_get_row(ra, iin, k+1);
        else {
            gwy_data_field_get_row(ra, iin, k);
            gwy_data_line_clear(iin);
        }

        gwy_data_line_fft(rin, iin, rout, iout,
                          windowing, direction, interpolation,
                          preserverms, level);

        /*extract back the two profiles FFTs*/
        rft1->data[0] = rout->data[0];
        ift1->data[0] = 0;
        rft2->data[0] = iout->data[0];
        ift2->data[0] = 0;
        for (j = 1; j < ra->xres; j++) {
            rft1->data[j] = (rout->data[j] + rout->data[ra->xres - j])/2;
            ift1->data[j] = (iout->data[j] - iout->data[ra->xres - j])/2;
            rft2->data[j] = (iout->data[j] + iout->data[ra->xres - j])/2;
            ift2->data[j] = -(rout->data[j] - rout->data[ra->xres - j])/2;
        }

        gwy_data_field_set_row(rb, rft1, k);
        gwy_data_field_set_row(ib, ift1, k);

        if (k < ra->yres-1) {
            gwy_data_field_set_row(rb, rft2, k+1);
            gwy_data_field_set_row(ib, ift2, k+1);
            k++;
        }
    }

    g_object_unref(rin);
    g_object_unref(rout);
    g_object_unref(iin);
    g_object_unref(iout);
    g_object_unref(rft1);
    g_object_unref(rft2);
    g_object_unref(ift1);
    g_object_unref(ift2);

    gwy_data_field_invalidate(rb);
    gwy_data_field_invalidate(ib);
}

static inline gdouble
edist(gint xc1, gint yc1, gint xc2, gint yc2)
{
    return sqrt(((gdouble)xc1-xc2)*((gdouble)xc1-xc2)
                + ((gdouble)yc1-yc2)*((gdouble)yc1-yc2));
}

/**
 * gwy_data_field_mult_wav:
 * @real_field: A data field of real values
 * @imag_field: A data field of imaginary values
 * @scale: wavelet scale
 * @wtype: waveelt type
 *
 * multiply a complex data field (real and imaginary)
 * with complex FT of spoecified wavelet at given scale.
 **/
static void
gwy_data_field_mult_wav(GwyDataField *real_field,
                        GwyDataField *imag_field,
                        gdouble scale,
                        Gwy2DCWTWaveletType wtype)
{
    gint xres, yres, xresh, yresh;
    gint i, j;
    gdouble mval, val;

    xres = real_field->xres;
    yres = real_field->yres;
    xresh = xres/2;
    yresh = yres/2;


    for (i = 0; i < xres; i++) {
        for (j = 0; j < yres; j++) {
            val = 1;
            if (i < xresh) {
                if (j < yresh)
                    mval = edist(0, 0, i, j);
                else
                    mval = edist(0, yres, i, j);
            }
            else {
                if (j < yresh)
                    mval = edist(xres, 0, i, j);
                else
                    mval = edist(xres, yres, i, j);
            }
            val = gwy_cwt_wfunc_2d(scale, mval, xres, wtype);


            real_field->data[i + j * xres] *= val;
            imag_field->data[i + j * xres] *= val;
        }
    }

}

/**
 * gwy_data_field_cwt:
 * @data_field: A data field.
 * @interpolation: Interpolation type.
 * @scale: Wavelet scale.
 * @wtype: Wavelet type.
 *
 * Computes a continuous wavelet transform (CWT) at given
 * scale and using given wavelet.
 **/
void
gwy_data_field_cwt(GwyDataField *data_field,
                   GwyInterpolationType interpolation,
                   gdouble scale,
                   Gwy2DCWTWaveletType wtype)
{
    GwyDataField *hlp_r, *hlp_i, *imag_field;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));

    hlp_r = gwy_data_field_new_alike(data_field, FALSE);
    hlp_i = gwy_data_field_new_alike(data_field, FALSE);
    imag_field = gwy_data_field_new_alike(data_field, TRUE);

    gwy_data_field_2dfft(data_field,
                         imag_field,
                         hlp_r,
                         hlp_i,
                         GWY_WINDOWING_RECT,
                         GWY_TRANSFORM_DIRECTION_FORWARD,
                         interpolation,
                         FALSE,
                         FALSE);
    gwy_data_field_mult_wav(hlp_r, hlp_i, scale, wtype);

    gwy_data_field_2dfft(hlp_r,
                         hlp_i,
                         data_field,
                         imag_field,
                         GWY_WINDOWING_RECT,
                         GWY_TRANSFORM_DIRECTION_BACKWARD,
                         interpolation,
                         FALSE,
                         FALSE);

    g_object_unref(hlp_r);
    g_object_unref(hlp_i);
    g_object_unref(imag_field);

    gwy_data_field_invalidate(data_field);
}

void
gwy_data_field_fft_filter_1d(GwyDataField *data_field,
                             GwyDataField *result_field,
                             GwyDataLine *weights,
                             GwyOrientation orientation,
                             GwyInterpolationType interpolation)
{
    gint i, j;
    GwyDataField *idata_field, *hlp_dfield, *hlp_idfield, *iresult_field;
    GwyDataLine *dline;

    idata_field = gwy_data_field_new_alike(data_field, TRUE);
    hlp_dfield = gwy_data_field_new_alike(data_field, TRUE);
    hlp_idfield = gwy_data_field_new_alike(data_field, TRUE);
    iresult_field = gwy_data_field_new_alike(data_field, TRUE);

    dline = GWY_DATA_LINE(gwy_data_line_new(data_field->xres, data_field->xres,
                                            FALSE));

    if (orientation == GWY_ORIENTATION_VERTICAL)
        gwy_data_field_rotate(data_field, G_PI/2, interpolation);

    gwy_data_field_xfft(data_field, result_field,
                        hlp_dfield, hlp_idfield,
                        GWY_WINDOWING_NONE,
                        GWY_TRANSFORM_DIRECTION_FORWARD,
                        interpolation,
                        FALSE, FALSE);

    if (orientation == GWY_ORIENTATION_VERTICAL)
        gwy_data_field_rotate(data_field, -G_PI/2, interpolation);

    gwy_data_line_resample(weights, hlp_dfield->xres/2, interpolation);
    for (i = 0; i < hlp_dfield->yres; i++) {
        gwy_data_field_get_row(hlp_dfield, dline, i);
        for (j = 0; j < weights->res; j++) {
            dline->data[j] *= weights->data[j];
            dline->data[dline->res - j - 1] *= weights->data[j];
        }
        gwy_data_field_set_row(hlp_dfield, dline, i);

        gwy_data_field_get_row(hlp_idfield, dline, i);
        for (j = 0; j < weights->res; j++) {
            dline->data[j] *= weights->data[j];
            dline->data[dline->res - j - 1] *= weights->data[j];
        }
        gwy_data_field_set_row(hlp_idfield, dline, i);
    }

    gwy_data_field_xfft(hlp_dfield, hlp_idfield,
                        result_field, iresult_field,
                        GWY_WINDOWING_NONE,
                        GWY_TRANSFORM_DIRECTION_BACKWARD,
                        interpolation,
                        FALSE, FALSE);

    if (orientation == GWY_ORIENTATION_VERTICAL)
        gwy_data_field_rotate(result_field, -G_PI/2, interpolation);

    g_object_unref(idata_field);
    g_object_unref(hlp_dfield);
    g_object_unref(hlp_idfield);
    g_object_unref(iresult_field);
    g_object_unref(dline);
}

/*FIXME this should be public (somewhere) or removed*/
static void
flip_xy(GwyDataField *source, GwyDataField *dest, gboolean minor)
{
    gint xres, yres, i, j;
    gdouble *dd;
    const gdouble *sd;

    xres = gwy_data_field_get_xres(source);
    yres = gwy_data_field_get_yres(source);
    gwy_data_field_resample(dest, yres, xres, GWY_INTERPOLATION_NONE);
    sd = gwy_data_field_get_data_const(source);
    dd = gwy_data_field_get_data(dest);
    if (minor) {
        for (i = 0; i < xres; i++) {
            for (j = 0; j < yres; j++) {
               dd[i*yres + j] = sd[j*xres + (xres - 1 - i)];
           }
        } 
    }
    else {
        for (i = 0; i < xres; i++) {
            for (j = 0; j < yres; j++) {
                dd[i*yres + (yres - 1 - j)] = sd[j*xres + i];
            }
        }
    }
    gwy_data_field_set_xreal(dest, gwy_data_field_get_yreal(source));
    gwy_data_field_set_yreal(dest, gwy_data_field_get_xreal(source));
}



/************************** Documentation ****************************/

/**
 * SECTION:inttrans
 * @title: inttrans
 * @short_description: FFT and other integral transforms
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
