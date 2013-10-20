/*
 *  $Id$
 *  Copyright (C) 2013 David Nečas (Yeti).
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

#ifndef __LIBGWYUI_ADJUSTMENT_H__
#define __LIBGWYUI_ADJUSTMENT_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GWY_TYPE_ADJUSTMENT \
    (gwy_adjustment_get_type())
#define GWY_ADJUSTMENT(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_ADJUSTMENT, GwyAdjustment))
#define GWY_ADJUSTMENT_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_ADJUSTMENT, GwyAdjustmentClass))
#define GWY_IS_ADJUSTMENT(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_ADJUSTMENT))
#define GWY_IS_ADJUSTMENT_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_ADJUSTMENT))
#define GWY_ADJUSTMENT_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_ADJUSTMENT, GwyAdjustmentClass))

typedef struct _GwyAdjustment      GwyAdjustment;
typedef struct _GwyAdjustmentClass GwyAdjustmentClass;

struct _GwyAdjustment {
    GtkAdjustment adjustment;
    struct _GwyAdjustmentPrivate *priv;
};

struct _GwyAdjustmentClass {
    /*<private>*/
    GtkAdjustmentClass adjustment_class;
};

GType          gwy_adjustment_get_type         (void)                             G_GNUC_CONST;
GwyAdjustment* gwy_adjustment_new              (void)                             G_GNUC_MALLOC;
GwyAdjustment* gwy_adjustment_new_set          (gdouble value,
                                                gdouble defaultval,
                                                gdouble lower,
                                                gdouble upper,
                                                gdouble step_increment,
                                                gdouble page_increment)           G_GNUC_MALLOC;
GwyAdjustment* gwy_adjustment_new_for_property (GObject *object,
                                                const gchar *propname)            G_GNUC_MALLOC;
void           gwy_adjustment_set_default      (GwyAdjustment *adjustment,
                                                gdouble defaultval);
gdouble        gwy_adjustment_get_default      (const GwyAdjustment *adjustment)  G_GNUC_PURE;
void           gwy_adjustment_reset            (GwyAdjustment *adjustment);
GObject*       gwy_adjustment_get_object       (const GwyAdjustment *adjustment);
const gchar*   gwy_adjustment_get_property_name(const GwyAdjustment *adjustment);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
