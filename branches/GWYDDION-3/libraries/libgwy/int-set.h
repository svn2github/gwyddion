/*
 *  $Id$
 *  Copyright (C) 2012 David Nečas (Yeti).
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

#ifndef __LIBGWY_INT_SET_H__
#define __LIBGWY_INT_SET_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct {
    gint32 from;
    gint32 to;
} GwyIntRange;

#define GWY_TYPE_INT_SET \
    (gwy_int_set_get_type())
#define GWY_INT_SET(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_INT_SET, GwyIntSet))
#define GWY_INT_SET_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_INT_SET, GwyIntSetClass))
#define GWY_IS_INT_SET(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_INT_SET))
#define GWY_IS_INT_SET_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_INT_SET))
#define GWY_INT_SET_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_INT_SET, GwyIntSetClass))

typedef struct _GwyIntSet GwyIntSet;
typedef struct _GwyIntSetClass GwyIntSetClass;

struct _GwyIntSet {
    GObject g_object;
    struct _GwyIntSetPrivate *priv;
};

struct _GwyIntSetClass {
    /*<private>*/
    GObjectClass g_object_class;
};

typedef void (*GwyIntSetForeachFunc)(gint value,
                                     gpointer user_data);

typedef struct {
    gint value;
    /*<private>*/
    guint priv;
} GwyIntSetIter;

#define gwy_int_set_duplicate(intset) \
        (GWY_INT_SET(gwy_serializable_duplicate(GWY_SERIALIZABLE(intset))))
#define gwy_int_set_assign(dest, src) \
        (gwy_serializable_assign(GWY_SERIALIZABLE(dest), GWY_SERIALIZABLE(src)))

GType              gwy_int_set_get_type       (void)                          G_GNUC_CONST;
GwyIntSet*         gwy_int_set_new            (void)                          G_GNUC_MALLOC;
GwyIntSet*         gwy_int_set_new_with_values(const gint *values,
                                               guint n)                       G_GNUC_MALLOC;
gboolean           gwy_int_set_contains       (const GwyIntSet *intset,
                                               gint value)                    G_GNUC_PURE;
gint               gwy_int_set_index          (const GwyIntSet *intset,
                                               gint value)                    G_GNUC_PURE;
gboolean           gwy_int_set_add            (GwyIntSet *intset,
                                               gint value);
gboolean           gwy_int_set_remove         (GwyIntSet *intset,
                                               gint value);
gboolean           gwy_int_set_toggle         (GwyIntSet *intset,
                                               gint value);
void               gwy_int_set_update         (GwyIntSet *intset,
                                               const gint *values,
                                               guint n);
void               gwy_int_set_fill           (GwyIntSet *intset,
                                               const gint *values,
                                               guint n);
gboolean           gwy_int_set_is_nonempty    (const GwyIntSet *intset)       G_GNUC_PURE;
guint              gwy_int_set_size           (const GwyIntSet *intset)       G_GNUC_PURE;
gint*              gwy_int_set_values         (const GwyIntSet *intset,
                                               guint *len)                    G_GNUC_MALLOC;
const GwyIntRange* gwy_int_set_ranges         (const GwyIntSet *intset,
                                               guint *n)                      G_GNUC_PURE;
void               gwy_int_set_foreach        (const GwyIntSet *intset,
                                               GwyIntSetForeachFunc function,
                                               gpointer user_data);
gboolean           gwy_int_set_first          (const GwyIntSet *intset,
                                               GwyIntSetIter *iter);
gboolean           gwy_int_set_next           (const GwyIntSet *intset,
                                               GwyIntSetIter *iter);
/* TODO: Permit getting/setting by interval. */

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
