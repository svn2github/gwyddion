/*
 *  $Id$
 *  Copyright (C) 2007,2009-2011,2013 David Nečas (Yeti).
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

#ifndef __LIBGWY_MATH_H__
#define __LIBGWY_MATH_H__

#include <math.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define GWY_SQRT3 1.73205080756887729352744634150587236694280525381038
#define GWY_SQRT_PI 1.77245385090551602729816748334114518279754945612237

typedef enum {
    GWY_DIMEN_X = 0,
    GWY_DIMEN_Y = 1,
    GWY_DIMEN_Z = 2,
    GWY_DIMEN_W = 3,
} GwyDimenType;

typedef struct {
    gdouble x;
    gdouble y;
} GwyXY;

typedef struct {
    gdouble x;
    gdouble y;
    gdouble z;
} GwyXYZ;

typedef struct {
    gdouble from;
    gdouble to;
} GwyRange;

typedef struct {
    gdouble k1;
    gdouble k2;
    gdouble phi1;
    gdouble phi2;
    gdouble xc;
    gdouble yc;
    gdouble zc;
} GwyCurvatureParams;

typedef gdouble (*GwyRealFunc)(gdouble value, gpointer user_data);

// Put the declaration we want gtk-doc to pick up first.
#ifndef __GNUC__
double gwy_powi(double x, int i) G_GNUC_CONST;
#else
#  define gwy_powi __builtin_powi
#endif

#define gwy_round(x) ((glong)floor((x) + 0.5))
#define gwy_round_to_half(x) (floor((x) + 1.0) - 0.5)

gdouble  gwy_spow                    (gdouble x,
                                      gdouble p)                      G_GNUC_CONST;
gdouble  gwy_power_sum               (guint n,
                                      guint p)                        G_GNUC_CONST;
gdouble  gwy_standardize_direction   (gdouble phi)                    G_GNUC_CONST;
gboolean gwy_overlapping             (guint pos1,
                                      guint len1,
                                      guint pos2,
                                      guint len2)                     G_GNUC_CONST;
gboolean gwy_math_intersecting       (gdouble a,
                                      gdouble b,
                                      gdouble A,
                                      gdouble B)                      G_GNUC_CONST;
guint    gwy_math_curvature_at_centre(const gdouble *coeffs,
                                      GwyCurvatureParams *curvature);
guint    gwy_math_curvature_at_origin(const gdouble *coeffs,
                                      GwyCurvatureParams *curvature);
gint     gwy_double_compare          (gconstpointer a,
                                      gconstpointer b)                G_GNUC_PURE;
gint     gwy_double_direct_compare   (gconstpointer a,
                                      gconstpointer b)                G_GNUC_CONST;
void     gwy_math_sort               (gdouble *array,
                                      guint *index_array,
                                      gsize n);
gdouble  gwy_math_median             (gdouble *array,
                                      gsize n);

#define gwy_triangular_matrix_length(n) \
    (((n) + 1)*(n)/2)

#define gwy_lower_triangular_matrix_index(a, i, j) \
    ((a)[(i)*((i) + 1)/2 + (j)])

gboolean gwy_cholesky_decompose(gdouble *matrix,
                                guint n);
void     gwy_cholesky_solve    (const gdouble *decomp,
                                gdouble *rhs,
                                guint n);
gboolean gwy_cholesky_invert   (gdouble *matrix,
                                guint n);
gdouble  gwy_cholesky_condition(const gdouble *matrix,
                                guint n)               G_GNUC_PURE;
gboolean gwy_linalg_solve      (gdouble *matrix,
                                gdouble *rhs,
                                gdouble *result,
                                guint n);
gboolean gwy_linalg_invert     (gdouble *matrix,
                                gdouble *inv,
                                guint n);

typedef gboolean (*GwyLinearFitFunc)(guint i,
                                     gdouble *fvalues,
                                     gdouble *value,
                                     gpointer user_data);

gboolean gwy_linear_fit        (GwyLinearFitFunc function,
                                guint npoints,
                                gdouble *params,
                                guint nparams,
                                gdouble *residuum,
                                gpointer user_data);

// Use negated condition in if to catch NaNs.
#define gwy_assert_floatval(val,ref,tol) \
    do { \
        long double __n1 = (val), __n2 = (ref), __n3 = (tol); \
        if (!(fabsl(__n1 - __n2) <= __n3)) \
            gwy_assertion_message_floatval(G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
                                           "|(" #val ") - (" #ref ")| <= " #tol, \
                                           __n1, __n2, __n3); \
    } while (0)

void gwy_assertion_message_floatval(const char *domain,
                                    const char *file,
                                    int line,
                                    const char *func,
                                    const char *expr,
                                    long double val,
                                    long double ref,
                                    long double tol) G_GNUC_NORETURN;

#define GWY_TYPE_XY (gwy_xy_get_type())

GType  gwy_xy_get_type(void)            G_GNUC_CONST;
GwyXY* gwy_xy_copy    (const GwyXY *xy) G_GNUC_MALLOC;
void   gwy_xy_free    (GwyXY *xy);

#define GWY_TYPE_XYZ (gwy_xyz_get_type())

GType   gwy_xyz_get_type(void)              G_GNUC_CONST;
GwyXYZ* gwy_xyz_copy    (const GwyXYZ *xyz) G_GNUC_MALLOC;
void    gwy_xyz_free    (GwyXYZ *xyz);

#define GWY_TYPE_RANGE (gwy_range_get_type())

GType     gwy_range_get_type(void)                  G_GNUC_CONST;
GwyRange* gwy_range_copy    (const GwyRange *range) G_GNUC_MALLOC;
void      gwy_range_free    (GwyRange *range);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
