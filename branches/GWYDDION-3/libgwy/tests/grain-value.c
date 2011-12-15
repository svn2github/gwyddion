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

// TODO: We may want non-random test on known data at least for some quantities
// that are difficult to verify independently:
// - minimum/maximum bounding direction/size
// - volume (exact integration of specific shapes)

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
                               &boundary_minimum_quarters, &min);
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
                               &boundary_maximum_quarters, &max);
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
                               &projected_boundary_length_quarters, &p[0]);
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
    g_assert_cmpfloat(fabs(result - reference), <=, eps);
}

void
test_grain_value_builtin_maximum_bounding_angle(void)
{
    test_one_value("Maximum bounding direction", "Boundary", TRUE,
                   &dumb_maximum_bounding_angle, &compare_bounding_direction);
}

static gdouble
dumb_volume_0(const GwyMaskField *mask, const GwyField *field)
{
    static const gdouble weights[3*3] = {
        1.0/576.0, 11.0/288.0, 1.0/576.0,
        11.0/288.0, 121.0/144.0, 11.0/288.0,
        1.0/576.0, 11.0/288.0, 1.0/576.0,
    };
    GwyField *kernel = gwy_field_new_sized(3, 3, FALSE);
    gwy_assign(kernel->data, weights, 3*3);
    GwyField *weighted = gwy_field_new_alike(field, FALSE);
    gwy_field_convolve(field, NULL, weighted, kernel,
                       GWY_EXTERIOR_MIRROR_EXTEND, NAN);
    g_object_unref(kernel);
    gdouble volume = (gwy_field_mean(weighted, NULL, mask, GWY_MASK_INCLUDE)
                      * gwy_mask_field_count(mask, NULL, TRUE)
                      * gwy_field_dx(field)*gwy_field_dy(field));
    g_object_unref(weighted);
    return volume;
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

static gdouble
dumb_curvature_center_z(const GwyMaskField *mask, const GwyField *field)
{
    guint cx = field->xres/2, ax = (field->xres - 1)/2;
    guint cy = field->yres/2, ay = (field->yres - 1)/2;
    GwyCurvatureParams curvature;
    gwy_field_curvature(field, mask, GWY_MASK_INCLUDE,
                        cx, cy, ax, ay, FALSE,
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

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
