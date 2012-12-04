/*
 *  $Id$
 *  Copyright (C) 2010-2012 David Nečas (Yeti).
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

#ifndef __LIBGWY_COORDS_H__
#define __LIBGWY_COORDS_H__

#include <libgwy/serializable.h>
#include <libgwy/unit.h>
#include <libgwy/array.h>
#include <libgwy/int-set.h>

G_BEGIN_DECLS

typedef enum {
    GWY_COORDS_TRANSFORM_TRANSLATE = 1 << 0,
    GWY_COORDS_TRANSFORM_FLIP      = 1 << 1,
    GWY_COORDS_TRANSFORM_TRANSPOSE = 1 << 2,
    GWY_COORDS_TRANSFORM_SCALE     = 1 << 3,
    GWY_COORDS_TRANSFORM_ROTATE    = 1 << 4,
    GWY_COORDS_TRANSFORM_CROP      = 1 << 5,
    GWY_COORDS_TRANSFORM_FUNCTION  = 1 << 6,
} GwyCoordsTransformFlags;

#define GWY_TYPE_COORDS \
    (gwy_coords_get_type())
#define GWY_COORDS(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_COORDS, GwyCoords))
#define GWY_COORDS_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_COORDS, GwyCoordsClass))
#define GWY_IS_COORDS(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_COORDS))
#define GWY_IS_COORDS_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_COORDS))
#define GWY_COORDS_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_COORDS, GwyCoordsClass))

typedef struct _GwyCoords      GwyCoords;
typedef struct _GwyCoordsClass GwyCoordsClass;

struct _GwyCoords {
    GwyArray array;
    struct _GwyCoordsPrivate *priv;
};

struct _GwyCoordsClass {
    /*<private>*/
    GwyArrayClass array_class;
    /*<public>*/
    guint shape_size;
    guint dimension;
    const guint *dimension_map;

    // TODO: Add more transformation types
    void (*translate)(GwyCoords *coords,
                      const GwyIntSet *indices,
                      const gdouble *offsets);
    void (*flip)(GwyCoords *coords,
                 const GwyIntSet *indices,
                 guint axes);
    void (*scale)(GwyCoords *coords,
                  const GwyIntSet *indices,
                  const gdouble *factors);
    void (*transpose)(GwyCoords *coords,
                      const GwyIntSet *indices,
                      const guint *permutation);
    void (*constrain_translation)(const GwyCoords *coords,
                                  const GwyIntSet *indices,
                                  gdouble *offsets,
                                  const gdouble *lower,
                                  const gdouble *upper);
};

typedef gboolean (*GwyCoordsFilterFunc)(GwyCoords *coords,
                                        guint i,
                                        gpointer user_data);

#define gwy_coords_duplicate(coords) \
        (GWY_COORDS(gwy_serializable_duplicate(GWY_SERIALIZABLE(coords))))
#define gwy_coords_assign(dest, src) \
        (gwy_serializable_assign(GWY_SERIALIZABLE(dest), GWY_SERIALIZABLE(src)))

GType        gwy_coords_get_type             (void)                               G_GNUC_CONST;
GwyCoords*   gwy_coords_new_subset           (const GwyCoords *coords,
                                              const GwyIntSet *indices)           G_GNUC_MALLOC;
guint        gwy_coords_shape_size           (const GwyCoords *coords)            G_GNUC_PURE;
guint        gwy_coords_dimension            (const GwyCoords *coords)            G_GNUC_PURE;
const guint* gwy_coords_dimension_map        (const GwyCoords *coords)            G_GNUC_PURE;
gboolean     gwy_coords_can_transform        (const GwyCoords *coords,
                                              GwyCoordsTransformFlags transforms) G_GNUC_PURE;
void         gwy_coords_clear                (GwyCoords *coords);
gboolean     gwy_coords_get                  (const GwyCoords *coords,
                                              guint i,
                                              gdouble *data);
void         gwy_coords_set                  (GwyCoords *coords,
                                              guint i,
                                              const gdouble *data);
void         gwy_coords_delete               (GwyCoords *coords,
                                              guint i);
void         gwy_coords_delete_subset        (GwyCoords *coords,
                                              const GwyIntSet *indices);
guint        gwy_coords_size                 (const GwyCoords *coords)            G_GNUC_PURE;
void         gwy_coords_get_data             (const GwyCoords *coords,
                                              gdouble *data);
void         gwy_coords_set_data             (GwyCoords *coords,
                                              guint n,
                                              const gdouble *data);
void         gwy_coords_filter               (GwyCoords *coords,
                                              GwyCoordsFilterFunc filter,
                                              gpointer user_data);
void         gwy_coords_finished             (GwyCoords *coords);
GwyUnit*     gwy_coords_get_unit             (GwyCoords *coords,
                                              guint i)                            G_GNUC_PURE;
GwyUnit*     gwy_coords_get_mapped_unit      (GwyCoords *coords,
                                              guint i)                            G_GNUC_PURE;
void         gwy_coords_translate            (GwyCoords *coords,
                                              GwyIntSet *indices,
                                              const gdouble *offsets);
void         gwy_coords_flip                 (GwyCoords *coords,
                                              GwyIntSet *indices,
                                              guint axes);
void         gwy_coords_scale                (GwyCoords *coords,
                                              GwyIntSet *indices,
                                              const gdouble *factors);
void         gwy_coords_transpose            (GwyCoords *coords,
                                              GwyIntSet *indices,
                                              const guint *permutation);
void         gwy_coords_transpose_units      (GwyCoords *coords,
                                              const guint *permutation);
void         gwy_coords_constrain_translation(const GwyCoords *coords,
                                              const GwyIntSet *indices,
                                              gdouble *offsets,
                                              const gdouble *lower,
                                              const gdouble *upper);

gboolean gwy_coords_class_can_transform         (const GwyCoordsClass *klass,
                                                 GwyCoordsTransformFlags transforms)  G_GNUC_PURE;
void     gwy_coords_class_set_generic_transforms(GwyCoordsClass *klass,
                                                 GwyCoordsTransformFlags transforms);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
