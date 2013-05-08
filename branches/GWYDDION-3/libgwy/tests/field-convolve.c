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

#include "testlibgwy.h"

/***************************************************************************
 *
 * Field convolutions and exterior
 *
 ***************************************************************************/

static gdouble
exterior_value_dumb(const gdouble *data,
                    guint size,
                    guint stride,
                    gint pos,
                    GwyExteriorType exterior,
                    gdouble fill_value)
{
    // Interior
    if (pos >= 0 && (guint)pos < size)
        return data[stride*pos];

    if (exterior == GWY_EXTERIOR_UNDEFINED)
        return NAN;

    if (exterior == GWY_EXTERIOR_FIXED_VALUE)
        return fill_value;

    if (exterior == GWY_EXTERIOR_BORDER_EXTEND) {
        pos = CLAMP(pos, 0, (gint)size-1);
        return data[stride*pos];
    }

    if (exterior == GWY_EXTERIOR_PERIODIC) {
        pos = (pos + 10000*size) % size;
        return data[stride*pos];
    }

    if (exterior == GWY_EXTERIOR_MIRROR_EXTEND) {
        guint p = (pos + 10000*2*size) % (2*size);
        return data[stride*(p < size ? p : 2*size-1 - p)];
    }

    g_return_val_if_reached(NAN);
}

static gdouble
exterior_value_dumb_2d(GwyField *field,
                       gint xpos, gint ypos,
                       GwyExteriorType exterior,
                       gdouble fill_value)
{
    // Interior
    if (xpos >= 0 && xpos < (gint)field->xres
        && ypos >= 0 && ypos < (gint)field->yres)
        return field->data[xpos + ypos*field->xres];

    if (exterior == GWY_EXTERIOR_UNDEFINED)
        return NAN;

    if (exterior == GWY_EXTERIOR_FIXED_VALUE)
        return fill_value;

    if (exterior == GWY_EXTERIOR_BORDER_EXTEND) {
        xpos = CLAMP(xpos, 0, (gint)field->xres-1);
        ypos = CLAMP(ypos, 0, (gint)field->yres-1);
        return field->data[xpos + ypos*field->xres];
    }

    if (exterior == GWY_EXTERIOR_PERIODIC) {
        xpos = (xpos + 10000*field->xres) % field->xres;
        ypos = (ypos + 10000*field->yres) % field->yres;
        return field->data[xpos + ypos*field->xres];
    }

    if (exterior == GWY_EXTERIOR_MIRROR_EXTEND) {
        xpos = (xpos + 10000*2*field->xres) % (2*field->xres);
        if (xpos >= (gint)field->xres)
            xpos = 2*field->xres-1 - xpos;
        ypos = (ypos + 10000*2*field->yres) % (2*field->yres);
        if (ypos >= (gint)field->yres)
            ypos = 2*field->yres-1 - ypos;
        return field->data[xpos + ypos*field->xres];
    }

    g_return_val_if_reached(NAN);
}

// The behaviour for interiors should not depend on the exterior type
static void
field_convolve_row_interior_one(GwyExteriorType exterior)
{
    enum { yres = 3 };
    GRand *rng = g_rand_new_with_seed(42);

    for (guint res = 1; res <= 30; res++) {
        GwyField *source = gwy_field_new_sized(res, yres, FALSE);
        field_randomize(source, rng);

        GwyField *field = gwy_field_new_alike(source, FALSE);
        for (guint kres = 1; kres <= res; kres++) {
            guint k0 = (kres - 1)/2;
            GwyLine *kernel = gwy_line_new_sized(kres, TRUE);

            GwyFieldPart fpart = { k0, 0, res - kres/2 - k0, yres };
            for (guint pos = 0; pos < kres; pos++) {
                gwy_line_clear(kernel, NULL);
                kernel->data[pos] = 1.0;

                g_assert_cmpuint(fpart.col + fpart.width, <=, res);
                gwy_field_copy_full(source, field);
                gwy_field_row_convolve(field, &fpart, field, kernel,
                                       exterior, G_E);

                // Everything outside @fpart must be equal to @source.
                // Data inside @fpart must be equal to @source data shifted
                // to the right by kres-1 - pos - (kres-1)/2.
                guint shift = (kres-1 - pos) - k0;
                for (guint i = 0; i < yres; i++) {
                    const gdouble *srow = source->data + i*res;
                    const gdouble *drow = field->data + i*res;
                    for (guint j = 0; j < res; j++) {
                        // Outside
                        if (j < fpart.col
                            || j >= fpart.col + fpart.width)
                            g_assert_cmpfloat(drow[j], ==, srow[j]);
                        // Inside
                        else {
                            g_assert_cmpfloat(fabs(drow[j] - srow[j + shift]),
                                              <, 1e-14);
                        }
                    }
                }
            }
            g_object_unref(kernel);
        }
        g_object_unref(field);
        g_object_unref(source);
    }
    g_rand_free(rng);
}

