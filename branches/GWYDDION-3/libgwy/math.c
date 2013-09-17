/*
 *  $Id$
 *  Copyright (C) 2007,2009-2013 David Nečas (Yeti).
 *  E-mail: yeti@gwyddion.net.
 *
 *  The quicksort algorithm was copied from GNU C library,
 *  Copyright (C) 1991, 1992, 1996, 1997, 1999 Free Software Foundation, Inc.
 *  See below.
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

#include <string.h>
#include <stdio.h>
#include "libgwy/macros.h"
#include "libgwy/math.h"
#include "libgwy/serializable-boxed.h"
#include "libgwy/serialize.h"
#include "libgwy/math-internal.h"

#define DSWAP(x, y) GWY_SWAP(gdouble, x, y)
#define ISWAP(x, y) GWY_SWAP(guint, x, y)
#define NORMALIZE(x) (isnormal(x) ? (x) : 0.0)

enum {
    N_ITEMS_XY = 2,
    N_ITEMS_XYZ = 3,
    N_ITEMS_RANGE = 2,
};

static gsize    gwy_xy_itemize     (gpointer boxed,
                                    GwySerializableItems *items);
static gpointer gwy_xy_construct   (GwySerializableItems *items,
                                    GwyErrorList **error_list);
static gsize    gwy_xyz_itemize    (gpointer boxed,
                                    GwySerializableItems *items);
static gpointer gwy_xyz_construct  (GwySerializableItems *items,
                                    GwyErrorList **error_list);
static gsize    gwy_range_itemize  (gpointer boxed,
                                    GwySerializableItems *items);
static gpointer gwy_range_construct(GwySerializableItems *items,
                                    GwyErrorList **error_list);
static void     sort_plain         (gdouble *array,
                                    gsize n);
static void     sort_with_index    (gdouble *array,
                                    guint *index_array,
                                    gsize n);

static const GwySerializableItem serialize_items_xy[N_ITEMS_XY] = {
    /*0*/ { .name = "x", .ctype = GWY_SERIALIZABLE_DOUBLE, },
    /*1*/ { .name = "y", .ctype = GWY_SERIALIZABLE_DOUBLE, },
};

static const GwySerializableItem serialize_items_range[N_ITEMS_RANGE] = {
    /*0*/ { .name = "from", .ctype = GWY_SERIALIZABLE_DOUBLE, },
    /*1*/ { .name = "to",   .ctype = GWY_SERIALIZABLE_DOUBLE, },
};

static const GwySerializableItem serialize_items_xyz[N_ITEMS_XYZ] = {
    /*0*/ { .name = "x", .ctype = GWY_SERIALIZABLE_DOUBLE, },
    /*1*/ { .name = "y", .ctype = GWY_SERIALIZABLE_DOUBLE, },
    /*2*/ { .name = "z", .ctype = GWY_SERIALIZABLE_DOUBLE, },
};

GType
gwy_xy_get_type(void)
{
    static GType xy_type = 0;

    if (G_UNLIKELY(!xy_type)) {
        xy_type = g_boxed_type_register_static("GwyXY",
                                               (GBoxedCopyFunc)gwy_xy_copy,
                                               (GBoxedFreeFunc)gwy_xy_free);
        static const GwySerializableBoxedInfo boxed_info = {
            sizeof(GwyXY), N_ITEMS_XY, gwy_xy_itemize, gwy_xy_construct,
            NULL, NULL,
        };
        gwy_serializable_boxed_register_static(xy_type, &boxed_info);
    }

    return xy_type;
}

static gsize
gwy_xy_itemize(gpointer boxed,
               GwySerializableItems *items)
{
    GwyXY *xy = (GwyXY*)boxed;

    g_return_val_if_fail(items->len - items->n >= N_ITEMS_XY, 0);

    GwySerializableItem *it = items->items + items->n;

    *it = serialize_items_xy[0];
    it->value.v_double = xy->x;
    it++, items->n++;

    *it = serialize_items_xy[1];
    it->value.v_double = xy->y;
    it++, items->n++;

    return N_ITEMS_XY;
}

static gpointer
gwy_xy_construct(GwySerializableItems *items,
                 GwyErrorList **error_list)
{
    GwySerializableItem its[N_ITEMS_XY];
    memcpy(its, serialize_items_xy, sizeof(serialize_items_xy));
    gwy_deserialize_filter_items(its, N_ITEMS_XY, items, NULL,
                                 "GwyXY", error_list);

    GwyXY *xy = g_slice_new(GwyXY);
    xy->x = NORMALIZE(its[0].value.v_double);
    xy->y = NORMALIZE(its[1].value.v_double);
    return xy;
}

/**
 * gwy_xy_copy:
 * @xy: Cartesian coordinates in plane.
 *
 * Copies Cartesian coordinates in plane.
 *
 * Returns: A copy of @xy. The result must be freed using gwy_xy_free(),
 *          not g_free().
 **/
GwyXY*
gwy_xy_copy(const GwyXY *xy)
{
    g_return_val_if_fail(xy, NULL);
    return g_slice_copy(sizeof(GwyXY), xy);
}

/**
 * gwy_xy_free:
 * @xy: Cartesian coordinates in plane.
 *
 * Frees Cartesian coordinates in plane created with gwy_xy_copy().
 **/
void
gwy_xy_free(GwyXY *xy)
{
    g_slice_free1(sizeof(GwyXY), xy);
}

GType
gwy_xyz_get_type(void)
{
    static GType xyz_type = 0;

    if (G_UNLIKELY(!xyz_type)) {
        xyz_type = g_boxed_type_register_static("GwyXYZ",
                                                (GBoxedCopyFunc)gwy_xyz_copy,
                                                (GBoxedFreeFunc)gwy_xyz_free);
        static const GwySerializableBoxedInfo boxed_info = {
            sizeof(GwyXYZ), N_ITEMS_XYZ, gwy_xyz_itemize, gwy_xyz_construct,
            NULL, NULL,
        };
        gwy_serializable_boxed_register_static(xyz_type, &boxed_info);
    }

    return xyz_type;
}

static gsize
gwy_xyz_itemize(gpointer boxed,
                GwySerializableItems *items)
{
    GwyXYZ *xyz = (GwyXYZ*)boxed;

    g_return_val_if_fail(items->len - items->n >= N_ITEMS_XYZ, 0);

    GwySerializableItem *it = items->items + items->n;

    *it = serialize_items_xyz[0];
    it->value.v_double = xyz->x;
    it++, items->n++;

    *it = serialize_items_xyz[1];
    it->value.v_double = xyz->y;
    it++, items->n++;

    *it = serialize_items_xyz[2];
    it->value.v_double = xyz->z;
    it++, items->n++;

    return N_ITEMS_XYZ;
}

static gpointer
gwy_xyz_construct(GwySerializableItems *items,
                  GwyErrorList **error_list)
{
    GwySerializableItem its[N_ITEMS_XYZ];
    memcpy(its, serialize_items_xyz, sizeof(serialize_items_xyz));
    gwy_deserialize_filter_items(its, N_ITEMS_XYZ, items, NULL,
                                 "GwyXYZ", error_list);

    GwyXYZ *xyz = g_slice_new(GwyXYZ);
    xyz->x = NORMALIZE(its[0].value.v_double);
    xyz->y = NORMALIZE(its[1].value.v_double);
    xyz->z = NORMALIZE(its[2].value.v_double);
    return xyz;
}

/**
 * gwy_xyz_copy:
 * @xyz: Cartesian coordinates in space.
 *
 * Copies Cartesian coordinates in space.
 *
 * Returns: A copy of @xyz. The result must be freed using gwy_xyz_free(),
 *          not g_free().
 **/
GwyXYZ*
gwy_xyz_copy(const GwyXYZ *xyz)
{
    g_return_val_if_fail(xyz, NULL);
    return g_slice_copy(sizeof(GwyXYZ), xyz);
}

/**
 * gwy_xyz_free:
 * @xyz: Cartesian coordinates in space.
 *
 * Frees Cartesian coordinates in space created with gwy_xyz_copy().
 **/
void
gwy_xyz_free(GwyXYZ *xyz)
{
    g_slice_free1(sizeof(GwyXYZ), xyz);
}

GType
gwy_range_get_type(void)
{
    static GType range_type = 0;

    if (G_UNLIKELY(!range_type)) {
        range_type = g_boxed_type_register_static
                                        ("GwyRange",
                                         (GBoxedCopyFunc)gwy_range_copy,
                                         (GBoxedFreeFunc)gwy_range_free);
        static const GwySerializableBoxedInfo boxed_info = {
            sizeof(GwyRange), N_ITEMS_RANGE,
            gwy_range_itemize, gwy_range_construct,
            NULL, NULL,
        };
        gwy_serializable_boxed_register_static(range_type, &boxed_info);
    }

    return range_type;
}

static gsize
gwy_range_itemize(gpointer boxed,
                  GwySerializableItems *items)
{
    GwyRange *range = (GwyRange*)boxed;

    g_return_val_if_fail(items->len - items->n >= N_ITEMS_RANGE, 0);

    GwySerializableItem *it = items->items + items->n;

    *it = serialize_items_range[0];
    it->value.v_double = range->from;
    it++, items->n++;

    *it = serialize_items_range[1];
    it->value.v_double = range->to;
    it++, items->n++;

    return N_ITEMS_RANGE;
}

