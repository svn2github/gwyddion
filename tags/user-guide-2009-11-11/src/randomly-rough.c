#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <libgwyddion/gwyddion.h>
#include <libprocess/gwyprocess.h>

int
main(int argc, char *argv[])
{
    enum { PSDF, ACF, HHCF } mode = PSDF;
    GwyDataField *dfield;
    GwyDataLine *dline;
    GwyNLFitPreset *preset;
    GwyNLFitter *fitter;
    GRand *rng;
    gdouble *data, *xdata, *params, *errors;
    const gchar *presetname = NULL;
    gdouble dx = 50e-9;
    gdouble sigma = 20e-9;
    gdouble T = 300e-9;
    gdouble r;
    gboolean ok;
    gint N, n, i;

    if (argc != 3) {
        g_printerr("Usage %s {psdf|acf|hhcf} DATA-SIZE\n", argv[0]);
        return 1;
    }

    if (gwy_strequal(argv[1], "psdf")) {
        mode = PSDF;
    }
    else if (gwy_strequal(argv[1], "acf")) {
        mode = ACF;
    }
    else if (gwy_strequal(argv[1], "hhcf")) {
        mode = HHCF;
    }
    else {
        g_printerr("Unknown type %s\n", argv[1]);
        return 1;
    }

    N = atoi(argv[2]);
    if (N < 16) {
        g_printerr("Invalid DATA-SIZE %d\n", N);
        return 1;
    }

    gwy_process_type_init();
    rng = g_rand_new();
    g_rand_set_seed(rng, 42);

    dfield = gwy_data_field_new(N, N, N*dx, N*dx, FALSE);
    data = gwy_data_field_get_data(dfield);
    for (i = 0; i < N*N; i++)
        data[i] = 2*G_PI*sigma*T/dx*g_rand_double(rng);

    gwy_data_field_filter_gaussian(dfield, 0.5*T/dx);

    dline = gwy_data_line_new(1, 1, FALSE);
    if (mode == PSDF) {
        gwy_data_field_psdf(dfield, dline,
                            GWY_ORIENTATION_HORIZONTAL,
                            GWY_INTERPOLATION_KEY,
                            GWY_WINDOWING_BLACKMANN,
                            -1);
        gwy_data_line_resize(dline, 0, gwy_data_line_get_res(dline)/2);
        presetname = "Gaussian (PSDF)";
    }
    else if (mode == ACF) {
        gwy_data_field_acf(dfield, dline,
                           GWY_ORIENTATION_HORIZONTAL,
                           GWY_INTERPOLATION_KEY,
                           -1);
        gwy_data_line_resize(dline, 0, gwy_data_line_get_res(dline)/12);
        presetname = "Gaussian (ACF)";
    }
    else if (mode == HHCF) {
        gwy_data_field_hhcf(dfield, dline,
                            GWY_ORIENTATION_HORIZONTAL,
                            GWY_INTERPOLATION_KEY,
                            -1);
        gwy_data_line_resize(dline, 0, gwy_data_line_get_res(dline)/12);
        presetname = "Gaussian (HHCF)";
    }
    else {
        g_assert_not_reached();
    }

    n = gwy_data_line_get_res(dline);
    r = gwy_data_line_get_real(dline);
    data = gwy_data_line_get_data(dline);
    xdata = g_new(gdouble, n);
    for (i = 0; i < n; i++)
        xdata[i] = i*r/n;

    preset = gwy_inventory_get_item(gwy_nlfit_presets(), presetname);
    g_assert(preset);
    params = g_new(gdouble, gwy_nlfit_preset_get_nparams(preset));
    errors = g_new(gdouble, gwy_nlfit_preset_get_nparams(preset));
    gwy_nlfit_preset_guess(preset, n, xdata, data, params, &ok);
    g_assert(ok);
    fitter = gwy_nlfit_preset_fit(preset, NULL, n, xdata, data, params, errors,
                                  NULL);

    fprintf(stderr, "sigma = %g, T = %g\n", params[0], params[1]);
    for (i = 0; i < n; i++) {
        printf("%g %g %g\n",
               xdata[i], data[i],
               gwy_nlfit_preset_get_value(preset, xdata[i], params, &ok));
        g_assert(ok);
    }

    g_rand_free(rng);

    return 0;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
