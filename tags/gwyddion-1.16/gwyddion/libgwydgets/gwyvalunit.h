/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

#ifndef __GWY_VAL_UNIT_H__
#define __GWY_VAL_UNIT_H__

#include <gdk/gdk.h>
#include <gtk/gtkadjustment.h>
#include <gtk/gtkwidget.h>
#include <libgwyddion/gwysiunit.h>


G_BEGIN_DECLS

#define GWY_TYPE_VAL_UNIT            (gwy_val_unit_get_type())
#define GWY_VAL_UNIT(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_VAL_UNIT, GwyValUnit))
#define GWY_VAL_UNIT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_VAL_UNIT, GwyValUnit))
#define GWY_IS_VAL_UNIT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_VAL_UNIT))
#define GWY_IS_VAL_UNIT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_VAL_UNIT))
#define GWY_VAL_UNIT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_VAL_UNIT, GwyValUnitClass))

typedef struct _GwyValUnit      GwyValUnit;
typedef struct _GwyValUnitClass GwyValUnitClass;


struct _GwyValUnit {
    GtkHBox hbox;

    GtkObject *adjustment;
    GtkWidget *spin;
    GtkWidget *selection;
    GtkWidget *label;

    gdouble dival;
    gint unit;
    GwySIUnit *base_si_unit;

    gpointer reserved1;
    gpointer reserved2;
};

struct _GwyValUnitClass {
    GtkVBoxClass parent_class;

    void (*value_changed)(GwyValUnit *val_unit);

    gpointer reserved1;
    gpointer reserved2;
};


GtkWidget* gwy_val_unit_new       (gchar *label_text, GwySIUnit *si_unit);
GType      gwy_val_unit_get_type  (void) G_GNUC_CONST;

void       gwy_val_unit_set_value (GwyValUnit *val_unit, gdouble value);

gdouble    gwy_val_unit_get_value (GwyValUnit *val_unit);

void gwy_val_unit_signal_value_changed(GwyValUnit *val_unit);


G_END_DECLS

#endif /*__GWY_VAL_UNIT_H__*/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