static gpointer
gwy_range_construct(GwySerializableItems *items,
                    GwyErrorList **error_list)
{
    GwySerializableItem its[N_ITEMS_RANGE];
    memcpy(its, serialize_items_range, sizeof(serialize_items_range));
    gwy_deserialize_filter_items(its, N_ITEMS_RANGE, items, NULL,
                                 "GwyRange", error_list);

    GwyRange *range = g_slice_new(GwyRange);
    range->from = NORMALIZE(its[0].value.v_double);
    range->to = NORMALIZE(its[1].value.v_double);
    return range;
}

/**
 * gwy_range_copy:
 * @range: Interval on the real axis.
 *
 * Copies interval on the real axis.
 *
 * Returns: A copy of @range. The result must be freed using gwy_range_free(),
 *          not g_free().
 **/
GwyRange*
gwy_range_copy(const GwyRange *range)
{
    g_return_val_if_fail(range, NULL);
    return g_slice_copy(sizeof(GwyRange), range);
}

/**
 * gwy_range_free:
 * @range: Interval on the real axis.
 *
 * Frees interval on the real axis created with gwy_range_copy().
 **/
void
gwy_range_free(GwyRange *range)
{
    g_slice_free1(sizeof(GwyRange), range);
}

/**
 * gwy_powi:
 * @x: Base.
 * @i: Integer exponent.
 *
 * Calculates the integer power of a number.
 *
 * This is usually the right function for calculation of (integer) powers of
 * 10 as exp10() suffers from precision.
 *
 * Returns: Value of @x raised to the integer power @i.
 **/
// Always define the function.  If the user compiles her code with gcc, they
// will get the builtin but otherwise the may actually need to link this one.
#undef gwy_powi
double
gwy_powi(double x, int i)
{
    gdouble r = 1.0;
    if (!i)
        return 1.0;
    gboolean negative = FALSE;
    if (i < 0) {
        negative = TRUE;
        i = -i;
    }
    for ( ; ; ) {
        if (i & 1)
            r *= x;
        if (!(i >>= 1))
            break;
        x *= x;
    }
    return negative ? 1.0/r : r;
}

/**
 * gwy_spow:
 * @x: Value to raise to some power.
 * @p: Power.
 *
 * Calculates signed power function.
 *
 * Signed power means value that has the same sign as @x and magnitude of
 * |@x|<superscript>@p</superscript>.  This function is equivalent to
 * <literal>copysign(pow(fabs(x), p), x)</literal>
 * which defines the precise semantics concerning infinities and undefined
 * values.
 *
 * Returns: Value of absolute value of @x raised to power @p, with sign of @x.
 **/
gdouble
gwy_spow(gdouble x, gdouble p)
{
    return copysign(pow(fabs(x), p), x);
}

/**
 * gwy_ssqrt:
 * @x: Value to take square root of.
 *
 * Calculates signed square root function.
 *
 * See gwy_spow() for details.  This is just a specialisation which uses sqrt()
 * instead of the slow pow() function.
 *
 * Returns: Value of square root of absolute value of @x, with sign of @x.
 **/
gdouble
gwy_ssqrt(gdouble x)
{
    return copysign(sqrt(fabs(x)), x);
}

/**
 * gwy_ssqr:
 * @x: Value to calculate square of.
 *
 * Calculates signed square function.
 *
 * See gwy_spow() for details.  This is just a specialisation which uses
 * multiplication instead of the slow pow() function.
 *
 * Returns: Value of square of @x, with sign of @x.
 **/
gdouble
gwy_ssqr(gdouble x)
{
    return x*fabs(x);
}

/**
 * gwy_power_sum:
 * @n: Number of terms in the sum.
 * @p: Power.
 *
 * Sums first N integers raised to speficied power.
 *
 * The function calculates the sum of @x<superscript>@p</superscript> for
 * integral @x from 1 to @n.  If you want to sum from 0 then add 1 if @p is 0.
 *
 * For small powers (at present up to @p = 20), the sum is calculated using an
 * explicit formula which is fast.  For large powers, the result is calculated
 * using a naïve summation which may be slow.
 *
 * Returns: Sum of first @n integers raised to power @p.
 **/
gdouble
gwy_power_sum(guint n,
              guint p)
{
    gdouble x = n;

    if (p % 2 == 0) {
        // This is the only place where summing from 0 or from 1 matters.
        if (p == 0)
            return n;
        if (p == 2)
            return ((2*x + 3)*x + 1)*x/6;

        gdouble y = x*x;
        if (p == 4)
            return (((6*x + 15)*x + 10)*y - 1)*x/30;
        if (p == 6)
            return ((((6*x + 21)*x + 21)*y - 7)*y + 1)*x/42;
        if (p == 8)
            return (((((10*x + 45)*x + 60)*y - 42)*y + 20)*y - 3)*x/90;
        if (p == 10)
            return ((((((6*x + 33)*x + 55)*y - 66)*y + 66)*y - 33)*y + 5)*x/66;
        if (p == 12)
            return (((((((210*x + 1365)*x + 2730)*y - 5005)*y + 8580)*y - 9009)*y + 4550)*y - 691)*x/2730;
        if (p == 14)
            return ((((((((6*x + 45)*x + 105)*y - 273)*y + 715)*y - 1287)*y + 1365)*y - 691)*y + 105)*x/90;
        if (p == 16)
            return (((((((((30*x + 255)*x + 680)*y - 2380)*y + 8840)*y - 24310)*y + 44200)*y - 46988)*y + 23800)*y - 3617)*x/510;
        if (p == 18)
            return ((((((((((210*x + 1995)*x + 5985)*y - 27132)*y + 135660)*y - 529074)*y + 1469650)*y - 2678316)*y + 2848860)*y - 1443183)*y + 219335)*x/3990;
        if (p == 20)
            return (((((((((((330*x + 3465)*x + 11550)*y - 65835)*y + 426360)*y - 2238390)*y + 8817900)*y - 24551230)*y + 44767800)*y - 47625039)*y + 24126850)*y - 3666831)*x/6930;
    }
    else {
        gdouble a = x*(x + 1)/2;
        if (p == 1)
            return a;
        if (p == 3)
            return a*a;
        if (p == 5)
            return (4*a - 1)*a*a/3;
        if (p == 7)
            return ((6*a - 4)*a + 1)*a*a/3;
        if (p == 9)
            return (((16*a - 20)*a + 12)*a - 3)*a*a/5;
        if (p == 11)
            return ((((16*a - 32)*a + 34)*a - 20)*a + 5)*a*a/3;
        if (p == 13)
            return (((((960*a - 2800)*a + 4592)*a - 4720)*a + 2764)*a - 691)*a*a/105;
        if (p == 15)
            return ((((((48*a - 192)*a + 448)*a - 704)*a + 718)*a - 420)*a + 105)*a*a/3;
        if (p == 17)
            return (((((((1280*a - 6720)*a + 21120)*a - 46880)*a + 72912)*a - 74220)*a + 43404)*a - 10851)*a*a/45;
        if (p == 19)
            return ((((((((5376*a - 35840)*a + 145824)*a - 433536)*a + 950544)*a - 1474592)*a + 1500334)*a - 877340)*a + 219335)*a*a/105;
    }

    gdouble s = 0.0;
    for (guint i = 1; i <= n; i++)
        s += gwy_powi(i, p);
    return s;
}

/**
 * gwy_standardize_direction:
 * @phi: An angle (in radians).
 *
 * Standardises an angle of an undirected line in plane.
 *
 * Undirected means the also angles @phi and @phi+π are considered equivalent,
 * not just @phi and @phi+2π.
 *
 * Returns: Angle @phi standardised to the interval (-π/2,π/2].
 **/
gdouble
gwy_standardize_direction(gdouble phi)
{
    phi = fmod(phi, G_PI);
    if (phi <= -G_PI/2.0)
        phi += G_PI;
    if (phi > G_PI/2.0)
        phi -= G_PI;
    return phi;
}

/**
 * gwy_overlapping:
 * @pos1: First segment start.
 * @len1: First segment length.
 * @pos2: Second segment start.
 * @len2: Second segment length.
 *
 * Checks whether two pixel-wise segments are overlapping.
 *
 * Returns: %TRUE if the two segments are overlapping, %FALSE if they are
 *          separate.
 **/
gboolean
gwy_overlapping(guint pos1, guint len1,
                guint pos2, guint len2)
{
    return MAX(pos1 + len1, pos2 + len2) - MIN(pos1, pos2) < len1 + len2;
}

/**
 * gwy_math_intersecting:
 * @a: First interval left endpoint.
 * @b: First interval right endpoint.
 * @A: Second interval left endpoint.
 * @B: Second interval right endpoint.
 *
 * Checks whether two intervals intersect.
 *
 * Intervals are considered to be closed, i.e. if they intersect only in an
 * endpoint this function returns %TRUE.  This makes it useful also for
 * zero-length intervals.
 *
 * Returns: %TRUE if the two intervals intersect, %FALSE if they are
 *          separate.
 **/
gboolean
gwy_math_intersecting(gdouble a, gdouble b,
                      gdouble A, gdouble B)
{
    return (a < A) ? b >= A : a <= B;
}

