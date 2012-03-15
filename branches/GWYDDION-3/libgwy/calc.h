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

#ifndef __LIBGWY_CALC_H__
#define __LIBGWY_CALC_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define GWY_CALC_ERROR gwy_calc_error_quark()

typedef enum {
    GWY_CALC_ERROR_CANCELLED = 1,
} GwyCalcError;

#define GWY_TYPE_CALC \
    (gwy_calc_get_type())
#define GWY_CALC(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_CALC, GwyCalc))
#define GWY_CALC_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_CALC, GwyCalcClass))
#define GWY_IS_CALC(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_CALC))
#define GWY_IS_CALC_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_CALC))
#define GWY_CALC_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_CALC, GwyCalcClass))

typedef struct _GwyCalc      GwyCalc;
typedef struct _GwyCalcClass GwyCalcClass;

struct _GwyCalc {
    GObject g_object;
    struct _GwyCalcPrivate *priv;
};

struct _GwyCalcClass {
    /*<private>*/
    GObjectClass g_object_class;
};

GQuark   gwy_calc_error_quark (void)              G_GNUC_CONST;
GType    gwy_calc_get_type    (void)              G_GNUC_CONST;
GwyCalc* gwy_calc_new         (void)              G_GNUC_MALLOC;
/*
void     gwy_calc_run         (GwyCalc *calc);
void     gwy_calc_cancel      (GwyCalc *calc);
void     gwy_calc_set_func    (GwyCalc *calc,
                               GwyCalcFunc func);
void     gwy_calc_set_data    (GwyCalc *calc,
                               gpointer data);
gdouble  gwy_calc_get_progress(GwyCalc *calc);
guint    gwy_calc_get_stage   (GwyCalc *calc);
*/

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
