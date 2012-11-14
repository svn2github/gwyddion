/*
 *  $Id: choice.c 14176 2012-11-05 20:34:18Z yeti-dn $
 *  Copyright (C) 2011-2012 David Nečas (Yeti).
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

#include <glib/gi18n-lib.h>
#include "libgwy/macros.h"
#include "libgwy/rgba.h"
#include "libgwy/object-utils.h"
#include "libgwyui/cairo-utils.h"
#include "libgwyui/choice.h"

enum {
    PROP_0,
    PROP_SENSITIVE,
    PROP_ACTIVE,
    N_PROPS
};

enum {
    SGNL_CHANGED,
    N_SIGNALS
};

typedef struct _GwyChoicePrivate Choice;

typedef struct {
    GtkWidget *widget;
    gulong handler_id;
    gint value;
    // use_boolean is a radio-boolean, i.e. we only set things "on" and expect
    // the other things to set "off" themselves in reaction.
    gboolean use_boolean : 1;
    gboolean use_list_index : 1;
} ChoiceProxy;

struct _GwyChoicePrivate {
    GArray *entries;
    GList *proxies;
    gint active;
    gint list_index;

    gboolean sensitive;
    gboolean in_update;

    GtkAccelGroup *accel_group;

    GtkTranslateFunc translate_func;
    gpointer translate_data;
    GDestroyNotify translate_notify;
};

static void     gwy_choice_finalize    (GObject *object);
static void     gwy_choice_set_property(GObject *object,
                                        guint prop_id,
                                        const GValue *value,
                                        GParamSpec *pspec);
static void     gwy_choice_get_property(GObject *object,
                                        guint prop_id,
                                        GValue *value,
                                        GParamSpec *pspec);
static gchar*   dgettext_swapped       (const gchar *msgid,
                                        const gchar *domainname);
static gboolean set_sensitive          (GwyChoice *choice,
                                        gboolean sensitive);
static gboolean set_active             (GwyChoice *choice,
                                        gint active);
static void     update_proxies         (GwyChoice *choice);
static gint     find_list_index        (GwyChoice *choice,
                                        gint value);
static void     add_entry_if_unique    (GwyChoice *choice,
                                        const GtkRadioActionEntry *entry);

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];

G_DEFINE_ABSTRACT_TYPE(GwyChoice, gwy_choice, G_TYPE_INITIALLY_UNOWNED);

static void
gwy_choice_class_init(GwyChoiceClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(Choice));

    gobject_class->finalize = gwy_choice_finalize;
    gobject_class->get_property = gwy_choice_get_property;
    gobject_class->set_property = gwy_choice_set_property;

    properties[PROP_ACTIVE]
        = g_param_spec_int("active",
                           "Active",
                           "Integer value of the currently active choice.",
                           G_MININT, G_MAXINT, 0,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_SENSITIVE]
        = g_param_spec_boolean("sensitive",
                               "Sensitive",
                               "Whether the widgets respond to user actions.",
                               TRUE,
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    for (guint i = 1; i < N_PROPS; i++)
        g_object_class_install_property(gobject_class, i, properties[i]);

    /**
     * GwyChoice::changed:
     * @gwychoice: The #GwyChoice which received the signal.
     *
     * The ::changed signal is emitted when the active item is changed, both
     * by the user and programatically.
     **/
    signals[SGNL_CHANGED]
        = g_signal_new_class_handler("changed",
                                     G_OBJECT_CLASS_TYPE(klass),
                                     G_SIGNAL_RUN_FIRST,
                                     NULL, NULL, NULL,
                                     g_cclosure_marshal_VOID__VOID,
                                     G_TYPE_NONE, 0);
}

static void
gwy_choice_init(GwyChoice *choice)
{
    choice->priv = G_TYPE_INSTANCE_GET_PRIVATE(choice, GWY_TYPE_CHOICE, Choice);
    Choice *priv = choice->priv;
    priv->entries = g_array_new(FALSE, FALSE, sizeof(GtkRadioActionEntry));
    priv->list_index = -1;
    priv->sensitive = TRUE;
}

