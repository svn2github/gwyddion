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

#ifndef __LIBGWYUI_MENU_BUTTON_H__
#define __LIBGWYUI_MENU_BUTTON_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GWY_TYPE_MENU_BUTTON \
    (gwy_menu_button_get_type())
#define GWY_MENU_BUTTON(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_MENU_BUTTON, GwyMenuButton))
#define GWY_MENU_BUTTON_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_MENU_BUTTON, GwyMenuButtonClass))
#define GWY_IS_MENU_BUTTON(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_MENU_BUTTON))
#define GWY_IS_MENU_BUTTON_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_MENU_BUTTON))
#define GWY_MENU_BUTTON_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_MENU_BUTTON, GwyMenuButtonClass))

typedef struct _GwyMenuButton      GwyMenuButton;
typedef struct _GwyMenuButtonClass GwyMenuButtonClass;

struct _GwyMenuButton {
    GtkButton button;
    struct _GwyMenuButtonPrivate *priv;
};

struct _GwyMenuButtonClass {
    /*<private>*/
    GtkButtonClass button_class;
};

GType      gwy_menu_button_get_type(void)                            G_GNUC_CONST;
GtkWidget* gwy_menu_button_new     (void)                            G_GNUC_MALLOC;
void       gwy_menu_button_set_menu(GwyMenuButton *menubutton,
                                    GtkMenu *menu);
GtkMenu*   gwy_menu_button_get_menu(const GwyMenuButton *menubutton) G_GNUC_PURE;

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
