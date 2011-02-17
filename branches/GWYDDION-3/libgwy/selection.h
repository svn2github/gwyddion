/*
 *  $Id$
 *  Copyright (C) 2010 David Nečas (Yeti).
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

#ifndef __LIBGWY_SELECTION_H__
#define __LIBGWY_SELECTION_H__

#include <libgwy/serializable.h>
#include <libgwy/unit.h>
#include <libgwy/array.h>

G_BEGIN_DECLS

#define GWY_TYPE_SELECTION \
    (gwy_selection_get_type())
#define GWY_SELECTION(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_SELECTION, GwySelection))
#define GWY_SELECTION_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_SELECTION, GwySelectionClass))
#define GWY_IS_SELECTION(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_SELECTION))
#define GWY_IS_SELECTION_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_SELECTION))
#define GWY_SELECTION_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_SELECTION, GwySelectionClass))

typedef struct _GwySelection      GwySelection;
typedef struct _GwySelectionClass GwySelectionClass;

struct _GwySelection {
    GwyArray array;
    struct _GwySelectionPrivate *priv;
};

struct _GwySelectionClass {
    /*<private>*/
    GwyArrayClass array_class;
    /*<public>*/
    guint shape_size;
    guint dimension;
    const guint *unit_map;
};

typedef gboolean (*GwySelectionFilterFunc)(GwySelection *selection,
                                           guint i,
                                           gpointer user_data);

#define gwy_selection_duplicate(selection) \
        (GWY_SELECTION(gwy_serializable_duplicate(GWY_SERIALIZABLE(selection))))
#define gwy_selection_assign(dest, src) \
        (gwy_serializable_assign(GWY_SERIALIZABLE(dest), GWY_SERIALIZABLE(src)))

GType        gwy_selection_get_type  (void)                          G_GNUC_CONST;
guint        gwy_selection_shape_size(GwySelection *selection)       G_GNUC_PURE;
guint        gwy_selection_dimension (GwySelection *selection)       G_GNUC_PURE;
const guint* gwy_selection_unit_map  (GwySelection *selection)       G_GNUC_PURE;
void         gwy_selection_clear     (GwySelection *selection);
gboolean     gwy_selection_get       (GwySelection *selection,
                                      guint i,
                                      gdouble *data);
void         gwy_selection_set       (GwySelection *selection,
                                      guint i,
                                      const gdouble *data);
void         gwy_selection_delete    (GwySelection *selection,
                                      guint i);
guint        gwy_selection_size      (GwySelection *selection)       G_GNUC_PURE;
void         gwy_selection_get_data  (GwySelection *selection,
                                      gdouble *data);
void         gwy_selection_set_data  (GwySelection *selection,
                                      guint n,
                                      const gdouble *data);
void         gwy_selection_filter    (GwySelection *selection,
                                      GwySelectionFilterFunc filter,
                                      gpointer user_data);
void         gwy_selection_finished  (GwySelection *selection);
GwyUnit*     gwy_selection_get_units (GwySelection *selection,
                                      guint i)                       G_GNUC_PURE;

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
