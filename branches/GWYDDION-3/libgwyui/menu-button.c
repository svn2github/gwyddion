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

#include "libgwy/macros.h"
#include "libgwy/object-utils.h"
#include "libgwyui/menu-button.h"

enum {
    PROP_0,
    PROP_MENU,
    N_PROPS,
};

struct _GwyMenuButtonPrivate {
    GtkMenu *menu;
    gulong menu_deactivated_id;
    gboolean menu_active;
};

typedef struct _GwyMenuButtonPrivate MenuButton;

static void     gwy_menu_button_dispose     (GObject *object);
static void     gwy_menu_button_finalize    (GObject *object);
static void     gwy_menu_button_set_property(GObject *object,
                                             guint prop_id,
                                             const GValue *value,
                                             GParamSpec *pspec);
static void     gwy_menu_button_get_property(GObject *object,
                                             guint prop_id,
                                             GValue *value,
                                             GParamSpec *pspec);
static gboolean gwy_menu_button_button_press(GtkWidget *widget,
                                             GdkEventButton *event);
static gboolean set_menu                    (GwyMenuButton *menubutton,
                                             GtkMenu *menu);
static void     menu_deactivated            (GwyMenuButton *menubutton,
                                             GtkWidget *menu);

static GParamSpec *properties[N_PROPS];

G_DEFINE_TYPE(GwyMenuButton, gwy_menu_button, GTK_TYPE_BUTTON);

static void
gwy_menu_button_class_init(GwyMenuButtonClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    g_type_class_add_private(klass, sizeof(MenuButton));

    gobject_class->dispose = gwy_menu_button_dispose;
    gobject_class->finalize = gwy_menu_button_finalize;
    gobject_class->get_property = gwy_menu_button_get_property;
    gobject_class->set_property = gwy_menu_button_set_property;

    widget_class->button_press_event = gwy_menu_button_button_press;

    properties[PROP_MENU]
        = g_param_spec_object("menu",
                              "Menu",
                              "The menu attached to this menu button.",
                              GTK_TYPE_MENU,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    for (guint i = 1; i < N_PROPS; i++)
        g_object_class_install_property(gobject_class, i, properties[i]);
}

static void
gwy_menu_button_init(GwyMenuButton *menubutton)
{
    menubutton->priv = G_TYPE_INSTANCE_GET_PRIVATE(menubutton,
                                                   GWY_TYPE_MENU_BUTTON,
                                                   MenuButton);
}

static void
gwy_menu_button_finalize(GObject *object)
{
    G_OBJECT_CLASS(gwy_menu_button_parent_class)->finalize(object);
}

static void
gwy_menu_button_dispose(GObject *object)
{
    GwyMenuButton *menubutton = GWY_MENU_BUTTON(object);
    set_menu(menubutton, NULL);
    G_OBJECT_CLASS(gwy_menu_button_parent_class)->dispose(object);
}

static void
gwy_menu_button_set_property(GObject *object,
                            guint prop_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
    GwyMenuButton *menubutton = GWY_MENU_BUTTON(object);

    switch (prop_id) {
        case PROP_MENU:
        set_menu(menubutton, g_value_get_object(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_menu_button_get_property(GObject *object,
                            guint prop_id,
                            GValue *value,
                            GParamSpec *pspec)
{
    MenuButton *priv = GWY_MENU_BUTTON(object)->priv;

    switch (prop_id) {
        case PROP_MENU:
        g_value_set_object(value, priv->menu);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static gboolean
gwy_menu_button_button_press(GtkWidget *widget,
                             GdkEventButton *event)
{
    if (event->button != 1)
        return FALSE;

    g_printerr("Pop!\n");
    return TRUE;
}

/**
 * gwy_menu_button_new:
 *
 * Creates a new menu button.
 *
 * Returns: A new menu button.
 **/
GtkWidget*
gwy_menu_button_new(void)
{
    return g_object_newv(GWY_TYPE_MENU_BUTTON, 0, NULL);
}

/**
 * gwy_menu_button_set_menu:
 * @menubutton: A menu button.
 * @menu: (allow-none):
 *        Menu widget to show.
 *
 * Sets the menu that a menu button visualises.
 **/
void
gwy_menu_button_set_menu(GwyMenuButton *menubutton,
                         GtkMenu *menu)
{
    g_return_if_fail(GWY_IS_MENU_BUTTON(menubutton));
    g_return_if_fail(GTK_IS_MENU(menu));
    if (!set_menu(menubutton, menu))
        return;

    g_object_notify_by_pspec(G_OBJECT(menubutton), properties[PROP_MENU]);
}

/**
 * gwy_menu_button_get_menu:
 * @menubutton: A menu button.
 *
 * Obtains the menu that a menu button shows.
 *
 * Returns: (allow-none) (transfer none):
 *          The menu used by @menubutton.  If no menu was set this function
 *          returns %NULL.
 **/
GtkMenu*
gwy_menu_button_get_menu(const GwyMenuButton *menubutton)
{
    g_return_val_if_fail(GWY_IS_MENU_BUTTON(menubutton), NULL);
    return menubutton->priv->menu;
}

static gboolean
set_menu(GwyMenuButton *menubutton,
         GtkMenu *menu)
{
    MenuButton *priv = menubutton->priv;
    if (priv->menu_active) {
        gtk_menu_shell_deactivate(GTK_MENU_SHELL(priv->menu));
        g_assert(!priv->menu_active);
    }
    if (!gwy_set_member_object(menubutton, menu, GTK_TYPE_MENU,
                               &priv->menu,
                               "deactivat", &menu_deactivated,
                               &priv->menu_deactivated_id,
                               G_CONNECT_SWAPPED,
                               NULL))
        return FALSE;

    // FIXME: Pop up the new menu if a menu was active before?
    return TRUE;
}

static void
menu_deactivated(GwyMenuButton *menubutton,
                 GtkWidget *menu)
{
    gtk_widget_hide(menu);
    gtk_menu_detach(GTK_MENU(menu));
    menubutton->priv->menu_active = FALSE;
}

/**
 * SECTION: menu-button
 * @title: GwyMenuButton
 * @short_description: Button that pops up a menu on clicking
 **/

/**
 * GwyMenuButton:
 *
 * Button widget that pops up a menu on clicking.
 *
 * The #GwyMenuButton struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GwyMenuButtonClass:
 *
 * Class of menu buttons.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