static void
field_convolve_row_exterior_one(GwyExteriorType exterior)
{
    enum { yres = 3 };
    GRand *rng = g_rand_new_with_seed(42);

    for (guint res = 1; res <= 30; res++) {
        guint niter = g_test_slow() ? MAX(4, 3*res*res) : MAX(4, res*res/2);

        GwyField *source = gwy_field_new_sized(res, yres, FALSE);
        field_randomize(source, rng);

        GwyField *field = gwy_field_new_alike(source, FALSE);
        for (guint iter = 0; iter < niter; iter++) {
            guint kres = g_rand_int_range(rng, 1, 2*res);
            guint k0 = (kres - 1)/2;
            GwyLine *kernel = gwy_line_new_sized(kres, TRUE);
            guint width = g_rand_int_range(rng, 0, res+1);
            guint col = g_rand_int_range(rng, 0, res-width+1);
            guint pos = g_rand_int_range(rng, 0, kres);
            GwyFieldPart fpart = { col, 0, width, yres };

            gwy_line_clear(kernel, NULL);
            kernel->data[pos] = 1.0;
            gwy_field_copy_full(source, field);
            gwy_field_row_convolve(field, &fpart, field, kernel,
                                   exterior, G_E);

            // Everything outside @fpart must be equal to @source.
            // Data inside @fpart must be equal to @source data shifted
            // to the right by kres-1 - pos - (kres-1)/2, possibly extended.
            gint shift = ((gint)kres-1 - (gint)pos) - (gint)k0;
            for (guint i = 0; i < yres; i++) {
                const gdouble *srow = source->data + i*res;
                const gdouble *drow = field->data + i*res;
                for (guint j = 0; j < res; j++) {
                    // Outside
                    if (j < fpart.col
                        || j >= fpart.col + fpart.width)
                        g_assert_cmpfloat(drow[j], ==, srow[j]);
                    // Inside
                    else {
                        gdouble refval = exterior_value_dumb(srow, res, 1,
                                                             (gint)j + shift,
                                                             exterior, G_E);
                        gwy_assert_floatval(drow[j], refval, 1e-14);
                    }
                }
            }
            g_object_unref(kernel);
        }
        g_object_unref(field);
        g_object_unref(source);
    }
    g_rand_free(rng);
}

void
test_field_convolve_row_interior_mirror_direct(void)
{
    gwy_tune_algorithms("convolution-method", "direct");
    field_convolve_row_interior_one(GWY_EXTERIOR_MIRROR_EXTEND);
    gwy_tune_algorithms("convolution-method", "auto");
}

void
test_field_convolve_row_interior_mirror_fft(void)
{
    gwy_tune_algorithms("convolution-method", "fft");
    gwy_fft_load_wisdom();
    field_convolve_row_interior_one(GWY_EXTERIOR_MIRROR_EXTEND);
    gwy_tune_algorithms("convolution-method", "auto");
}

