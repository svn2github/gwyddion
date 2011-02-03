/*
 *  $Id$
 *  Copyright (C) 2009-2010 David Necas (Yeti).
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
#include <libgwy/rectangle.h>

G_BEGIN_DECLS

typedef enum {
    GWY_MASK_IGNORE  = 0,
    GWY_MASK_INCLUDE = 1,
    GWY_MASK_EXCLUDE = 2,
} GwyMaskingType;

typedef enum {
    GWY_LOGICAL_ZERO,
    GWY_LOGICAL_AND,
    GWY_LOGICAL_NIMPL,
    GWY_LOGICAL_A,
    GWY_LOGICAL_NCIMPL,
    GWY_LOGICAL_B,
    GWY_LOGICAL_XOR,
    GWY_LOGICAL_OR,
    GWY_LOGICAL_NOR,
    GWY_LOGICAL_NXOR,
    GWY_LOGICAL_NB,
    GWY_LOGICAL_CIMPL,
    GWY_LOGICAL_NA,
    GWY_LOGICAL_IMPL,
    GWY_LOGICAL_NAND,
    GWY_LOGICAL_ONE,
} GwyLogicalOperator;

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

#include <libgwy/field.h>

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
    /*<private>*/
    GObjectClass g_object_class;
};

#define gwy_mask_field_duplicate(field) \
        (GWY_MASK_FIELD(gwy_serializable_duplicate(GWY_SERIALIZABLE(field))))
#define gwy_mask_field_assign(dest, src) \
        (gwy_serializable_assign(GWY_SERIALIZABLE(dest), GWY_SERIALIZABLE(src)))

GType         gwy_mask_field_get_type      (void)                      G_GNUC_CONST;
GwyMaskField* gwy_mask_field_new           (void)                      G_GNUC_MALLOC;
GwyMaskField* gwy_mask_field_new_sized     (guint xres,
                                            guint yres,
                                            gboolean clear)            G_GNUC_MALLOC;
GwyMaskField* gwy_mask_field_new_part      (const GwyMaskField *field,
                                            const GwyRectangle *rectangle) G_GNUC_MALLOC;
GwyMaskField* gwy_mask_field_new_resampled (const GwyMaskField *field,
                                            guint xres,
                                            guint yres)                G_GNUC_MALLOC;
GwyMaskField* gwy_mask_field_new_from_field(const GwyField *field,
                                            const GwyRectangle *rectangle,
                                            gdouble lower,
                                            gdouble upper,
                                            gboolean complement)       G_GNUC_MALLOC;
void          gwy_mask_field_set_size      (GwyMaskField *field,
                                            guint xres,
                                            guint yres,
                                            gboolean clear);

#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
#define gwy_mask_field_get(field, col, row) \
    ((field)->data[(field)->stride*(row) + ((col) >> 5)] \
     & ((guint32)1 << (((col) & 0x1f))))

#define gwy_mask_field_set(field, col, row, value) \
    do { \
        if (value) \
            (field)->data[(field)->stride*(row) + ((col) >> 5)] \
                |= ((guint32)1 << (((col) & 0x1f))); \
        else \
            (field)->data[(field)->stride*(row) + ((col) >> 5)] \
                &= ~((guint32)1 << (((col) & 0x1f))); \
    } while (0)
#endif

#if (G_BYTE_ORDER == G_BIG_ENDIAN)
#define gwy_mask_field_get(field, col, row) \
    ((field)->data[(field)->stride*(row) + ((col) >> 5)] \
     & ((guint32)0x80000000u >> (((col) & 0x1f))))

#define gwy_mask_field_set(field, col, row, value) \
    do { \
        if (value) \
            (field)->data[(field)->stride*(row) + ((col) >> 5)] \
                |= ((guint32)0x80000000u >> (((col) & 0x1f))); \
        else \
            (field)->data[(field)->stride*(row) + ((col) >> 5)] \
                &= ~((guint32)0x80000000u >> (((col) & 0x1f))); \
    } while (0)
#endif

typedef struct {
    guint32 *p;
    guint32 bit;
} GwyMaskIter;

#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
#define gwy_mask_field_iter_init(field, iter, col, row) \
    do { \
        iter.p = (field)->data + (field)->stride*(row) + ((col) >> 5); \
        iter.bit = 1u << ((col) & 0x1f); \
    } while (0)

#define gwy_mask_iter_next(iter) \
    do { if (!(iter.bit <<= 1)) { iter.bit = 1u; iter.p++; } } while (0)

#define gwy_mask_iter_prev(iter) \
    do { if (!(iter.bit >>= 1)) { iter.bit = 0x80000000u; iter.p--; } } while (0)
#endif

#if (G_BYTE_ORDER == G_BIG_ENDIAN)
#define gwy_mask_field_iter_init(field, iter, col, row) \
    do { \
        iter.p = (field)->data + (field)->stride*(row) + ((col) >> 5); \
        iter.bit = 0x80000000u >> ((col) & 0x1f); \
    } while (0)

#define gwy_mask_iter_next(iter) \
    do { if (!(iter.bit >>= 1)) { iter.bit = 0x80000000u; iter.p++; } } while (0)

#define gwy_mask_iter_prev(iter) \
    do { if (!(iter.bit <<= 1)) { iter.bit = 1u; iter.p--; } } while (0)
#endif

#define gwy_mask_iter_get(iter) \
    (*(iter).p & (iter).bit)

#define gwy_mask_iter_set(iter, value) \
    do { if (value) *(iter).p |= (iter).bit; else *(iter).p &= ~(iter).bit; } while (0)

void         gwy_mask_field_data_changed (GwyMaskField *field);
void         gwy_mask_field_copy         (const GwyMaskField *src,
                                          const GwyRectangle *srcrectangle,
                                          GwyMaskField *dest,
                                          guint destcol,
                                          guint destrow);
void         gwy_mask_field_copy_full    (const GwyMaskField *src,
                                          GwyMaskField *dest);
guint32*     gwy_mask_field_get_data     (GwyMaskField *field);
void         gwy_mask_field_invalidate   (GwyMaskField *field);
void         gwy_mask_field_fill         (GwyMaskField *field,
                                          const GwyRectangle *rectangle,
                                          gboolean value);
void         gwy_mask_field_fill_ellipse (GwyMaskField *field,
                                          const GwyRectangle *rectangle,
                                          gboolean entire_rectangle,
                                          gboolean value);
void         gwy_mask_field_logical      (GwyMaskField *field,
                                          const GwyMaskField *operand,
                                          const GwyMaskField *mask,
                                          GwyLogicalOperator op);
void         gwy_mask_field_part_logical (GwyMaskField *field,
                                          const GwyRectangle *rectangle,
                                          const GwyMaskField *operand,
                                          guint opcol,
                                          guint oprow,
                                          GwyLogicalOperator op);
void         gwy_mask_field_shrink       (GwyMaskField *field,
                                          gboolean from_borders);
void         gwy_mask_field_grow         (GwyMaskField *field,
                                          gboolean separate_grains);
guint        gwy_mask_field_count        (const GwyMaskField *field,
                                          const GwyMaskField *mask,
                                          gboolean value);
guint        gwy_mask_field_part_count   (const GwyMaskField *field,
                                          const GwyRectangle *rectangle,
                                          gboolean value);
guint        gwy_mask_field_count_rows   (const GwyMaskField *field,
                                          const GwyRectangle *rectangle,
                                          gboolean value,
                                          guint *counts);
const guint* gwy_mask_field_number_grains(GwyMaskField *field,
                                          guint *ngrains);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
