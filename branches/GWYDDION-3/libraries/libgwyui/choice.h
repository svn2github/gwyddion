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

#ifndef __LIBGWYUI_CHOICE_H__
#define __LIBGWYUI_CHOICE_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct {
    const gchar *stock_id;
    const gchar *label;
    const gchar *tooltip;
    gint value;
} GwyChoiceOption;

#define GWY_TYPE_CHOICE \
    (gwy_choice_get_type())
#define GWY_CHOICE(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_CHOICE, GwyChoice))
#define GWY_CHOICE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_CHOICE, GwyChoiceClass))
#define GWY_IS_CHOICE(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_CHOICE))
#define GWY_IS_CHOICE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_CHOICE))
#define GWY_CHOICE_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_CHOICE, GwyChoiceClass))

typedef struct _GwyChoice      GwyChoice;
typedef struct _GwyChoiceClass GwyChoiceClass;

struct _GwyChoice {
    GInitiallyUnowned unowned;
    struct _GwyChoicePrivate *priv;
};

struct _GwyChoiceClass {
    /*<private>*/
    GInitiallyUnownedClass unowned_class;
};

GType        gwy_choice_get_type              (void)                             G_GNUC_CONST;
GwyChoice*   gwy_choice_new                   (void)                             G_GNUC_MALLOC;
GwyChoice*   gwy_choice_new_with_options      (const GwyChoiceOption *options,
                                               guint n)                          G_GNUC_MALLOC;
void         gwy_choice_set_active            (GwyChoice *choice,
                                               int active);
gint         gwy_choice_get_active            (const GwyChoice *choice)          G_GNUC_PURE;
void         gwy_choice_set_sensitive         (GwyChoice *choice,
                                               gboolean sensitive);
gboolean     gwy_choice_get_sensitive         (const GwyChoice *choice)          G_GNUC_PURE;
void         gwy_choice_set_visible           (GwyChoice *choice,
                                               gboolean visible);
gboolean     gwy_choice_get_visible           (const GwyChoice *choice)          G_GNUC_PURE;
void         gwy_choice_add_options           (GwyChoice *choice,
                                               const GwyChoiceOption *options,
                                               guint n);
guint        gwy_choice_size                  (const GwyChoice *choice)          G_GNUC_PURE;
GObject*     gwy_choice_find_widget           (const GwyChoice *choice,
                                               gint value)                       G_GNUC_PURE;
GtkWidget**  gwy_choice_create_menu_items     (GwyChoice *choice)                G_GNUC_MALLOC;
guint        gwy_choice_append_to_menu_shell  (GwyChoice *choice,
                                               GtkMenuShell *shell);
GtkWidget**  gwy_choice_create_buttons        (GwyChoice *choice,
                                               GtkIconSize icon_size,
                                               const gchar *first_property_name,
                                               ...)                              G_GNUC_MALLOC;
guint        gwy_choice_attach_to_grid        (GwyChoice *choice,
                                               GtkGrid *grid,
                                               gint left,
                                               gint top,
                                               guint width);
GtkWidget*   gwy_choice_create_combo          (GwyChoice *choice)                G_GNUC_MALLOC;
void         gwy_choice_set_translate_func    (GwyChoice *choice,
                                               GtkTranslateFunc func,
                                               gpointer data,
                                               GDestroyNotify notify);
void         gwy_choice_set_translation_domain(GwyChoice *choice,
                                               const gchar *domain);
const gchar* gwy_choice_translate_string      (const GwyChoice *choice,
                                               const gchar *string)              G_GNUC_PURE;

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