void
test_field_convolve_row_interior_border_direct(void)
{
    gwy_tune_algorithms("convolution-method", "direct");
    field_convolve_row_interior_one(GWY_EXTERIOR_BORDER_EXTEND);
    gwy_tune_algorithms("convolution-method", "auto");
}

void
test_field_convolve_row_interior_border_fft(void)
{
    gwy_tune_algorithms("convolution-method", "fft");
    gwy_fft_load_wisdom();
    field_convolve_row_interior_one(GWY_EXTERIOR_BORDER_EXTEND);
    gwy_tune_algorithms("convolution-method", "auto");
}

void
test_field_convolve_row_interior_periodic_direct(void)
{
    gwy_tune_algorithms("convolution-method", "direct");
    field_convolve_row_interior_one(GWY_EXTERIOR_PERIODIC);
    gwy_tune_algorithms("convolution-method", "auto");
}

void
test_field_convolve_row_interior_periodic_fft(void)
{
    gwy_tune_algorithms("convolution-method", "fft");
    gwy_fft_load_wisdom();
    field_convolve_row_interior_one(GWY_EXTERIOR_PERIODIC);
    gwy_tune_algorithms("convolution-method", "auto");
}

void
test_field_convolve_row_interior_fixed_direct(void)
{
    gwy_tune_algorithms("convolution-method", "direct");
    field_convolve_row_interior_one(GWY_EXTERIOR_FIXED_VALUE);
    gwy_tune_algorithms("convolution-method", "auto");
}

void
test_field_convolve_row_interior_fixed_fft(void)
{
    gwy_tune_algorithms("convolution-method", "fft");
    gwy_fft_load_wisdom();
    field_convolve_row_interior_one(GWY_EXTERIOR_FIXED_VALUE);
    gwy_tune_algorithms("convolution-method", "auto");
}

void
test_field_convolve_row_exterior_mirror_direct(void)
{
    gwy_tune_algorithms("convolution-method", "direct");
    field_convolve_row_exterior_one(GWY_EXTERIOR_MIRROR_EXTEND);
    gwy_tune_algorithms("convolution-method", "auto");
}

void
test_field_convolve_row_exterior_mirror_fft(void)
{
    gwy_tune_algorithms("convolution-method", "fft");
    gwy_fft_load_wisdom();
    field_convolve_row_exterior_one(GWY_EXTERIOR_MIRROR_EXTEND);
    gwy_tune_algorithms("convolution-method", "auto");
}

void
test_field_convolve_row_exterior_border_direct(void)
{
    gwy_tune_algorithms("convolution-method", "direct");
    field_convolve_row_exterior_one(GWY_EXTERIOR_BORDER_EXTEND);
    gwy_tune_algorithms("convolution-method", "auto");
}

void
test_field_convolve_row_exterior_border_fft(void)
{
    gwy_tune_algorithms("convolution-method", "fft");
    gwy_fft_load_wisdom();
    field_convolve_row_exterior_one(GWY_EXTERIOR_BORDER_EXTEND);
    gwy_tune_algorithms("convolution-method", "auto");
}

void
test_field_convolve_row_exterior_periodic_direct(void)
{
    gwy_tune_algorithms("convolution-method", "direct");
    field_convolve_row_exterior_one(GWY_EXTERIOR_PERIODIC);
    gwy_tune_algorithms("convolution-method", "auto");
}

void
test_field_convolve_row_exterior_periodic_fft(void)
{
    gwy_tune_algorithms("convolution-method", "fft");
    gwy_fft_load_wisdom();
    field_convolve_row_exterior_one(GWY_EXTERIOR_PERIODIC);
    gwy_tune_algorithms("convolution-method", "auto");
}

void
test_field_convolve_row_exterior_fixed_direct(void)
{
    gwy_tune_algorithms("convolution-method", "direct");
    field_convolve_row_exterior_one(GWY_EXTERIOR_FIXED_VALUE);
    gwy_tune_algorithms("convolution-method", "auto");
}

