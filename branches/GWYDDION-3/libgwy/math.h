/*
 *  @(#) $Id$
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

#ifndef __GWY_MATH_H__
#define __GWY_MATH_H__

#include <math.h>
#include <glib/gutils.h>
/*#include <gwyconfig.h>*/

G_BEGIN_DECLS

/* This is necessary to fool gtk-doc that ignores static inline functions */
#define _GWY_STATIC_INLINE static inline

_GWY_STATIC_INLINE double gwy_exp10(double x);
_GWY_STATIC_INLINE double gwy_exp2 (double x);
_GWY_STATIC_INLINE double gwy_log2 (double x);
_GWY_STATIC_INLINE double gwy_cbrt (double x);
_GWY_STATIC_INLINE double gwy_hypot(double x, double y);
_GWY_STATIC_INLINE double gwy_acosh(double x);
_GWY_STATIC_INLINE double gwy_asinh(double x);
_GWY_STATIC_INLINE double gwy_atanh(double x);
_GWY_STATIC_INLINE double gwy_powi (double x, int i);

#undef _GWY_STATIC_INLINE

/*
 * The empty comments fix gtk-doc.
 * See http://bugzilla.gnome.org/show_bug.cgi?id=481811
 */

#ifdef GWY_HAVE_EXP10
# define gwy_exp10 exp10
#else
static inline double
gwy_exp10(double x) {
    return /**/ pow(10.0, x);
}
# ifdef GWY_MATH_POLLUTE_NAMESPACE
#  define exp10 gwy_exp10
# endif
#endif

#ifdef GWY_HAVE_EXP2
# define gwy_exp2 exp2
#else
static inline double
gwy_exp2(double x) {
    return /**/ pow(2.0, x);
}
# ifdef GWY_MATH_POLLUTE_NAMESPACE
#  define exp2 gwy_exp2
# endif
#endif

#ifdef GWY_HAVE_LOG2
# define gwy_log2 log2
#else
static inline double
gwy_log2(double x) {
    return /**/ log(x)/G_LN2;
}
# ifdef GWY_MATH_POLLUTE_NAMESPACE
#  define log2 gwy_log2
# endif
#endif

#ifdef GWY_HAVE_CBRT
# define gwy_cbrt cbrt
#else
static inline double
gwy_cbrt(double x) {
    return /**/ pow(x, 1.0/3.0);
}
# ifdef GWY_MATH_POLLUTE_NAMESPACE
#  define cbrt gwy_cbrt
# endif
#endif

#ifdef GWY_HAVE_HYPOT
# define gwy_hypot hypot
#else
static inline double
gwy_hypot(double x, double y) {
    return /**/ sqrt(x*x + y*y);
}
# ifdef GWY_MATH_POLLUTE_NAMESPACE
#  define hypot gwy_hypot
# endif
#endif

#ifdef GWY_HAVE_ACOSH
# define gwy_acosh acosh
#else
static inline double
gwy_acosh(double x)
{
    return /**/ log(x + sqrt(x*x - 1.0));
}
# ifdef GWY_MATH_POLLUTE_NAMESPACE
#  define acosh gwy_acosh
# endif
#endif

#ifdef GWY_HAVE_ASINH
# define gwy_asinh asinh
#else
static inline double
gwy_asinh(double x)
{
    return /**/ log(x + sqrt(x*x + 1.0));
}
# ifdef GWY_MATH_POLLUTE_NAMESPACE
#  define asinh gwy_asinh
# endif
#endif

#ifdef GWY_HAVE_ATANH
# define gwy_atanh atanh
#else
static inline double
gwy_atanh(double x)
{
    return /**/ log((1.0 + x)/(1.0 - x));
}
# ifdef GWY_MATH_POLLUTE_NAMESPACE
#  define atanh gwy_atanh
# endif
#endif

#ifdef GWY_HAVE_POWI
# define gwy_powi __builtin_powi
# ifdef GWY_MATH_POLLUTE_NAMESPACE
#  define powi __builtin_powi
# endif
#else
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

void    gwy_math_sort  (gsize n,
                        gdouble *array,
                        guint *index_array);
gdouble gwy_math_median(gsize n,
                        gdouble *array);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
