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

#ifndef __LIBGWYAPP_CHANNEL_EXT_H__
#define __LIBGWYAPP_CHANNEL_EXT_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define GWY_TYPE_CHANNEL_EXT \
    (gwy_channel_ext_get_type())
#define GWY_CHANNEL_EXT(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_CHANNEL_EXT, GwyChannelExt))
#define GWY_CHANNEL_EXT_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_CHANNEL_EXT, GwyChannelExtClass))
#define GWY_IS_CHANNEL_EXT(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_CHANNEL_EXT))
#define GWY_IS_CHANNEL_EXT_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_CHANNEL_EXT))
#define GWY_CHANNEL_EXT_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_CHANNEL_EXT, GwyChannelExtClass))

typedef struct _GwyChannelExt      GwyChannelExt;
typedef struct _GwyChannelExtClass GwyChannelExtClass;

struct _GwyChannelExt {
    GObject g_object;
    struct _GwyChannelExtPrivate *priv;
};

struct _GwyChannelExtClass {
    /*<private>*/
    GObjectClass gobject_class;
    /*<public>*/
    void (*run)(GwyChannelExt *extfunc);
    const gchar *path;
    const gchar *icon;
};

GType        gwy_channel_ext_get_type(void)                         G_GNUC_CONST;
void         gwy_channel_ext_run     (GwyChannelExt *extfunc);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
