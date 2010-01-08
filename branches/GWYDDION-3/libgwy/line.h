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

#ifndef __LIBGWY_LINE_H__
#define __LIBGWY_LINE_H__

#include <libgwy/serializable.h>
#include <libgwy/interpolation.h>
#include <libgwy/unit.h>

G_BEGIN_DECLS

#define GWY_TYPE_LINE \
    (gwy_line_get_type())
#define GWY_LINE(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_LINE, GwyLine))
#define GWY_LINE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_LINE, GwyLineClass))
#define GWY_IS_LINE(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_LINE))
#define GWY_IS_LINE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_LINE))
#define GWY_LINE_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_LINE, GwyLineClass))

typedef struct _GwyLine      GwyLine;
typedef struct _GwyLineClass GwyLineClass;

struct _GwyLine {
    GObject g_object;
    struct _GwyLinePrivate *priv;
    /*<public>*/
    guint res;
    gdouble real;
    gdouble off;
    gdouble *data;
};

struct _GwyLineClass {
    GObjectClass g_object_class;
};

#define gwy_line_duplicate(line) \
        (GWY_LINE(gwy_serializable_duplicate(GWY_SERIALIZABLE(line))))
#define gwy_line_assign(dest, src) \
        (gwy_serializable_assign(GWY_SERIALIZABLE(dest), GWY_SERIALIZABLE(src)))

GType           gwy_line_get_type     (void)                               G_GNUC_CONST;
GwyLine*        gwy_line_new          (void)                               G_GNUC_MALLOC;
GwyLine*        gwy_line_new_sized    (guint res,
                                       gboolean clear)                     G_GNUC_MALLOC;
GwyLine*        gwy_line_new_alike    (const GwyLine *model,
                                       gboolean clear)                     G_GNUC_MALLOC;
GwyLine*        gwy_line_new_part     (const GwyLine *line,
                                       guint pos,
                                       guint len,
                                       gboolean keep_offset)               G_GNUC_MALLOC;
GwyLine*        gwy_line_new_resampled(const GwyLine *line,
                                       guint res,
                                       GwyInterpolationType interpolation) G_GNUC_MALLOC;
void            gwy_line_set_size     (GwyLine *line,
                                       guint res,
                                       gboolean clear);
void            gwy_line_data_changed (GwyLine *line);
void            gwy_line_copy         (const GwyLine *src,
                                       GwyLine *dest);
void            gwy_line_part_copy    (const GwyLine *src,
                                       guint pos,
                                       guint len,
                                       GwyLine *dest,
                                       guint destpos);
void            gwy_line_set_real     (GwyLine *line,
                                       gdouble real);
void            gwy_line_set_offset   (GwyLine *line,
                                       gdouble offset);
GwyUnit*        gwy_line_get_unit_x   (GwyLine *line)                      G_GNUC_PURE;
GwyUnit*        gwy_line_get_unit_y   (GwyLine *line)                      G_GNUC_PURE;
void            gwy_line_clear        (GwyLine *line);
void            gwy_line_fill         (GwyLine *line,
                                       gdouble value);
void            gwy_line_part_clear   (GwyLine *line,
                                       guint pos,
                                       guint len);
void            gwy_line_part_fill    (GwyLine *line,
                                       guint pos,
                                       guint len,
                                       gdouble value);
GwyValueFormat* gwy_line_get_format_x (GwyLine *line,
                                       GwyValueFormatStyle style,
                                       GwyValueFormat *format);
GwyValueFormat* gwy_line_get_format_y (GwyLine *line,
                                       GwyValueFormatStyle style,
                                       GwyValueFormat *format);

#define gwy_line_index(line, pos) \
    ((line)->data[pos])
#define gwy_line_dx(line) \
    ((line)->real/(line)->res)

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