static guint
calc_quadratic_curvatue(GwyCurvatureParams *curvature,
                        gdouble a, gdouble bx, gdouble by,
                        gdouble cxx, gdouble cxy, gdouble cyy)
{
    /* At least one quadratic term */
    gdouble cm = cxx - cyy;
    gdouble cp = cxx + cyy;
    gdouble phi = 0.5*atan2(cxy, cm);
    gdouble cx = cp + hypot(cm, cxy);
    gdouble cy = cp - hypot(cm, cxy);
    gdouble bx1 = bx*cos(phi) + by*sin(phi);
    gdouble by1 = -bx*sin(phi) + by*cos(phi);
    guint degree = 2;
    gdouble xc, yc;

    /* Eliminate linear terms */
    if (fabs(cx) < 1e-10*fabs(cy)) {
        /* Only y quadratic term */
        xc = 0.0;
        yc = -by1/cy;
        degree = 1;
    }
    else if (fabs(cy) < 1e-10*fabs(cx)) {
        /* Only x quadratic term */
        xc = -bx1/cx;
        yc = 0.0;
        degree = 1;
    }
    else {
        /* Two quadratic terms */
        xc = -bx1/cx;
        yc = -by1/cy;
    }

    curvature->xc = xc*cos(phi) - yc*sin(phi);
    curvature->yc = xc*sin(phi) + yc*cos(phi);
    curvature->zc = a + xc*bx1 + yc*by1 + 0.5*(xc*xc*cx + yc*yc*cy);

    if (cx > cy) {
        GWY_SWAP(gdouble, cx, cy);
        phi += G_PI/2.0;
    }

    curvature->k1 = cx;
    curvature->k2 = cy;
    curvature->phi1 = phi;
    curvature->phi2 = phi + G_PI/2.0;

    return degree;
}

/**
 * gwy_math_curvature_at_centre:
 * @coeffs: (array fixed-size=6):
 *          Array of the six polynomial coefficients of a quadratic surface in
 *          the following order: 1, x, y, x², xy, y².
 * @curvature: (out):
 *             Location to store the curvature parameters.
 *
 * Calculates curvature parameters from two-dimensional quadratic polynomial
 * coefficients.
 *
 * The coefficients should be scaled so that the lateral dimensions of the
 * relevant area (e.g. fitting area if the coefficients were obtained by
 * fitting a quadratic surface) do not differ from 1 by many orders.  Otherwise
 * recognition of flat surfaces will not work.
 *
 * Returns: The number of curved dimensions (0 to 2).
 **/
guint
gwy_math_curvature_at_centre(const gdouble *coeffs,
                             GwyCurvatureParams *curvature)
{
    g_return_val_if_fail(curvature, 0);

    gdouble a = coeffs[0], bx = coeffs[1], by = coeffs[2],
            cxx = coeffs[3], cxy = coeffs[4], cyy = coeffs[5];

    /* Eliminate the mixed term */
    if (fabs(cxx) + fabs(cxy) + fabs(cyy) <= 1e-10*(fabs(bx) + fabs(by))) {
        /* Linear gradient */
        gwy_clear(curvature, 1);
        curvature->phi2 = G_PI/2.0;
        curvature->zc = a;
        return 0;
    }

    guint degree = calc_quadratic_curvatue(curvature,
                                           a, bx, by, cxx, cxy, cyy);

    curvature->phi1 = gwy_standardize_direction(curvature->phi1);
    curvature->phi2 = gwy_standardize_direction(curvature->phi2);

    return degree;
}

/**
 * gwy_math_curvature_at_origin:
 * @coeffs: (array fixed-size=6):
 *          Array of the six polynomial coefficients of a quadratic surface in
 *          the following order: 1, x, y, x², xy, y².
 * @curvature: (out):
 *             Location to store the curvature parameters.
 *
 * Calculates curvature parameters at origin from two-dimensional quadratic
 * polynomial coefficients.
 *
 * Unlike gwy_math_curvature_at_centre() which calculates parameters at the
 * centre of cuvrature, this function calculates the curvature at origin.  This
 * means the centre is also always set to the origin and the value at centre is
 * simply equal to the constant term.
 *
 * See gwy_math_curvature_at_centre() for a coefficient normalisation note.
 *
 * Returns: The number of curved dimensions (0 to 2).
 **/
guint
gwy_math_curvature_at_origin(const gdouble *coeffs,
                             GwyCurvatureParams *curvature)
{
    g_return_val_if_fail(curvature, 0);

    gdouble a = coeffs[0], bx = coeffs[1], by = coeffs[2],
            cxx = coeffs[3], cxy = coeffs[4], cyy = coeffs[5];

    /* Eliminate the mixed term */
    if (fabs(cxx) + fabs(cxy) + fabs(cyy) <= 1e-10*(fabs(bx) + fabs(by))) {
        /* Linear gradient */
        gwy_clear(curvature, 1);
        curvature->phi2 = G_PI/2.0;
        curvature->zc = a;
        return 0;
    }

    gdouble b = hypot(bx, by), beta = atan2(by, bx);
    if (b > 1e-10) {
        gdouble cosbeta = bx/b,
                sinbeta = by/b,
                cbeta2 = cosbeta*cosbeta,
                sbeta2 = sinbeta*sinbeta,
                csbeta = cosbeta*sinbeta,
                qb = hypot(1.0, b);
        gdouble cxx1 = (cxx*cbeta2 + cxy*csbeta + cyy*sbeta2)/(qb*qb*qb),
                cxy1 = (2.0*(cyy - cxx)*csbeta + cxy*(cbeta2 - sbeta2))/(qb*qb),
                cyy1 = (cyy*cbeta2 - cxy*csbeta + cxx*sbeta2)/qb;
        cxx = cxx1;
        cxy = cxy1;
        cyy = cyy1;
    }
    else
        beta = 0.0;

    guint degree = calc_quadratic_curvatue(curvature,
                                           a, 0, 0, cxx, cxy, cyy);

    curvature->phi1 = gwy_standardize_direction(curvature->phi1 + beta);
    curvature->phi2 = gwy_standardize_direction(curvature->phi2 + beta);
    // This should already hold approximately.  Enforce it exactly.
    curvature->xc = curvature->yc = 0.0;
    curvature->zc = a;

    return degree;
}

/**
 * gwy_line_point_distance2:
 * @lx0: X-coordinate of first line endpoint.
 * @ly0: Y-coordinate of first line endpoint.
 * @lx1: X-coordinate of second line endpoint.
 * @ly1: Y-coordinate of second line endpoint.
 * @x: X-coordinate of the point.
 * @y: Y-coordinate of the point.
 * @endpoint: (out) (allow-none):
 *            Location to store which endpoint was closed, or %NULL.
 *
 * Calculates the distance from a point to a line segment.
 *
 * Returns: Squared distance from point (@x,@y) to the line between points
 *          (@lx0,@ly) and (@lx1,@ly1).
 **/
gdouble
gwy_line_point_distance2(gdouble lx0, gdouble ly0, gdouble lx1, gdouble ly1,
                         gdouble x, gdouble y,
                         guint *endpoint)
{
    gdouble lx = lx1 - lx0, ly = ly1 - ly0, l2 = lx*lx + ly*ly;
    gdouble dx0 = x - lx0, dy0 = y - ly0,
            dx1 = x - lx1, dy1 = y - ly1;
    gdouble d02 = dx0*dx0 + dy0*dy0, d12 = dx1*dx1 + dy1*dy1;

    GWY_MAYBE_SET(endpoint, d12 <= d02);

    // Degenerated line
    if (!l2)
        return 0.5*(d02 + d12);

    // Point is before (lx0, ly0)
    if ((lx1 - x)*lx + (ly1 - y)*ly > l2)
        return d02;

    // Point is beyond (lx1, ly1)
    if ((x - lx0)*lx + (y - ly0)*ly > l2)
        return d12;

    gdouble mx = 0.5*(lx0 + lx1), my = 0.5*(ly0 + ly1);
    gdouble dmx = x - mx, dmy = y - my, dm2 = dmx*lx + dmy*ly;
    return dmx*dmx + dmy*dmy - dm2*dm2/l2;
}

/* Quickly find median value in an array
 * based on public domain code by Nicolas Devillard */
/**
 * gwy_math_median:
 * @array: Array of doubles.  It is modified by this function.  All values are
 *         kept, but their positions in the array change.
 * @n: Number of items in @array.
 *
 * Finds median of an array of values using Quick select algorithm.
 *
 * Returns: The median value of @array.
 **/
gdouble
gwy_math_median(gdouble *array, gsize n)
{
    gsize lo, hi;
    gsize median;
    gsize middle, ll, hh;

    g_return_val_if_fail(n, NAN);

    lo = 0;
    hi = n - 1;
    median = n/2;
    while (TRUE) {
        if (hi <= lo)        /* One element only */
            return array[median];

        if (hi == lo + 1) {  /* Two elements only */
            if (array[lo] > array[hi])
                DSWAP(array[lo], array[hi]);
            return array[median];
        }

        /* Find median of lo, middle and hi items; swap into position lo */
        middle = (lo + hi)/2;
        if (array[middle] > array[hi])
            DSWAP(array[middle], array[hi]);
        if (array[lo] > array[hi])
            DSWAP(array[lo], array[hi]);
        if (array[middle] > array[lo])
            DSWAP(array[middle], array[lo]);

        /* Swap low item (now in position middle) into position (lo+1) */
        DSWAP(array[middle], array[lo + 1]);

        /* Nibble from each end towards middle, swapping items when stuck */
        ll = lo + 1;
        hh = hi;
        while (TRUE) {
            do {
                ll++;
            } while (array[lo] > array[ll]);
            do {
                hh--;
            } while (array[hh] > array[lo]);

            if (hh < ll)
                break;

            DSWAP(array[ll], array[hh]);
        }

        /* Swap middle item (in position lo) back into correct position */
        DSWAP(array[lo], array[hh]);

        /* Re-set active partition */
        if (hh <= median)
            lo = ll;
        if (hh >= median)
            hi = hh - 1;
    }
}

