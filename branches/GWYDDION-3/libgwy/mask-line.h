/*
 *  $Id$
 *  Copyright (C) 2011 David Necas (Yeti).
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

#ifndef __LIBGWY_MASK_LINE_H__
#define __LIBGWY_MASK_LINE_H__

#include <libgwy/serializable.h>
#include <libgwy/mask-iter.h>

G_BEGIN_DECLS

#define GWY_TYPE_MASK_LINE \
    (gwy_mask_line_get_type())
#define GWY_MASK_LINE(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_MASK_LINE, GwyMaskLine))
#define GWY_MASK_LINE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_MASK_LINE, GwyMaskLineClass))
#define GWY_IS_MASK_LINE(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_MASK_LINE))
#define GWY_IS_MASK_LINE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_MASK_LINE))
#define GWY_MASK_LINE_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_MASK_LINE, GwyMaskLineClass))

typedef struct _GwyMaskLine      GwyMaskLine;
typedef struct _GwyMaskLineClass GwyMaskLineClass;

struct _GwyMaskLine {
    GObject g_object;
    struct _GwyMaskLinePrivate *priv;
    /*<public>*/
    guint res;
    guint32 *data;
};

struct _GwyMaskLineClass {
    /*<private>*/
    GObjectClass g_object_class;
};

#define gwy_mask_line_duplicate(line) \
        (GWY_MASK_LINE(gwy_serializable_duplicate(GWY_SERIALIZABLE(line))))
#define gwy_mask_line_assign(dest, src) \
        (gwy_serializable_assign(GWY_SERIALIZABLE(dest), GWY_SERIALIZABLE(src)))

GType        gwy_mask_line_get_type     (void)                    G_GNUC_CONST;
GwyMaskLine* gwy_mask_line_new          (void)                    G_GNUC_MALLOC;
GwyMaskLine* gwy_mask_line_new_sized    (guint res,
                                         gboolean clear)          G_GNUC_MALLOC;
GwyMaskLine* gwy_mask_line_new_part     (const GwyMaskLine *line,
                                         guint pos,
                                         guint len)               G_GNUC_MALLOC;
GwyMaskLine* gwy_mask_line_new_resampled(const GwyMaskLine *line,
                                         guint res)               G_GNUC_MALLOC;
void         gwy_mask_line_set_size     (GwyMaskLine *line,
                                         guint res,
                                         gboolean clear);

#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
#define gwy_mask_line_get(line, pos) \
    ((line)->data[(pos) >> 5] & ((guint32)1 << (((pos) & 0x1f))))

#define gwy_mask_line_set(line, pos, value) \
    do { \
        if (value) \
            (line)->data[(pos) >> 5] |= ((guint32)1 << (((pos) & 0x1f))); \
        else \
            (line)->data[(pos) >> 5]  &= ~((guint32)1 << (((pos) & 0x1f))); \
    } while (0)
#endif

#if (G_BYTE_ORDER == G_BIG_ENDIAN)
#define gwy_mask_line_get(line, pos) \
    ((line)->data[(pos) >> 5] & ((guint32)0x80000000u >> (((pos) & 0x1f))))

#define gwy_mask_line_set(line, pos, value) \
    do { \
        if (value) \
            (line)->data[(pos) >> 5] |= ((guint32)0x80000000u >> (((pos) & 0x1f))); \
        else \
            (line)->data[(pos) >> 5] &= ~((guint32)0x80000000u >> (((pos) & 0x1f))); \
    } while (0)
#endif

#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
#define gwy_mask_line_iter_init(line, iter, pos) \
    do { \
        iter.p = (line)->data + ((pos) >> 5); \
        iter.bit = 1u << ((pos) & 0x1f); \
    } while (0)
#endif

#if (G_BYTE_ORDER == G_BIG_ENDIAN)
#define gwy_mask_line_iter_init(line, iter, col) \
    do { \
        iter.p = (line)->data + ((col) >> 5); \
        iter.bit = 0x80000000u >> ((col) & 0x1f); \
    } while (0)

#endif

void     gwy_mask_line_data_changed(GwyMaskLine *line);
void     gwy_mask_line_copy        (const GwyMaskLine *src,
                                    guint srcpos,
                                    guint srclen,
                                    GwyMaskLine *dest,
                                    guint destpos);
void     gwy_mask_line_invalidate  (GwyMaskLine *line);
guint    gwy_mask_line_count       (const GwyMaskLine *line,
                                    const GwyMaskLine *mask,
                                    gboolean value);
guint    gwy_mask_line_part_count  (const GwyMaskLine *line,
                                    guint pos,
                                    guint len,
                                    gboolean value);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
