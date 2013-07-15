/*
 *  $Id$
 *  Copyright (C) 2009-2013 David Nečas (Yeti).
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

#ifndef __LIBGWY_MASK_FIELD_INTERNAL_H__
#define __LIBGWY_MASK_FIELD_INTERNAL_H__

#include "libgwy/mask-field.h"
#include "libgwy/mask-internal.h"

G_BEGIN_DECLS

struct _GwyMaskFieldPrivate {
    guint *grains;
    guint *distances;
    guint *grain_sizes;
    GwyFieldPart *grain_bounding_boxes;
    GwyXY *grain_positions;
    guint ngrains;

    gboolean allocated;
    guint32 storage;
    gchar *name;
    guint32 *serialized_swapped;    // serialisation-only
};

typedef struct _GwyMaskFieldPrivate MaskField;

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

// Must be signed, we use signed differences.
typedef struct {
    gint i;
    gint j;
} GridPoint;

typedef struct {
    guint size;
    guint len;
    GridPoint *points;
} GridPointList;

typedef struct {
    guint size;
    guint len;
    gint *data;
} IntList;

/* Merge grains i and j in map with full resolution */
static inline void
resolve_grain_map(guint *m, guint i, guint j)
{
    guint ii, jj;

    // Find what i and j fully resolve to.
    for (ii = i; m[ii] != ii; ii = m[ii])
        ;
    for (jj = j; m[jj] != jj; jj = m[jj])
        ;
    guint k = MIN(ii, jj);

    // Turn partial resultions to full along the way.
    for (ii = m[i]; m[ii] != ii; ii = m[ii]) {
        m[i] = k;
        i = ii;
    }
    m[ii] = k;
    for (jj = m[j]; m[jj] != jj; jj = m[jj]) {
        m[j] = k;
        j = jj;
    }
    m[jj] = k;
}

G_GNUC_UNUSED
static inline GridPointList*
grid_point_list_new(guint prealloc)
{
    GridPointList *list = g_slice_new0(GridPointList);
    prealloc = MAX(prealloc, 16);
    list->size = prealloc;
    list->points = g_new(GridPoint, list->size);
    return list;
}

G_GNUC_UNUSED
static inline void
grid_point_list_add(GridPointList *list,
                    gint j, gint i)
{
    if (G_UNLIKELY(list->len == list->size)) {
        list->size = MAX(2*list->size, 16);
        list->points = g_renew(GridPoint, list->points, list->size);
    }

    list->points[list->len].i = i;
    list->points[list->len].j = j;
    list->len++;
}

G_GNUC_UNUSED
static void
grid_point_list_free(GridPointList *list)
{
    g_free(list->points);
    g_slice_free(GridPointList, list);
}

G_GNUC_UNUSED
static inline IntList*
int_list_new(guint prealloc)
{
    IntList *list = g_slice_new0(IntList);
    prealloc = MAX(prealloc, 16);
    list->size = prealloc;
    list->data = g_new(gint, list->size);
    return list;
}

G_GNUC_UNUSED
static inline void
int_list_add(IntList *list, gint i)
{
    if (G_UNLIKELY(list->len == list->size)) {
        list->size = MAX(2*list->size, 16);
        list->data = g_renew(gint, list->data, list->size);
    }

    list->data[list->len] = i;
    list->len++;
}

G_GNUC_UNUSED
static void
int_list_free(IntList *list)
{
    g_free(list->data);
    g_slice_free(IntList, list);
}

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