/**
 * gwy_cholesky_decompose:
 * @matrix: Lower triangular part of a symmetric, presumably positive definite,
 *          matrix.  See gwy_lower_triangular_matrix_index() for storage
 *          details.
 * @n: Dimension of @matrix.
 *
 * Decomposes a symmetric positive definite matrix in place.
 *
 * Regardless of the return value, the contents of @matrix is overwritten.
 * In case of failure, it will not contain any meaningful values.
 *
 * Returns: %TRUE if the decomposition succeeded, %FALSE if the matrix was not
 *          numerically positive definite.
 **/
gboolean
gwy_cholesky_decompose(gdouble *a, guint n)
{
    for (guint k = 0; k < n; k++) {
        /* diagonal element */
        gdouble s = SLi(a, k, k);
        for (guint i = 0; i < k; i++)
            s -= SLi(a, k, i) * SLi(a, k, i);
        if (s <= 0.0)
            return FALSE;
        SLi(a, k, k) = s = sqrt(s);

        /* nondiagonal elements */
        for (guint j = k+1; j < n; j++) {
            gdouble r = SLi(a, j, k);
            for (guint i = 0; i < k; i++)
                r -= SLi(a, k, i) * SLi(a, j, i);
            SLi(a, j, k) = r/s;
        }
    }

    return TRUE;
}

/**
 * gwy_cholesky_solve:
 * @decomp: Lower triangular part of Cholesky decomposition as computed
 *          by gwy_cholesky_decompose().
 * @rhs: Right hand side vector.  It is modified in place, on return it
 *       contains the solution.
 * @n: Dimension of @decomp and length of @rhs.
 *
 * Solves a system of linear equations with predecomposed symmetric positive
 * definite matrix.
 *
 * Once the matrix is decomposed, this function can be used repeatedly to
 * calculate the solution of the system with different right-hand sides.
 **/
void
gwy_cholesky_solve(const gdouble *a, gdouble *b, guint n)
{
    guint i, j;

    /* back-substitution with the lower triangular matrix */
    for (j = 0; j < n; j++) {
        for (i = 0; i < j; i++)
            b[j] -= SLi(a, j, i)*b[i];
        b[j] /= SLi(a, j, j);
    }

    /* back-substitution with the upper triangular matrix */
    for (j = n; j > 0; j--) {
        for (i = j; i < n; i++)
            b[j-1] -= SLi(a, i, j-1)*b[i];
        b[j-1] /= SLi(a, j-1, j-1);
    }
}

/**
 * gwy_cholesky_invert:
 * @matrix: Lower triangular part of a symmetric, presumably positive definite,
 *          matrix.  See gwy_lower_triangular_matrix_index() for storage
 *          details.
 * @n: Dimension of @matrix.
 *
 * Inverts a positive definite matrix in place.
 *
 * Regardless of the return value, the contents of @matrix is overwritten.
 * In case of failure, it will not contain any meaningful values.
 *
 * Returns: %TRUE if the inversion succeeded, %FALSE if the matrix was not
 *          numerically positive definite.
 **/
gboolean
gwy_cholesky_invert(gdouble *a, guint n)
{
    gdouble x[n];
    for (guint k = n; k > 0; k--) {
        gdouble s = a[0];
        if (s <= 0)
            return FALSE;
        guint m = 0, q = 0;
        for (guint i = 0; i < n-1; i++) {
            q = m+1;
            m += i+2;
            gdouble t = a[q];
            x[i] = -t/s;      /* note use temporary x */
            if (i >= k-1)
                x[i] = -x[i];
            for (guint j = q; j < m; j++)
                a[j - (i+1)] = a[j+1] + t * x[j - q];
        }
        a[m] = 1.0/s;
        for (guint i = 0; i < n-1; i++)
            a[q + i] = x[i];
    }
    return TRUE;
}

/**
 * gwy_cholesky_multiply:
 * @matrix: Lower triangular part of a symmetric matrix.  See
 *          gwy_lower_triangular_matrix_index() for storage details.
 * @vec: (inout):
 *       Vector to be multiplied, it will be modified in place.
 * @n: Dimension of @matrix and @vec.
 *
 * Multiplies a vector with a symmetrical matrix in triangular storage form.
 *
 * Note this function interprets @matrix as symmetrical,
 * <emphasis>not</emphasis> triangular, even though only half of the matrix
 * is kept.  See gwy_triangular_multiply_right() and
 * gwy_triangular_multiply_left() for multiplication with triangular matrices.
 *
 * Since the matrix is symmetrical, left and right multiplication lead to the
 * same result.  The matrix does not need to be positive definite.
 **/
void
gwy_cholesky_multiply(const gdouble *m, gdouble *v, guint n)
{
    gdouble a[n];
    for (guint i = 0; i < n; i++) {
        a[i] = 0.0;
        for (guint j = 0; j < n; j++) {
            guint ij = MAX(i, j);
            guint ji = MIN(i, j);
            a[i] += SLi(m, ij, ji) * v[j];
        }
    }
    gwy_assign(v, a, n);
}

/**
 * gwy_cholesky_dotprod:
 * @matrix: Lower triangular part of a symmetric matrix.  Presumably positive
 *          semidefinite in order to actually represent a scalar product.
 *          See gwy_lower_triangular_matrix_index() for storage details.
 * @veca: First vector of the scalar product.
 * @vecb: Second vector of the scalar product.
 * @n: Dimension of @matrix, @veca and @vecb.
 *
 * Calculates dot product of two vectors given a symmetrical matrix.
 *
 * Returns: The scalar product of the two vectors.
 **/
gdouble
gwy_cholesky_dotprod(const gdouble *m,
                     const gdouble *a,
                     const gdouble *b,
                     guint n)
{
    gdouble s = 0.0;
    for (guint i = 0; i < n; i++) {
        gdouble ai = a[i], bi = b[i];
        for (guint j = 0; j < i; j++, m++)
            s += (*m)*(ai*b[j] + a[j]*bi);
        s += (*m)*ai*bi;
        m++;
    }
    return s;
}

/**
 * gwy_triangular_multiply_right:
 * @vec: (inout):
 *       Vector to be multiplied, it will be modified in place.
 * @matrix: Lower triangular part of a symmetric, presumably positive definite,
 *          matrix.  See gwy_lower_triangular_matrix_index() for storage
 *          details.
 * @n: Dimension of @matrix and @vec.
 *
 * Multiplies a column vector by a upper right triangular matrix.
 *
 * The operation can be considered either left-multiplication of column vector
 * by upper right triangular matrix, or right-multiplication of row vector with
 * lower left triangular matrix.  The result is the same.
 *
 * The matrix will be typically obtained by gwy_cholesky_decompose() but this
 * function does not require any property such as positive definiteness.
 **/
void
gwy_triangular_multiply_right(gdouble *vec, const gdouble *a, guint n)
{
    gdouble x[n];
    gwy_clear(x, n);
    for (guint i = 0; i < n; i++) {
        gdouble vi = vec[i];
        for (guint j = 0; j <= i; j++, a++)
            x[j] += (*a)*vi;
    }
    gwy_assign(vec, x, n);
}

/**
 * gwy_triangular_multiply_left:
 * @matrix: Lower triangular part of a symmetric, presumably positive definite,
 *          matrix.  See gwy_lower_triangular_matrix_index() for storage
 *          details.
 * @vec: (inout):
 *       Vector to be multiplied, it will be modified in place.
 * @n: Dimension of @matrix and @vec.
 *
 * Multiplies a column vector by a lower left triangular matrix.
 *
 * The operation can be considered either left-multiplication of column vector
 * by lower left triangular matrix, or right-multiplication of row vector with
 * upper right triangular matrix.  The result is the same.
 *
 * The matrix will be typically obtained by gwy_cholesky_decompose() but this
 * function does not require any property such as positive definiteness.
 **/
void
gwy_triangular_multiply_left(const gdouble *a, gdouble *vec, guint n)
{
    a += MATRIX_LEN(n) - 1;
    for (guint i = n; i; i--) {
        gdouble s = 0;
        const gdouble *v = vec + (i-1);
        for (guint j = i; j; j--, v--, a--)
            s += (*a)*(*v);
        // Overwrite vec[i] with the result, we will not need the tail values.
        vec[i-1] = s;
    }
}

// Note it does not return the norm but the norm divided by √n.
static gdouble
symetrical_norm2(const gdouble *a, guint n)
{
    gdouble s = 0.0;

    for (guint i = 0; i < n; i++) {
        for (guint j = 0; j < i; j++) {
            s += 2.0*(*a)*(*a);
            a++;
        }
        s += (*a)*(*a);
        a++;
    }

    return sqrt(s/MATRIX_LEN(n));
}

/**
 * gwy_cholesky_condition:
 * @matrix: Lower triangular part of a symmetric, presumably positive definite,
 *          matrix.  See gwy_lower_triangular_matrix_index() for storage
 *          details.
 * @n: Dimension of @matrix.
 *
 * Estimates condition number of a symmetric positive definite matrix.
 *
 * The estimate is obtained by actually inverting the matrix and calculating
 * the L₂ norms of the matrix and its inverse.  If the inversion fails the
 * matrix is considered to be singular.
 *
 * Returns: The estimated conditon number, <constant>HUGE_VAL</constant> if the
 *          matrix appears to be singular.
 **/
