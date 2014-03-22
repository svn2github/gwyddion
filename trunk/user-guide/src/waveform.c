#include <stdio.h>
#include <string.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/gwyprocess.h>

/* The functions are copied from wave_synth.c. */
enum {
    APPROX_WAVE_BITS = 10,
    APPROX_WAVE_SIZE = 1 << APPROX_WAVE_BITS,
    APPROX_WAVE_MASK = APPROX_WAVE_SIZE - 1,
};

typedef enum {
    WAVE_TYPE_COSINE  = 0,
    WAVE_TYPE_INVCOSH = 1,
    WAVE_TYPE_FLATTOP = 2,
    WAVE_TYPE_NTYPES
} WaveTypeType;

static void
complement_wave(const gdouble *cwave, gdouble *swave, guint n)
{
    gdouble *buf1 = g_new(gdouble, 3*n);
    gdouble *buf2 = buf1 + n;
    gdouble *buf3 = buf2 + n;
    guint i;

    gwy_clear(swave, n);
    gwy_fft_simple(GWY_TRANSFORM_DIRECTION_FORWARD, n,
                   1, cwave, swave,
                   1, buf1, buf2);
    for (i = 0; i < n/2; i++) {
        gdouble t = buf2[i];
        buf2[i] = buf1[i];
        buf1[i] = t;
    }
    for (i = n/2; i < n; i++) {
        gdouble t = buf2[i];
        buf2[i] = -buf1[i];
        buf1[i] = t;
    }
    gwy_fft_simple(GWY_TRANSFORM_DIRECTION_BACKWARD, n,
                   1, buf1, buf2,
                   1, swave, buf3);
    g_free(buf1);
}

static void
complement_table(gdouble *dbltab, gfloat *tab, guint n)
{
    guint i;
    gdouble s = 0.0, s2 = 0.0;

    for (i = 0; i < n; i++)
        s += dbltab[i];
    s /= n;

    for (i = 0; i < n; i++) {
        dbltab[i] -= s;
        s2 += dbltab[i]*dbltab[i];
    }
    s2 = sqrt(s2/n);

    complement_wave(dbltab, dbltab + n, n);
    for (i = 0; i < 2*n; i++)
        tab[i] = dbltab[i]/s2;
}

static void
precalculate_wave_table(gfloat *tab, guint n, WaveTypeType type)
{
    guint i;

    if (type == WAVE_TYPE_COSINE) {
        for (i = 0; i < n; i++) {
            gdouble x = (i + 0.5)/n * 2.0*G_PI;
            tab[i] = cos(x);
            tab[i + n] = sin(x);
        }
    }
    else if (type == WAVE_TYPE_INVCOSH) {
        gdouble *dbltab = g_new(gdouble, 2*n);

        for (i = 0; i < n; i++) {
            gdouble x = (i + 0.5)/n * 10.0;
            dbltab[i] = 1.0/cosh(x) + 1.0/cosh(10.0 - x);
        }

        complement_table(dbltab, tab, n);
        g_free(dbltab);
    }
    else if (type == WAVE_TYPE_FLATTOP) {
        for (i = 0; i < n; i++) {
            gdouble x = (i + 0.5)/n * 2.0*G_PI;
            tab[i] = cos(x) - cos(3*x)/6 + cos(5*x)/50;
            tab[i + n] = sin(x) - sin(3*x)/6 + sin(5*x)/50;
        }
    }
    else {
        g_return_if_reached();
    }
}

int
main(int argc, char *argv[])
{
    static const GwyEnum wave_types[] = {
        { N_("cosine"),  WAVE_TYPE_COSINE,  },
        { N_("invcosh"), WAVE_TYPE_INVCOSH, },
        { N_("flattop"), WAVE_TYPE_FLATTOP, },
    };

    WaveTypeType type;
    gfloat *tab;
    guint i;

    if (argc != 2) {
        g_printerr("Usage: %s WAVEFORM-NAME\n", argv[0]);
        return 1;
    }

    gwy_process_type_init();

    type = gwy_string_to_enum(argv[1], wave_types, G_N_ELEMENTS(wave_types));
    if (type == (WaveTypeType)-1) {
        g_printerr("Unknown waveform name %s\n", argv[1]);
        return 1;
    }

    tab = g_new(gfloat, 2*APPROX_WAVE_SIZE);
    precalculate_wave_table(tab, APPROX_WAVE_SIZE, type);
    for (i = 0; i < APPROX_WAVE_SIZE; i++) {
        printf("%g %g %g\n",
                (i + 0.5)/APPROX_WAVE_SIZE,
                tab[i], tab[i + APPROX_WAVE_SIZE]);
    }

    return 0;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