void
test_field_convolve_row_exterior_fixed_fft(void)
{
    gwy_tune_algorithms("convolution-method", "fft");
    gwy_fft_load_wisdom();
    field_convolve_row_exterior_one(GWY_EXTERIOR_FIXED_VALUE);
    gwy_tune_algorithms("convolution-method", "auto");
}

static void
field_convolve_simple_one(const GwyField *source,
                          const GwyField *kernel,
                          const GwyField *reference,
                          GwyExteriorType exterior)
{
    GwyField *field = gwy_field_new_alike(source, FALSE);
    gwy_field_convolve(source, NULL, field, kernel, exterior, G_E);
    field_assert_numerically_equal(field, reference, 1e-14);
    g_object_unref(field);
}

static GwyField*
field_make_deltafunction(guint xres, guint yres, guint xpos, guint ypos)
{
    GwyField *field = gwy_field_new_sized(xres, yres, TRUE);
    field->data[ypos*xres + xpos] = 1.0;
    gwy_field_invalidate(field);
    return field;
}

static void
field_convolve_field_simple_odd_odd(void)
{
    GwyField *field0 = field_make_deltafunction(3, 3, 0, 0);
    GwyField *field1 = field_make_deltafunction(3, 3, 1, 0);
    GwyField *field2 = field_make_deltafunction(3, 3, 2, 0);
    GwyField *field3 = field_make_deltafunction(3, 3, 0, 1);
    GwyField *field4 = field_make_deltafunction(3, 3, 1, 1);
    GwyField *field5 = field_make_deltafunction(3, 3, 2, 1);
    GwyField *field6 = field_make_deltafunction(3, 3, 0, 2);
    GwyField *field7 = field_make_deltafunction(3, 3, 1, 2);
    GwyField *field8 = field_make_deltafunction(3, 3, 2, 2);

    field_convolve_simple_one(field0, field0, field8, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field0, field1, field6, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field0, field2, field7, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field0, field3, field2, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field0, field4, field0, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field0, field5, field1, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field0, field6, field5, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field0, field7, field3, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field0, field8, field4, GWY_EXTERIOR_PERIODIC);

    field_convolve_simple_one(field1, field0, field6, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field1, field1, field7, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field1, field2, field8, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field1, field3, field0, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field1, field4, field1, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field1, field5, field2, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field1, field6, field3, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field1, field7, field4, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field1, field8, field5, GWY_EXTERIOR_PERIODIC);

    field_convolve_simple_one(field2, field0, field7, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field2, field1, field8, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field2, field2, field6, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field2, field3, field1, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field2, field4, field2, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field2, field5, field0, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field2, field6, field4, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field2, field7, field5, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field2, field8, field3, GWY_EXTERIOR_PERIODIC);

    field_convolve_simple_one(field3, field0, field2, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field3, field1, field0, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field3, field2, field1, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field3, field3, field5, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field3, field4, field3, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field3, field5, field4, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field3, field6, field8, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field3, field7, field6, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field3, field8, field7, GWY_EXTERIOR_PERIODIC);

    field_convolve_simple_one(field4, field0, field0, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field4, field1, field1, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field4, field2, field2, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field4, field3, field3, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field4, field4, field4, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field4, field5, field5, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field4, field6, field6, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field4, field7, field7, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field4, field8, field8, GWY_EXTERIOR_PERIODIC);

    field_convolve_simple_one(field5, field0, field1, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field5, field1, field2, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field5, field2, field0, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field5, field3, field4, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field5, field4, field5, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field5, field5, field3, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field5, field6, field7, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field5, field7, field8, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field5, field8, field6, GWY_EXTERIOR_PERIODIC);

    field_convolve_simple_one(field6, field0, field5, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field6, field1, field3, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field6, field2, field4, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field6, field3, field8, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field6, field4, field6, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field6, field5, field7, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field6, field6, field2, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field6, field7, field0, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field6, field8, field1, GWY_EXTERIOR_PERIODIC);

    field_convolve_simple_one(field7, field0, field3, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field7, field1, field4, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field7, field2, field5, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field7, field3, field6, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field7, field4, field7, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field7, field5, field8, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field7, field6, field0, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field7, field7, field1, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field7, field8, field2, GWY_EXTERIOR_PERIODIC);

    field_convolve_simple_one(field8, field0, field4, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field8, field1, field5, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field8, field2, field3, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field8, field3, field7, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field8, field4, field8, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field8, field5, field6, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field8, field6, field1, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field8, field7, field2, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field8, field8, field0, GWY_EXTERIOR_PERIODIC);

    g_object_unref(field8);
    g_object_unref(field7);
    g_object_unref(field6);
    g_object_unref(field5);
    g_object_unref(field4);
    g_object_unref(field3);
    g_object_unref(field2);
    g_object_unref(field1);
    g_object_unref(field0);
}