gdouble
gwy_cholesky_condition(const gdouble *a, guint n)
{
    gdouble c = symetrical_norm2(a, n);
    if (G_UNLIKELY(!c))
        return HUGE_VAL;

    guint len = MATRIX_LEN(n);
    gdouble *inva = g_slice_alloc(len*sizeof(gdouble));
    // Factor out the norm of a, making the elements ~1 for condition number 1.
    for (guint i = 0; i < len; i++)
        inva[i] = a[i]/c;
    gboolean ok = gwy_cholesky_invert(inva, n);
    gdouble cinv = ok ? symetrical_norm2(inva, n) : HUGE_VAL;
    g_slice_free1(len*sizeof(gdouble), inva);
    return cinv;
}

/* Solve an arbitrary number (i.e. @k) of linear systems with the same matrix
 * simultaneously.  @b and @k are arrays of size @k×@n.  This interface is too
 * silly to make it public... */
static gboolean
gwy_linalg_multisolve(gdouble *a,
                      gdouble *b,
                      gdouble *result,
                      guint n,
                      guint k)
{
    g_return_val_if_fail(n > 0, FALSE);
    g_return_val_if_fail(k > 0, FALSE);
    g_return_val_if_fail(a && b && result, FALSE);

    gint perm[n];

    // Elimination.
    for (guint i = 0; i < n; i++) {
        gdouble *row = a + i*n;
        gdouble piv = 0;
        guint pivj = 0;

        // Find pivot.
        for (guint j = 0; j < n; j++) {
            if (fabs(row[j]) > piv) {
                pivj = j;
                piv = fabs(row[j]);
            }
        }
        if (!piv)
            return FALSE;
        piv = row[pivj];
        perm[i] = pivj;

        // Subtract rows.
        for (guint j = i+1; j < n; j++) {
            gdouble *jrow = a + j*n;
            gdouble q = jrow[pivj]/piv;

            for (guint jj = 0; jj < n; jj++)
                jrow[jj] -= q*row[jj];

            jrow[pivj] = 0.0;
            for (guint m = 0; m < k; m++)
                b[m*n + j] -= q*b[m*n + i];
        }
    }

    // Back substitute.
    for (guint m = 0; m < k; m++) {
        for (guint i = n; i > 0; ) {
            i--;
            gdouble *row = a + i*n;
            gdouble x = b[m*n + i];

            for (guint j = n-1; j > i; j--)
                x -= result[m*n + perm[j]]*row[perm[j]];

            result[m*n + perm[i]] = x/row[perm[i]];
        }
    }

    return TRUE;
}

/**
 * gwy_linalg_solve:
 * @matrix: Matrix of the system (@n×@n), ordered by row, then column.
 *          It will be overwritten during the solving with intermediate
 *          results.
 * @rhs: Right hand side of the sytem.  It will be overwritten during the
 *       solving with intermediate results.
 * @result: Array of length @n to store solution to.
 * @n: Size of the system.
 *
 * Solves a regular system of linear equations.
 *
 * The solution is calculated by simple Gauss elimination with partial
 * pivoting.
 *
 * Returns: %TRUE if the calculation succeeded, %FALSE if the matrix was found
 *          to be numerically singular.
 **/
gboolean
gwy_linalg_solve(gdouble *a,
                 gdouble *b,
                 gdouble *result,
                 guint n)
{
    return gwy_linalg_multisolve(a, b, result, n, 1);
}

/**
 * gwy_linalg_invert:
 * @matrix: Matrix @n×@n, ordered by row, then column.  It will be overwritten
 *          during the inversion with intermediate results.
 * @inv: Matrix @n×@n to store the inverted @a to.
 * @n: Size of the system.
 *
 * Inverts a regular square matrix.
 *
 * The inversion is calculated by simple Gauss elimination with partial
 * pivoting.
 *
 * Returns: %TRUE if the calculation succeeded, %FALSE if the matrix was found
 *          to be numerically singular.
 **/
gboolean
gwy_linalg_invert(gdouble *a,
                  gdouble *inv,
                  guint n)
{
    g_return_val_if_fail(n > 0, FALSE);
    gdouble *unity = g_slice_alloc0(n*n*sizeof(gdouble));
    for (guint i = 0; i < n; i++)
        unity[i*n + i] = 1.0;
    gboolean ok = gwy_linalg_multisolve(a, unity, inv, n, n);
    for (guint i = 0; i < n; i++) {
        for (guint j = 0; j < i; j++)
            DSWAP(inv[i*n + j], inv[j*n + i]);
    }
    g_slice_free1(n*n*sizeof(gdouble), unity);
    return ok;
}

/**
 * gwy_linear_fit:
 * @function: (scope call):
 *            Function to fit.
 * @npoints: Number of fitted points, it must be larger than the number of
 *           parameters.
 * @params: (out) (array length=nparams):
 *          Array of length @nparams to store the parameters (coefficients)
 *          corresponding to the minimum on success.  It will be overwritten
 *          also on failure but not with anything useful.
 * @nparams: The number of parameters.
 * @residuum: Location to store the residual sum of squares to, or %NULL if
 *            you are not interested.
 * @user_data: User data to pass to @function.
 *
 * Performs a linear least-squares fit.
 *
 * An example demonstrating the fitting of one-dimensional data with a
 * quadratic polynomial:
 * |[
 * gdouble
 * *poly2(guint i, gdouble *fvalues, GwyXY *data)
 * {
 *     gdouble x = data[i].x;
 *     fvalues[0] = 1.0;
 *     fvalues[1] = x;
 *     fvalues[2] = x*x;
 *     return data[i].y;
 * }
 *
 * gboolean
 * fit_poly2(GwyXY *data, guint ndata, gdouble *coefficients)
 * {
 *     return gwy_linear_fit((GwyLinearFitFunc)poly2, ndata, coefficients, 3,
 *                           NULL, data);
 * }
 * ]|
 *
 * Returns: %TRUE on success, %FALSE on failure namely when the matrix is not
 *          found to be positive definite.
 **/
gboolean
gwy_linear_fit(GwyLinearFitFunc function,
               guint npoints,
               gdouble *params,
               guint nparams,
               gdouble *residuum,
               gpointer user_data)
{
    g_return_val_if_fail(npoints > nparams, FALSE);
    g_return_val_if_fail(nparams > 0, FALSE);
    g_return_val_if_fail(function && params, FALSE);

    guint matrix_len = MATRIX_LEN(nparams);
    gdouble hessian[matrix_len];
    gdouble fval[nparams];
    gwy_clear(hessian, matrix_len);
    gwy_clear(params, nparams);
    gdouble sumy2 = 0.0;

    for (guint i = 0; i < npoints; i++) {
        gdouble y;
        if (function(i, fval, &y, user_data)) {
            sumy2 += y*y;
            for (guint j = 0; j < nparams; j++) {
                params[j] += y*fval[j];
                for (guint k = 0; k <= j; k++) {
                    SLi(hessian, j, k) += fval[j]*fval[k];
                }
            }
        }
    }
    if (!gwy_cholesky_decompose(hessian, nparams))
        return FALSE;
    if (residuum)
        gwy_assign(fval, params, nparams);
    gwy_cholesky_solve(hessian, params, nparams);
    if (residuum) {
        for (guint j = 0; j < nparams; j++)
            sumy2 -= params[j]*fval[j];
        *residuum = MAX(sumy2, 0.0);
    }
    return TRUE;
}

/**
 * gwy_create_xy_poly_powers_sum:
 * @degree: Polynomial degree.
 * @xpowers: (out):
 *           Array of gwy_triangular_matrix_length(@degree) items to fill with
 *           powers of @x.
 * @ypowers: (out):
 *           Array of gwy_triangular_matrix_length(@degree) items to fill with
 *           powers of @y.
 *
 * Constructs x and y powers for polynomial fitting in plane with limited total
 * degree.
 *
 * The arrays are filled with all combinations of non-negative @x and @y powers
 * that do not exceed, in the total degree, given @degree.
 **/
void
gwy_create_xy_poly_powers_sum(guint degree,
                              guint *xpowers,
                              guint *ypowers)
{
    g_return_if_fail(xpowers && ypowers);

    guint k = 0;
    for (guint i = 0; i <= degree; i++) {
        for (guint j = 0; j <= degree-i; j++) {
            xpowers[k] = j;
            ypowers[k] = i;
        }
    }
}

/**
 * gwy_assert_floatval:
 * @val: Floating point value to check.
 * @ref: Reference value for @val.
 * @tol: Tolerance for @val.
 *
 * Debugging macro to terminate the application with a warning message if a
 * floating point value fails to fall into given tolerance from reference
 * value.
 *
 * The effect of gwy_assert_floatval(@val, @ref, @tol) is the same as
 * g_assert(fabsl(@val - @ref) &lt;= @tol). The advantage of this macro is that
 * it can produce a message that includes the actual values entering the
 * tolerance check.
 **/

/**
 * gwy_assertion_message_floatval:
 * @domain: Log domain.
 * @file: File in which the assertion failed.
 * @line: Line at which the assertion failed.
 * @func: Function in which the assertion failed.
 * @expr: Expression representing the failed comparison.
 * @val: Value that was found to be out of tolerance.
 * @ref: Reference value for @val.
 * @tol: Tolerance for @val.
 *
 * Logs a failed assertion for numerical comparison and aborts the program.
 *
 * This function is used to implement gwy_assert_floatval().
 **/
