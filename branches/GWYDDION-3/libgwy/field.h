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

#ifndef __LIBGWY_FIELD_H__
#define __LIBGWY_FIELD_H__

#include <libgwy/serializable.h>
#include <libgwy/interpolation.h>
#include <libgwy/unit.h>

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
    GObjectClass g_object_class;
};

#define gwy_field_duplicate(field) \
        (GWY_FIELD(gwy_serializable_duplicate(GWY_SERIALIZABLE(field))))
#define gwy_field_assign(dest, src) \
        (gwy_serializable_assign(GWY_SERIALIZABLE(dest), GWY_SERIALIZABLE(src)))

GType     gwy_field_get_type     (void)                               G_GNUC_CONST;
GwyField* gwy_field_new          (void)                               G_GNUC_MALLOC;
GwyField* gwy_field_new_sized    (guint xres,
                                  guint yres,
                                  gboolean clear)                     G_GNUC_MALLOC;
GwyField* gwy_field_new_alike    (GwyField *model,
                                  gboolean clear)                     G_GNUC_MALLOC;
GwyField* gwy_field_new_part     (GwyField *field,
                                  guint col,
                                  guint row,
                                  guint width,
                                  guint height)                       G_GNUC_MALLOC;
GwyField* gwy_field_new_resampled(GwyField *field,
                                  guint xres,
                                  guint yres,
                                  GwyInterpolationType interpolation) G_GNUC_MALLOC;
void      gwy_field_data_changed (GwyField *field);
void      gwy_field_copy         (GwyField *src,
                                  GwyField *dest);
void      gwy_field_part_copy    (GwyField *src,
                                  GwyField *dest,
                                  guint col,
                                  guint row,
                                  guint width,
                                  guint height,
                                  guint destcol,
                                  guint destrow);
gdouble*  gwy_field_get_data     (GwyField *field);
void      gwy_field_set_xreal    (GwyField *field,
                                  gdouble xreal);
void      gwy_field_set_yreal    (GwyField *field,
                                  gdouble yreal);
void      gwy_field_set_xoffset  (GwyField *field,
                                  gdouble xoff);
void      gwy_field_set_yoffset  (GwyField *field,
                                  gdouble yoff);
GwyUnit*  gwy_field_get_unit_xy  (GwyField *field);
GwyUnit*  gwy_field_get_unit_z   (GwyField *field);
void      gwy_field_clear        (GwyField *field);
void      gwy_field_fill         (GwyField *field,
                                  gdouble value);
void      gwy_field_part_clear   (GwyField *field,
                                  guint col,
                                  guint row,
                                  guint width,
                                  guint height);
void      gwy_field_part_fill    (GwyField *field,
                                  guint col,
                                  guint row,
                                  guint width,
                                  guint height,
                                  gdouble value);

#define gwy_field_get_xmeasure(field) \
    ((field)->xreal/(field)->xres)
#define gwy_field_get_ymeasure(field) \
    ((field)->yreal/(field)->yres)

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
