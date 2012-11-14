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

typedef enum {
    CHOICE_PROXY_TOGGLE,
    CHOICE_PROXY_LIST,
} ChoiceProxyStyle;

typedef struct _GwyChoicePrivate Choice;

typedef struct {
    GObject *object;
    gint value;
    // use_boolean is a radio-boolean, i.e. we only set things "on" and expect
    // the other things to set "off" themselves in reaction.
    ChoiceProxyStyle style;
} ChoiceProxy;

struct _GwyChoicePrivate {
    GArray *entries;
    GSList *proxies;
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
static gint     find_list_index        (const GwyChoice *choice,
                                        gint value);
static void     add_entry_if_unique    (GwyChoice *choice,
                                        const GtkRadioActionEntry *entry);
static void     register_toggle_proxy  (GwyChoice *choice,
                                        GObject *object,
                                        gint value);
static void     proxy_toggled          (GwyChoice *choice,
                                        GObject *toggle);
static void     proxy_gone             (gpointer user_data,
                                        GObject *where_the_object_was);
static GSList*  find_proxy             (const GwyChoice *choice,
                                        const GObject *object);

static GParamSpec *properties[N_PROPS];

G_DEFINE_TYPE(GwyChoice, gwy_choice, G_TYPE_INITIALLY_UNOWNED);

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

// FIXME: Is it reasonable to take action entries if we completely ignore
// @name?  (At present, more is ignored, but that may be implemented.)   Also,
// do we *ever* want to use accelerators paths with this kind of choices?
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
 * gwy_choice_append_to_menu_shell:
 * @choice: A choice.
 * @shell: A menu shell.
 *
 * Creates a group of radio menu items representing a choice and appends them
 * to a menu shell.
 *
 * Returns: The number of items appended.
 **/
guint
gwy_choice_append_to_menu_shell(GwyChoice *choice,
                                GtkMenuShell *shell)
{
    g_return_val_if_fail(GWY_IS_CHOICE(choice), 0);
    g_return_val_if_fail(GTK_IS_MENU_SHELL(shell), 0);
    GtkMenuShell *menushell = GTK_MENU_SHELL(shell);
    Choice *priv = choice->priv;
    GtkRadioActionEntry *entries = (GtkRadioActionEntry*)priv->entries->data;
    guint n = priv->entries->len;
    GSList *group = NULL;

    for (guint i = 0; i < n; i++) {
        GtkRadioActionEntry *entry = entries + i;
        GtkStockItem stock_item;
        gwy_clear1(stock_item);
        if (entry->stock_id)
            gtk_stock_lookup(entry->stock_id, &stock_item);

        const gchar *label = entry->label;
        if (entry->label && *entry->label)
            label = gwy_choice_translate_string(choice, entry->label);
        else if (stock_item.label && *stock_item.label)
            label = dgettext_swapped(stock_item.label,
                                     stock_item.translation_domain);

        GtkWidget *widget;
        if (label)
           widget = gtk_radio_menu_item_new_with_mnemonic(group, label);
        else
           widget = gtk_radio_menu_item_new(group);


        register_toggle_proxy(choice, G_OBJECT(widget), entry->value);

        gtk_menu_shell_append(menushell, widget);
        group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(widget));
    }

    return n;
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
    return string;
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

    for (GSList *l = priv->proxies; l; l = g_slist_next(l)) {
        ChoiceProxy *proxy = (ChoiceProxy*)l->data;
        g_object_set(proxy->object, "sensitive", FALSE, NULL);
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

    priv->active = active;
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
    for (GSList *l = priv->proxies; l; l = g_slist_next(l)) {
        ChoiceProxy *proxy = (ChoiceProxy*)l->data;
        GObject *object = proxy->object;

        if (proxy->style == CHOICE_PROXY_LIST) {
            // XXX: Someday, with properties in Gtk+ implemented with
            // GProperty, we may be able to just set the new value.
            gint current;
            g_object_get(object, "active", &current, NULL);
            if (current != list_index)
                g_object_set(object, "active", list_index, NULL);
        }
        else if (proxy->style == CHOICE_PROXY_TOGGLE) {
            if (proxy->value == active) {
                gboolean state;
                g_object_get(object, "active", &state, NULL);
                if (!state)
                    g_object_set(object, "active", TRUE, NULL);
            }
        }
        else {
            g_warning("Unhandled object of type %s.",
                      G_OBJECT_TYPE_NAME(object));
        }
    }
    priv->in_update = FALSE;
}

static gint
find_list_index(const GwyChoice *choice,
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

static void
register_toggle_proxy(GwyChoice *choice,
                      GObject *object,
                      gint value)
{
    Choice *priv = choice->priv;
    ChoiceProxy *proxy = g_slice_new(ChoiceProxy);
    proxy->value = value;
    proxy->object = object;
    proxy->style = CHOICE_PROXY_TOGGLE;

    if (!priv->sensitive)
        g_object_set(object, "sensitive", FALSE, NULL);
    if (value == priv->active)
        g_object_set(object, "active", TRUE, NULL);

    priv->proxies = g_slist_prepend(priv->proxies, proxy);
    // Simulate @object taking a reference to @choice.
    g_object_ref_sink(choice);
    g_signal_connect_swapped(object, "toggled",
                             G_CALLBACK(proxy_toggled), choice);
    g_object_weak_ref(object, proxy_gone, choice);
}

static void
proxy_toggled(GwyChoice *choice,
              GObject *toggle)
{
    Choice *priv = choice->priv;
    if (priv->in_update)
        return;

    gboolean state;
    g_object_get(toggle, "active", &state, NULL);
    if (!state)
        return;

    GSList *l = find_proxy(choice, toggle);
    g_return_if_fail(l);
    ChoiceProxy *proxy = (ChoiceProxy*)l->data;
    gwy_choice_set_active(choice, proxy->value);
}

static void
proxy_gone(gpointer user_data,
           GObject *where_the_object_was)
{
    // When the last proxy object goes poof we destroy ourselves.
    GwyChoice *choice = (GwyChoice*)user_data;
    GSList *l = find_proxy(choice, where_the_object_was);
    g_assert(l);
    choice->priv->proxies = g_slist_delete_link(choice->priv->proxies, l);
    g_object_unref(choice);
}

static GSList*
find_proxy(const GwyChoice *choice,
           const GObject *object)
{
    const Choice *priv = choice->priv;
    GSList *l = priv->proxies;
    while (l && ((ChoiceProxy*)l->data)->object != object)
        l = g_slist_next(l);
    return l;
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
 * #GwyChoice differs from the Gtk+ action objects.  Namely, for almost all
 * practical purposes, a #GwyChoice is born with a floating reference and any
 * widgets it creates take references to the choice object (possibly sinking it
 * first).  Once the last widget is destroyed the choice object is finalised
 * too.  If you want to reuse the choice object you need to take a reference
 * yourself.
 **/

/**
 * GwyChoice:
 *
 * Object representing an abstraction of widgets representing a choice.
 *
 * The #GwyChoice struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GwyChoiceClass:
 *
 * Class of abstractions of widgets representing a choice.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
