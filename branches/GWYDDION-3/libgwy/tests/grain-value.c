/*
 *  $Id$
 *  Copyright (C) 2011 David Nečas (Yeti).
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
 * Grain value
 *
 ***************************************************************************/

G_GNUC_UNUSED
static void
print_mask_field(const gchar *name, const GwyMaskField *maskfield)
{
    g_printerr("%s %s %p %ux%u (stride %u)\n",
               G_OBJECT_TYPE_NAME(maskfield), name, maskfield,
               maskfield->xres, maskfield->yres, maskfield->stride);
    for (guint i = 0; i < maskfield->yres; i++) {
        for (guint j = 0; j < maskfield->xres; j++)
            g_printerr("%c", gwy_mask_field_get(maskfield, j, i) ? '#' : '.');
        g_printerr("%c", '\n');
    }
}

G_GNUC_UNUSED
static void
print_mask_field_grains(const gchar *name,
                        const GwyMaskField *maskfield,
                        guint grain_id)
{
    g_printerr("%s %s %p %ux%u (stride %u)\n",
               G_OBJECT_TYPE_NAME(maskfield), name, maskfield,
               maskfield->xres, maskfield->yres, maskfield->stride);
    const guint *grains = gwy_mask_field_grain_numbers(maskfield);
    for (guint i = 0; i < maskfield->yres; i++) {
        for (guint j = 0; j < maskfield->xres; j++) {
            guint k = i*maskfield->xres + j;
            if (grains[k] == grain_id)
                g_printerr("**");
            else if (grains[k])
                g_printerr("%02x", grains[k]);
            else
                g_printerr("..");
        }
        g_printerr("\n");
    }
}

static void
extract_grain_with_data(const GwyMaskField *mask,
                        GwyMaskField *mask_target,
                        const GwyField *field,
                        GwyField *field_target,
                        guint grain_id)
{
    const GwyFieldPart *bboxes = gwy_mask_field_grain_bounding_boxes(mask);
    const GwyFieldPart *fpart = bboxes + grain_id;
    gwy_mask_field_set_size(mask_target, fpart->width, fpart->height, FALSE);
    gwy_mask_field_extract_grain(mask, mask_target, grain_id, 0);
    gwy_field_extend(field, fpart, field_target, 0, 0, 0, 0,
                     GWY_EXTERIOR_MIRROR_EXTEND, NAN, FALSE);
    g_assert_cmpfloat(fabs(gwy_field_dx(field_target) - gwy_field_dx(field)),
                      <=, 1e-16);
    g_assert_cmpfloat(fabs(gwy_field_dy(field_target) - gwy_field_dy(field)),
                      <=, 1e-16);
}

static void
test_one_value(const gchar *name,
               const gchar *expected_group,
               gboolean expected_builtin,
               gdouble (*dumb_evaluator)(const GwyMaskField *mask,
                                         const GwyField *field))
{
    enum { max_size = 85, niter = 30 };

    GwyGrainValue *grainvalue = gwy_grain_value_new(name);
    g_assert(grainvalue);
    g_assert(gwy_grain_value_is_valid(grainvalue));
    g_assert_cmpstr(gwy_grain_value_get_name(grainvalue), ==, name);
    g_assert_cmpstr(gwy_grain_value_get_group(grainvalue), ==, expected_group);
    g_assert_cmpuint(!gwy_grain_value_get_resource(grainvalue),
                     ==, expected_builtin);

    GRand *rng = g_rand_new_with_seed(42);
    GwyField *grainfield = gwy_field_new();
    GwyMaskField *grainmask = gwy_mask_field_new();

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
        field_randomize(field, rng);
        GwyMaskField *mask = random_mask_field(xres, yres, rng);
        guint ngrains = gwy_mask_field_n_grains(mask);
        //print_mask_field_grains("Entire mask", mask, G_MAXUINT);

        gwy_field_evaluate_grains(field, mask, &grainvalue, 1);
        guint grainvaluengrains;
        const gdouble *values = gwy_grain_value_data(grainvalue,
                                                     &grainvaluengrains);
        g_assert(values);
        g_assert_cmpuint(grainvaluengrains, ==, ngrains);

        for (guint gno = 1; gno <= ngrains; gno++) {
            extract_grain_with_data(mask, grainmask, field, grainfield, gno);
            //print_mask_field("Grain", grainmask);
            gdouble reference = dumb_evaluator(grainmask, grainfield);
            gdouble value = values[gno];
            gdouble eps = 1e-9*fmax(fabs(value), fabs(reference));
            //g_printerr("%s[%u] %g %g\n", name, gno, reference, value);
            g_assert_cmpfloat(fabs(value - reference), <=, eps);
        }

        g_object_unref(mask);
        g_object_unref(field);
    }

    g_object_unref(grainmask);
    g_object_unref(grainfield);
    g_object_unref(grainvalue);
    g_rand_free(rng);
}

static gdouble
dumb_projected_area(const GwyMaskField *mask,
                    const GwyField *field)
{
    gdouble dxdy = gwy_field_dx(field)*gwy_field_dy(field);
    return gwy_mask_field_count(mask, NULL, TRUE)*dxdy;
}

void
test_grain_value_builtin_projected_area(void)
{
    test_one_value("Projected area", "Area", TRUE, &dumb_projected_area);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
