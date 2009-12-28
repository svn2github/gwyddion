/*
 *  $Id$
 *  Copyright (C) 2009 David Necas (Yeti).
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

#ifndef __LIBGWY_PROCESSING_INTERNAL_H__
#define __LIBGWY_PROCESSING_INTERNAL_H__

#include "libgwy/unit.h"

/* Cache operations */
#define CVAL(arg, bit)  ((arg)->cache[GWY_FIELD_CACHE_##bit])
#define CBIT(bit)       (1 << GWY_FIELD_CACHE_##bit)
#define CTEST(arg, bit) ((arg)->cached & CBIT(bit))

G_BEGIN_DECLS

typedef enum {
    GWY_FIELD_CACHE_MIN = 0,
    GWY_FIELD_CACHE_MAX,
    GWY_FIELD_CACHE_AVG,
    GWY_FIELD_CACHE_RMS,
    GWY_FIELD_CACHE_MED,
    GWY_FIELD_CACHE_ARF,
    GWY_FIELD_CACHE_ART,
    GWY_FIELD_CACHE_ARE,
    GWY_FIELD_CACHE_SIZE
} GwyFieldCached;

struct _GwyFieldPrivate {
    /* FIXME: Consider permitting x-units != y-units. */
    GwyUnit *unit_xy;
    GwyUnit *unit_z;
    guint32 cached;
    gdouble cache[GWY_FIELD_CACHE_SIZE];
};

typedef struct _GwyFieldPrivate Field;

typedef struct {
    guint row;
    guint col;
    guint len;
} GwyGrainSegment;

typedef struct {
    guint id;
    guint len;
    GwyGrainSegment segments[];
} GwyGrain;

G_GNUC_INTERNAL
gboolean _gwy_field_check_mask(const GwyField *field,
                               const GwyMaskField *mask,
                               GwyMaskingType *masking,
                               guint col, guint row,
                               guint width, guint height,
                               guint *maskcol, guint *maskrow);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
