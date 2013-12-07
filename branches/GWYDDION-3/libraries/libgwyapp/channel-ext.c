/*
 *  $Id$
 *  Copyright (C) 2013 David Nečas (Yeti).
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
#include "libgwyapp/channel-ext.h"

struct _GwyChannelExtPrivate {
    gchar dummy;
};

typedef struct _GwyChannelExtPrivate ChannelExt;

G_DEFINE_TYPE(GwyChannelExt, gwy_channel_ext, G_TYPE_OBJECT);

static void
gwy_channel_ext_class_init(GwyChannelExtClass *klass)
{
    g_type_class_add_private(klass, sizeof(ChannelExt));
}

static void
gwy_channel_ext_init(GwyChannelExt *extfunc)
{
    extfunc->priv = G_TYPE_INSTANCE_GET_PRIVATE(extfunc,
                                                GWY_TYPE_CHANNEL_EXT,
                                                ChannelExt);
    // Our superclass load properties from settings.
    // The superclass should also have function to load a named set of
    // properties (preset).
    // The properties in settings would be called ‘last used’.
}

/*
 * FIXME: Must distinguish run modes: interactive; with last used params; maybe
 * non-interactive with explicit params (but that can be simulated by setting
 * the properties before running the function).
 *
 * Generally, when an instance is created we want to
 * (a) Set properties (parametres) from some database, setting in 2.x, maybe
 *     some per-function history of settings (presets) here.  Simple modules do
 *     not need such database – there is no advantage in loading of a preset
 *     when the only option is include/exclude/ignore mask.  But it should be
 *     easy for modules with complex settings to offer presets; they should
 *     just say they want them supported.
 * (b) Set properties (parametres) explicitly.  This can happen after (a) so
 *     we can always set up parameters from settings upon construction.
 */
/**
 * gwy_channel_ext_run:
 * @extfunc: A channel extension module function.
 *
 * Interactively runs a channel module function.
 **/
void
gwy_channel_ext_run(GwyChannelExt *extfunc)
{
    g_return_if_fail(GWY_IS_CHANNEL_EXT(extfunc));
    GwyChannelExtClass *klass = GWY_CHANNEL_EXT_GET_CLASS(extfunc);
    g_return_if_fail(klass->run);
    // TODO: Load properties from settings here?  This may only be useful if 
    // there are more instances of the ext function but we may want just do it
    // anyway for consistency.
    klass->run(extfunc);
    // TODO: Save properties to settings.
}

/************************** Documentation ****************************/

/**
 * SECTION: channel-ext
 * @title: GwyChannelExt
 * @short_description: Channel extension module function
 *
 * #GwyChannelExt represent an extension module function operating on channels.
 **/

/**
 * GwyChannelExt:
 *
 * Object representing a channel extension module function.
 *
 * The #GwyChannelExt struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GwyChannelExtClass:
 * @run: Interactively run the function.  If the function does not have any
 *       GUI, interactively means immediately.
 * @get_path: Get menu path for the function.
 * @get_icon: Get icon for the function.
 *
 * Class of channel extension module functions.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