void
gwy_assertion_message_floatval(const char *domain,
                               const char *file,
                               int line,
                               const char *func,
                               const char *expr,
                               long double val,
                               long double ref,
                               long double tol)
{
    gchar *s = g_strdup_printf("assertion failed (%s): "
                               "|%.17Lg - %.17Lg| = |%Lg| <= %Lg",
                               expr, val, ref, val-ref, tol);
    g_assertion_message(domain, file, line, func, s);
    g_free(s);
}

/**
 * gwy_double_compare:
 * @a: Pointer to the first double value.
 * @b: Pointer to the second double value.
 *
 * Compares two doubles given pointer to them.
 *
 * This function is intended to be passed as #GCompareFunc or #GCompareDataFunc
 * (typecased) to GLib functions.  It does not treat NaNs specifically.
 *
 * Returns: A negative integer if the first value comes before the second, 0 if
 *          they are equal, or a positive integer if the first value comes
 *          after the second.
 **/
gint
gwy_double_compare(gconstpointer pa,
                   gconstpointer pb)
{
    gdouble a = *(const gdouble*)pa;
    gdouble b = *(const gdouble*)pb;

    if (a < b)
        return -1;
    if (a > b)
        return 1;
    return 0;
}

/**
 * gwy_double_direct_compare:
 * @a: The first double value, typecast to #gpointer.
 * @b: The second double value, typecast to #gpointer.
 *
 * Compares two doubles typecast to pointers.
 *
 * Unlike gwy_double_compare() which takes pointers to double values, this
 * function compares doubles that are directly stored in the pointers (i.e.
 * have benn typecast to them).  This obviously works only if pointers are
 * at least 64bit.
 *
 * Returns: A negative integer if the first value comes before the second, 0 if
 *          they are equal, or a positive integer if the first value comes
 *          after the second.
 **/
gint
gwy_double_direct_compare(gconstpointer pa,
                          gconstpointer pb)
{
    union { gdouble d; gconstpointer p; } a = { .p = pa }, b = { .p = pb };

    if (a.d < b.d)
        return -1;
    if (a.d > b.d)
        return 1;
    return 0;
}

/**
 * gwy_math_sort:
 * @n: Number of items in @array.
 * @array: (inout) (array length=n):
 *         Array of doubles to sort in place.
 * @index_array: (inout) (allow-none) (array length=n):
 *               Array of integer identifiers of the items that are permuted
 *               simultaneously with @array.  Pass %NULL if the identity
 *               of the items need not be preserved.
 *
 * Sorts an array of doubles using a quicksort algorithm.
 *
 * With %NULL @index_array this is usually about twice as fast as the generic
 * quicksort function thanks to specialization for doubles.
 *
 * The simplest and probably most common use of @index_array is to fill it with
 * numbers 0 to @n-1 before calling gwy_math_sort().  After sorting,
 * @index_array[@i] then contains the original position of the @i-th item of
 * the sorted array.
 **/
void
gwy_math_sort(gdouble *array,
              guint *index_array,
              gsize n)
{
    if (index_array)
        sort_with_index(array, index_array, n);
    else
        sort_plain(array, n);
}

/****************************************************************************
 * Quicksort
 ****************************************************************************/

/* Note: The implementation was specialized for doubles and a few things were
 * renamed, otherwise it has not changed.  It is about twice as fast as the
 * generic version. */

/* Copyright (C) 1991, 1992, 1996, 1997, 1999 Free Software Foundation, Inc.
   This file is part of the GNU C Library.
   Written by Douglas C. Schmidt (schmidt@ics.uci.edu).

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

/* If you consider tuning this algorithm, you should consult first:
   Engineering a sort function; Jon Bentley and M. Douglas McIlroy;
   Software - Practice and Experience; Vol. 23 (11), 1249-1265, 1993.  */

/* Discontinue quicksort algorithm when partition gets below this size.
   This particular magic number was chosen to work best on a Sun 4/260. */
/* #define MAX_THRESH 4 */

/* The next 4 #defines implement a very fast in-line stack abstraction. */
/* The stack needs log (total_elements) entries (we could even subtract
   log(MAX_THRESH)).  Since total_elements has type size_t, we get as
   upper bound for log (total_elements):
   bits per byte (CHAR_BIT) * sizeof(size_t).  */
#define STACK_SIZE      (CHAR_BIT * sizeof(gsize))
#define PUSH(low, high) ((void) ((top->lo = (low)), (top->hi = (high)), ++top))
#define POP(low, high)  ((void) (--top, (low = top->lo), (high = top->hi)))
#define STACK_NOT_EMPTY (stack < top)

/* Order size using quicksort.  This implementation incorporates
   four optimizations discussed in Sedgewick:

   1. Non-recursive, using an explicit stack of pointer that store the
   next array partition to sort.  To save time, this maximum amount
   of space required to store an array of SIZE_MAX is allocated on the
   stack.  Assuming a 32-bit (64 bit) integer for size_t, this needs
   only 32 * sizeof(stack_node) == 256 bytes (for 64 bit: 1024 bytes).
   Pretty cheap, actually.

   2. Chose the pivot element using a median-of-three decision tree.
   This reduces the probability of selecting a bad pivot value and
   eliminates certain extraneous comparisons.

   3. Only quicksorts TOTAL_ELEMS / MAX_THRESH partitions, leaving
   insertion sort to order the MAX_THRESH items within each partition.
   This is a big win, since insertion sort is faster for small, mostly
   sorted array segments.

   4. The larger of the two sub-partitions is always pushed onto the
   stack first, with the algorithm then concentrating on the
   smaller partition.  This *guarantees* no more than log(n)
   stack size is needed (actually O(1) in this case)!  */

static void
sort_plain(gdouble *array,
           gsize n)
{
    /* Note: Specialization makes the insertion sort part relatively more
     * efficient, after some benchmarking this seems be about the best value
     * on Athlon 64. */
    enum { MAX_THRESH = 20 };

    // Stack node declarations used to store unfulfilled partition obligations.
    typedef struct {
        gdouble *lo;
        gdouble *hi;
    } stack_node;

    if (n < 2)
        /* Avoid lossage with unsigned arithmetic below.  */
        return;

    if (n > MAX_THRESH) {
        gdouble *lo = array;
        gdouble *hi = lo + (n - 1);
        stack_node stack[STACK_SIZE];
        stack_node *top = stack + 1;

        while (STACK_NOT_EMPTY) {
            gdouble *left_ptr;
            gdouble *right_ptr;

            /* Select median value from among LO, MID, and HI. Rearrange
               LO and HI so the three values are sorted. This lowers the
               probability of picking a pathological pivot value and
               skips a comparison for both the LEFT_PTR and RIGHT_PTR in
               the while loops. */

            gdouble *mid = lo + ((hi - lo) >> 1);

            if (*mid < *lo)
                DSWAP(*mid, *lo);
            if (*hi < *mid)
                DSWAP(*mid, *hi);
            else
                goto jump_over;
            if (*mid < *lo)
                DSWAP(*mid, *lo);

jump_over:
          left_ptr  = lo + 1;
          right_ptr = hi - 1;

          /* Here's the famous ``collapse the walls'' section of quicksort.
             Gotta like those tight inner loops!  They are the main reason
             that this algorithm runs much faster than others. */
          do {
              while (*left_ptr < *mid)
                  left_ptr++;

              while (*mid < *right_ptr)
                  right_ptr--;

              if (left_ptr < right_ptr) {
                  DSWAP(*left_ptr, *right_ptr);
                  if (mid == left_ptr)
                      mid = right_ptr;
                  else if (mid == right_ptr)
                      mid = left_ptr;
                  left_ptr++;
                  right_ptr--;
              }
              else if (left_ptr == right_ptr) {
                  left_ptr++;
                  right_ptr--;
                  break;
              }
          }
          while (left_ptr <= right_ptr);

          /* Set up pointers for next iteration.  First determine whether
             left and right partitions are below the threshold size.  If so,
             ignore one or both.  Otherwise, push the larger partition's
             bounds on the stack and continue sorting the smaller one. */

          if ((gsize)(right_ptr - lo) <= MAX_THRESH) {
              if ((gsize)(hi - left_ptr) <= MAX_THRESH)
                  /* Ignore both small partitions. */
                  POP(lo, hi);
              else
                  /* Ignore small left partition. */
                  lo = left_ptr;
          }
          else if ((gsize)(hi - left_ptr) <= MAX_THRESH)
              /* Ignore small right partition. */
              hi = right_ptr;
          else if ((right_ptr - lo) > (hi - left_ptr)) {
              /* Push larger left partition indices. */
              PUSH(lo, right_ptr);
              lo = left_ptr;
          }
          else {
              /* Push larger right partition indices. */
              PUSH(left_ptr, hi);
              hi = right_ptr;
          }
        }
    }

    /* Once the BASE_PTR array is partially sorted by quicksort the rest
       is completely sorted using insertion sort, since this is efficient
       for partitions below MAX_THRESH size. BASE_PTR points to the beginning
       of the array to sort, and END_PTR points at the very last element in
       the array (*not* one beyond it!). */

    {
        gdouble *const end_ptr = array + (n - 1);
        gdouble *tmp_ptr = array;
        gdouble *thresh = MIN(end_ptr, array + MAX_THRESH);
        gdouble *run_ptr;

        /* Find smallest element in first threshold and place it at the
           array's beginning.  This is the smallest array element,
           and the operation speeds up insertion sort's inner loop. */

        for (run_ptr = tmp_ptr + 1; run_ptr <= thresh; run_ptr++) {
            if (*run_ptr < *tmp_ptr)
                tmp_ptr = run_ptr;
        }

        if (tmp_ptr != array)
            DSWAP(*tmp_ptr, *array);

        /* Insertion sort, running from left-hand-side up to right-hand-side.
         */

        run_ptr = array + 1;
        while (++run_ptr <= end_ptr) {
            tmp_ptr = run_ptr - 1;
            while (*run_ptr < *tmp_ptr)
                tmp_ptr--;

            tmp_ptr++;
            if (tmp_ptr != run_ptr) {
                gdouble *hi, *lo;
                gdouble d;

                d = *run_ptr;
                for (hi = lo = run_ptr; --lo >= tmp_ptr; hi = lo)
                    *hi = *lo;
                *hi = d;
            }
        }
    }
}

