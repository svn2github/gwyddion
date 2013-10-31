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

#ifndef __LIBGWYUI_CELL_RENDERER_GRADIENT_H__
#define __LIBGWYUI_CELL_RENDERER_GRADIENT_H__

#include <gtk/gtk.h>
#include <libgwy/gradient.h>

G_BEGIN_DECLS

#define GWY_TYPE_CELL_RENDERER_GRADIENT \
    (gwy_cell_renderer_gradient_get_type())
#define GWY_CELL_RENDERER_GRADIENT(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_CELL_RENDERER_GRADIENT, GwyCellRendererGradient))
#define GWY_CELL_RENDERER_GRADIENT_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_CELL_RENDERER_GRADIENT, GwyCellRendererGradientClass))
#define GWY_IS_CELL_RENDERER_GRADIENT(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_CELL_RENDERER_GRADIENT))
#define GWY_IS_CELL_RENDERER_GRADIENT_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_CELL_RENDERER_GRADIENT))
#define GWY_CELL_RENDERER_GRADIENT_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_CELL_RENDERER_GRADIENT, GwyCellRendererGradientClass))

typedef struct _GwyCellRendererGradient      GwyCellRendererGradient;
typedef struct _GwyCellRendererGradientClass GwyCellRendererGradientClass;

struct _GwyCellRendererGradient {
    GtkCellRenderer cell_renderer;
    struct _GwyCellRendererGradientPrivate *priv;
};

struct _GwyCellRendererGradientClass {
    /*<private>*/
    GtkCellRendererClass cell_renderer_class;
    void (*reserved1)(void);
    void (*reserved2)(void);
    void (*reserved3)(void);
    void (*reserved4)(void);
    /*<public>*/
};

GType            gwy_cell_renderer_gradient_get_type(void) G_GNUC_CONST;
GtkCellRenderer* gwy_cell_renderer_gradient_new     (void) G_GNUC_MALLOC;

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
