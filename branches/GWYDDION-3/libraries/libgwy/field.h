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

G_END_DECLS

#include <libgwy/mask-field.h>

G_BEGIN_DECLS

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

GType           gwy_field_get_type       (void)                           G_GNUC_CONST;
GwyField*       gwy_field_new            (void)                           G_GNUC_MALLOC;
GwyField*       gwy_field_new_sized      (guint xres,
                                          guint yres,
                                          gboolean clear)                 G_GNUC_MALLOC;
GwyField*       gwy_field_new_alike      (const GwyField *model,
                                          gboolean clear)                 G_GNUC_MALLOC;
GwyField*       gwy_field_new_part       (const GwyField *field,
                                          const GwyFieldPart *fpart,
                                          gboolean keep_offsets)          G_GNUC_MALLOC;
GwyField*       gwy_field_new_resampled  (const GwyField *field,
                                          guint xres,
                                          guint yres,
                                          GwyInterpolation interpolation) G_GNUC_MALLOC;
GwyField*       gwy_field_new_from_mask  (const GwyMaskField *mask,
                                          gdouble zero,
                                          gdouble one)                    G_GNUC_MALLOC;
void            gwy_field_set_size       (GwyField *field,
                                          guint xres,
                                          guint yres,
                                          gboolean clear);
void            gwy_field_data_changed   (GwyField *field,
                                          GwyFieldPart *fpart);
void            gwy_field_copy           (const GwyField *src,
                                          const GwyFieldPart *srcpart,
                                          GwyField *dest,
                                          guint destcol,
                                          guint destrow);
void            gwy_field_copy_full      (const GwyField *src,
                                          GwyField *dest);
void            gwy_field_invalidate     (GwyField *field);
void            gwy_field_set_xreal      (GwyField *field,
                                          gdouble xreal);
void            gwy_field_set_yreal      (GwyField *field,
                                          gdouble yreal);
void            gwy_field_set_xoffset    (GwyField *field,
                                          gdouble xoffset);
void            gwy_field_set_yoffset    (GwyField *field,
                                          gdouble yoffset);
void            gwy_field_clear_offsets  (GwyField *field);
GwyUnit*        gwy_field_get_xunit      (const GwyField *field)          G_GNUC_PURE;
GwyUnit*        gwy_field_get_yunit      (const GwyField *field)          G_GNUC_PURE;
GwyUnit*        gwy_field_get_zunit      (const GwyField *field)          G_GNUC_PURE;
gboolean        gwy_field_xy_units_match (const GwyField *field)          G_GNUC_PURE;
gboolean        gwy_field_xyz_units_match(const GwyField *field)          G_GNUC_PURE;
GwyValueFormat* gwy_field_format_x       (const GwyField *field,
                                          GwyValueFormatStyle style)      G_GNUC_MALLOC;
GwyValueFormat* gwy_field_format_y       (const GwyField *field,
                                          GwyValueFormatStyle style)      G_GNUC_MALLOC;
GwyValueFormat* gwy_field_format_xy      (const GwyField *field,
                                          GwyValueFormatStyle style)      G_GNUC_MALLOC;
GwyValueFormat* gwy_field_format_z       (const GwyField *field,
                                          GwyValueFormatStyle style)      G_GNUC_MALLOC;
void            gwy_field_set_name       (GwyField *field,
                                          const gchar *name);
const gchar*    gwy_field_get_name       (const GwyField *field)          G_GNUC_PURE;
gdouble         gwy_field_dx             (const GwyField *field)          G_GNUC_PURE;
gdouble         gwy_field_dy             (const GwyField *field)          G_GNUC_PURE;

#define gwy_field_index(field, col, row) \
    ((field)->data[(field)->xres*(row) + (col)])

gdouble        gwy_field_get          (const GwyField *field,
                                       guint col,
                                       guint row)                 G_GNUC_PURE;
void           gwy_field_set          (const GwyField *field,
                                       guint col,
                                       guint row,
                                       gdouble value);
gdouble*       gwy_field_get_data     (const GwyField *field,
                                       const GwyFieldPart *fpart,
                                       const GwyMaskField *mask,
                                       GwyMasking masking,
                                       guint *ndata)              G_GNUC_MALLOC;
const gdouble* gwy_field_get_data_full(const GwyField *field,
                                       guint *ndata)              G_GNUC_PURE;
void           gwy_field_set_data     (GwyField *field,
                                       const GwyFieldPart *fpart,
                                       const GwyMaskField *mask,
                                       GwyMasking masking,
                                       const gdouble *data,
                                       guint ndata);
void           gwy_field_set_data_full(GwyField *field,
                                       const gdouble *data,
                                       guint ndata);

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
                                     GwyMasking *masking,
                                     guint *col,
                                     guint *row,
                                     guint *width,
                                     guint *height,
                                     guint *maskcol,
                                     guint *maskrow);
gboolean gwy_field_check_target_mask(const GwyField *field,
                                     const GwyMaskField *target,
                                     const GwyFieldPart *fpart,
                                     guint *targetcol,
                                     guint *targetrow);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
