/*
 *  $Id$
 *  Copyright (C) 2011-2012 David Nečas (Yeti).
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
#include <libgwy/math.h>
#include <libgwy/coords.h>
#include <libgwy/int-set.h>

G_BEGIN_DECLS

typedef enum {
    GWY_SHAPES_STATE_NORMAL   = 0,
    GWY_SHAPES_STATE_SELECTED = 1 << 0,
    GWY_SHAPES_STATE_PRELIGHT = 1 << 1,
} GwyShapesStateType;

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

    /*<public>*/
    GwyIntSet *selection;
    cairo_rectangle_t bounding_box;
    cairo_matrix_t coords_to_view;
    cairo_matrix_t view_to_coords;
    cairo_matrix_t pixel_to_view;
    cairo_matrix_t view_to_pixel;
    gboolean has_current_point;
    GwyXY current_point;
};

struct _GwyShapesClass {
    /*<private>*/
    GInitiallyUnownedClass unowned_class;
    /*<public>*/
    GType coords_type;
    void (*draw)(GwyShapes *shapes,
                 cairo_t *cr);
    // FIXME: By making these virtual methods we force the signals to be
    // expensive because something is always connected even if the subclass
    // does not need the signal – but can it really function without them?
    void (*coords_item_inserted)(GwyShapes *shapes,
                                 guint id);
    void (*coords_item_deleted)(GwyShapes *shapes,
                                guint id);
    void (*coords_item_updated)(GwyShapes *shapes,
                                guint id);
    void (*cancel_editing)(GwyShapes *shapes,
                           gint id);
    void (*selection_added)(GwyShapes *shapes,
                            gint value);
    void (*selection_removed)(GwyShapes *shapes,
                              gint value);
    void (*selection_assigned)(GwyShapes *shapes);
    /*<private>*/
    void (*reseved1)(void);
    void (*reseved2)(void);
    void (*reseved3)(void);
    void (*reseved4)(void);
    /*<public>*/
    gboolean (*button_press)(GwyShapes *shapes,
                             GdkEventButton *event);
    gboolean (*button_release)(GwyShapes *shapes,
                               GdkEventButton *event);
    gboolean (*motion_notify)(GwyShapes *shapes,
                              GdkEventMotion *event);
    gboolean (*key_press)(GwyShapes *shapes,
                          GdkEventKey *event);
    gboolean (*key_release)(GwyShapes *shapes,
                            GdkEventKey *event);
    /*<private>*/
    gboolean (*reservedevent1)(void);
    gboolean (*reservedevent2)(void);
};

GType        gwy_shapes_get_type           (void)                                  G_GNUC_CONST;
void         gwy_shapes_set_coords         (GwyShapes *shapes,
                                            GwyCoords *coords);
GwyCoords*   gwy_shapes_get_coords         (GwyShapes *shapes);
GType        gwy_shapes_class_coords_type  (const GwyShapesClass *klass)           G_GNUC_PURE;
GType        gwy_shapes_coords_type        (const GwyShapes *shapes)               G_GNUC_PURE;
void         gwy_shapes_set_coords_matrices(GwyShapes *shapes,
                                            const cairo_matrix_t *coords_to_view,
                                            const cairo_matrix_t *view_to_coords);
void         gwy_shapes_set_pixel_matrices (GwyShapes *shapes,
                                            const cairo_matrix_t *pixel_to_view,
                                            const cairo_matrix_t *view_to_pixel);
void         gwy_shapes_set_bounding_box   (GwyShapes *shapes,
                                            const cairo_rectangle_t *bbox);
void         gwy_shapes_set_max_shapes     (GwyShapes *shapes,
                                            guint max_shapes);
guint        gwy_shapes_get_max_shapes     (const GwyShapes *shapes)               G_GNUC_PURE;
void         gwy_shapes_set_editable       (GwyShapes *shapes,
                                            gboolean editable);
gboolean     gwy_shapes_get_editable       (const GwyShapes *shapes)               G_GNUC_PURE;
void         gwy_shapes_draw               (GwyShapes *shapes,
                                            cairo_t *cr);
GdkEventMask gwy_shapes_gdk_event_mask     (const GwyShapes *shapes)               G_GNUC_PURE;
gboolean     gwy_shapes_button_press       (GwyShapes *shapes,
                                            GdkEventButton *event);
gboolean     gwy_shapes_button_release     (GwyShapes *shapes,
                                            GdkEventButton *event);
gboolean     gwy_shapes_motion_notify      (GwyShapes *shapes,
                                            GdkEventMotion *event);
gboolean     gwy_shapes_key_press          (GwyShapes *shapes,
                                            GdkEventKey *event);
gboolean     gwy_shapes_key_release        (GwyShapes *shapes,
                                            GdkEventKey *event);
void         gwy_shapes_update             (GwyShapes *shapes);
gboolean     gwy_shapes_is_updated         (GwyShapes *shapes);
void         gwy_shapes_editing_started    (GwyShapes *shapes);
gboolean     gwy_shapes_current_point      (const GwyShapes *shapes,
                                            GwyXY *xy);
void         gwy_shapes_stroke             (GwyShapes *shapes,
                                            cairo_t *cr,
                                            GwyShapesStateType state);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
