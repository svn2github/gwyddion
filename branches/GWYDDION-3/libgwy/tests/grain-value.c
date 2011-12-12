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
    gwy_mask_field_extract_grain(mask, mask_target, grain_id, 1);
    gwy_field_extend(field, fpart, field_target, 1, 1, 1, 1,
                     GWY_EXTERIOR_MIRROR_EXTEND, NAN, TRUE);
    g_assert_cmpfloat(fabs(gwy_field_dx(field_target) - gwy_field_dx(field)),
                      <=, 1e-16);
    g_assert_cmpfloat(fabs(gwy_field_dy(field_target) - gwy_field_dy(field)),
                      <=, 1e-16);
}

static void
test_one_value(const gchar *name,
               const gchar *expected_group,
               gboolean expected_builtin,
               gdouble (*dumb_evaluate)(const GwyMaskField *mask,
                                        const GwyField *field),
               void (*compare)(gdouble reference,
                               gdouble result,
                               const GwyMaskField *mask,
                               guint grain_id))
{
    enum { max_size = 85, niter = 30/30 };

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
        const guint *sizes = gwy_mask_field_grain_sizes(mask);
        const GwyFieldPart *bboxes = gwy_mask_field_grain_bounding_boxes(mask);

        gwy_field_evaluate_grains(field, mask, &grainvalue, 1);
        guint grainvaluengrains;
        const gdouble *values = gwy_grain_value_data(grainvalue,
                                                     &grainvaluengrains);
        g_assert(values);
        g_assert_cmpuint(grainvaluengrains, ==, ngrains);

        for (guint gno = 1; gno <= ngrains; gno++) {
            extract_grain_with_data(mask, grainmask, field, grainfield, gno);
            gdouble reference = dumb_evaluate(grainmask, grainfield);
            gdouble value = values[gno];
            gdouble eps = 5e-9*fmax(fabs(value), fabs(reference));
            if (g_test_verbose())
                g_printerr("%s[%u:%u, %u, %ux%u] %g %g (%.8g)\n",
                           name, iter, gno,
                           sizes[gno], bboxes[gno].width, bboxes[gno].height,
                           reference, value, (value - reference));
            if (compare)
                compare(reference, value, mask, gno);
            else {
                g_assert_cmpfloat(fabs(value - reference), <=, eps);
            }
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
dumb_projected_area(const GwyMaskField *mask, const GwyField *field)
{
    gdouble dxdy = gwy_field_dx(field)*gwy_field_dy(field);
    return gwy_mask_field_count(mask, NULL, TRUE)*dxdy;
}

void
test_grain_value_builtin_projected_area(void)
{
    test_one_value("Projected area", "Area", TRUE,
                   &dumb_projected_area, NULL);
}

static gdouble
dumb_equivalent_radius(const GwyMaskField *mask, const GwyField *field)
{
    gdouble dxdy = gwy_field_dx(field)*gwy_field_dy(field);
    return sqrt(gwy_mask_field_count(mask, NULL, TRUE)*dxdy/G_PI);
}

void
test_grain_value_builtin_equivalent_radius(void)
{
    test_one_value("Equivalent disc radius", "Area", TRUE,
                   &dumb_equivalent_radius, NULL);
}

static gdouble
dumb_surface_area(const GwyMaskField *mask, const GwyField *field)
{
    return gwy_field_surface_area(field, NULL, mask, GWY_MASK_INCLUDE);
}

void
test_grain_value_builtin_surface_area(void)
{
    test_one_value("Surface area", "Area", TRUE,
                   &dumb_surface_area, NULL);
}

static gdouble
dumb_center_x(const GwyMaskField *mask, const GwyField *field)
{
    GwyField *xfield = gwy_field_new_alike(field, FALSE);
    for (guint i = 0; i < xfield->yres; i++) {
        for (guint j = 0; j < xfield->xres; j++) {
            gdouble z = xfield->xoff + (j + 0.5)*gwy_field_dx(xfield);
            xfield->data[i*xfield->xres + j] = z;
        }
    }
    gdouble retval = gwy_field_mean(xfield, NULL, mask, GWY_MASK_INCLUDE);
    g_object_unref(xfield);
    return retval;
}

void
test_grain_value_builtin_center_x(void)
{
    test_one_value("Center x position", "Position", TRUE,
                   &dumb_center_x, NULL);
}

static gdouble
dumb_center_y(const GwyMaskField *mask, const GwyField *field)
{
    GwyField *yfield = gwy_field_new_alike(field, FALSE);
    for (guint i = 0; i < yfield->yres; i++) {
        gdouble z = yfield->yoff + (i + 0.5)*gwy_field_dy(yfield);
        for (guint j = 0; j < yfield->xres; j++)
            yfield->data[i*yfield->xres + j] = z;
    }
    gdouble retval = gwy_field_mean(yfield, NULL, mask, GWY_MASK_INCLUDE);
    g_object_unref(yfield);
    return retval;
}

void
test_grain_value_builtin_center_y(void)
{
    test_one_value("Center y position", "Position", TRUE,
                   &dumb_center_y, NULL);
}

static gdouble
dumb_minimum(const GwyMaskField *mask, const GwyField *field)
{
    gdouble min;
    gwy_field_min_max(field, NULL, mask, GWY_MASK_INCLUDE, &min, NULL);
    return min;
}

void
test_grain_value_builtin_minimum(void)
{
    test_one_value("Minimum value", "Value", TRUE,
                   &dumb_minimum, NULL);
}

static gdouble
dumb_maximum(const GwyMaskField *mask, const GwyField *field)
{
    gdouble max;
    gwy_field_min_max(field, NULL, mask, GWY_MASK_INCLUDE, NULL, &max);
    return max;
}

void
test_grain_value_builtin_maximum(void)
{
    test_one_value("Maximum value", "Value", TRUE,
                   &dumb_maximum, NULL);
}

static gdouble
dumb_mean(const GwyMaskField *mask, const GwyField *field)
{
    return gwy_field_mean(field, NULL, mask, GWY_MASK_INCLUDE);
}

void
test_grain_value_builtin_mean(void)
{
    test_one_value("Mean value", "Value", TRUE,
                   &dumb_mean, NULL);
}

static gdouble
dumb_median(const GwyMaskField *mask, const GwyField *field)
{
    return gwy_field_median(field, NULL, mask, GWY_MASK_INCLUDE);
}

void
test_grain_value_builtin_median(void)
{
    test_one_value("Median value", "Value", TRUE,
                   &dumb_median, NULL);
}

static gdouble
dumb_slope_theta(const GwyMaskField *mask, const GwyField *field)
{
    guint cx = field->xres/2, ax = (field->xres - 1)/2;
    guint cy = field->yres/2, ay = (field->yres - 1)/2;
    gdouble bx, by;
    gwy_field_slope(field, mask, GWY_MASK_INCLUDE,
                    cx, cy, ax, ay, FALSE,
                    GWY_EXTERIOR_MIRROR_EXTEND, NAN,
                    NULL, &bx, &by);
    return atan(hypot(bx, by));
}

void
test_grain_value_builtin_slope_theta(void)
{
    test_one_value("Slope normal angle", "Slope", TRUE,
                   &dumb_slope_theta, NULL);
}

static gdouble
dumb_slope_phi(const GwyMaskField *mask, const GwyField *field)
{
    guint cx = field->xres/2, ax = (field->xres - 1)/2;
    guint cy = field->yres/2, ay = (field->yres - 1)/2;
    gdouble bx, by;
    gwy_field_slope(field, mask, GWY_MASK_INCLUDE,
                    cx, cy, ax, ay, FALSE,
                    GWY_EXTERIOR_MIRROR_EXTEND, NAN,
                    NULL, &bx, &by);
    return atan2(by, -bx);
}

// Rounding errors and sign-of-zero issues may cause null gradients to be
// either returned with direction 0 or π.  Accept both.
static void
compare_slope_phi(gdouble reference, gdouble result,
                  const GwyMaskField *mask, guint grain_id)
{
    const GwyFieldPart *bboxes = gwy_mask_field_grain_bounding_boxes(mask);

    if (bboxes[grain_id].width == 1 || bboxes[grain_id].height == 1) {
        if (fabs(reference - G_PI) < 1e-13 && fabs(result) < 1e-13)
            return;
        if (fabs(reference) < 1e-13 && fabs(result - G_PI) < 1e-13)
            return;
    }

    gdouble eps = 2e-9*fmax(fabs(result), fabs(reference));
    g_assert_cmpfloat(fabs(result - reference), <=, eps);
}

void
test_grain_value_builtin_slope_phi(void)
{
    test_one_value("Slope direction", "Slope", TRUE,
                   &dumb_slope_phi, &compare_slope_phi);
}

static gdouble
dumb_curvature_k1(const GwyMaskField *mask, const GwyField *field)
{
    guint cx = field->xres/2, ax = (field->xres - 1)/2;
    guint cy = field->yres/2, ay = (field->yres - 1)/2;
    GwyCurvatureParams curvature;
    gwy_field_curvature(field, mask, GWY_MASK_INCLUDE,
                        cx, cy, ax, ay, FALSE,
                        GWY_EXTERIOR_MIRROR_EXTEND, NAN,
                        &curvature);
    return curvature.k1;
}

void
test_grain_value_builtin_curvature_k1(void)
{
    test_one_value("Curvature 1", "Curvature", TRUE,
                   &dumb_curvature_k1, NULL);
}

static gdouble
dumb_curvature_k2(const GwyMaskField *mask, const GwyField *field)
{
    guint cx = field->xres/2, ax = (field->xres - 1)/2;
    guint cy = field->yres/2, ay = (field->yres - 1)/2;
    GwyCurvatureParams curvature;
    gwy_field_curvature(field, mask, GWY_MASK_INCLUDE,
                        cx, cy, ax, ay, FALSE,
                        GWY_EXTERIOR_MIRROR_EXTEND, NAN,
                        &curvature);
    return curvature.k2;
}

void
test_grain_value_builtin_curvature_k2(void)
{
    test_one_value("Curvature 2", "Curvature", TRUE,
                   &dumb_curvature_k2, NULL);
}

static gdouble
dumb_curvature_phi1(const GwyMaskField *mask, const GwyField *field)
{
    guint cx = field->xres/2, ax = (field->xres - 1)/2;
    guint cy = field->yres/2, ay = (field->yres - 1)/2;
    GwyCurvatureParams curvature;
    gwy_field_curvature(field, mask, GWY_MASK_INCLUDE,
                        cx, cy, ax, ay, FALSE,
                        GWY_EXTERIOR_MIRROR_EXTEND, NAN,
                        &curvature);
    return curvature.phi1;
}

void
test_grain_value_builtin_curvature_phi1(void)
{
    test_one_value("Curvature direction 1", "Curvature", TRUE,
                   &dumb_curvature_phi1, NULL);
}

static gdouble
dumb_curvature_phi2(const GwyMaskField *mask, const GwyField *field)
{
    guint cx = field->xres/2, ax = (field->xres - 1)/2;
    guint cy = field->yres/2, ay = (field->yres - 1)/2;
    GwyCurvatureParams curvature;
    gwy_field_curvature(field, mask, GWY_MASK_INCLUDE,
                        cx, cy, ax, ay, FALSE,
                        GWY_EXTERIOR_MIRROR_EXTEND, NAN,
                        &curvature);
    return curvature.phi2;
}

void
test_grain_value_builtin_curvature_phi2(void)
{
    test_one_value("Curvature direction 2", "Curvature", TRUE,
                   &dumb_curvature_phi2, NULL);
}

static gdouble
dumb_curvature_center_x(const GwyMaskField *mask, const GwyField *field)
{
    guint cx = field->xres/2, ax = (field->xres - 1)/2;
    guint cy = field->yres/2, ay = (field->yres - 1)/2;
    GwyCurvatureParams curvature;
    if (gwy_field_curvature(field, mask, GWY_MASK_INCLUDE,
                            cx, cy, ax, ay, FALSE,
                            GWY_EXTERIOR_MIRROR_EXTEND, NAN,
                            &curvature) >= 0)
        return curvature.xc;
    else
        return dumb_center_x(mask, field);
}

void
test_grain_value_builtin_curvature_center_x(void)
{
    test_one_value("Curvature center x position", "Curvature", TRUE,
                   &dumb_curvature_center_x, NULL);
}

static gdouble
dumb_curvature_center_y(const GwyMaskField *mask, const GwyField *field)
{
    guint cx = field->xres/2, ax = (field->xres - 1)/2;
    guint cy = field->yres/2, ay = (field->yres - 1)/2;
    GwyCurvatureParams curvature;
    if (gwy_field_curvature(field, mask, GWY_MASK_INCLUDE,
                            cx, cy, ax, ay, FALSE,
                            GWY_EXTERIOR_MIRROR_EXTEND, NAN,
                            &curvature) >= 0)
        return curvature.yc;
    else
        return dumb_center_y(mask, field);
}

void
test_grain_value_builtin_curvature_center_y(void)
{
    test_one_value("Curvature center y position", "Curvature", TRUE,
                   &dumb_curvature_center_y, NULL);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
