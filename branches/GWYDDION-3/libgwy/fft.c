/*
 *  $Id$
 *  Copyright (C) 2009 David Necas (Yeti).
 *  E-mail: yeti@gwyddion.net.
 *
 *  The quicksort algorithm was copied from GNU C library,
 *  Copyright (C) 1991, 1992, 1996, 1997, 1999 Free Software Foundation, Inc.
 *  See below.
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
#include "libgwy/fft.h"
#include "libgwy/libgwy-aliases.h"

typedef gdouble (*GwyFFTWindowingFunc)(guint i, guint n);

static gdouble gwy_fft_window_none     (guint i, guint n);
static gdouble gwy_fft_window_hann     (guint i, guint n);
static gdouble gwy_fft_window_hamming  (guint i, guint n);
static gdouble gwy_fft_window_blackman (guint i, guint n);
static gdouble gwy_fft_window_lanczos  (guint i, guint n);
static gdouble gwy_fft_window_welch    (guint i, guint n);
static gdouble gwy_fft_window_rect     (guint i, guint n);
static gdouble gwy_fft_window_nuttall  (guint i, guint n);
static gdouble gwy_fft_window_flat_top (guint i, guint n);
static gdouble gwy_fft_window_kaiser25 (guint i, guint n);

/* The order must match GwyWindowingType enum */
static const GwyFFTWindowingFunc windowings[] = {
    &gwy_fft_window_none,
    &gwy_fft_window_hann,
    &gwy_fft_window_hamming,
    &gwy_fft_window_blackman,
    &gwy_fft_window_lanczos,
    &gwy_fft_window_welch,
    &gwy_fft_window_rect,
    &gwy_fft_window_nuttall,
    &gwy_fft_window_flat_top,
    &gwy_fft_window_kaiser25,
};

/**
 * smooth_upper_bound:
 * @n: A number.
 *
 * Finds a smooth (highly factorable) number larger or equal to @n.
 *
 * Returns: A smooth number larger or equal to @n.
 **/
static guint
smooth_upper_bound(guint n)
{
    static const guint primes[] = { 2, 3, 5, 7 };

    guint j, p, r;

    for (r = 1; ; ) {
        /* the factorable part */
        for (j = 0; j < G_N_ELEMENTS(primes); j++) {
            p = primes[j];
            while (n % p == 0) {
                n /= p;
                r *= p;
            }
        }

        if (n == 1)
            return r;

        /* gosh... make it factorable again */
        n++;
    }
}

/**
 * gwy_fft_nice_transform_size:
 * @size: Transform size.
 *
 * Finds a nice-for-FFT array size.
 *
 * In this context, <quote>nice</quote> means the following properties are
 * guaranteed:
 * the size is greater than or equal to @size;
 * the transform of this size is possible;
 * and the transform of this size is fast, i.e. the number is highly
 * factorable.
 *
 * Note the second condition is true for any transform size.  So the main
 * use of this function is to find a convenient transform size that ensures
 * fast evaluation in case you have to pad the data anyway (e.g. to calculate
 * the autocorrelation function).
 *
 * Returns: Recommended FFT transform size.
 **/
guint
gwy_fft_nice_transform_size(guint size)
{
    if (size <= 16)
        return size;

    return smooth_upper_bound(size);
}

static gdouble
gwy_fft_window_none(G_GNUC_UNUSED guint i, G_GNUC_UNUSED guint n)
{
    return 1.0;
}

static gdouble
gwy_fft_window_hann(guint i, guint n)
{
    gdouble x = 2*G_PI*i/n;

    return 0.5 - 0.5*cos(x);
}

static gdouble
gwy_fft_window_hamming(guint i, guint n)
{
    gdouble x = 2*G_PI*i/n;

    return 0.54 - 0.46*cos(x);
}

static gdouble
gwy_fft_window_blackman(guint i, guint n)
{
    gdouble x = 2*G_PI*i/n;

    return 0.42 - 0.5*cos(x) + 0.08*cos(2*x);
}

static gdouble
gwy_fft_window_lanczos(guint i, guint n)
{
    gdouble x = 2*G_PI*i/n - G_PI;

    return fabs(x) < 1e-20 ? 1.0 : sin(x)/x;
}

static gdouble
gwy_fft_window_welch(guint i, guint n)
{
    gdouble x = 2.0*i/n - 1.0;

    return 1 - x*x;
}

static gdouble
gwy_fft_window_rect(guint i, guint n)
{
    gdouble par;

    if (i == 0 || i == (n-1))
        par = 0.5;
    else
        par = 1.0;
    return par;
}

static gdouble
gwy_fft_window_nuttall(guint i, guint n)
{
    gdouble x = 2*G_PI*i/n;

    return 0.355768 - 0.487396*cos(x) + 0.144232*cos(2*x) - 0.012604*cos(3*x);
}

static gdouble
gwy_fft_window_flat_top(guint i, guint n)
{
    gdouble x = 2*G_PI*i/n;

    return (1.0 - 1.93*cos(x) + 1.29*cos(2*x)
            - 0.388*cos(3*x) + 0.032*cos(4*x))/4;
}

static inline gdouble
bessel_I0(gdouble x)
{
    gdouble t, s;
    guint i = 1;

    t = x = x*x/4;
    s = 1.0;
    do {
        s += t;
        i++;
        t *= x/i/i;
    } while (t > 1e-7*s);

    return s + t;
}

/* General function */
static gdouble
gwy_fft_window_kaiser(guint i, guint n, gdouble alpha)
{
    gdouble x = 2.0*i/(n - 1) - 1.0;

    return bessel_I0(G_PI*alpha*sqrt(1.0 - x*x));
}

static gdouble
gwy_fft_window_kaiser25(guint i, guint n)
{
    return gwy_fft_window_kaiser(i, n, 2.5)/373.0206312536293446480;
}

/**
 * gwy_fft_window_sample:
 * @data: Data values.
 * @n: Number of data values.
 * @windowing: Windowing method.
 *
 * Samples a windowing function for a specific number of data points.
 **/
void
gwy_fft_window_sample(gdouble *data,
                      guint n,
                      GwyWindowingType windowing)
{
    g_return_if_fail(data);
    if (G_UNLIKELY(windowing >= G_N_ELEMENTS(windowings))) {
        g_critical("Invalid windowing type %u.", windowing);
        windowing = GWY_WINDOWING_NONE;
    }
    if (n == 1) {
        *data = 1.0;
        return;
    }
    GwyFFTWindowingFunc window = windowings[windowing];
    for (guint i = 0; i < n; i++)
        data[i] = window(i, n);
}

#define __LIBGWY_FFT_C__
#include "libgwy/libgwy-aliases.c"

/**
 * SECTION: fft
 * @title: FFT
 * @short_description: Fourier Transform tools
 **/

/**
 * GwyWindowingType:
 * @GWY_WINDOWING_NONE: No windowing is applied.
 * @GWY_WINDOWING_HANN: Hann window.
 * @GWY_WINDOWING_HAMMING: Hamming window.
 * @GWY_WINDOWING_BLACKMANN: Blackmann window.
 * @GWY_WINDOWING_LANCZOS: Lanczos window.
 * @GWY_WINDOWING_WELCH: Welch window.
 * @GWY_WINDOWING_RECT: Rectangular window.
 * @GWY_WINDOWING_NUTTALL: Nuttall window.
 * @GWY_WINDOWING_FLAT_TOP: Flat-top window.
 * @GWY_WINDOWING_KAISER25: Kaiser window with β=2.5.
 *
 * Windowing types that can be applied to data before the Fourier transform.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
