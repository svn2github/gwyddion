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

#ifndef __LIBGWYUI_SHAPES_H__
#define __LIBGWYUI_SHAPES_H__

#include <gtk/gtk.h>
#include <libgwy/coords.h>

G_BEGIN_DECLS

#define GWY_TYPE_SHAPES \
    (gwy_shapes_get_type())
#define GWY_SHAPES(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_SHAPES, GwyShapes))
#define GWY_SHAPES_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_SHAPES, GwyShapesClass))
#define GWY_IS_SHAPES(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_SHAPES))
#define GWY_IS_SHAPES_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_SHAPES))
#define GWY_SHAPES_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_SHAPES, GwyShapesClass))

typedef struct _GwyShapes      GwyShapes;
typedef struct _GwyShapesClass GwyShapesClass;

struct _GwyShapes {
    GInitiallyUnowned unowned;
    struct _GwyShapesPrivate *priv;
};

struct _GwyShapesClass {
    /*<private>*/
    GInitiallyUnownedClass unowned_class;
    /*<public>*/
    GType coords_type;
    void (*draw)(GwyShapes *shapes,
                 cairo_t *cr);
    void (*coords_item_inserted)(GwyShapes *shapes,
                                 guint id);
    void (*coords_item_deleted)(GwyShapes *shapes,
                                guint id);
    void (*coords_item_changed)(GwyShapes *shapes,
                                guint id);
    /*<private>*/
    void (*reseved1)(void);
    void (*reseved2)(void);
    void (*reseved3)(void);
    void (*reseved4)(void);
    /*<public>*/
    gboolean (*button_press)(GwyShapes *shapes,
                             GdkEvent *event);
    gboolean (*button_release)(GwyShapes *shapes,
                               GdkEvent *event);
    gboolean (*motion_notify)(GwyShapes *shapes,
                              GdkEvent *event);
    gboolean (*key_press)(GwyShapes *shapes,
                          GdkEvent *event);
    gboolean (*key_release)(GwyShapes *shapes,
                            GdkEvent *event);
    /*<private>*/
    gboolean (*reservedevent1)(void);
    gboolean (*reservedevent2)(void);
};

typedef void (*GwyShapesTransformFunc)(const gdouble *coords_from,
                                       gdouble *coords_to,
                                       gpointer user_data);

GType      gwy_shapes_get_type                    (void)                        G_GNUC_CONST;
void       gwy_shapes_set_coords                  (GwyShapes *shapes,
                                                   GwyCoords *coords);
GwyCoords* gwy_shapes_get_coords                  (GwyShapes *shapes);
GType      gwy_shapes_coords_type                 (const GwyShapes *shapes)     G_GNUC_PURE;
void       gwy_shapes_set_coords_to_view_transform(GwyShapes *shapes,
                                                   GwyShapesTransformFunc func,
                                                   gpointer user_data,
                                                   GDestroyNotify destroy);
void       gwy_shapes_set_view_to_coords_transform(GwyShapes *shapes,
                                                   GwyShapesTransformFunc func,
                                                   gpointer user_data,
                                                   GDestroyNotify destroy);

void         gwy_shapes_draw          (GwyShapes *shapes,
                                       cairo_t *cr);
void         gwy_shapes_set_focus     (GwyShapes *shapes,
                                       gint id);
gint         gwy_shapes_get_focus     (const GwyShapes *shapes) G_GNUC_PURE;
void         gwy_shapes_coords_to_view(const GwyShapes *shapes,
                                       const gdouble *coords,
                                       gdouble *view);
void         gwy_shapes_view_to_coords(const GwyShapes *shapes,
                                       const gdouble *coords,
                                       gdouble *view);
GdkEventMask gwy_shapes_gdk_event_mask(const GwyShapes *shapes) G_GNUC_PURE;
gboolean     gwy_shapes_button_press  (GwyShapes *shapes,
                                       GdkEvent *event);
gboolean     gwy_shapes_button_release(GwyShapes *shapes,
                                       GdkEvent *event);
gboolean     gwy_shapes_motion_notify (GwyShapes *shapes,
                                       GdkEvent *event);
gboolean     gwy_shapes_key_press     (GwyShapes *shapes,
                                       GdkEvent *event);
gboolean     gwy_shapes_key_release   (GwyShapes *shapes,
                                       GdkEvent *event);
void         gwy_shapes_update        (GwyShapes *shapes);
gboolean     gwy_shapes_is_updated    (GwyShapes *shapes);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