static void
field_convolve_field_simple_even_even(void)
{
    GwyField *field0 = field_make_deltafunction(2, 2, 0, 0);
    GwyField *field1 = field_make_deltafunction(2, 2, 1, 0);
    GwyField *field2 = field_make_deltafunction(2, 2, 0, 1);
    GwyField *field3 = field_make_deltafunction(2, 2, 1, 1);

    field_convolve_simple_one(field0, field0, field3, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field0, field1, field2, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field0, field2, field1, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field0, field3, field0, GWY_EXTERIOR_PERIODIC);

    field_convolve_simple_one(field1, field0, field2, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field1, field1, field3, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field1, field2, field0, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field1, field3, field1, GWY_EXTERIOR_PERIODIC);

    field_convolve_simple_one(field2, field0, field1, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field2, field1, field0, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field2, field2, field3, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field2, field3, field2, GWY_EXTERIOR_PERIODIC);

    field_convolve_simple_one(field3, field0, field0, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field3, field1, field1, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field3, field2, field2, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field3, field3, field3, GWY_EXTERIOR_PERIODIC);

    g_object_unref(field3);
    g_object_unref(field2);
    g_object_unref(field1);
    g_object_unref(field0);
}

static void
field_convolve_field_simple_odd_even(void)
{
    GwyField *field0 = field_make_deltafunction(3, 3, 0, 0);
    GwyField *field1 = field_make_deltafunction(3, 3, 1, 0);
    GwyField *field2 = field_make_deltafunction(3, 3, 2, 0);
    GwyField *field3 = field_make_deltafunction(3, 3, 0, 1);
    GwyField *field4 = field_make_deltafunction(3, 3, 1, 1);
    GwyField *field5 = field_make_deltafunction(3, 3, 2, 1);
    GwyField *field6 = field_make_deltafunction(3, 3, 0, 2);
    GwyField *field7 = field_make_deltafunction(3, 3, 1, 2);
    GwyField *field8 = field_make_deltafunction(3, 3, 2, 2);
    GwyField *kernel0 = field_make_deltafunction(2, 2, 0, 0);
    GwyField *kernel1 = field_make_deltafunction(2, 2, 1, 0);
    GwyField *kernel2 = field_make_deltafunction(2, 2, 0, 1);
    GwyField *kernel3 = field_make_deltafunction(2, 2, 1, 1);

    field_convolve_simple_one(field0, kernel0, field8, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field0, kernel1, field6, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field0, kernel2, field2, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field0, kernel3, field0, GWY_EXTERIOR_PERIODIC);

    field_convolve_simple_one(field1, kernel0, field6, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field1, kernel1, field7, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field1, kernel2, field0, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field1, kernel3, field1, GWY_EXTERIOR_PERIODIC);

    field_convolve_simple_one(field2, kernel0, field7, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field2, kernel1, field8, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field2, kernel2, field1, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field2, kernel3, field2, GWY_EXTERIOR_PERIODIC);

    field_convolve_simple_one(field3, kernel0, field2, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field3, kernel1, field0, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field3, kernel2, field5, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field3, kernel3, field3, GWY_EXTERIOR_PERIODIC);

    field_convolve_simple_one(field4, kernel0, field0, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field4, kernel1, field1, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field4, kernel2, field3, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field4, kernel3, field4, GWY_EXTERIOR_PERIODIC);

    field_convolve_simple_one(field5, kernel0, field1, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field5, kernel1, field2, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field5, kernel2, field4, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field5, kernel3, field5, GWY_EXTERIOR_PERIODIC);

    field_convolve_simple_one(field6, kernel0, field5, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field6, kernel1, field3, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field6, kernel2, field8, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field6, kernel3, field6, GWY_EXTERIOR_PERIODIC);

    field_convolve_simple_one(field7, kernel0, field3, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field7, kernel1, field4, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field7, kernel2, field6, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field7, kernel3, field7, GWY_EXTERIOR_PERIODIC);

    field_convolve_simple_one(field8, kernel0, field4, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field8, kernel1, field5, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field8, kernel2, field7, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field8, kernel3, field8, GWY_EXTERIOR_PERIODIC);

    g_object_unref(kernel3);
    g_object_unref(kernel2);
    g_object_unref(kernel1);
    g_object_unref(kernel0);
    g_object_unref(field8);
    g_object_unref(field7);
    g_object_unref(field6);
    g_object_unref(field5);
    g_object_unref(field4);
    g_object_unref(field3);
    g_object_unref(field2);
    g_object_unref(field1);
    g_object_unref(field0);
}

