#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <gsl/gsl_errno.h>
#include <gsl/gsl_math.h>
#include <gsl/gsl_fft_real.h>
#include "wavelet.h"

static void
calculate_psd(double *data, long int n, long int zoom)
{
    gsl_fft_real_wavetable *wavetable;
    gsl_fft_real_workspace *workspace;
    double *padded;
    long int i;

    wavetable = gsl_fft_real_wavetable_alloc(n*zoom);
    workspace = gsl_fft_real_workspace_alloc(n*zoom);
    padded = (double*)xmalloc(n*zoom*sizeof(double));
    memset(padded, 0, n*zoom*sizeof(double));
    memcpy(padded, data, n*sizeof(double));
    gsl_fft_real_transform(padded, 1, n*zoom, wavetable, workspace);
    data[0] = fabs(padded[0])/n;
    /* zoom must be at least 2 for this */
    for (i = 1; i < n; i++)
        data[i] = hypot(padded[2*i - 1], padded[2*i])/n;
    padded = xfree(padded);
    gsl_fft_real_wavetable_free(wavetable);
    gsl_fft_real_workspace_free(workspace);
}

int
main(int argc, char *argv[])
{
    double *phi, *psi, *c;
    long int i;
    long int K, Km, scaling_steps, unity, zoom = 4;
    bool do_fft = false;
    double q;

    if (argc != 3) {
        printf("Usage: %s DUBECHIES-K SCALING-STEPS\n", argv[0]);
        printf("Output: X, SCALING-FUNC, WAVELET\n");
        printf("If K is negative, output is the Fourier coefficient modulus "
               "instead.\n");
        return 1;
    }

    K = atol(argv[1]);
    if (K < 0) {
        do_fft = true;
        K = -K;
    }
    if (K < 2 || (K & 1)) {
        fprintf(stderr, "Invalid K %ld\n", K);
        return 1;
    }
    scaling_steps = atol(argv[2]);
    if (scaling_steps < 1) {
        fprintf(stderr, "Invalid scaling steps %ld\n", scaling_steps);
        return 1;
    }
    for (i = K/6; i; i /= 2)
        scaling_steps--;
    unity = 1 << scaling_steps;
    zoom = GSL_MAX(4, unity/zoom);

    c = (double*)xmalloc(K*sizeof(double));
    phi = (double*)xmalloc(K*unity*sizeof(double));
    psi = (double*)xmalloc(K*unity*sizeof(double));

    /* Generate Haar wavelet for K=2, must special-case that */
    Km = K - 1;
    if (K == 2) {
        static const char haardump[] =
            "-0.2 0 0\n"
            "-0.0001 0 0\n"
            "0.0001 1.0001 1.0001\n"
            "0.4999 1.0001 1.0001\n"
            "0.5001 1.0001 -1.0001\n"
            "0.9999 1.0001 -1.0001\n"
            "1.0001 0 0\n"
            "1.2 0 0\n";

        /* If we do not do FFT, don't bother spewing out tons of identical
         * values. */
        if (!do_fft) {
            fputs(haardump, stdout);
            return 0;
        }

        for (i = 0; i < Km*unity; i++)
            phi[i] = 1.0;
        for (i = 0; i < Km*unity/2; i++)
            psi[i] = -1.0;
        for (i = Km*unity/2; i < Km*unity; i++)
            psi[i] = 1.0;
    }
    else {
        coefficients_daubechies(K, c);
        scaling_function(scaling_steps, phi, psi, K, c);
    }

    if (do_fft) {
        calculate_psd(phi, Km*unity, zoom);
        calculate_psd(psi, Km*unity, zoom);
        q = 1.0/(Km*zoom);
        for (i = 0; i < Km*unity; i++) {
            double x = q*i;
            phi[i] *= Km;
            psi[i] *= Km;
            printf("%g %g %g\n", x, phi[i], psi[i]);
        }
    }
    else {
        q = 1.0/unity;
        /* This makes the known wavelets look as everyone is used to.
         * coefficients_daubechies() is stable but does not always produce
         * the usual coefficient order. */
        if (K >= 8 && K <= 14) {
            for (i = 0; i < Km*unity; i++) {
                double x = q*i;
                printf("%g %g %g\n", x, phi[i], psi[i]);
            }
        }
        else {
            for (i = 0; i < Km*unity; i++) {
                double x = q*i;
                printf("%g %g %g\n", x, phi[Km*unity-1 - i], psi[Km*unity-1 - i]);
            }
        }
    }

    return 0;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
