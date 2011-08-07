/*
 *  $Id$
 *  Copyright (C) 2009 David Nečas (Yeti).
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

/*< private_header >*/

#ifndef __LIBGWY_FIELD_INTERNAL_H__
#define __LIBGWY_FIELD_INTERNAL_H__

#include "libgwy/interpolation.h"
#include "libgwy/field.h"

G_BEGIN_DECLS

/* Cache operations */
#define GWY_FIELD_CVAL(arg, bit)  ((arg)->cache[GWY_FIELD_CACHE_##bit])
#define GWY_FIELD_CBIT(bit)       (1 << GWY_FIELD_CACHE_##bit)
#define GWY_FIELD_CTEST(arg, bit) ((arg)->cached & GWY_FIELD_CBIT(bit))

typedef enum {
    GWY_FIELD_CACHE_MIN = 0,
    GWY_FIELD_CACHE_MAX,
    GWY_FIELD_CACHE_AVG,
    GWY_FIELD_CACHE_RMS,
    GWY_FIELD_CACHE_MSQ,
    GWY_FIELD_CACHE_MED,
    GWY_FIELD_CACHE_ARF,  // Not implemented yet
    GWY_FIELD_CACHE_ART,  // Not implemented yet
    GWY_FIELD_CACHE_ARE,
    GWY_FIELD_CACHE_SIZE
} GwyFieldCached;

struct _GwyFieldPrivate {
    /* FIXME: Consider permitting x-units != y-units. */
    GwyUnit *unit_xy;
    GwyUnit *unit_z;
    guint32 cached;
    gboolean allocated;
    gdouble storage;
    gdouble cache[GWY_FIELD_CACHE_SIZE];
};

typedef struct _GwyFieldPrivate Field;

typedef void (*RectExtendFunc)(const gdouble *in,
                               guint inrowstride,
                               gdouble *out,
                               guint outrowstride,
                               guint xpos,
                               guint ypos,
                               guint width,
                               guint height,
                               guint xres,
                               guint yres,
                               guint extend_left,
                               guint extend_right,
                               guint extend_up,
                               guint extend_down,
                               gdouble value);

G_GNUC_INTERNAL
void _gwy_field_set_cache_for_flat(GwyField *field,
                                   gdouble value);

G_GNUC_INTERNAL
void _gwy_ensure_defined_exterior(GwyExteriorType *exterior,
                                  gdouble *fill_value);

G_GNUC_INTERNAL
void _gwy_make_symmetrical_extension(guint size,
                                     guint extsize,
                                     guint *extend_begining,
                                     guint *extend_end);

G_GNUC_INTERNAL
RectExtendFunc _gwy_get_rect_extend_func(GwyExteriorType exterior);

G_GNUC_INTERNAL
void _gwy_extend_kernel_rect(const gdouble *kernel,
                             guint kxlen,
                             guint kylen,
                             gdouble *extended,
                             guint xsize,
                             guint ysize,
                             guint rowstride);

G_GNUC_INTERNAL
void _gwy_tune_convolution_method(const gchar *method);

G_GNUC_INTERNAL
void _gwy_tune_median_filter_method(const gchar *method);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