void
test_field_convolve_field_simple_odd_odd_direct(void)
{
    gwy_tune_algorithms("convolution-method", "direct");
    field_convolve_field_simple_odd_odd();
    gwy_tune_algorithms("convolution-method", "auto");
}

void
test_field_convolve_field_simple_odd_odd_fft(void)
{
    gwy_tune_algorithms("convolution-method", "fft");
    field_convolve_field_simple_odd_odd();
    gwy_tune_algorithms("convolution-method", "auto");
}

void
test_field_convolve_field_simple_even_even_direct(void)
{
    gwy_tune_algorithms("convolution-method", "direct");
    field_convolve_field_simple_even_even();
    gwy_tune_algorithms("convolution-method", "auto");
}

void
test_field_convolve_field_simple_even_even_fft(void)
{
    gwy_tune_algorithms("convolution-method", "fft");
    field_convolve_field_simple_even_even();
    gwy_tune_algorithms("convolution-method", "auto");
}

void
test_field_convolve_field_simple_odd_even_direct(void)
{
    gwy_tune_algorithms("convolution-method", "direct");
    field_convolve_field_simple_odd_even();
    gwy_tune_algorithms("convolution-method", "auto");
}

void
test_field_convolve_field_simple_odd_even_fft(void)
{
    gwy_tune_algorithms("convolution-method", "fft");
    field_convolve_field_simple_odd_even();
    gwy_tune_algorithms("convolution-method", "auto");
}

