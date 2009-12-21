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

#ifndef __LIBGWY_MASK_FIELD_H__
#define __LIBGWY_MASK_FIELD_H__

#include <libgwy/serializable.h>
#include <libgwy/field.h>

G_BEGIN_DECLS

#define GWY_TYPE_MASK_FIELD \
    (gwy_mask_field_get_type())
#define GWY_MASK_FIELD(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_MASK_FIELD, GwyMaskField))
#define GWY_MASK_FIELD_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_MASK_FIELD, GwyMaskFieldClass))
#define GWY_IS_MASK_FIELD(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_MASK_FIELD))
#define GWY_IS_MASK_FIELD_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_MASK_FIELD))
#define GWY_MASK_FIELD_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_MASK_FIELD, GwyMaskFieldClass))

typedef struct _GwyMaskField      GwyMaskField;
typedef struct _GwyMaskFieldClass GwyMaskFieldClass;

struct _GwyMaskField {
    GObject g_object;
    struct _GwyMaskFieldPrivate *priv;
    /*<public>*/
    guint xres;
    guint yres;
    guint stride;
    guint32 *data;
};

struct _GwyMaskFieldClass {
    GObjectClass g_object_class;
};

#define gwy_mask_field_duplicate(maskfield) \
        (GWY_MASK_FIELD(gwy_serializable_duplicate(GWY_SERIALIZABLE(maskfield))))
#define gwy_mask_field_assign(dest, src) \
        (gwy_serializable_assign(GWY_SERIALIZABLE(dest), GWY_SERIALIZABLE(src)))

GType         gwy_mask_field_get_type      (void)                          G_GNUC_CONST;
GwyMaskField* gwy_mask_field_new           (void)                          G_GNUC_MALLOC;
GwyMaskField* gwy_mask_field_new_sized     (guint xres,
                                            guint yres,
                                            gboolean clear)                G_GNUC_MALLOC;
GwyMaskField* gwy_mask_field_new_part      (const GwyMaskField *maskfield,
                                            guint col,
                                            guint row,
                                            guint width,
                                            guint height)                  G_GNUC_MALLOC;
GwyMaskField* gwy_mask_field_new_resampled (const GwyMaskField *maskfield,
                                            guint xres,
                                            guint yres)                    G_GNUC_MALLOC;
GwyMaskField* gwy_mask_field_new_from_field(const GwyField *field,
                                            guint col,
                                            guint row,
                                            guint width,
                                            guint height,
                                            gdouble lower,
                                            gdouble upper,
                                            gboolean complement)           G_GNUC_MALLOC;
void          gwy_mask_field_data_changed  (GwyMaskField *maskfield);
void          gwy_mask_field_copy          (const GwyMaskField *src,
                                            GwyMaskField *dest);
void          gwy_mask_field_part_copy     (const GwyMaskField *src,
                                            guint col,
                                            guint row,
                                            guint width,
                                            guint height,
                                            GwyMaskField *dest,
                                            guint destcol,
                                            guint destrow);
guint32*      gwy_mask_field_get_data      (GwyMaskField *maskfield)       G_GNUC_PURE;
void          gwy_mask_field_invalidate    (GwyMaskField *maskfield);
void          gwy_mask_field_fill          (GwyMaskField *maskfield,
                                            gboolean value);
void          gwy_mask_field_part_fill     (GwyMaskField *maskfield,
                                            guint col,
                                            guint row,
                                            guint width,
                                            guint height,
                                            gboolean value);
void          gwy_mask_field_lnot          (GwyMaskField *maskfield,
                                            const GwyMaskField *mask);
void          gwy_mask_field_land          (GwyMaskField *maskfield,
                                            const GwyMaskField *operand,
                                            const GwyMaskField *mask);
void          gwy_mask_field_lor           (GwyMaskField *maskfield,
                                            const GwyMaskField *operand,
                                            const GwyMaskField *mask);
void          gwy_mask_field_lcopy         (GwyMaskField *maskfield,
                                            const GwyMaskField *operand,
                                            const GwyMaskField *mask);
void          gwy_mask_field_lsubtract     (GwyMaskField *maskfield,
                                            const GwyMaskField *operand,
                                            const GwyMaskField *mask);

#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
#define gwy_mask_field_get(maskfield, col, row) \
    ((maskfield)->data[(maskfield)->stride*(row) + ((col) >> 5)] & ((guint32)1 << (((col) & 0x1f))))
#define gwy_mask_field_set(maskfield, col, row, value) \
    do { \
        if (value) \
            (maskfield)->data[(maskfield)->stride*(row) + ((col) >> 5)] |= ((guint32)1 << (((col) & 0x1f))); \
        else \
            (maskfield)->data[(maskfield)->stride*(row) + ((col) >> 5)] &= ~((guint32)1 << (((col) & 0x1f))); \
    } while (0)
#endif

#if (G_BYTE_ORDER == G_BIG_ENDIAN)
#define gwy_mask_field_get(maskfield, col, row) \
    ((maskfield)->data[(maskfield)->stride*(row) + ((col) >> 5)] & ((guint32)0x80000000u >> (((col) & 0x1f))))
#define gwy_mask_field_set(maskfield, col, row, value) \
    do { \
        if (value) \
            (maskfield)->data[(maskfield)->stride*(row) + ((col) >> 5)] |= ((guint32)0x80000000u >> (((col) & 0x1f))); \
        else \
            (maskfield)->data[(maskfield)->stride*(row) + ((col) >> 5)] &= ~((guint32)0x80000000u >> (((col) & 0x1f))); \
    } while (0)
#endif

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
