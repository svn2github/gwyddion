/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2007 David Necas (Yeti), Petr Klapetek.
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

#ifndef __GWY_LAYER_BASIC_H__
#define __GWY_LAYER_BASIC_H__

#include <libgwydgets/gwypixmaplayer.h>
#include <libdraw/gwygradient.h>

G_BEGIN_DECLS

#define GWY_TYPE_LAYER_BASIC            (gwy_layer_basic_get_type())
#define GWY_LAYER_BASIC(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_LAYER_BASIC, GwyLayerBasic))
#define GWY_LAYER_BASIC_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_LAYER_BASIC, GwyLayerBasicClass))
#define GWY_IS_LAYER_BASIC(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_LAYER_BASIC))
#define GWY_IS_LAYER_BASIC_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_LAYER_BASIC))
#define GWY_LAYER_BASIC_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_LAYER_BASIC, GwyLayerBasicClass))

typedef struct _GwyLayerBasic      GwyLayerBasic;
typedef struct _GwyLayerBasicClass GwyLayerBasicClass;

struct _GwyLayerBasic {
    GwyPixmapLayer parent_instance;

    GQuark range_type_key;
    gulong range_type_id;

    GwyGradient *gradient;
    GQuark gradient_key;
    gulong gradient_id;
    gulong gradient_item_id;

    GQuark show_key;
    GObject *show_field;
    gulong show_id;
    gulong show_item_id;

    GQuark fixed_key;
    gulong min_id;
    gulong max_id;
    gulong handler_id;

    gpointer default_range_type;  /* In fact GwyLayerBasicRangeType */
    gpointer reserved2;
};

struct _GwyLayerBasicClass {
    GwyPixmapLayerClass parent_class;

    /* signals */
    void (*presentation_switched)(GwyLayerBasic *basic_layer);

    void (*reserved1)(void);
    void (*reserved2)(void);
};

GType           gwy_layer_basic_get_type           (void) G_GNUC_CONST;
GwyPixmapLayer* gwy_layer_basic_new                (void);
void            gwy_layer_basic_get_range          (GwyLayerBasic *basic_layer,
                                                    gdouble *min,
                                                    gdouble *max);
void            gwy_layer_basic_set_gradient_key   (GwyLayerBasic *basic_layer,
                                                    const gchar *key);
const gchar*    gwy_layer_basic_get_gradient_key   (GwyLayerBasic *basic_layer);
void           gwy_layer_basic_set_presentation_key(GwyLayerBasic *basic_layer,
                                                    const gchar *key);
const gchar*   gwy_layer_basic_get_presentation_key(GwyLayerBasic *basic_layer);
gboolean       gwy_layer_basic_get_has_presentation(GwyLayerBasic *basic_layer);
void            gwy_layer_basic_set_min_max_key    (GwyLayerBasic *basic_layer,
                                                    const gchar *prefix);
const gchar*    gwy_layer_basic_get_min_max_key    (GwyLayerBasic *basic_layer);
void            gwy_layer_basic_set_range_type_key (GwyLayerBasic *basic_layer,
                                                    const gchar *key);
const gchar*    gwy_layer_basic_get_range_type_key (GwyLayerBasic *basic_layer);

G_END_DECLS

#endif /* __GWY_LAYER_BASIC_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