static void
gwy_choice_finalize(GObject *object)
{
    GwyChoice *choice = GWY_CHOICE(object);
    Choice *priv = choice->priv;
    g_assert(!priv->proxies);
    gwy_set_user_func(NULL, NULL, NULL,
                      &priv->translate_func, &priv->translate_data,
                      &priv->translate_notify);
    g_array_free(priv->entries, TRUE);
    GWY_OBJECT_UNREF(priv->accel_group);
    G_OBJECT_CLASS(gwy_choice_parent_class)->finalize(object);
}

static void
gwy_choice_set_property(GObject *object,
                        guint prop_id,
                        const GValue *value,
                        GParamSpec *pspec)
{
    GwyChoice *choice = GWY_CHOICE(object);

    switch (prop_id) {
        case PROP_SENSITIVE:
        set_sensitive(choice, g_value_get_boolean(value));
        break;

        case PROP_ACTIVE:
        set_active(choice, g_value_get_int(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_choice_get_property(GObject *object,
                        guint prop_id,
                        GValue *value,
                        GParamSpec *pspec)
{
    GwyChoice *choice = GWY_CHOICE(object);
    Choice *priv = choice->priv;

    switch (prop_id) {
        case PROP_SENSITIVE:
        g_value_set_boolean(value, priv->sensitive);
        break;

        case PROP_ACTIVE:
        g_value_set_int(value, priv->active);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

/**
 * gwy_choice_new:
 *
 * Creates a new choice abstraction.
 *
 * Returns: A newly created choice abstraction.
 **/
GwyChoice*
gwy_choice_new(void)
{
    return g_object_newv(GWY_TYPE_CHOICE, 0, NULL);
}

/**
 * gwy_choice_set_active:
 * @choice: A choice.
 * @active: Value of the item that should be selected.
 *
 * Sets the current selected item of a choice.
 *
 * An item should be explicitly selected during the setup.  Although it is not
 * a strict requirement, construction of widgets enforces exactly one item
 * being selected so something will be selected anyway, normally the first
 * item.
 **/
void
gwy_choice_set_active(GwyChoice *choice,
                      int active)
{
    g_return_if_fail(GWY_IS_CHOICE(choice));
    if (!set_active(choice, active))
        return;

    g_object_notify_by_pspec(G_OBJECT(choice), properties[PROP_ACTIVE]);
}

/**
 * gwy_choice_get_active:
 * @choice: A choice.
 *
 * Gets the currently selected item of a choice.
 *
 * Returns: The numeric value of the currently selected item.
 **/
gint
gwy_choice_get_active(const GwyChoice *choice)
{
    g_return_val_if_fail(GWY_IS_CHOICE(choice), 0);
    return choice->priv->active;
}

/**
 * gwy_choice_set_sensitive:
 * @choice: A choice.
 * @sensitive: %TRUE to make @choice sensitive, %FALSE to make it insensitive.
 *
 * Sets the sensitivity of a choice.
 **/
void
gwy_choice_set_sensitive(GwyChoice *choice,
                         gboolean sensitive)
{
    g_return_if_fail(GWY_IS_CHOICE(choice));
    if (!set_sensitive(choice, sensitive))
        return;

    g_object_notify_by_pspec(G_OBJECT(choice), properties[PROP_SENSITIVE]);
}

/**
 * gwy_choice_get_sensitive:
 * @choice: A choice.
 *
 * Gets whether a choice is sensitive.
 *
 * This means that the choice as a whole is sensitive.
 *
 * Returns: %TRUE if @choice is sensitive, %FALSE if it is insensitive.
 **/
gboolean
gwy_choice_get_sensitive(const GwyChoice *choice)
{
    g_return_val_if_fail(GWY_IS_CHOICE(choice), FALSE);
    return choice->priv->sensitive;
}

/**
 * gwy_choice_add_actions:
 * @choice: A choice.
 * @entries: (array length=n):
 *           Array of entries describing the items to add.  The array contents
 *           is shallowly copied, i.e. ownership is not taken, however, the
 *           strings within are assumed to be static.
 * @n: Number of items in @entries.
 *
 * Adds items specified using #GtkRadioActionEntry structs to a choice.
 *
 * This method can be called several times during construction to build the
 * options piecemeal.  Adding choices once widgets have been created does
 * <emphasis>not</emphasis> affect these widgets and, consequently, can lead
 * to all sorts of trouble.
 **/
void
gwy_choice_add_actions(GwyChoice *choice,
                       const GtkRadioActionEntry *entries,
                       guint n)
{
    g_return_if_fail(GWY_IS_CHOICE(choice));
    g_return_if_fail(!n || entries);
    for (guint i = 0; i < n; i++)
        add_entry_if_unique(choice, entries + i);
}

/**
 * gwy_choice_size:
 * @choice: A choice.
 *
 * Obtains the number of items in a choice.
 *
 * Returns: The number of items in @choice.  For multi-widget representations
 *          of choices this is typically the number of widget that would be
 *          created.
 **/
guint
gwy_choice_size(const GwyChoice *choice)
{
    g_return_val_if_fail(GWY_IS_CHOICE(choice), 0);
    return choice->priv->entries->len;
}

/**
 * gwy_choice_set_translate_func:
 * @choice: A choice.
 * @func: Label and tooltip translation function.
 * @data: Data to pass to @func and @notify.
 * @notify: Function to be called when @choice is destroyed or when the
 *          translation function is changed.
 *
 * Sets the function to be used for translating labels and tootips of
 * entries added by gwy_choice_add_actions().
 *
 * If you use gettext() it is sufficient to set the translation domain
 * with gwy_choice_set_translation_domain().  Setting the translation function
 * does <emphasis>not</emphasis> change the labels and tooltips of already
 * constructed widgets.  Therefore, it is usually meaningful only during
 * initial #GwyChoice setup.
 **/
void
gwy_choice_set_translate_func(GwyChoice *choice,
                              GtkTranslateFunc func,
                              gpointer data,
                              GDestroyNotify notify)
{
    g_return_if_fail (GWY_IS_CHOICE(choice));
    Choice *priv = choice->priv;
    gwy_set_user_func(func, data, notify,
                      &priv->translate_func, &priv->translate_data,
                      &priv->translate_notify);
}

/**
 * gwy_choice_set_translation_domain:
 * @choice: A choice.
 * @domain: (allow-none):
 *          The translation domain to use for g_dgettext() calls, or %NULL to
 *          use the domain set with textdomain().
 *
 * Sets the translation domain for translation and sets up label and tooltip
 * translations to be made with g_dgettext().
 *
 * See gwy_choice_set_translate_func() if you do not use gettext() for
 * localization.
 **/
void
gwy_choice_set_translation_domain(GwyChoice *choice,
                                  const gchar *domain)
{
    gwy_choice_set_translate_func(choice,
                                  (GtkTranslateFunc)dgettext_swapped,
                                  g_strdup(domain), g_free);
}

/**
 * gwy_choice_translate_string:
 * @choice: A choice.
 * @string: String to translate.
 *
 * Translates a string using the function set with
 * gwy_choice_set_translate_func().
 *
 * This is mainly intended for language bindings.
 *
 * Returns: The translation of @string.
 **/
const gchar*
gwy_choice_translate_string(const GwyChoice *choice,
                            const gchar *string)
{
    g_return_val_if_fail(GWY_IS_CHOICE(choice), string);
    Choice *priv = choice->priv;
    if (string && priv->translate_func)
        return priv->translate_func(string, priv->translate_data);
    return NULL;
}

// The same as Gtk+ uses.
static gchar*
dgettext_swapped(const gchar *msgid,
                 const gchar *domainname)
{
    /* Pass through g_dgettext if and only if msgid is nonempty. */
    if (msgid && *msgid)
        return (gchar*)g_dgettext(domainname, msgid);
    else
        return (gchar*)msgid;
}

static gboolean
set_sensitive(GwyChoice *choice,
              gboolean sensitive)
{
    Choice *priv = choice->priv;
    if (sensitive == priv->sensitive)
        return FALSE;

    for (GList *l = priv->proxies; l; l = g_list_next(l)) {
        ChoiceProxy *proxy = (ChoiceProxy*)l->data;
        gtk_widget_set_sensitive(proxy->widget, sensitive);
    }
    return TRUE;
}

static gboolean
set_active(GwyChoice *choice,
           gint active)
{
    Choice *priv = choice->priv;
    if (active == priv->active)
        return FALSE;

    priv->list_index = find_list_index(choice, active);
    if (priv->proxies)
        update_proxies(choice);

    return TRUE;
}

static void
update_proxies(GwyChoice *choice)
{
    Choice *priv = choice->priv;
    g_return_if_fail(!priv->in_update);
    gint active = priv->active, list_index = priv->list_index;
    // FIXME: Can we do anything better?
    if (list_index < 0)
        return;

    priv->in_update = TRUE;
    for (GList *l = priv->proxies; l; l = g_list_next(l)) {
        ChoiceProxy *proxy = (ChoiceProxy*)l->data;
        GtkWidget *widget = proxy->widget;

        if (proxy->use_list_index) {
            // XXX: Someday, with properties in Gtk+ implemented with
            // GProperty, we may be able to just set the new value.
            gint current;
            g_object_get(widget, "active", &current, NULL);
            if (current != list_index)
                g_object_set(widget, "active", list_index, NULL);
        }
        else if (proxy->use_boolean) {
            if (proxy->value == active) {
                gboolean state;
                g_object_get(widget, "active", &state, NULL);
                if (!state)
                    g_object_set(widget, "active", TRUE, NULL);
            }
        }
        else {
            g_warning("Unhandled widget of type %s.",
                      G_OBJECT_TYPE_NAME(widget));
        }
    }
    priv->in_update = FALSE;
}

static gint
find_list_index(GwyChoice *choice,
                gint value)
{
    Choice *priv = choice->priv;
    GArray *array = priv->entries;
    for (guint i = 0; i < array->len; i++) {
        if (g_array_index(array, GtkRadioActionEntry, i).value == value)
            return i;
    }
    return -1;
}

static void
add_entry_if_unique(GwyChoice *choice,
                    const GtkRadioActionEntry *entry)
{
    Choice *priv = choice->priv;
    GtkRadioActionEntry *entries = (GtkRadioActionEntry*)priv->entries->data;
    guint n = priv->entries->len;
    for (guint i = 0; i < n; i++) {
        if (entry->value == entries[i].value) {
            g_warning("Non-unique choice value %d, ignoring it.", entry->value);
            return;
        }
    }
    g_array_append_vals(priv->entries, entry, 1);
}

/**
 * SECTION: choice
 * @title: GwyChoice
 * @short_description: Abstraction for widgets representing a choice.
 *
 * #GwyChoice is an abstraction of widgets or groups of widgets representing
 * a choice of one option from a set.  Each item is uniquely identified by an
 * integer value.  The current selection is maintained by #GwyChoice but it can
 * be represented in the user interface using a variety of specific widgets
 * (proxies).
 *
 * #GwyChoice shares some features with groups of #GtkToggleActions, however,
 * it is aimed at uses cases where the choices originate from data or code and
 * once enumerated they do not change.  Thus there is no merging and no XML
 * parsing involved.  Furthermore, #GwyChoice can also abstract combo boxes and
 * similar widgets that do not map 1:1 to #GtkAction<!-- -->s.
 *
 * Note, however, the ownership semantics of pretty much everything in
 * #GwyChoice differs from the Gtk+ action objects.
 **/

/**
 * GwyChoice:
 *
 * Object representing an abstraction of widgets representing a choice.
 *
 * The #GwyChoice struct contains some public fields that can be directly
 * accessed for reading which is useful namely for subclassing.  To set them,
 * you must always use the methods such as gwy_choice_set_bounding_box().
 **/

/**
 * GwyChoiceClass:
 *
 * Class of abstractions of widgets representing a choice.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