/**
 * gwy_sort_uint:
 * @n: Number of items in @array.
 * @array: (inout) (array length=n):
 *         Array of unsigned integers to sort in place.
 *
 * Sorts an array of unsigned integers using a quicksort algorithm.
 **/
void
gwy_sort_uint(guint *array,
              gsize n)
{
    enum { MAX_THRESH = 15 };

    // Stack node declarations used to store unfulfilled partition obligations.
    typedef struct {
        guint *lo;
        guint *hi;
    } stack_node;

    if (n < 2)
        /* Avoid lossage with unsigned arithmetic below.  */
        return;

    if (n > MAX_THRESH) {
        guint *lo = array;
        guint *hi = lo + (n - 1);
        stack_node stack[STACK_SIZE];
        stack_node *top = stack + 1;

        while (STACK_NOT_EMPTY) {
            guint *left_ptr;
            guint *right_ptr;

            /* Select median value from among LO, MID, and HI. Rearrange
               LO and HI so the three values are sorted. This lowers the
               probability of picking a pathological pivot value and
               skips a comparison for both the LEFT_PTR and RIGHT_PTR in
               the while loops. */

            guint *mid = lo + ((hi - lo) >> 1);

            if (*mid < *lo)
                ISWAP(*mid, *lo);
            if (*hi < *mid)
                ISWAP(*mid, *hi);
            else
                goto jump_over;
            if (*mid < *lo)
                ISWAP(*mid, *lo);

jump_over:
          left_ptr  = lo + 1;
          right_ptr = hi - 1;

          /* Here's the famous ``collapse the walls'' section of quicksort.
             Gotta like those tight inner loops!  They are the main reason
             that this algorithm runs much faster than others. */
          do {
              while (*left_ptr < *mid)
                  left_ptr++;

              while (*mid < *right_ptr)
                  right_ptr--;

              if (left_ptr < right_ptr) {
                  ISWAP(*left_ptr, *right_ptr);
                  if (mid == left_ptr)
                      mid = right_ptr;
                  else if (mid == right_ptr)
                      mid = left_ptr;
                  left_ptr++;
                  right_ptr--;
              }
              else if (left_ptr == right_ptr) {
                  left_ptr++;
                  right_ptr--;
                  break;
              }
          }
          while (left_ptr <= right_ptr);

          /* Set up pointers for next iteration.  First determine whether
             left and right partitions are below the threshold size.  If so,
             ignore one or both.  Otherwise, push the larger partition's
             bounds on the stack and continue sorting the smaller one. */

          if ((gsize)(right_ptr - lo) <= MAX_THRESH) {
              if ((gsize)(hi - left_ptr) <= MAX_THRESH)
                  /* Ignore both small partitions. */
                  POP(lo, hi);
              else
                  /* Ignore small left partition. */
                  lo = left_ptr;
          }
          else if ((gsize)(hi - left_ptr) <= MAX_THRESH)
              /* Ignore small right partition. */
              hi = right_ptr;
          else if ((right_ptr - lo) > (hi - left_ptr)) {
              /* Push larger left partition indices. */
              PUSH(lo, right_ptr);
              lo = left_ptr;
          }
          else {
              /* Push larger right partition indices. */
              PUSH(left_ptr, hi);
              hi = right_ptr;
          }
        }
    }

    /* Once the BASE_PTR array is partially sorted by quicksort the rest
       is completely sorted using insertion sort, since this is efficient
       for partitions below MAX_THRESH size. BASE_PTR points to the beginning
       of the array to sort, and END_PTR points at the very last element in
       the array (*not* one beyond it!). */

    {
        guint *const end_ptr = array + (n - 1);
        guint *tmp_ptr = array;
        guint *thresh = MIN(end_ptr, array + MAX_THRESH);
        guint *run_ptr;

        /* Find smallest element in first threshold and place it at the
           array's beginning.  This is the smallest array element,
           and the operation speeds up insertion sort's inner loop. */

        for (run_ptr = tmp_ptr + 1; run_ptr <= thresh; run_ptr++) {
            if (*run_ptr < *tmp_ptr)
                tmp_ptr = run_ptr;
        }

        if (tmp_ptr != array)
            ISWAP(*tmp_ptr, *array);

        /* Insertion sort, running from left-hand-side up to right-hand-side.
         */

        run_ptr = array + 1;
        while (++run_ptr <= end_ptr) {
            tmp_ptr = run_ptr - 1;
            while (*run_ptr < *tmp_ptr)
                tmp_ptr--;

            tmp_ptr++;
            if (tmp_ptr != run_ptr) {
                guint *hi, *lo;
                guint d;

                d = *run_ptr;
                for (hi = lo = run_ptr; --lo >= tmp_ptr; hi = lo)
                    *hi = *lo;
                *hi = d;
            }
        }
    }
}

/* FIXME: It is questionable whether it is still more efficient to use pointers
 * instead of array indices when it effectively doubles the number of
 * variables.  This might force some variables from registers to memory... */
static void
sort_with_index(gdouble *array,
                guint *index_array,
                gsize n)
{
    enum { MAX_THRESH = 12 };

    // Stack node declarations used to store unfulfilled partition obligations.
    typedef struct {
        gdouble *lo;
        gdouble *hi;
        guint *loi;
        guint *hii;
    } stack_node;

    if (n < 2)
        /* Avoid lossage with unsigned arithmetic below.  */
        return;

    if (n > MAX_THRESH) {
        gdouble *lo = array;
        gdouble *hi = lo + (n - 1);
        guint *loi = index_array;
        guint *hii = loi + (n - 1);
        stack_node stack[STACK_SIZE];
        stack_node *top = stack + 1;

        while (STACK_NOT_EMPTY) {
            gdouble *left_ptr;
            gdouble *right_ptr;
            guint *left_ptri;
            guint *right_ptri;

            /* Select median value from among LO, MID, and HI. Rearrange
               LO and HI so the three values are sorted. This lowers the
               probability of picking a pathological pivot value and
               skips a comparison for both the LEFT_PTR and RIGHT_PTR in
               the while loops. */

            gdouble *mid = lo + ((hi - lo) >> 1);
            guint *midi = loi + ((hii - loi) >> 1);

            if (*mid < *lo) {
                DSWAP(*mid, *lo);
                ISWAP(*midi, *loi);
            }
            if (*hi < *mid) {
                DSWAP(*mid, *hi);
                ISWAP(*midi, *hii);
            }
            else
                goto jump_over;

            if (*mid < *lo) {
                DSWAP(*mid, *lo);
                ISWAP(*midi, *loi);
            }

jump_over:
          left_ptr  = lo + 1;
          right_ptr = hi - 1;
          left_ptri  = loi + 1;
          right_ptri = hii - 1;

          /* Here's the famous ``collapse the walls'' section of quicksort.
             Gotta like those tight inner loops!  They are the main reason
             that this algorithm runs much faster than others. */
          do {
              while (*left_ptr < *mid) {
                  left_ptr++;
                  left_ptri++;
              }

              while (*mid < *right_ptr) {
                  right_ptr--;
                  right_ptri--;
              }

              if (left_ptr < right_ptr) {
                  DSWAP(*left_ptr, *right_ptr);
                  ISWAP(*left_ptri, *right_ptri);
                  if (mid == left_ptr) {
                      mid = right_ptr;
                      midi = right_ptri;
                  }
                  else if (mid == right_ptr) {
                      mid = left_ptr;
                      midi = left_ptri;
                  }
                  left_ptr++;
                  left_ptri++;
                  right_ptr--;
                  right_ptri--;
              }
              else if (left_ptr == right_ptr) {
                  left_ptr++;
                  left_ptri++;
                  right_ptr--;
                  right_ptri--;
                  break;
              }
          }
          while (left_ptr <= right_ptr);

          /* Set up pointers for next iteration.  First determine whether
             left and right partitions are below the threshold size.  If so,
             ignore one or both.  Otherwise, push the larger partition's
             bounds on the stack and continue sorting the smaller one. */

          if ((gsize)(right_ptr - lo) <= MAX_THRESH) {
              if ((gsize)(hi - left_ptr) <= MAX_THRESH) {
                  /* Ignore both small partitions. */
                  --top;
                  lo = top->lo;
                  hi = top->hi;
                  loi = top->loi;
                  hii = top->hii;
              }
              else {
                  /* Ignore small left partition. */
                  lo = left_ptr;
                  loi = left_ptri;
              }
          }
          else if ((gsize)(hi - left_ptr) <= MAX_THRESH) {
              /* Ignore small right partition. */
              hi = right_ptr;
              hii = right_ptri;
          }
          else if ((right_ptr - lo) > (hi - left_ptr)) {
              /* Push larger left partition indices. */
              top->lo = lo;
              top->loi = loi;
              top->hi = right_ptr;
              top->hii = right_ptri;
              ++top;
              lo = left_ptr;
              loi = left_ptri;
          }
          else {
              /* Push larger right partition indices. */
              top->lo = left_ptr;
              top->loi = left_ptri;
              top->hi = hi;
              top->hii = hii;
              ++top;
              hi = right_ptr;
              hii = right_ptri;
          }
        }
    }

    /* Once the BASE_PTR array is partially sorted by quicksort the rest
       is completely sorted using insertion sort, since this is efficient
       for partitions below MAX_THRESH size. BASE_PTR points to the beginning
       of the array to sort, and END_PTR points at the very last element in
       the array (*not* one beyond it!). */

    {
        gdouble *const end_ptr = array + (n - 1);
        gdouble *tmp_ptr = array;
        guint *tmp_ptri = index_array;
        gdouble *thresh = MIN(end_ptr, array + MAX_THRESH);
        gdouble *run_ptr;
        guint *run_ptri;

        /* Find smallest element in first threshold and place it at the
           array's beginning.  This is the smallest array element,
           and the operation speeds up insertion sort's inner loop. */

        for (run_ptr = tmp_ptr + 1, run_ptri = tmp_ptri + 1;
             run_ptr <= thresh;
             run_ptr++, run_ptri++) {
            if (*run_ptr < *tmp_ptr) {
                tmp_ptr = run_ptr;
                tmp_ptri = run_ptri;
            }
        }

        if (tmp_ptr != array) {
            DSWAP(*tmp_ptr, *array);
            ISWAP(*tmp_ptri, *index_array);
        }

        /* Insertion sort, running from left-hand-side up to right-hand-side.
         */

        run_ptr = array + 1;
        run_ptri = index_array + 1;
        while (++run_ptr <= end_ptr) {
            tmp_ptr = run_ptr - 1;
            tmp_ptri = run_ptri;
            ++run_ptri;
            while (*run_ptr < *tmp_ptr) {
                tmp_ptr--;
                tmp_ptri--;
            }

            tmp_ptr++;
            tmp_ptri++;
            if (tmp_ptr != run_ptr) {
                gdouble *hi, *lo;
                guint *hii, *loi;
                gdouble d;
                guint i;

                d = *run_ptr;
                for (hi = lo = run_ptr; --lo >= tmp_ptr; hi = lo)
                    *hi = *lo;
                *hi = d;

                i = *run_ptri;
                for (hii = loi = run_ptri; --loi >= tmp_ptri; hii = loi)
                    *hii = *loi;
                *hii = i;
            }
        }
    }
}

