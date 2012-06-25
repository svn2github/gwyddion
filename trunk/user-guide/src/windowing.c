#include <stdio.h>
#include <string.h>
#include <fftw3.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/gwyprocess.h>

#define WISDOM_NAME "fftw-wisdom.dat"

enum { EXTEND = 400 };

static void
smooth2(GwyDataLine *xline, GwyDataLine *yline)
{
    gdouble *xdata, *ydata;
    gint i, j, n;

    n = gwy_data_line_get_res(xline);
    g_assert(gwy_data_line_get_res(yline) == n);
    xdata = gwy_data_line_get_data(xline);
    ydata = gwy_data_line_get_data(yline);
    for (i = j = 0; i < n; i++) {
        if ((i == 0 || ydata[i] > ydata[i-1])
            || (i == n-1 || ydata[i] > ydata[i+1])) {
            ydata[j] = ydata[i];
            xdata[j] = xdata[i];
            j++;
        }
        else {
            ydata[j] = ydata[i];
            xdata[j] = xdata[i];
        }
    }
    gwy_data_line_resize(xline, 0, j);
    gwy_data_line_resize(yline, 0, j);
}

int
main(int argc, char *argv[])
{
    const GwyEnum *wtypes;
    guint i, j, n, npt = 60000;
    GwyDataLine *line, *zero, *rout, *iout, *xdata;
    const gchar *name, *cmpname;
    gboolean do_fft;
    FILE *fh;

    if (argc != 3) {
        g_printerr("Usage: %s {win|fft} WINDOW-NAME\n", argv[0]);
        return 1;
    }

    do_fft = gwy_strequal(argv[1], "fft");

    gwy_process_type_init();
    wtypes = gwy_windowing_type_get_enum();
    name = cmpname = argv[2];
    if (gwy_strequal(name, "Kaiser-2.5"))
        cmpname = "Kaiser 2.5";
    for (i = 0; wtypes[i].name; i++) {
        const gchar *wname = gwy_sgettext(wtypes[i].name);
        if (gwy_strequal(cmpname, wname))
            break;
    }
    if (!wtypes[i].name) {
        g_printerr("Unknown type %s\n", name);
        return 1;
    }

    line = gwy_data_line_new(npt, 1.0, FALSE);
    gwy_data_line_fill(line, 1.0);
    zero = gwy_data_line_new_alike(line, TRUE);
    rout = gwy_data_line_new_alike(line, FALSE);
    iout = gwy_data_line_new_alike(line, FALSE);
    xdata = gwy_data_line_new_alike(line, FALSE);

    gwy_data_line_part_clear(line, 0, (EXTEND-1)*npt/(2*EXTEND));
    gwy_data_line_part_clear(line, (EXTEND+1)*npt/(2*EXTEND), npt);
    gwy_fft_window(npt/EXTEND,
                   gwy_data_line_get_data(line) + (EXTEND-1)*npt/(2*EXTEND),
                   i);

    if (do_fft) {
        double prevx = -1.0, prevy = -1.0;

        fh = fopen(WISDOM_NAME, "rb");
        if (fh) {
            fftw_import_wisdom_from_file(fh);
            fclose(fh);
        }

        gwy_data_line_fft_raw(line, zero, rout, iout,
                              GWY_TRANSFORM_DIRECTION_FORWARD);
        for (j = 0; j < npt; j++)
            gwy_data_line_set_val(line, (j + npt/2) % npt,
                                  hypot(gwy_data_line_get_val(rout, j),
                                        gwy_data_line_get_val(iout, j)));

        gwy_data_line_resample(xdata, npt, GWY_INTERPOLATION_NONE);
        for (j = 0; j < npt; j++)
            gwy_data_line_set_val(xdata, j, (gdouble)j/npt);

        for (j = 0; j < EXTEND/2; j++)
            smooth2(xdata, line);

        n = gwy_data_line_get_res(xdata);
        for (j = 0; j < n; j++) {
            gdouble x = gwy_data_line_get_val(xdata, j);
            gdouble y = gwy_data_line_get_val(line, j);

            if (x < 0.001 || x > 0.998)
                continue;

            if (fabs(x - prevx) < 0.005 && fabs(y - prevy) < 0.005)
                continue;

            printf("%g %g\n", 2.0*x - 1.0, y);
            prevx = x;
            prevy = y;
        }

        fh = fopen(WISDOM_NAME, "wb");
        if (fh) {
            fftw_export_wisdom_to_file(fh);
            fclose(fh);
        }
    }
    else {
        for (j = 0; j < npt/EXTEND; j++) {
            printf("%g %g\n", (gdouble)j*EXTEND/npt,
                   gwy_data_line_get_val(line, j + (EXTEND-1)*npt/(2*EXTEND)));
        }
    }

    g_object_unref(xdata);
    g_object_unref(zero);
    g_object_unref(rout);
    g_object_unref(iout);
    g_object_unref(line);

    return 0;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
