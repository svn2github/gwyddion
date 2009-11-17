/*
 *  $Id$
 *  Copyright (C) 2007,2009 David Necas (Yeti).
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
#include <glib.h>
#include <gwyconfig.h>

G_BEGIN_DECLS

/* This is necessary to fool gtk-doc that ignores static inline functions */
#define _GWY_STATIC_INLINE static inline

/*
 * The empty comments fix gtk-doc.
 * See http://bugzilla.gnome.org/show_bug.cgi?id=481811
 */

#ifdef GWY_HAVE_EXP10
# define gwy_exp10 exp10
#else
_GWY_STATIC_INLINE double gwy_exp10(double x);

static inline double
gwy_exp10(double x) {
    return /**/ pow(10.0, x);
}
# ifdef GWY_MATH_POLLUTE_NAMESPACE
#  define exp10 gwy_exp10
# endif
#endif

#ifdef __GNUC__
# define gwy_powi __builtin_powi
# ifdef GWY_MATH_POLLUTE_NAMESPACE
#  define powi __builtin_powi
# endif
#else
_GWY_STATIC_INLINE double gwy_powi (double x, int i);

static inline double
gwy_powi(double x, int i)
{
    double r = 1.0;;
    if (!i)
        return 1.0;
    if (i < 0) {
        i = -i;
        x = 1.0/x;
    }
    for ( ; ; ) {
        if (i & 1)
            r *= x;
        if (!(i >>= 1))
            break;
        x *= x;
    }
    return /**/ r;
}
# ifdef GWY_MATH_POLLUTE_NAMESPACE
#  define powi gwy_powi
# endif
#endif

#undef _GWY_STATIC_INLINE

#define gwy_round(x) ((glong)floor((x) + 0.5))

void    gwy_math_sort  (gdouble *array,
                        guint *index_array,
                        gsize n);
gdouble gwy_math_median(gdouble *array,
                        gsize n);

#define gwy_lower_triangular_matrix_index(a, i, j) \
    ((a)[(i)*((i) + 1)/2 + (j)])

gboolean gwy_choleski_decompose(gdouble *matrix,
                                guint n);
void     gwy_choleski_solve    (const gdouble *decomp,
                                gdouble *rhs,
                                guint n);
gboolean gwy_choleski_invert   (gdouble *a,
                                guint n);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
