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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

/*< private_header >*/

#ifndef __GWY_AXIS_DIALOG_H__
#define __GWY_AXIS_DIALOG_H__

#include <gtk/gtkdialog.h>
#include <libgwydgets/gwyaxis.h>

G_BEGIN_DECLS

#define GWY_TYPE_AXIS_DIALOG            (_gwy_axis_dialog_get_type())
#define GWY_AXIS_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_AXIS_DIALOG, GwyAxisDialog))
#define GWY_AXIS_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_AXIS_DIALOG, GwyAxisDialogClass))
#define GWY_IS_AXIS_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_AXIS_DIALOG))
#define GWY_IS_AXIS_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_AXIS_DIALOG))
#define GWY_AXIS_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_AXIS_DIALOG, GwyAxisDialogClass))

typedef struct _GwyAxisDialog      GwyAxisDialog;
typedef struct _GwyAxisDialogClass GwyAxisDialogClass;

struct _GwyAxisDialog {
    GtkDialog dialog;

    GtkWidget *sci_text;
    GtkWidget *is_auto;
    GtkObject *major_length;
    GtkObject *major_thickness;
    GtkObject *major_division;
    GtkWidget *major_division_spin;
    GtkObject *minor_length;
    GtkObject *minor_thickness;
    GtkObject *minor_division;
    GtkWidget *minor_division_spin;
    GtkObject *line_thickness;

    GwyAxis *axis;
};

struct _GwyAxisDialogClass {
    GtkDialogClass parent_class;
};

G_GNUC_INTERNAL
GType       _gwy_axis_dialog_get_type    (void) G_GNUC_CONST;

G_GNUC_INTERNAL
GtkWidget*  _gwy_axis_dialog_new         (GwyAxis *axis);

G_GNUC_INTERNAL
GtkWidget*  _gwy_axis_dialog_get_sci_text(GtkWidget* dialog);

G_END_DECLS

#endif /* __GWY_AXIS_DIALOG_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