/**
 * SECTION: math
 * @title: math
 * @short_description: Mathematical functions
 *
 * Some of the less standard but useful mathematical functions is provided to
 * ensure they are available on all platforms.  They are prefixed with
 * <literal>gwy_</literal> and they may be actually macros, especially if the
 * platform provides the respective function the
 * <literal>gwy_</literal>-prefixed version simply resolves to the name of this
 * system function.  However, it is always possible to take the address of
 * the symbol.
 **/

/**
 * GWY_SQRT3:
 *
 * The square root of 3.
 **/

/**
 * GWY_SQRT_PI:
 *
 * The square root of π.
 **/

/**
 * GwyDimenType:
 * @GWY_DIMEN_X: First dimension (@x).
 * @GWY_DIMEN_Y: Second dimension (@y).
 * @GWY_DIMEN_Z: Third dimension (@z).
 * @GWY_DIMEN_W: Fourth dimension (@w).
 *
 * Type of dimension axes.
 *
 * This enumeration is defined for code readability.  Dimensions can usually
 * be just indexed numerically.
 **/

/**
 * gwycomplex:
 *
 * Shorthand for C99 <type>double complex</type> type.
 *
 * Provided that <filename class='headerfile'>fftw3.h</filename> is included
 * after <filename class='headerfile'>complex.h</filename> of Gwyddion headers,
 * it is also an alias for <type>fftw_complex</type>.  It should be also
 * bit-wise compatible with the C++ complex template specialised to doubles.
 **/

/**
 * GwyXY:
 * @x: X-coordinate.
 * @y: Y-coordinate.
 *
 * Representation of Cartesian coordinates in plane.
 **/

/**
 * GWY_TYPE_XY:
 *
 * The #GType for a boxed type holding a #GwyXY.
 **/

/**
 * GwyXYZ:
 * @x: X-coordinate.
 * @y: Y-coordinate.
 * @z: Z-coordinate.
 *
 * Representation of Cartesian coordinates in space.
 **/

/**
 * GWY_TYPE_XYZ:
 *
 * The #GType for a boxed type holding a #GwyXYZ.
 **/

/**
 * GwyRange:
 * @from: Begining of the interval.
 * @to: End of the interval.
 *
 * Representation of an interval on the real axis.
 *
 * Usually it holds @from ≤ @to.  However, in some uses the interval is
 * oriented and thus either value can be larger or smaller.
 **/

/**
 * GWY_TYPE_RANGE:
 *
 * The #GType for a boxed type holding a #GwyRange.
 **/

/**
 * GwyCurvatureParams:
 * @k1: The smaller curvature.
 * @k2: The larger curvature.
 * @phi1: Direction of the smaller curvature.
 * @phi2: Direction of the larger curvature.
 * @xc: X-coordinate of the centre of the quadratic surface.
 * @yc: Y-coordinate of the centre of the quadratic surface.
 * @zc: Value at the centre of the quadratic surface.
 *
 * Parameters describing the curvature of a surface.
 *
 * Curvatures have signs, positive mean a concave (cup-like) surface, negative
 * mean a convex (cap-like) surface.  They are ordered including the sign.
 *
 * Directions are angles from the interval [-π/2, π/2].  The angle is measured
 * from the positive @x axis, increasing towards the positive @y axis.  This
 * does not depend on the handedness.
 *
 * If the quadratic surface is degenerate, i.e. flat in at least one direction,
 * the position centre is undefined in the flat direction(s).  Function
 * calculating the curvature then choose the centre in the point closest to the
 * origin of coordinates.  For flat surfaces this means the origin is simply
 * returned as the centre position.  Consequently, you should use Cartesian
 * coordinates with origin in a natural centre, for instance centre of image or
 * grain, to obtain meaningful @xc and @yc also in these cases.
 **/

/**
 * GwyRealFunc:
 * @value: Abscissa or, generally, the input value.
 * @user_data: User data.
 *
 * The type of a plain real-valued function.
 *
 * Returns: Function value for input value @value.
 **/

/**
 * gwy_round:
 * @x: Double value.
 *
 * Rounds a number to nearest integer.
 *
 * It expands to a #glong so the result is of an integral type.  Half-integers
 * are rounded up, as usual.
 *
 * This macro evaluates its argument only once.
 **/

/**
 * gwy_round_to_half:
 * @x: Double value.
 *
 * Rounds a number to nearest half-integer.
 *
 * The result the nearest number of form @n+½ which is closest to @x.  Integers
 * are rounded up.
 *
 * This function may be useful for Cairo drawing if you want sharp instead
 * precisely positioned lines of an odd width.
 *
 * This macro evaluates its argument only once.
 **/

/**
 * gwy_triangular_matrix_length:
 * @n: Matrix dimension.
 *
 * Evaluates to the length of an array necessary to store a triangular matrix.
 *
 * This macro may evaluate its arguments several times.
 *
 * The length is the same for triangular matrices of both kinds and also for
 * symmetrical matrices although the element interpretation differs.
 **/

/**
 * gwy_lower_triangular_matrix_index:
 * @a: Lower triangular matrix stored by rows.
 * @i: Row index.
 * @j: Column index, it must be at most equal to @i.
 *
 * Accesses an element of lower triangular matrix.
 *
 * This macro may evaluate its arguments several times.
 * This macro expands to a left-hand side expression.
 *
 * The matrix is assumed to be stored as follows:
 * [a_00 a_10 a_11 a_20 a_21 a_22 a_30 ...] which is suitable also for
 * symmetrical matrices as only half of the matrix needs to be stored.
 * However, as triangular matrices are complemented with zeroes while
 * symmetrical matrices with reflected values one has to be careful and not
 * confuse them despite the same representation.
 *
 * For instance, to multiply rows and columns of @a by the corresponding
 * elements of vector @v one can do:
 * |[
 * for (guint i = 0; i < n; i++) {
 *     for (guint j = 0; j <= i; j++)
 *         gwy_lower_triangular_matrix_index(a, i, j) *= v[i] * v[j];
 * }
 * ]|
 **/

/**
 * GwyLinearFitFunc:
 * @i: Index of data point to calculate the function value in.
 * @fvalues: Array of length @nparams (as passed to gwy_linear_fit()) to store
 *           the function values to.
 * @value: Location to store the value of the @i-th fitted data point.
 * @user_data: User data passed to gwy_linear_fit().
 *
 * Type of linear least-squares fitting function.
 *
 * The function takes a point index argument, not the abscissa value.  This
 * enables easy fitting of not only one-dimensional data but also
 * multidimensional data using a simple index decomposition.
 *
 * To fit vector-valued data, the caller can multiply the real number of points
 * by the number of vector components and return successively individual
 * components.  It is guaranteed the points will be processed sequentially.
 * Hence if the evaluation of the vector-valued function components requires
 * a non-trivial calculation, the result can be cached in @user_data for the
 * subsequent calls.
 *
 * Returns: %TRUE to include the point in fitting, %FALSE to skip it.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