static void
field_convolve_field_one(GwyExteriorType exterior)
{
    enum { max_size = 30, niter = 200 };

    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        guint width = g_rand_int_range(rng, 1, xres+1);
        guint height = g_rand_int_range(rng, 1, yres+1);
        guint col = g_rand_int_range(rng, 0, xres-width+1);
        guint row = g_rand_int_range(rng, 0, yres-height+1);
        guint kxres = g_rand_int_range(rng, 1, max_size);
        guint kyres = g_rand_int_range(rng, 1, max_size);
        guint xpos = g_rand_int_range(rng, 0, kxres);
        guint ypos = g_rand_int_range(rng, 0, kyres);
        guint kx0 = (kxres - 1)/2;
        guint ky0 = (kyres - 1)/2;

        GwyField *source = gwy_field_new_sized(xres, yres, FALSE);
        field_randomize(source, rng);
        GwyField *field = gwy_field_new_alike(source, FALSE);
        gwy_field_copy_full(source, field);
        GwyField *kernel = gwy_field_new_sized(kxres, kyres, TRUE);
        kernel->data[xpos + ypos*kxres] = 1.0;
        gwy_field_invalidate(kernel);
        GwyFieldPart fpart = { col, row, width, height };
        gwy_field_convolve(field, &fpart, field, kernel, exterior, G_E);

        // Everything outside @fpart must be equal to @source.
        // Data inside @fpart must be equal to @source data shifted
        // down by kyres-1 - ypos - (ykres-1)/2
        // and to the right by kxres-1 - xpos - (kxres-1)/2
        gint xshift = ((gint)kxres-1 - (gint)xpos) - (gint)kx0;
        gint yshift = ((gint)kyres-1 - (gint)ypos) - (gint)ky0;
        for (guint i = 0; i < yres; i++) {
            const gdouble *srow = source->data + i*xres;
            const gdouble *drow = field->data + i*xres;
            for (guint j = 0; j < xres; j++) {
                // Outside
                if (j < fpart.col
                    || j >= fpart.col + fpart.width
                    || i < fpart.row
                    || i >= fpart.row + fpart.height) {
                    g_assert_cmpfloat(drow[j], ==, srow[j]);
                }
                // Inside
                else {
                    if (exterior == GWY_EXTERIOR_UNDEFINED)
                        continue;
                    gdouble refval = exterior_value_dumb_2d(source,
                                                            (gint)j + xshift,
                                                            (gint)i + yshift,
                                                            exterior, G_E);
                    gwy_assert_floatval(drow[j], refval, 1e-14);
                }
            }
        }
        g_object_unref(kernel);
        g_object_unref(field);
        g_object_unref(source);
    }
    g_rand_free(rng);
}

void
test_field_convolve_field_mirror_direct(void)
{
    gwy_tune_algorithms("convolution-method", "direct");
    field_convolve_field_one(GWY_EXTERIOR_MIRROR_EXTEND);
    gwy_tune_algorithms("convolution-method", "auto");
}

void
test_field_convolve_field_mirror_fft(void)
{
    gwy_tune_algorithms("convolution-method", "fft");
    field_convolve_field_one(GWY_EXTERIOR_MIRROR_EXTEND);
    gwy_tune_algorithms("convolution-method", "auto");
}

void
test_field_convolve_field_border_direct(void)
{
    gwy_tune_algorithms("convolution-method", "direct");
    field_convolve_field_one(GWY_EXTERIOR_BORDER_EXTEND);
    gwy_tune_algorithms("convolution-method", "auto");
}

void
test_field_convolve_field_border_fft(void)
{
    gwy_tune_algorithms("convolution-method", "fft");
    field_convolve_field_one(GWY_EXTERIOR_BORDER_EXTEND);
    gwy_tune_algorithms("convolution-method", "auto");
}

void
test_field_convolve_field_periodic_direct(void)
{
    gwy_tune_algorithms("convolution-method", "direct");
    field_convolve_field_one(GWY_EXTERIOR_PERIODIC);
    gwy_tune_algorithms("convolution-method", "auto");
}

void
test_field_convolve_field_periodic_fft(void)
{
    gwy_tune_algorithms("convolution-method", "fft");
    field_convolve_field_one(GWY_EXTERIOR_PERIODIC);
    gwy_tune_algorithms("convolution-method", "auto");
}

void
test_field_convolve_field_fixed_direct(void)
{
    gwy_tune_algorithms("convolution-method", "direct");
    field_convolve_field_one(GWY_EXTERIOR_FIXED_VALUE);
    gwy_tune_algorithms("convolution-method", "auto");
}

void
test_field_convolve_field_fixed_fft(void)
{
    gwy_tune_algorithms("convolution-method", "fft");
    field_convolve_field_one(GWY_EXTERIOR_FIXED_VALUE);
    gwy_tune_algorithms("convolution-method", "auto");
}

