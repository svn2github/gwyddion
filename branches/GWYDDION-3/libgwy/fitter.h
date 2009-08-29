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

#ifndef __LIBGWY_FITTER_H__
#define __LIBGWY_FITTER_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define GWY_FITTER_ERROR gwy_fitter_error_quark()

typedef enum {
    GWY_FITTER_ERROR_DUMMY,
} GwyFitterError;

#define GWY_TYPE_FITTER \
    (gwy_fitter_get_type())
#define GWY_FITTER(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_FITTER, GwyFitter))
#define GWY_FITTER_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_FITTER, GwyFitterClass))
#define GWY_IS_FITTER(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_FITTER))
#define GWY_IS_FITTER_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_FITTER))
#define GWY_FITTER_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_FITTER, GwyFitterClass))

typedef struct _GwyFitter      GwyFitter;
typedef struct _GwyFitterClass GwyFitterClass;

struct _GwyFitter {
    GObject g_object;
};

struct _GwyFitterClass {
    GObjectClass g_object_class;
};

GQuark     gwy_fitter_error_quark(void) G_GNUC_CONST;
GType      gwy_fitter_get_type   (void) G_GNUC_CONST;
GwyFitter* gwy_fitter_new        (void) G_GNUC_MALLOC;

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

