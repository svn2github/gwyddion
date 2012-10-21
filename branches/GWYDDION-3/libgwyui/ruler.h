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

#ifndef __LIBGWYUI_RULER_H__
#define __LIBGWYUI_RULER_H__

#include <libgwyui/axis.h>

G_BEGIN_DECLS

#define GWY_TYPE_RULER \
    (gwy_ruler_get_type())
#define GWY_RULER(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_RULER, GwyRuler))
#define GWY_RULER_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_RULER, GwyRulerClass))
#define GWY_IS_RULER(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_RULER))
#define GWY_IS_RULER_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_RULER))
#define GWY_RULER_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_RULER, GwyRulerClass))

typedef struct _GwyRuler      GwyRuler;
typedef struct _GwyRulerClass GwyRulerClass;

struct _GwyRuler {
    GwyAxis axis;
    struct _GwyRulerPrivate *priv;
};

struct _GwyRulerClass {
    /*<private>*/
    GwyAxisClass axis_class;
};

GType      gwy_ruler_get_type(void) G_GNUC_CONST;
GtkWidget* gwy_ruler_new     (void) G_GNUC_MALLOC;

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