static void
field_extend_one(GwyExteriorType exterior)
{
    enum { max_size = 35 };

    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = g_test_slow() ? 300 : 100;

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        guint left = g_rand_int_range(rng, 0, 2*xres);
        guint right = g_rand_int_range(rng, 0, 2*xres);
        guint up = g_rand_int_range(rng, 0, 2*yres);
        guint down = g_rand_int_range(rng, 0, 2*yres);
        guint width = g_rand_int_range(rng, 1, xres+1);
        guint height = g_rand_int_range(rng, 1, yres+1);
        guint col = g_rand_int_range(rng, 0, xres-width+1);
        guint row = g_rand_int_range(rng, 0, yres-height+1);

        GwyField *source = gwy_field_new_sized(xres, yres, FALSE);
        field_randomize(source, rng);

        GwyFieldPart fpart = { col, row, width, height };
        GwyField *field = gwy_field_new_extended(source, &fpart,
                                                 left, right, up, down,
                                                 exterior, G_E, TRUE);

        g_assert_cmpuint(field->xres, ==, width + left + right);
        g_assert_cmpuint(field->yres, ==, height + up + down);
        g_assert_cmpfloat(fabs(log(gwy_field_dx(field)/gwy_field_dx(source))),
                          <=, 1e-14);
        g_assert_cmpfloat(fabs(log(gwy_field_dy(field)/gwy_field_dy(source))),
                          <=, 1e-14);

        for (guint i = 0; i < field->yres; i++) {
            gint ypos = (gint)i + (gint)row - (gint)up;
            for (guint j = 0; j < field->xres; j++) {
                gint xpos = (gint)j + (gint)col - (gint)left;

                if (exterior == GWY_EXTERIOR_UNDEFINED
                    && (xpos != CLAMP(xpos, 0, (gint)source->xres-1)
                        || ypos != CLAMP(ypos, 0, (gint)source->yres-1)))
                    continue;

                gdouble value = field->data[i*field->xres + j];
                gdouble reference = exterior_value_dumb_2d(source, xpos, ypos,
                                                           exterior, G_E);
                g_assert_cmpfloat(value, ==, reference);
            }
        }

        g_object_unref(field);

        field = gwy_field_new();
        gwy_field_extend(source, &fpart, field, left, right, up, down,
                         exterior, G_E, TRUE);

        g_assert_cmpuint(field->xres, ==, width + left + right);
        g_assert_cmpuint(field->yres, ==, height + up + down);
        g_assert_cmpfloat(fabs(log(gwy_field_dx(field)/gwy_field_dx(source))),
                          <=, 1e-14);
        g_assert_cmpfloat(fabs(log(gwy_field_dy(field)/gwy_field_dy(source))),
                          <=, 1e-14);

        for (guint i = 0; i < field->yres; i++) {
            gint ypos = (gint)i + (gint)row - (gint)up;
            for (guint j = 0; j < field->xres; j++) {
                gint xpos = (gint)j + (gint)col - (gint)left;

                if (exterior == GWY_EXTERIOR_UNDEFINED
                    && (xpos != CLAMP(xpos, 0, (gint)source->xres-1)
                        || ypos != CLAMP(ypos, 0, (gint)source->yres-1)))
                    continue;

                gdouble value = field->data[i*field->xres + j];
                gdouble reference = exterior_value_dumb_2d(source, xpos, ypos,
                                                           exterior, G_E);
                g_assert_cmpfloat(value, ==, reference);
            }
        }

        g_object_unref(field);
        g_object_unref(source);
    }
    g_rand_free(rng);
}

void
test_field_extend_undef(void)
{
    field_extend_one(GWY_EXTERIOR_UNDEFINED);
}

void
test_field_extend_fixed(void)
{
    field_extend_one(GWY_EXTERIOR_FIXED_VALUE);
}

void
test_field_extend_mirror(void)
{
    field_extend_one(GWY_EXTERIOR_MIRROR_EXTEND);
}

void
test_field_extend_border(void)
{
    field_extend_one(GWY_EXTERIOR_BORDER_EXTEND);
}

void
test_field_extend_periodic(void)
{
    field_extend_one(GWY_EXTERIOR_PERIODIC);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
