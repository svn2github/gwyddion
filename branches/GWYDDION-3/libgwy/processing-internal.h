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

#include "libgwy/field.h"
#include "libgwy/mask-field.h"

G_BEGIN_DECLS

#define STATICP \
    (G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB)

#define _GWY_FFTW_PATIENCE FFTW_ESTIMATE

#define ASSIGN(p, q, n) memcpy((p), (q), (n)*sizeof(gdouble))
#define SLi gwy_lower_triangular_matrix_index
#define MATRIX_LEN gwy_triangular_matrix_length

#define ASSIGN_UNITS(dest, src) \
    do { \
        if (src && dest) \
            gwy_unit_assign(dest, src); \
        else if (dest) \
            GWY_OBJECT_UNREF(dest); \
        else if (src) \
            dest = gwy_unit_duplicate(src); \
    } while (0)

struct _GwyLinePrivate {
    GwyUnit *unit_x;
    GwyUnit *unit_y;
    gboolean allocated;
};

typedef struct _GwyLinePrivate Line;

struct _GwyCurvePrivate {
    GwyUnit *unit_x;
    GwyUnit *unit_y;
};

typedef struct _GwyCurvePrivate Curve;

/* Cache operations */
#define CVAL(arg, bit)  ((arg)->cache[GWY_FIELD_CACHE_##bit])
#define CBIT(bit)       (1 << GWY_FIELD_CACHE_##bit)
#define CTEST(arg, bit) ((arg)->cached & CBIT(bit))

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
    gboolean allocated;
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

/* Note we use run-time conditions for endianess-branching even though it is
 * known at compile time.  This is to get the big-endian branch at least
 * syntax-checked.  A good optimizing compiler then eliminates the unused
 * branch entirely so we do not need to care. */
#if (G_BYTE_ORDER != G_LITTLE_ENDIAN && G_BYTE_ORDER != G_BIG_ENDIAN)
#error Byte order used on this system is not supported.
#endif

#define ALL_SET ((guint32)0xffffffffu)
#define ALL_CLEAR ((guint32)0x00000000u)

/* SHR moves the bits right in the mask field, which means towards the higher
 * bits on little-endian and towards the lower bits on big endian. */
#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
#define FIRST_BIT ((guint32)0x1u)
#define SHR <<
#define SHL >>
#endif

#if (G_BYTE_ORDER == G_BIG_ENDIAN)
#define FIRST_BIT ((guint32)0x80000000u)
#define SHR >>
#define SHL <<
#endif

/* Make a 32bit bit mask with nbits set, starting from bit firstbit.  The
 * lowest bit is 0, the highest 0x1f for little endian and the reverse for
 * big endian. */
#define NTH_BIT(n) (FIRST_BIT SHR (n))
#define MAKE_MASK(firstbit, nbits) \
    (nbits ? ((ALL_SET SHL (0x20 - (nbits))) SHR (firstbit)) : 0u)

G_GNUC_INTERNAL
gboolean _gwy_field_check_mask(const GwyField *field,
                               const GwyMaskField *mask,
                               GwyMaskingType *masking,
                               guint col, guint row,
                               guint width, guint height,
                               guint *maskcol, guint *maskrow);

void _gwy_notify_properties(GObject *object,
                            const gchar **properties,
                            guint nproperties);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
