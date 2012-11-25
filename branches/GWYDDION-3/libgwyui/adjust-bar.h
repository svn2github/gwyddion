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

#ifndef __LIBGWYUI_ADJUST_BAR_H__
#define __LIBGWYUI_ADJUST_BAR_H__

#include <gtk/gtk.h>
#include <libgwyui/spin-button.h>

G_BEGIN_DECLS

typedef enum {
    GWY_SCALE_MAPPING_LINEAR = 0,
    GWY_SCALE_MAPPING_SQRT   = 1,
    GWY_SCALE_MAPPING_LOG    = 2,
} GwyScaleMappingType;

#define GWY_TYPE_ADJUST_BAR \
    (gwy_adjust_bar_get_type())
#define GWY_ADJUST_BAR(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_ADJUST_BAR, GwyAdjustBar))
#define GWY_ADJUST_BAR_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_ADJUST_BAR, GwyAdjustBarClass))
#define GWY_IS_ADJUST_BAR(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_ADJUST_BAR))
#define GWY_IS_ADJUST_BAR_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_ADJUST_BAR))
#define GWY_ADJUST_BAR_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_ADJUST_BAR, GwyAdjustBarClass))

typedef struct _GwyAdjustBar      GwyAdjustBar;
typedef struct _GwyAdjustBarClass GwyAdjustBarClass;

struct _GwyAdjustBar {
    GtkWidget widget;
    struct _GwyAdjustBarPrivate *priv;
};

struct _GwyAdjustBarClass {
    /*<private>*/
    GtkWidgetClass widget_class;
};

GType               gwy_adjust_bar_get_type           (void)                         G_GNUC_CONST;
GtkWidget*          gwy_adjust_bar_new                (void)                         G_GNUC_MALLOC;
void                gwy_adjust_bar_set_adjustment     (GwyAdjustBar *adjbar,
                                                       GtkAdjustment *adjustment);
GtkAdjustment*      gwy_adjust_bar_get_adjustment     (const GwyAdjustBar *adjbar)   G_GNUC_PURE;
void                gwy_adjust_bar_set_label          (GwyAdjustBar *adjbar,
                                                       const gchar *text);
void                gwy_adjust_bar_set_label_full     (GwyAdjustBar *adjbar,
                                                       const gchar *text,
                                                       gboolean use_markup,
                                                       gboolean use_underline);
const gchar*        gwy_adjust_bar_get_label          (const GwyAdjustBar *adjbar)   G_GNUC_PURE;
void                gwy_adjust_bar_set_mapping        (GwyAdjustBar *adjbar,
                                                       GwyScaleMappingType mapping);
GwyScaleMappingType gwy_adjust_bar_get_mapping        (const GwyAdjustBar *adjbar)   G_GNUC_PURE;
void                gwy_adjust_bar_set_mnemonic_widget(GwyAdjustBar *adjbar,
                                                       GtkWidget *widget);
GtkWidget*          gwy_adjust_bar_get_mnemonic_widget(const GwyAdjustBar *adjbar)   G_GNUC_PURE;

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
