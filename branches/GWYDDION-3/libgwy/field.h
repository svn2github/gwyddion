/*
 *  $Id$
 *  Copyright (C) 2009-2011 David Nečas (Yeti).
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

#ifndef __LIBGWY_FIELD_H__
#define __LIBGWY_FIELD_H__

#include <libgwy/serializable.h>
#include <libgwy/interpolation.h>
#include <libgwy/unit.h>
#include <libgwy/field-part.h>

G_BEGIN_DECLS

#define GWY_TYPE_FIELD \
    (gwy_field_get_type())
#define GWY_FIELD(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_FIELD, GwyField))
#define GWY_FIELD_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_FIELD, GwyFieldClass))
#define GWY_IS_FIELD(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_FIELD))
#define GWY_IS_FIELD_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_FIELD))
#define GWY_FIELD_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_FIELD, GwyFieldClass))

typedef struct _GwyField      GwyField;
typedef struct _GwyFieldClass GwyFieldClass;

#include <libgwy/mask-field.h>

struct _GwyField {
    GObject g_object;
    struct _GwyFieldPrivate *priv;
    /*<public>*/
    guint xres;
    guint yres;
    gdouble xreal;
    gdouble yreal;
    gdouble xoff;
    gdouble yoff;
    gdouble *data;
};

struct _GwyFieldClass {
    /*<private>*/
    GObjectClass g_object_class;
};

#define gwy_field_duplicate(field) \
        (GWY_FIELD(gwy_serializable_duplicate(GWY_SERIALIZABLE(field))))
#define gwy_field_assign(dest, src) \
        (gwy_serializable_assign(GWY_SERIALIZABLE(dest), GWY_SERIALIZABLE(src)))

GType           gwy_field_get_type     (void)                               G_GNUC_CONST;
GwyField*       gwy_field_new          (void)                               G_GNUC_MALLOC;
GwyField*       gwy_field_new_sized    (guint xres,
                                        guint yres,
                                        gboolean clear)                     G_GNUC_MALLOC;
GwyField*       gwy_field_new_alike    (const GwyField *model,
                                        gboolean clear)                     G_GNUC_MALLOC;
GwyField*       gwy_field_new_part     (const GwyField *field,
                                        const GwyFieldPart *fpart,
                                        gboolean keep_offsets)              G_GNUC_MALLOC;
GwyField*       gwy_field_new_resampled(const GwyField *field,
                                        guint xres,
                                        guint yres,
                                        GwyInterpolationType interpolation) G_GNUC_MALLOC;
GwyField*       gwy_field_new_from_mask(const GwyMaskField *mask,
                                        gdouble zero,
                                        gdouble one)                        G_GNUC_MALLOC;
void            gwy_field_set_size     (GwyField *field,
                                        guint xres,
                                        guint yres,
                                        gboolean clear);
void            gwy_field_data_changed (GwyField *field,
                                        GwyFieldPart *fpart);
void            gwy_field_copy         (const GwyField *src,
                                        const GwyFieldPart *srcpart,
                                        GwyField *dest,
                                        guint destcol,
                                        guint destrow);
void            gwy_field_copy_full    (const GwyField *src,
                                        GwyField *dest);
void            gwy_field_invalidate   (GwyField *field);
void            gwy_field_set_xreal    (GwyField *field,
                                        gdouble xreal);
void            gwy_field_set_yreal    (GwyField *field,
                                        gdouble yreal);
void            gwy_field_set_xoffset  (GwyField *field,
                                        gdouble xoffset);
void            gwy_field_set_yoffset  (GwyField *field,
                                        gdouble yoffset);
GwyUnit*        gwy_field_get_unit_xy  (const GwyField *field)              G_GNUC_PURE;
GwyUnit*        gwy_field_get_unit_z   (const GwyField *field)              G_GNUC_PURE;
GwyValueFormat* gwy_field_format_xy    (const GwyField *field,
                                        GwyValueFormatStyle style)          G_GNUC_MALLOC;
GwyValueFormat* gwy_field_format_z     (const GwyField *field,
                                        GwyValueFormatStyle style)          G_GNUC_MALLOC;

#define gwy_field_index(field, col, row) \
    ((field)->data[(field)->xres*(row) + (col)])
#define gwy_field_dx(field) \
    ((field)->xreal/(field)->xres)
#define gwy_field_dy(field) \
    ((field)->yreal/(field)->yres)

gboolean gwy_field_check_part       (const GwyField *field,
                                     const GwyFieldPart *fpart,
                                     guint *col,
                                     guint *row,
                                     guint *width,
                                     guint *height);
gboolean gwy_field_check_target_part(const GwyField *field,
                                     const GwyFieldPart *fpart,
                                     guint width_full,
                                     guint height_full,
                                     guint *col,
                                     guint *row,
                                     guint *width,
                                     guint *height);
gboolean gwy_field_limit_parts      (const GwyField *src,
                                     const GwyFieldPart *srcpart,
                                     const GwyField *dest,
                                     guint destcol,
                                     guint destrow,
                                     gboolean transpose,
                                     guint *col,
                                     guint *row,
                                     guint *width,
                                     guint *height);
gboolean gwy_field_check_target     (const GwyField *field,
                                     const GwyField *target,
                                     const GwyFieldPart *fpart,
                                     guint *targetcol,
                                     guint *targetrow);
gboolean gwy_field_check_mask       (const GwyField *field,
                                     const GwyFieldPart *fpart,
                                     const GwyMaskField *mask,
                                     GwyMaskingType *masking,
                                     guint *col,
                                     guint *row,
                                     guint *width,
                                     guint *height,
                                     guint *maskcol,
                                     guint *maskrow);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
