/*
 *  $Id$
 *  Copyright (C) 2009 David Nečas (Yeti).
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

#include <stdlib.h>
#include <stdio.h>
#include <fftw3.h>
#include "libgwy/macros.h"
#include "libgwy/strfuncs.h"
#include "libgwy/math.h"
#include "libgwy/main.h"
#include "libgwy/fft.h"
#include "libgwy/math-internal.h"

#define WISDOM_FILENAME_TEMPLATE "fftw3-wisdom-%s"

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

static guint fft_rigour = FFTW_ESTIMATE;

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
 * Returns: An even smooth number larger or equal to @n.
 **/
static guint
smooth_upper_bound(guint n)
{
    static const guint primes[] = { 2, 3, 5, 7 };

    n += n & 1;
    while (TRUE) {
        guint r = n;
        for (guint j = 0; j < G_N_ELEMENTS(primes); j++) {
            guint p = primes[j];
            while (r % p == 0)
                r /= p;
        }
        if (r == 1 || r == 11 || r == 13)
            return n;
        n += 2;
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
 * the size is even and greater than or equal to @size;
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
        return size + (size & 1);

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
    x = x*x;
    x = fmax(x, 0.0);

    return bessel_I0(G_PI*alpha*sqrt(x));
}

static gdouble
gwy_fft_window_kaiser25(guint i, guint n)
{
    return gwy_fft_window_kaiser(i, n, 2.5)/373.0206312536293446480;
}

/**
 * gwy_fft_window_sample:
 * @data: (out) (array length=n):
 *        Data values.
 * @n: Number of data values.
 * @windowing: Windowing method.
 * @normalize: 0 to return the windowing function as-defined, 1 to normalise
 *             the mean value of coefficients to unity, 2 to normalise the mean
 *             squared value of coefficients to unity.
 *
 * Samples a windowing function for a specific number of data points.
 **/
void
gwy_fft_window_sample(gdouble *data,
                      guint n,
                      GwyWindowingType windowing,
                      guint normalize)
{
    g_return_if_fail(data);
    g_return_if_fail(normalize <= 2);
    if (G_UNLIKELY(windowing >= G_N_ELEMENTS(windowings))) {
        g_critical("Invalid windowing type %u.", windowing);
        windowing = GWY_WINDOWING_NONE;
    }
    if (n == 1) {
        *data = 1.0;
        return;
    }
    GwyFFTWindowingFunc window = windowings[windowing];
    if (!normalize) {
        for (guint i = 0; i < n; i++)
            data[i] = window(i, n);
        return;
    }

    gdouble wcorr = 0.0;
    if (normalize == 2) {
        for (guint i = 0; i < n; i++) {
            data[i] = window(i, n);
            wcorr += data[i]*data[i];
        }
        wcorr = sqrt(n/wcorr);
    }
    else {
        for (guint i = 0; i < n; i++)
            wcorr += data[i] = window(i, n);
        wcorr = n/wcorr;
    }
    for (guint i = 0; i < n; i++)
        data[i] *= wcorr;
}

// FIXME: Linux-only at this moment.  This means on other systems we
// effectively assume the CPU never changes.
static gchar*
find_cpu_configuration(void)
{
    gchar *buffer = NULL;
    gsize length = 0;

    /* Do not bother with error reporting, if the file does not exist, we are
     * on some other Unix than Linux. */
    if (!g_file_get_contents("/proc/cpuinfo", &buffer, &length, NULL)) {
        g_warning("Cannot determine the CPU type.");
        return g_strdup("unknown");
    }

    // XXX: In principle, the types of invididual processors can differ.
    guint family = 0, model = 0, cpuno, ncores = 0;
    gchar *p = buffer;
    for (gchar *line = gwy_str_next_line(&p);
         line;
         line = gwy_str_next_line(&p)) {

        if (sscanf(line, "cpu family	:%u", &family))
            continue;
        if (sscanf(line, "model	:%u", &model))
            continue;
        if (sscanf(line, "processor	:%u", &cpuno))
            ncores++;
    }
    g_free(buffer);

    return g_strdup_printf("f%u-m%u-c%u", family, model, ncores);
}

static gchar*
build_wisdom_file_name(void)
{
    gchar *userdir = gwy_user_directory(NULL);
    g_return_val_if_fail(userdir, NULL);

    gchar *cputype = find_cpu_configuration();
    gchar *wisdomname = g_strdup_printf(WISDOM_FILENAME_TEMPLATE, cputype);
    gchar *fullpath = g_build_filename(userdir, wisdomname, NULL);
    g_free(wisdomname);
    g_free(cputype);
    g_free(userdir);

    return fullpath;
}

static gpointer
load_wisdom(G_GNUC_UNUSED gpointer arg)
{
    // If we can't get a reasonable file name, we won't be able to save the
    // wisdom either.  So return before changing the planning rigour.
    gchar *filename = build_wisdom_file_name();
    if (!filename)
        return NULL;

    fft_rigour = FFTW_MEASURE;

    // No error reporting, the file simply may not exist.
    gchar *wisdom = NULL;
    if (!g_file_get_contents(filename, &wisdom, NULL, NULL))
        return NULL;

    // Use the string variants of the wisdom functions as passing filehandles
    // to FFTW might fail on Win32.
    if (!fftw_import_wisdom_from_string(wisdom))
        g_warning("FFTW rejected wisdom from %s.", filename);

    g_free(filename);
    g_free(wisdom);

    return NULL;
}

/**
 * gwy_fft_load_wisdom:
 *
 * Loads FFTW wisdom from a file user directory.
 *
 * This function can be safely called any number of times from arbitrary
 * threads (if GLib thread support is initialised).  The wisdom will be loaded
 * just once.
 *
 * Unless/until gwy_fft_load_wisdom() is called libgwy functions utilising
 * FFTW set the planning rigour to <constant>FFTW_ESTIMATE</constant>.  This is
 * done to save the considerable plan optimisation times because the functions
 * often use many different transform sizes and types.  Calling this function
 * will change the rigour to <constant>FFTW_MEASURE</constant> based on the
 * assumption that if wisdom is used the time invested to planning will
 * eventually return.  In order to make it happen you need to also save the
 * wisdom with gwy_fft_save_wisdom() at program exit – like wisdom loading,
 * this is not done automatically.
 **/
void
gwy_fft_load_wisdom(void)
{
    static GOnce wisdom_loaded = G_ONCE_INIT;
    g_once(&wisdom_loaded, load_wisdom, NULL);
}

/**
 * gwy_fft_save_wisdom:
 *
 * Saves FFTW wisdom into a file in user directory.
 *
 * The wisdom file name contains encoded bits of hardware configuration.
 * Wisdom files not corresponding to the current hardware configuration are
 * simply ignored and kept in place so wisdom saving and loading should work
 * reasonably also with shared home directories.
 *
 * Unlike gwy_fft_load_wisdom(), this function actually saves the wisdom each
 * time it is called and this calling it from multiple threads at once might
 * lead to unhappines.
 **/
void
gwy_fft_save_wisdom(void)
{
    gchar *filename = build_wisdom_file_name();
    if (!filename)
        return;

    // Use the string variants of the wisdom functions as passing filehandles
    // to FFTW might fail on Win32.
    gchar *wisdom = fftw_export_wisdom_to_string();
    GError *error = NULL;
    if (!g_file_set_contents(filename, wisdom, -1, &error)) {
        g_warning("Cannot save FFTW wisdom to %s: %s.",
                  filename, error->message);
        g_clear_error(&error);
    }
    // g_free() and free() might not be equivalent.
    free(wisdom);
}

guint
_gwy_fft_rigour(void)
{
    return fft_rigour;
}

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

/**
 * GwyTransformDirection:
 * @GWY_TRANSFORM_BACKWARD: Backward (inverse) transform.
 * @GWY_TRANSFORM_FORWARD: Forward (direct) transform.
 *
 * Direction of a transformation, namely integral transformation.
 *
 * In FFT, it is equal to sign of the exponent, that is the backward transform
 * uses 1, the forward transform -1.  This is in line with FFTW so
 * %GWY_TRANSFORM_FORWARD and %FFTW_FORWARD can be used interchangably (but it
 * is the opposite of Gwyddion 1 and 2 convention).
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
