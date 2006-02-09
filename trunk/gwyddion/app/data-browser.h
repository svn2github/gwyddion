/*
 *  @(#) $Id$
 *  Copyright (C) 2006 David Necas (Yeti), Petr Klapetek, Chris Anderson
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net, sidewinderasu@gmail.com.
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

#ifndef __GWY_DATA_BROWSER_H__
#define __GWY_DATA_BROWSER_H__

#include <libprocess/datafield.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwygraph.h>

G_BEGIN_DECLS

typedef enum {
    GWY_APP_CONTAINER = 1,
    GWY_APP_DATA_VIEW,
    GWY_APP_GRAPH,
    GWY_APP_DATA_FIELD,
    GWY_APP_DATA_FIELD_KEY,
    GWY_APP_DATA_FIELD_ID,
    GWY_APP_MASK_FIELD,
    GWY_APP_MASK_FIELD_KEY,
    GWY_APP_SHOW_FIELD,
    GWY_APP_SHOW_FIELD_KEY,
    GWY_APP_GRAPH_MODEL,
    GWY_APP_GRAPH_MODEL_KEY,
    GWY_APP_GRAPH_MODEL_ID,
} GwyAppWhat;   /* XXX: silly name */

typedef enum {
    GWY_DATA_ITEM_GRADIENT = 1,
    GWY_DATA_ITEM_PALETTE = GWY_DATA_ITEM_GRADIENT,
    GWY_DATA_ITEM_RANGE,
    GWY_DATA_ITEM_MASK_COLOR,
    /* GWY_DATA_ITEM_SELECTION: Current, all? */
} GwyDataItem;

void   gwy_app_data_browser_add                 (GwyContainer *data);
void   gwy_app_data_browser_select_data_view    (GwyDataView *data_view);
void   gwy_app_data_browser_select_graph        (GwyGraph *graph);
gint   gwy_app_data_browser_add_data_field      (GwyDataField *dfield,
                                                 GwyContainer *data,
                                                 gboolean showit);
gint   gwy_app_data_browser_add_graph_model     (GwyGraphModel *gmodel,
                                                 GwyContainer *data,
                                                 gboolean showit);
void   gwy_app_data_browser_get_current         (GwyAppWhat what,
                                                 ...);
void   gwy_app_copy_data_items                  (GwyContainer *source,
                                                 GwyContainer *dest,
                                                 gint from_id,
                                                 gint to_id,
                                                 ...);
GQuark gwy_app_get_mask_key_for_id              (gint id);
GQuark gwy_app_get_presentation_key_for_id      (gint id);
/* XXX */
void   gwy_app_data_browser_shut_down           (void);

G_END_DECLS

#endif /* __GWY_DATA_BROWSER_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
