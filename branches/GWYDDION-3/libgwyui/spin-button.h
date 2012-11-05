/*
 *  $Id$
 *
 *  GTK - The GIMP Toolkit
 *  Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 *  GwySpinButton widget for GTK+
 *  Copyright (C) 1998 Lars Hamann and Stefan Jeske
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the
 *  Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 */

/*
 *  Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 *  file for a list of people on the GTK+ Team.  See the ChangeLog
 *  files for a list of changes.  These files are distributed with
 *  GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

/*  Modified by Yeti 2012.
 *  This file branched off the last good commit of GtkSpinButton before
 *  horizontal madness arrived: 68c74e142709458b95ccc76d8d21c3f038e42ecf */

#ifndef __LIBGWYUI_SPIN_BUTTON_H__
#define __LIBGWYUI_SPIN_BUTTON_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GWY_TYPE_SPIN_BUTTON \
    (gwy_spin_button_get_type())
#define GWY_SPIN_BUTTON(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_SPIN_BUTTON, GwySpinButton))
#define GWY_SPIN_BUTTON_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_SPIN_BUTTON, GwySpinButtonClass))
#define GWY_IS_SPIN_BUTTON(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_SPIN_BUTTON))
#define GWY_IS_SPIN_BUTTON_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_SPIN_BUTTON))
#define GWY_SPIN_BUTTON_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_SPIN_BUTTON, GwySpinButtonClass))

typedef struct _GwySpinButton              GwySpinButton;
typedef struct _GwySpinButtonPrivate       GwySpinButtonPrivate;
typedef struct _GwySpinButtonClass         GwySpinButtonClass;

struct _GwySpinButton {
    GtkEntry entry;
    struct _GwySpinButtonPrivate *priv;
};

struct _GwySpinButtonClass {
    GtkEntryClass parent_class;

    /*<public>*/
    gint (*input)(GwySpinButton *spinbutton,
                  gdouble *new_value);
    gint (*output) (GwySpinButton *spinbutton);
    void (*value_changed)(GwySpinButton *spinbutton);

    /* Action signals for keybindings, do not connect to these */
    void (*change_value)(GwySpinButton *spinbutton,
                         GtkScrollType  scroll);

    void (*wrapped)(GwySpinButton *spinbutton);

    /*<private>*/
    /* Padding for future expansion */
    void (*reserved1) (void);
    void (*reserved2) (void);
    void (*reserved3) (void);
    void (*reserved4) (void);
};


GType                     gwy_spin_button_get_type         (void)                              G_GNUC_CONST;
void                      gwy_spin_button_configure        (GwySpinButton *spinbutton,
                                                            GtkAdjustment *adjustment,
                                                            gdouble climb_rate,
                                                            guint digits);
GtkWidget*                gwy_spin_button_new              (GtkAdjustment *adjustment,
                                                            gdouble climb_rate,
                                                            guint digits);
GtkWidget*                gwy_spin_button_new_with_range   (gdouble min,
                                                            gdouble max,
                                                            gdouble step);
void                      gwy_spin_button_set_adjustment   (GwySpinButton *spinbutton,
                                                            GtkAdjustment *adjustment);
GtkAdjustment*            gwy_spin_button_get_adjustment   (GwySpinButton *spinbutton);
void                      gwy_spin_button_set_digits       (GwySpinButton *spinbutton,
                                                            guint digits);
guint                     gwy_spin_button_get_digits       (GwySpinButton *spinbutton);
void                      gwy_spin_button_set_increments   (GwySpinButton *spinbutton,
                                                            gdouble step,
                                                            gdouble page);
void                      gwy_spin_button_get_increments   (GwySpinButton *spinbutton,
                                                            gdouble *step,
                                                            gdouble *page);
void                      gwy_spin_button_set_range        (GwySpinButton *spinbutton,
                                                            gdouble min,
                                                            gdouble max);
void                      gwy_spin_button_get_range        (GwySpinButton *spinbutton,
                                                            gdouble *min,
                                                            gdouble *max);
gdouble                   gwy_spin_button_get_value        (GwySpinButton *spinbutton);
gint                      gwy_spin_button_get_value_as_int (GwySpinButton *spinbutton);
void                      gwy_spin_button_set_value        (GwySpinButton *spinbutton,
                                                            gdouble value);
void                      gwy_spin_button_set_update_policy(GwySpinButton *spinbutton,
                                                            GtkSpinButtonUpdatePolicy policy);
GtkSpinButtonUpdatePolicy gwy_spin_button_get_update_policy(GwySpinButton *spinbutton);
void                      gwy_spin_button_set_numeric      (GwySpinButton *spinbutton,
                                                            gboolean numeric);
gboolean                  gwy_spin_button_get_numeric      (GwySpinButton *spinbutton);
void                      gwy_spin_button_spin             (GwySpinButton *spinbutton,
                                                            GtkSpinType direction,
                                                            gdouble increment);
void                      gwy_spin_button_set_wrap         (GwySpinButton *spinbutton,
                                                            gboolean wrap);
gboolean                  gwy_spin_button_get_wrap         (GwySpinButton *spinbutton);
void                      gwy_spin_button_set_snap_to_ticks(GwySpinButton *spinbutton,
                                                            gboolean snap_to_ticks);
gboolean                  gwy_spin_button_get_snap_to_ticks(GwySpinButton *spinbutton);
void                      gwy_spin_button_update           (GwySpinButton *spinbutton);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
