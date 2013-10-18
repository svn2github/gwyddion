/*
 *  $Id$
 *  Copyright (C) 2011-2013 David Nečas (Yeti).
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

// TODO: We may want non-random test on known data at least for some quantities
// that are difficult to verify independently:
// - minimum/maximum bounding direction/size
// - volume (exact integration of specific shapes)

void
grain_value_assert_equal(const GwyGrainValue *result,
                         const GwyGrainValue *reference)
{
    g_assert(GWY_IS_GRAIN_VALUE(result));
    g_assert(GWY_IS_GRAIN_VALUE(reference));
    compare_properties(G_OBJECT(result), G_OBJECT(reference));

    guint nres, nref;
    const gdouble *datares = gwy_grain_value_data(result, &nres);
    const gdouble *dataref = gwy_grain_value_data(result, &nref);
    g_assert_cmpuint(nres, ==, nref);
    g_assert(datares);
    g_assert(dataref);
    const GwyUnit *unitres = gwy_grain_value_unit(result);
    const GwyUnit *unitref = gwy_grain_value_unit(reference);
    g_assert(gwy_unit_equal(unitres, unitref));

    for (guint i = 1; i <= nref; i++)
        g_assert_cmpfloat(datares[i], ==, dataref[i]);
}

static void
extract_grain_with_data(const GwyMaskField *mask,
                        GwyMaskField *mask_target,
                        const GwyField *field,
                        GwyField *field_target,
                        guint grain_id,
                        guint border)
{
    const GwyFieldPart *bboxes = gwy_mask_field_grain_bounding_boxes(mask);
    const GwyFieldPart *fpart = bboxes + grain_id;
    gwy_mask_field_set_size(mask_target, fpart->width, fpart->height, FALSE);
    gwy_mask_field_extract_grain(mask, mask_target, grain_id, border);
    gwy_field_extend(field, fpart, field_target, border, border, border, border,
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
    enum { max_size = 85, niter = 15 };

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
            extract_grain_with_data(mask, grainmask, field, grainfield, gno, 1);
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
dumb_half_height_area(const GwyMaskField *mask, const GwyField *field)
{
    gdouble min, max;
    gwy_field_min_max(field, NULL, mask, GWY_MASK_INCLUDE, &min, &max);
    gdouble avg = 0.5*(min + max);
    GwyMaskField *above = gwy_mask_field_new_from_field(field, NULL,
                                                        avg, G_MAXDOUBLE,
                                                        FALSE);
    gdouble dxdy = gwy_field_dx(field)*gwy_field_dy(field);
    gdouble area = gwy_mask_field_count(mask, above, TRUE)*dxdy;
    g_object_unref(above);

    return area;
}

void
test_grain_value_builtin_half_height_area(void)
{
    test_one_value("Area above half-height", "Area", TRUE,
                   &dumb_half_height_area, NULL);
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
dumb_rms_intra(const GwyMaskField *mask, const GwyField *field)
{
    return gwy_field_rms(field, NULL, mask, GWY_MASK_INCLUDE);
}

void
test_grain_value_builtin_rms_intra(void)
{
    test_one_value("Value rms (intragrain)", "Value", TRUE,
                   &dumb_rms_intra, NULL);
}

static gdouble
dumb_skewness_intra(const GwyMaskField *mask, const GwyField *field)
{
    gdouble retval;
    gwy_field_statistics(field, NULL, mask, GWY_MASK_INCLUDE,
                         NULL, NULL, NULL, &retval, NULL);
    return retval;
}

static void
compare_gamma(gdouble reference, gdouble result,
              G_GNUC_UNUSED const GwyMaskField *mask,
              G_GNUC_UNUSED guint grain_id)
{
    if (isnan(reference)) {
        g_assert_cmpfloat(result, ==, 0.0);
        return;
    }

    gwy_assert_floatval(result, reference, 1e-14);
}

void
test_grain_value_builtin_skewness_intra(void)
{
    test_one_value("Value skewness (intragrain)", "Value", TRUE,
                   &dumb_skewness_intra, &compare_gamma);
}

static gdouble
dumb_kurtosis_intra(const GwyMaskField *mask, const GwyField *field)
{
    gdouble retval;
    gwy_field_statistics(field, NULL, mask, GWY_MASK_INCLUDE,
                         NULL, NULL, NULL, NULL, &retval);
    return retval;
}

void
test_grain_value_builtin_kurtosis_intra(void)
{
    test_one_value("Value kurtosis (intragrain)", "Value", TRUE,
                   &dumb_kurtosis_intra, &compare_gamma);
}

static void
boundary_minimum_quarters(gdouble zul, gdouble zur, gdouble zlr, gdouble zll,
                          guint wul, guint wur, guint wlr, guint wll,
                          gpointer user_data)
{
    const gdouble z[4] = { zul, zur, zlr, zll };
    const guint w[4] = { wul, wur, wlr, wll };
    gdouble *min = (gdouble*)user_data;
    for (guint i = 0; i < 4; i++) {
        if (w[i] && (!w[(i + 1) % 4] || !w[(i + 3) % 4]) && z[i] < *min)
            *min = z[i];
    }
}

static gdouble
dumb_boundary_minimum(const GwyMaskField *mask, const GwyField *field)
{
    gdouble min = G_MAXDOUBLE;
    gwy_field_process_quarters(field, NULL, mask, GWY_MASK_INCLUDE, FALSE,
                               &boundary_minimum_quarters, NULL, &min);
    return min;
}

void
test_grain_value_builtin_boundary_minimum(void)
{
    test_one_value("Minimum value on boundary", "Boundary", TRUE,
                   &dumb_boundary_minimum, NULL);
}

static void
boundary_maximum_quarters(gdouble zul, gdouble zur, gdouble zlr, gdouble zll,
                          guint wul, guint wur, guint wlr, guint wll,
                          gpointer user_data)
{
    const gdouble z[4] = { zul, zur, zlr, zll };
    const guint w[4] = { wul, wur, wlr, wll };
    gdouble *max = (gdouble*)user_data;
    for (guint i = 0; i < 4; i++) {
        if (w[i] && (!w[(i + 1) % 4] || !w[(i + 3) % 4]) && z[i] > *max)
            *max = z[i];
    }
}

static gdouble
dumb_boundary_maximum(const GwyMaskField *mask, const GwyField *field)
{
    gdouble max = -G_MAXDOUBLE;
    gwy_field_process_quarters(field, NULL, mask, GWY_MASK_INCLUDE, FALSE,
                               &boundary_maximum_quarters, NULL, &max);
    return max;
}

void
test_grain_value_builtin_boundary_maximum(void)
{
    test_one_value("Maximum value on boundary", "Boundary", TRUE,
                   &dumb_boundary_maximum, NULL);
}

static void
projected_boundary_length_quarters(G_GNUC_UNUSED gdouble zul,
                                   G_GNUC_UNUSED gdouble zur,
                                   G_GNUC_UNUSED gdouble zlr,
                                   G_GNUC_UNUSED gdouble zll,
                                   guint wul, guint wur, guint wlr, guint wll,
                                   gpointer user_data)
{
    gdouble *p = (gdouble*)user_data;
    p[16] += p[8*wul + 4*wur + 2*wlr + wll];
}

static gdouble
dumb_projected_boundary_length(const GwyMaskField *mask, const GwyField *field)
{
    gdouble dx = gwy_field_dx(field), dy = gwy_field_dy(field),
            hh = hypot(dx, dy), h = 0.5*hh;
    gdouble p[17] = {
        0.0, h, h, dx, h, hh, dy, h, h, dy, hh, h, dx, h, h, 0.0,
        0.0,
    };
    gwy_field_process_quarters(field, NULL, mask, GWY_MASK_INCLUDE, FALSE,
                               &projected_boundary_length_quarters, NULL,
                               &p[0]);
    return p[16];
}

void
test_grain_value_builtin_projected_boundary_length(void)
{
    test_one_value("Projected boundary length", "Boundary", TRUE,
                   &dumb_projected_boundary_length, NULL);
}

static void
increment(GHashTable *table,
          guint key)
{
    gpointer pkey = GUINT_TO_POINTER(key);
    guint val = GPOINTER_TO_UINT(g_hash_table_lookup(table, pkey));
    g_hash_table_insert(table, pkey, GUINT_TO_POINTER(val+1));
}

static void
find_all_vertices(const GwyMaskField *mask,
                  GHashTable *vertices)
{
    guint xres = mask->xres, yres = mask->yres;
    guint k = 0;

    for (guint i = 0; i < yres; i++, k++) {
        for (guint j = 0; j < xres; j++, k++) {
            if (gwy_mask_field_get(mask, j, i)) {
                increment(vertices, k);
                increment(vertices, k+1);
                increment(vertices, k+xres+1);
                increment(vertices, k+xres+2);
            }
        }
    }
}

static GArray*
outer_vertices(const GwyMaskField *mask)
{
    GHashTable *vertices = g_hash_table_new(g_direct_hash, g_direct_equal);
    find_all_vertices(mask, vertices);
    GHashTableIter iter;
    g_hash_table_iter_init(&iter, vertices);
    GArray *outervertices = g_array_new(FALSE, FALSE, sizeof(guint));
    gpointer pkey, pvalue;
    while (g_hash_table_iter_next(&iter, &pkey, &pvalue)) {
        if (GPOINTER_TO_UINT(pvalue) == 1) {
            guint k = GPOINTER_TO_UINT(pkey);
            g_array_append_val(outervertices, k);
        }
    }
    g_hash_table_destroy(vertices);
    return outervertices;
}

static gdouble
dumb_maximum_bounding_size(const GwyMaskField *mask, const GwyField *field)
{
    GArray *vertices = outer_vertices(mask);
    guint xres = field->xres;
    gdouble dx = gwy_field_dx(field), dy = gwy_field_dy(field);
    gdouble max = 0.0;
    for (guint m = 1; m < vertices->len; m++) {
        guint km = g_array_index(vertices, guint, m);
        gdouble xm = (km % (xres+1))*dx;
        gdouble ym = (km/(xres+1))*dy;
        for (guint l = 0; l < m; l++) {
            guint kl = g_array_index(vertices, guint, l);
            gdouble xl = (kl % (xres+1))*dx;
            gdouble yl = (kl/(xres+1))*dy;
            gdouble d = (xm - xl)*(xm - xl) + (ym - yl)*(ym - yl);
            if (d > max)
                max = d;
        }
    }
    g_array_free(vertices, TRUE);
    return sqrt(max);
}

void
test_grain_value_builtin_maximum_bounding_size(void)
{
    test_one_value("Maximum bounding size", "Boundary", TRUE,
                   &dumb_maximum_bounding_size, NULL);
}

static gdouble
dumb_maximum_bounding_angle(const GwyMaskField *mask, const GwyField *field)
{
    GArray *vertices = outer_vertices(mask);
    guint xres = field->xres;
    gdouble dx = gwy_field_dx(field), dy = gwy_field_dy(field);
    gdouble max = 0.0, amax = NAN;
    for (guint m = 1; m < vertices->len; m++) {
        guint km = g_array_index(vertices, guint, m);
        gdouble xm = (km % (xres+1))*dx;
        gdouble ym = (km/(xres+1))*dy;
        for (guint l = 0; l < m; l++) {
            guint kl = g_array_index(vertices, guint, l);
            gdouble xl = (kl % (xres+1))*dx;
            gdouble yl = (kl/(xres+1))*dy;
            gdouble d = (xm - xl)*(xm - xl) + (ym - yl)*(ym - yl);
            if (d > max) {
                max = d;
                amax = gwy_standardize_direction(atan2(yl - ym, xm - xl));
            }
        }
    }
    g_array_free(vertices, TRUE);
    return amax;
}

// The maximum bounding direction is has two values for rectangular grains.
static void
compare_bounding_direction(gdouble reference, gdouble result,
                           G_GNUC_UNUSED const GwyMaskField *mask,
                           G_GNUC_UNUSED guint grain_id)
{
    gdouble eps = 2e-9*fmax(fabs(result), fabs(reference));
    result = copysign(result, reference);
    gwy_assert_floatval(result, reference, eps);
}

void
test_grain_value_builtin_maximum_bounding_angle(void)
{
    test_one_value("Maximum bounding direction", "Boundary", TRUE,
                   &dumb_maximum_bounding_angle, &compare_bounding_direction);

    GwyGrainValue *grainvalue = gwy_grain_value_new("Maximum bounding direction");
    g_assert(gwy_grain_value_is_angle(grainvalue));
    g_assert_cmpuint(gwy_grain_value_needs_same_units(grainvalue),
                     ==,
                     GWY_GRAIN_VALUE_SAME_UNITS_LATERAL);
    g_object_unref(grainvalue);
}

static gdouble
dumb_volume_0(const GwyMaskField *mask, const GwyField *field)
{
    return gwy_field_volume(field, NULL, mask, GWY_MASK_INCLUDE,
                            GWY_FIELD_VOLUME_DEFAULT);
}

void
test_grain_value_builtin_volume_0(void)
{
    test_one_value("Zero-based volume", "Volume", TRUE,
                   &dumb_volume_0, NULL);
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
                        cx, cy, ax, ay, FALSE, TRUE,
                        GWY_EXTERIOR_MIRROR_EXTEND, NAN,
                        &curvature);
    return curvature.k1;
}

void
test_grain_value_builtin_curvature_k1(void)
{
    test_one_value("Curvature 1", "Curvature", TRUE,
                   &dumb_curvature_k1, NULL);

    GwyGrainValue *grainvalue = gwy_grain_value_new("Curvature 1");
    g_assert(!gwy_grain_value_is_angle(grainvalue));
    g_assert_cmpuint(gwy_grain_value_needs_same_units(grainvalue),
                     ==,
                     GWY_GRAIN_VALUE_SAME_UNITS_ALL);
    g_object_unref(grainvalue);
}

static gdouble
dumb_curvature_k2(const GwyMaskField *mask, const GwyField *field)
{
    guint cx = field->xres/2, ax = (field->xres - 1)/2;
    guint cy = field->yres/2, ay = (field->yres - 1)/2;
    GwyCurvatureParams curvature;
    gwy_field_curvature(field, mask, GWY_MASK_INCLUDE,
                        cx, cy, ax, ay, FALSE, TRUE,
                        GWY_EXTERIOR_MIRROR_EXTEND, NAN,
                        &curvature);
    return curvature.k2;
}

void
test_grain_value_builtin_curvature_k2(void)
{
    test_one_value("Curvature 2", "Curvature", TRUE,
                   &dumb_curvature_k2, NULL);

    GwyGrainValue *grainvalue = gwy_grain_value_new("Curvature 2");
    g_assert(!gwy_grain_value_is_angle(grainvalue));
    g_assert_cmpuint(gwy_grain_value_needs_same_units(grainvalue),
                     ==,
                     GWY_GRAIN_VALUE_SAME_UNITS_ALL);
    g_object_unref(grainvalue);
}

static gdouble
dumb_curvature_phi1(const GwyMaskField *mask, const GwyField *field)
{
    guint cx = field->xres/2, ax = (field->xres - 1)/2;
    guint cy = field->yres/2, ay = (field->yres - 1)/2;
    GwyCurvatureParams curvature;
    gwy_field_curvature(field, mask, GWY_MASK_INCLUDE,
                        cx, cy, ax, ay, FALSE, TRUE,
                        GWY_EXTERIOR_MIRROR_EXTEND, NAN,
                        &curvature);
    return curvature.phi1;
}

void
test_grain_value_builtin_curvature_phi1(void)
{
    test_one_value("Curvature direction 1", "Curvature", TRUE,
                   &dumb_curvature_phi1, NULL);

    GwyGrainValue *grainvalue = gwy_grain_value_new("Curvature direction 1");
    g_assert(gwy_grain_value_is_angle(grainvalue));
    g_assert_cmpuint(gwy_grain_value_needs_same_units(grainvalue),
                     ==,
                     GWY_GRAIN_VALUE_SAME_UNITS_LATERAL);
    g_object_unref(grainvalue);
}

static gdouble
dumb_curvature_phi2(const GwyMaskField *mask, const GwyField *field)
{
    guint cx = field->xres/2, ax = (field->xres - 1)/2;
    guint cy = field->yres/2, ay = (field->yres - 1)/2;
    GwyCurvatureParams curvature;
    gwy_field_curvature(field, mask, GWY_MASK_INCLUDE,
                        cx, cy, ax, ay, FALSE, TRUE,
                        GWY_EXTERIOR_MIRROR_EXTEND, NAN,
                        &curvature);
    return curvature.phi2;
}

void
test_grain_value_builtin_curvature_phi2(void)
{
    test_one_value("Curvature direction 2", "Curvature", TRUE,
                   &dumb_curvature_phi2, NULL);

    GwyGrainValue *grainvalue = gwy_grain_value_new("Curvature direction 2");
    g_assert(gwy_grain_value_is_angle(grainvalue));
    g_assert_cmpuint(gwy_grain_value_needs_same_units(grainvalue),
                     ==,
                     GWY_GRAIN_VALUE_SAME_UNITS_LATERAL);
    g_object_unref(grainvalue);
}

static gdouble
dumb_curvature_center_x(const GwyMaskField *mask, const GwyField *field)
{
    guint cx = field->xres/2, ax = (field->xres - 1)/2;
    guint cy = field->yres/2, ay = (field->yres - 1)/2;
    GwyCurvatureParams curvature;
    if (gwy_field_curvature(field, mask, GWY_MASK_INCLUDE,
                            cx, cy, ax, ay, FALSE, TRUE,
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
                            cx, cy, ax, ay, FALSE, TRUE,
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

static gdouble
dumb_curvature_center_z(const GwyMaskField *mask, const GwyField *field)
{
    guint cx = field->xres/2, ax = (field->xres - 1)/2;
    guint cy = field->yres/2, ay = (field->yres - 1)/2;
    GwyCurvatureParams curvature;
    gwy_field_curvature(field, mask, GWY_MASK_INCLUDE,
                        cx, cy, ax, ay, FALSE, TRUE,
                        GWY_EXTERIOR_MIRROR_EXTEND, NAN,
                        &curvature);
    return curvature.zc;
}

void
test_grain_value_builtin_curvature_center_z(void)
{
    test_one_value("Curvature center value", "Curvature", TRUE,
                   &dumb_curvature_center_z, NULL);
}

void
test_grain_value_builtin_inscribed_disc(void)
{
    enum { NVALUES = 3 };
    const gchar *names[NVALUES] = {
        "Maximum inscribed disc radius",
        "Maximum inscribed disc center x position",
        "Maximum inscribed disc center y position",
    };
    enum { max_size = 85, niter = 25, circsteps = 720 };

    GwyGrainValue *grainvalues[NVALUES];
    for (guint i = 0; i < NVALUES; i++) {
        grainvalues[i] = gwy_grain_value_new(names[i]);
        g_assert(grainvalues[i]);
        g_assert(gwy_grain_value_is_valid(grainvalues[i]));
        g_assert_cmpstr(gwy_grain_value_get_name(grainvalues[i]), ==, names[i]);
        g_assert_cmpstr(gwy_grain_value_get_group(grainvalues[i]),
                        ==, "Boundary");
        g_assert_cmpuint(!gwy_grain_value_get_resource(grainvalues[i]), ==, 1);
    }

    GRand *rng = g_rand_new_with_seed(42);
    GwyField *grainfield = gwy_field_new();
    GwyMaskField *grainmask = gwy_mask_field_new();

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
        field_randomize(field, rng);
        gdouble dx = gwy_field_dx(field), dy = gwy_field_dy(field);
        GwyMaskField *mask = random_mask_field(xres, yres, rng);
        guint ngrains = gwy_mask_field_n_grains(mask);
        const guint *grains = gwy_mask_field_grain_numbers(mask);

        gwy_field_evaluate_grains(field, mask, grainvalues, NVALUES);
        const gdouble *values[NVALUES];
        for (guint i = 0; i < NVALUES; i++) {
            guint grainvaluengrains;
            values[i] = gwy_grain_value_data(grainvalues[i],
                                             &grainvaluengrains);
            g_assert(values[i]);
            g_assert_cmpuint(grainvaluengrains, ==, ngrains);
        }

        for (guint gno = 1; gno <= ngrains; gno++) {
            gdouble R = values[0][gno], x = values[1][gno], y = values[2][gno];

            for (guint j = 0; j < circsteps; j++) {
                gdouble shiftx = (1.0 - 1e-6)*R*cos(2.0*M_PI*j/circsteps),
                        shifty = (1.0 - 1e-6)*R*sin(2.0*M_PI*j/circsteps);
                gdouble px = (x + shiftx)/dx, py = (y + shifty)/dy;
                g_assert_cmpfloat(px, >=, 0.0);
                g_assert_cmpfloat(px, <, xres);
                g_assert_cmpfloat(py, >=, 0.0);
                g_assert_cmpfloat(py, <, yres);

                gint jj = (gint)floor(px), ii = (gint)floor(py);
                g_assert_cmpint(jj, >=, 0.0);
                g_assert_cmpint(jj, <, xres);
                g_assert_cmpint(ii, >=, 0.0);
                g_assert_cmpint(ii, <, yres);

                g_assert_cmpuint(grains[ii*xres + jj], ==, gno);
            }
        }

        g_object_unref(mask);
        g_object_unref(field);
    }

    g_object_unref(grainmask);
    g_object_unref(grainfield);
    for (guint i = 0; i < NVALUES; i++)
        g_object_unref(grainvalues[i]);
    g_rand_free(rng);
}

void
test_grain_value_builtin_exscribed_circle(void)
{
    enum { NVALUES = 3 };
    const gchar *names[NVALUES] = {
        "Minimum circumcircle radius",
        "Minimum circumcircle center x position",
        "Minimum circumcircle center y position",
    };
    enum { max_size = 85, niter = 25, circsteps = 720 };

    GwyGrainValue *grainvalues[NVALUES];
    for (guint i = 0; i < NVALUES; i++) {
        grainvalues[i] = gwy_grain_value_new(names[i]);
        g_assert(grainvalues[i]);
        g_assert(gwy_grain_value_is_valid(grainvalues[i]));
        g_assert_cmpstr(gwy_grain_value_get_name(grainvalues[i]), ==, names[i]);
        g_assert_cmpstr(gwy_grain_value_get_group(grainvalues[i]),
                        ==, "Boundary");
        g_assert_cmpuint(!gwy_grain_value_get_resource(grainvalues[i]), ==, 1);
    }

    GRand *rng = g_rand_new_with_seed(42);
    GwyField *grainfield = gwy_field_new();
    GwyMaskField *grainmask = gwy_mask_field_new();

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
        field_randomize(field, rng);
        gdouble dx = gwy_field_dx(field), dy = gwy_field_dy(field);
        GwyMaskField *mask = random_mask_field(xres, yres, rng);
        guint ngrains = gwy_mask_field_n_grains(mask);
        const guint *grains = gwy_mask_field_grain_numbers(mask);

        gwy_field_evaluate_grains(field, mask, grainvalues, NVALUES);
        const gdouble *values[NVALUES];
        for (guint i = 0; i < NVALUES; i++) {
            guint grainvaluengrains;
            values[i] = gwy_grain_value_data(grainvalues[i],
                                             &grainvaluengrains);
            g_assert(values[i]);
            g_assert_cmpuint(grainvaluengrains, ==, ngrains);
        }

        for (guint gno = 1; gno <= ngrains; gno++) {
            gdouble R = values[0][gno], x = values[1][gno], y = values[2][gno];

            for (guint j = 0; j < circsteps; j++) {
                gdouble shiftx = (1.0 + 1e-6)*R*cos(2.0*M_PI*j/circsteps),
                        shifty = (1.0 + 1e-6)*R*sin(2.0*M_PI*j/circsteps);
                gdouble px = (x + shiftx)/dx, py = (y + shifty)/dy;
                gint jj = (gint)floor(px), ii = (gint)floor(py);
                if (jj >= 0 && jj < (gint)xres && ii >= 0 && ii < (gint)yres)
                    g_assert_cmpuint(grains[ii*xres + jj], !=, gno);
            }
        }

        g_object_unref(mask);
        g_object_unref(field);
    }

    g_object_unref(grainmask);
    g_object_unref(grainfield);
    for (guint i = 0; i < NVALUES; i++)
        g_object_unref(grainvalues[i]);
    g_rand_free(rng);
}

void
test_grain_value_builtin_edge_distance_circular(void)
{
    enum { NVALUES = 2 };
    const gchar *names[NVALUES] = {
        "Mean edge distance",
        "Shape number",
    };
    enum { max_size = 85, niter = 100 };

    GwyGrainValue *grainvalues[NVALUES];
    for (guint i = 0; i < NVALUES; i++) {
        grainvalues[i] = gwy_grain_value_new(names[i]);
        g_assert(grainvalues[i]);
        g_assert(gwy_grain_value_is_valid(grainvalues[i]));
        g_assert_cmpstr(gwy_grain_value_get_name(grainvalues[i]), ==, names[i]);
        g_assert_cmpstr(gwy_grain_value_get_group(grainvalues[i]),
                        ==, "Boundary");
        g_assert_cmpuint(!gwy_grain_value_get_resource(grainvalues[i]), ==, 1);
    }

    GRand *rng = g_rand_new_with_seed(42);
    GwyField *grainfield = gwy_field_new();
    GwyMaskField *grainmask = gwy_mask_field_new();

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 5, max_size);
        guint yres = g_rand_int_range(rng, 5, max_size);
        GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
        // Since xreal = yreal = 1 This may require resampling up to 1:17.
        GwyMaskField *mask = gwy_mask_field_new_sized(xres, yres, FALSE);
        gwy_mask_field_fill_ellipse(mask, NULL, TRUE, TRUE);
        guint ngrains = gwy_mask_field_n_grains(mask);
        g_assert_cmpuint(ngrains, ==, 1);

        gwy_field_evaluate_grains(field, mask, grainvalues, NVALUES);
        const gdouble *values[NVALUES];
        for (guint i = 0; i < NVALUES; i++) {
            guint grainvaluengrains;
            values[i] = gwy_grain_value_data(grainvalues[i],
                                             &grainvaluengrains);
            g_assert(values[i]);
            g_assert_cmpuint(grainvaluengrains, ==, ngrains);
        }

        // Exact circle has d_e = 1/6, F_s = 1.  A discretised circle has
        // always smaller d_e and larger F_s.
        gdouble edmean = values[0][1], shapeno = values[1][1];
        g_assert_cmpfloat(edmean, <=, 1.0/6.0);
        g_assert_cmpfloat(shapeno, >=, 1.0);
        // Tolerance, not based on exact calculation.
        gdouble badness = (fabs(log((gdouble)xres/yres))
                          + 400.0/(xres*xres + yres*yres)
                          + 1.0);
        g_assert_cmpfloat(edmean, >=, 1.0/6.0 - 0.01*badness);
        g_assert_cmpfloat(shapeno, <=, 1.0 + 0.15*badness);

        g_object_unref(mask);
        g_object_unref(field);
    }

    g_object_unref(grainmask);
    g_object_unref(grainfield);
    for (guint i = 0; i < NVALUES; i++)
        g_object_unref(grainvalues[i]);
    g_rand_free(rng);
}

static void
set_3x3(GwyMaskField *mask,
        guint j, guint i)
{
    gwy_mask_field_set(mask, j-1, i-1, TRUE);
    gwy_mask_field_set(mask, j, i-1, TRUE);
    gwy_mask_field_set(mask, j+1, i-1, TRUE);
    gwy_mask_field_set(mask, j-1, i, TRUE);
    gwy_mask_field_set(mask, j, i, TRUE);
    gwy_mask_field_set(mask, j+1, i, TRUE);
    gwy_mask_field_set(mask, j-1, i+1, TRUE);
    gwy_mask_field_set(mask, j, i+1, TRUE);
    gwy_mask_field_set(mask, j+1, i+1, TRUE);
}

static void
make_diagonal_line(GwyMaskField *mask, gboolean maindiag)
{
    guint xres = mask->xres, yres = mask->yres;

    if (yres <= xres) {
        for (guint j = 1; j < xres-1; j++) {
            guint i = (2*(j - 1) + 1)*(yres - 2)/(2*(xres - 2)) + 1;
            if (!maindiag)
                i = yres-1 - i;
            set_3x3(mask, j, i);
        }
    }
    else {
        for (guint i = 1; i < yres-1; i++) {
            guint j = (2*(i - 1) + 1)*(xres - 2)/(2*(yres - 2)) + 1;
            if (!maindiag)
                j = xres-1 - j;
            set_3x3(mask, j, i);
        }
    }
}

void
test_grain_value_builtin_moment_lines(void)
{
    enum { NVALUES = 4 };
    const gchar *names[NVALUES] = {
        "Semimajor axis length",
        "Semiminor axis length",
        "Semimajor axis direction",
        "Semiminor axis direction",
    };
    enum { max_size = 85, niter = 100 };

    GwyGrainValue *grainvalues[NVALUES];
    for (guint i = 0; i < NVALUES; i++) {
        grainvalues[i] = gwy_grain_value_new(names[i]);
        g_assert(grainvalues[i]);
        g_assert(gwy_grain_value_is_valid(grainvalues[i]));
        g_assert_cmpstr(gwy_grain_value_get_name(grainvalues[i]), ==, names[i]);
        g_assert_cmpstr(gwy_grain_value_get_group(grainvalues[i]),
                        ==, "Moment");
        g_assert_cmpuint(!gwy_grain_value_get_resource(grainvalues[i]), ==, 1);
    }

    GRand *rng = g_rand_new_with_seed(42);
    GwyField *grainfield = gwy_field_new();
    GwyMaskField *grainmask = gwy_mask_field_new();

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 8, max_size);
        guint yres = g_rand_int_range(rng, 8, max_size);
        GwyMaskField *mask = gwy_mask_field_new_sized(xres, yres, FALSE);
        GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
        gdouble xreal = g_rand_double(rng) + 0.2;
        gdouble yreal = g_rand_double(rng) + 0.2;
        gdouble alpha = atan2(yreal, xreal);
        gdouble diag = 0.5*hypot(xreal, yreal);
        gdouble ortho = 1.5*hypot(gwy_field_dx(field), gwy_field_dy(field));
        gdouble alphamaj, alphamin, amaj, amin;
        gwy_field_set_xreal(field, xreal);
        gwy_field_set_yreal(field, yreal);

        gwy_mask_field_fill(mask, NULL, FALSE);
        make_diagonal_line(mask, TRUE);
        guint ngrains = gwy_mask_field_n_grains(mask);
        g_assert_cmpuint(ngrains, ==, 1);

        gwy_field_evaluate_grains(field, mask, grainvalues, NVALUES);
        const gdouble *values[NVALUES];
        for (guint i = 0; i < NVALUES; i++) {
            guint grainvaluengrains;
            values[i] = gwy_grain_value_data(grainvalues[i],
                                             &grainvaluengrains);
            g_assert(values[i]);
            g_assert_cmpuint(grainvaluengrains, ==, ngrains);
        }

        amaj = values[0][1];
        amin = values[1][1];
        alphamaj = values[2][1];
        alphamin = values[3][1];
        gwy_assert_floatval(alphamaj, -alpha, 0.1);
        gwy_assert_floatval(alphamin, 0.5*G_PI - alpha, 0.1);
        gwy_assert_floatval(amaj/diag, 1.05, 0.1);
        // XXX: How to get better bounds?
        g_assert_cmpfloat(amin/ortho, >=, 0.2);
        g_assert_cmpfloat(amin/ortho, <=, 1.2);

        gwy_mask_field_fill(mask, NULL, FALSE);
        make_diagonal_line(mask, FALSE);
        ngrains = gwy_mask_field_n_grains(mask);
        g_assert_cmpuint(ngrains, ==, 1);

        gwy_field_evaluate_grains(field, mask, grainvalues, NVALUES);
        for (guint i = 0; i < NVALUES; i++) {
            guint grainvaluengrains;
            values[i] = gwy_grain_value_data(grainvalues[i],
                                             &grainvaluengrains);
            g_assert(values[i]);
            g_assert_cmpuint(grainvaluengrains, ==, ngrains);
        }

        amaj = values[0][1];
        amin = values[1][1];
        alphamaj = values[2][1];
        alphamin = values[3][1];
        gwy_assert_floatval(amaj/diag, 1.05, 0.1);
        gwy_assert_floatval(alphamaj, alpha, 0.1);
        gwy_assert_floatval(alphamin, alpha - 0.5*G_PI, 0.1);
        // XXX: How to get better bounds?
        g_assert_cmpfloat(amin/ortho, >=, 0.2);
        g_assert_cmpfloat(amin/ortho, <=, 1.2);

        g_object_unref(mask);
        g_object_unref(field);
    }

    g_object_unref(grainmask);
    g_object_unref(grainfield);
    for (guint i = 0; i < NVALUES; i++)
        g_object_unref(grainvalues[i]);
    g_rand_free(rng);
}

void
test_grain_value_builtin_moment_rectangles(void)
{
    enum { NVALUES = 4 };
    const gchar *names[NVALUES] = {
        "Semimajor axis length",
        "Semiminor axis length",
        "Semimajor axis direction",
        "Semiminor axis direction",
    };
    enum { max_size = 25, niter = 100 };

    GwyGrainValue *grainvalues[NVALUES];
    for (guint i = 0; i < NVALUES; i++) {
        grainvalues[i] = gwy_grain_value_new(names[i]);
        g_assert(grainvalues[i]);
        g_assert(gwy_grain_value_is_valid(grainvalues[i]));
        g_assert_cmpstr(gwy_grain_value_get_name(grainvalues[i]), ==, names[i]);
        g_assert_cmpstr(gwy_grain_value_get_group(grainvalues[i]),
                        ==, "Moment");
        g_assert_cmpuint(!gwy_grain_value_get_resource(grainvalues[i]), ==, 1);
    }

    GRand *rng = g_rand_new_with_seed(42);
    GwyField *grainfield = gwy_field_new();
    GwyMaskField *grainmask = gwy_mask_field_new();
    gdouble q = 1.0/sqrt(sqrt(3.0*G_PI));

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        GwyMaskField *mask = gwy_mask_field_new_sized(xres, yres, FALSE);
        GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
        gdouble xreal = g_rand_double(rng) + 0.2;
        gdouble yreal = g_rand_double(rng) + 0.2;
        gwy_field_set_xreal(field, xreal);
        gwy_field_set_yreal(field, yreal);

        gwy_mask_field_fill(mask, NULL, TRUE);
        guint ngrains = gwy_mask_field_n_grains(mask);
        g_assert_cmpuint(ngrains, ==, 1);

        gwy_field_evaluate_grains(field, mask, grainvalues, NVALUES);
        const gdouble *values[NVALUES];
        for (guint i = 0; i < NVALUES; i++) {
            guint grainvaluengrains;
            values[i] = gwy_grain_value_data(grainvalues[i],
                                             &grainvaluengrains);
            g_assert(values[i]);
            g_assert_cmpuint(grainvaluengrains, ==, ngrains);
        }

        gdouble amaj = values[0][1];
        gdouble amin = values[1][1];
        gdouble alphamaj = values[2][1];
        gdouble alphamin = values[3][1];
        if (xreal >= yreal) {
            gwy_assert_floatval(alphamaj, 0.0, 1e-14);
            gwy_assert_floatval(alphamin, 0.5*G_PI, 1e-14);
            gwy_assert_floatval(amaj, q*xreal, 1e-14);
            gwy_assert_floatval(amin, q*yreal, 1e-14);
        }
        else {
            gwy_assert_floatval(alphamaj, 0.5*G_PI, 1e-14);
            gwy_assert_floatval(alphamin, 0.0, 1e-14);
            gwy_assert_floatval(amaj, q*yreal, 1e-14);
            gwy_assert_floatval(amin, q*xreal, 1e-14);
        }

        g_object_unref(mask);
        g_object_unref(field);
    }

    g_object_unref(grainmask);
    g_object_unref(grainfield);
    for (guint i = 0; i < NVALUES; i++)
        g_object_unref(grainvalues[i]);
    g_rand_free(rng);
}

void
test_grain_value_evaluate_multiple(void)
{
    enum { xres = 30, yres = 20, niter = 40, nmiter = 5 };
    const gchar* const *names = gwy_grain_value_list_builtins();
    guint n = g_strv_length((gchar**)names);

    GRand *rng = g_rand_new_with_seed(42);
    for (guint miter = 0; miter < nmiter; miter++) {
        GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
        field_randomize(field, rng);
        GwyMaskField *mask = random_mask_field(xres, yres, rng);

        for (guint iter = 0; iter < niter; iter++) {
            guint nvalues = g_rand_int_range(rng, 2, 7)
                            + g_rand_int_range(rng, 2, 7);
            GwyGrainValue **grainvalues = g_new(GwyGrainValue*, nvalues);
            for (guint i = 0; i < nvalues; i++) {
                guint id = g_rand_int_range(rng, 0, n);
                const gchar *name = names[id];
                //g_printerr("[%u] %u <%s> %u\n", i, id, name, n);
                grainvalues[i] = gwy_grain_value_new(name);
                g_assert(GWY_IS_GRAIN_VALUE(grainvalues[i]));
                g_assert(gwy_grain_value_is_valid(grainvalues[i]));
                g_assert_cmpstr(gwy_grain_value_get_name(grainvalues[i]),
                                ==, name);
            }

            gwy_field_evaluate_grains(field, mask, grainvalues, nvalues);

            for (guint i = 0; i < nvalues; i++) {
                const gchar *name = gwy_grain_value_get_name(grainvalues[i]);
                GwyGrainValue *grainvalue = gwy_grain_value_new(name);
                g_assert(GWY_IS_GRAIN_VALUE(grainvalue));
                g_assert(gwy_grain_value_is_valid(grainvalue));
                g_assert_cmpstr(gwy_grain_value_get_name(grainvalue), ==, name);
                gwy_grain_value_evaluate(grainvalue, field, mask);
                grain_value_assert_equal(grainvalues[i], grainvalue);
                g_object_unref(grainvalue);
            }

            for (guint i = 0; i < nvalues; i++)
                g_object_unref(grainvalues[i]);
            g_free(grainvalues);
        }

        g_object_unref(mask);
        g_object_unref(field);
    }
    g_rand_free(rng);
}

static gdouble
dumb_height(const GwyMaskField *mask, const GwyField *field)
{
    gdouble min, max;
    gwy_field_min_max(field, NULL, mask, GWY_MASK_INCLUDE, &min, &max);
    return max - min;
}

void
test_grain_value_user(void)
{
    GwyInventory *usergrainvalues = gwy_user_grain_values();
    g_assert(GWY_IS_INVENTORY(usergrainvalues));
    if (gwy_inventory_get(usergrainvalues, "TESTLIBGWY Height"))
        gwy_inventory_delete(usergrainvalues, "TESTLIBGWY Height");
    GwyUserGrainValue *height = gwy_user_grain_value_new();
    g_assert(GWY_IS_USER_GRAIN_VALUE(height));
    GwyResource *resource = GWY_RESOURCE(height);
    gwy_resource_set_name(resource, "TESTLIBGWY Height");
    gwy_inventory_insert(usergrainvalues, height);
    g_object_unref(height);
    GError *error = NULL;
    g_assert(gwy_user_grain_value_set_formula(height, "z_max-z_min", &error));
    g_assert_no_error(error);
    gwy_user_grain_value_set_group(height, "User");
    gwy_user_grain_value_set_ident(height, "h_testlibgwy");
    gwy_user_grain_value_set_symbol(height, "h<sub>testlibwgy</sub>");
    gwy_user_grain_value_set_power_x(height, 0);
    gwy_user_grain_value_set_power_y(height, 0);
    gwy_user_grain_value_set_power_z(height, 1);
    gwy_user_grain_value_set_is_angle(height, FALSE);
    gwy_user_grain_value_set_same_units(height, FALSE);

    test_one_value("TESTLIBGWY Height", "User", FALSE, &dumb_height, NULL);

    GwyGrainValue *grainvalue = gwy_grain_value_new("TESTLIBGWY Height");
    g_assert(GWY_IS_GRAIN_VALUE(grainvalue));
    g_assert(!gwy_grain_value_is_angle(grainvalue));
    g_assert(!gwy_grain_value_needs_same_units(grainvalue));
    GwyUserGrainValue *usergrainvalue = gwy_grain_value_get_resource(grainvalue);
    g_assert(usergrainvalue == height);
    g_object_unref(grainvalue);
    gwy_inventory_delete(usergrainvalues, "TESTLIBGWY Height");
}

void
test_grain_value_ident(void)
{
    const gchar* const* names = gwy_grain_value_list_builtins();
    guint n = g_strv_length((gchar**)names);

    for (guint i = 0; i < n; i++) {
        GwyGrainValue *grainvalue = gwy_grain_value_new(names[i]);
        g_assert(GWY_IS_GRAIN_VALUE(grainvalue));
        g_assert(gwy_grain_value_is_valid(grainvalue));
        const gchar *ident = gwy_grain_value_get_ident(grainvalue);
        g_assert(gwy_ascii_strisident(ident, "_", NULL));
        g_object_unref(grainvalue);
    }
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
