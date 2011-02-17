/*
 *  $Id$
 *  Copyright (C) 2011 David Nečas (Yeti).
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

#ifndef __LIBGWY_BRICK_H__
#define __LIBGWY_BRICK_H__

#include <libgwy/serializable.h>
#include <libgwy/interpolation.h>
#include <libgwy/unit.h>
#include <libgwy/brick-part.h>

G_BEGIN_DECLS

#define GWY_TYPE_BRICK \
    (gwy_brick_get_type())
#define GWY_BRICK(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_BRICK, GwyBrick))
#define GWY_BRICK_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_BRICK, GwyBrickClass))
#define GWY_IS_BRICK(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_BRICK))
#define GWY_IS_BRICK_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_BRICK))
#define GWY_BRICK_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_BRICK, GwyBrickClass))

typedef struct _GwyBrick      GwyBrick;
typedef struct _GwyBrickClass GwyBrickClass;

struct _GwyBrick {
    GObject g_object;
    struct _GwyBrickPrivate *priv;
    /*<public>*/
    guint xres;
    guint yres;
    guint zres;
    gdouble xreal;
    gdouble yreal;
    gdouble zreal;
    gdouble xoff;
    gdouble yoff;
    gdouble zoff;
    gdouble *data;
};

struct _GwyBrickClass {
    /*<private>*/
    GObjectClass g_object_class;
};

#define gwy_brick_duplicate(brick) \
        (GWY_BRICK(gwy_serializable_duplicate(GWY_SERIALIZABLE(brick))))
#define gwy_brick_assign(dest, src) \
        (gwy_serializable_assign(GWY_SERIALIZABLE(dest), GWY_SERIALIZABLE(src)))

GType           gwy_brick_get_type    (void)                      G_GNUC_CONST;
GwyBrick*       gwy_brick_new         (void)                      G_GNUC_MALLOC;
GwyBrick*       gwy_brick_new_sized   (guint xres,
                                       guint yres,
                                       guint zres,
                                       gboolean clear)            G_GNUC_MALLOC;
GwyBrick*       gwy_brick_new_alike   (const GwyBrick *model,
                                       gboolean clear)            G_GNUC_MALLOC;
GwyBrick*       gwy_brick_new_part    (const GwyBrick *brick,
                                       const GwyBrickPart *bpart,
                                       gboolean keep_offsets)     G_GNUC_MALLOC;
void            gwy_brick_set_size    (GwyBrick *brick,
                                       guint xres,
                                       guint yres,
                                       gboolean clear);
void            gwy_brick_data_changed(GwyBrick *brick);
void            gwy_brick_copy        (const GwyBrick *src,
                                       const GwyBrickPart *srcpart,
                                       GwyBrick *dest,
                                       guint destcol,
                                       guint destrow,
                                       guint destlevel);
void            gwy_brick_copy_full   (const GwyBrick *src,
                                       GwyBrick *dest);
void            gwy_brick_invalidate  (GwyBrick *brick);
void            gwy_brick_set_xreal   (GwyBrick *brick,
                                       gdouble xreal);
void            gwy_brick_set_yreal   (GwyBrick *brick,
                                       gdouble yreal);
void            gwy_brick_set_zreal   (GwyBrick *brick,
                                       gdouble zreal);
void            gwy_brick_set_xoffset (GwyBrick *brick,
                                       gdouble xoffset);
void            gwy_brick_set_yoffset (GwyBrick *brick,
                                       gdouble yoffset);
void            gwy_brick_set_zoffset (GwyBrick *brick,
                                       gdouble zoffset);
GwyUnit*        gwy_brick_get_unit_xy (const GwyBrick *brick)     G_GNUC_PURE;
GwyUnit*        gwy_brick_get_unit_z  (const GwyBrick *brick)     G_GNUC_PURE;
GwyUnit*        gwy_brick_get_unit_w  (const GwyBrick *brick)     G_GNUC_PURE;
GwyValueFormat* gwy_brick_format_xy   (const GwyBrick *brick,
                                       GwyValueFormatStyle style) G_GNUC_MALLOC;

#define gwy_brick_index(brick, col, row, level) \
    ((brick)->data[(brick)->xres*((brick)->yres*(level) + (row)) + (col)])
#define gwy_brick_dx(brick) \
    ((brick)->xreal/(brick)->xres)
#define gwy_brick_dy(brick) \
    ((brick)->yreal/(brick)->yres)
#define gwy_brick_dz(brick) \
    ((brick)->zreal/(brick)->zres)

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
