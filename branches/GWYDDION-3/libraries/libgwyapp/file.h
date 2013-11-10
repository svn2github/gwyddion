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

#ifndef __LIBGWYAPP_FILE_H__
#define __LIBGWYAPP_FILE_H__

#include <glib-object.h>

G_BEGIN_DECLS

#ifndef __GI_SCANNER__
typedef union {
    guint64 gid;
    struct {
        guint type : 8;
        guint fileno : 24;
        guint datano : 32;
    };
} GwyDataId;
#else
typedef union {
    guint64 gid;
    struct {
        guint type : 8;
        guint fileno : 24;
        guint datano : 32;
    } split;
} GwyDataId;
#endif

#define GWY_TYPE_FILE \
    (gwy_file_get_type())
#define GWY_FILE(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_FILE, GwyFile))
#define GWY_FILE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_FILE, GwyFileClass))
#define GWY_IS_FILE(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_FILE))
#define GWY_IS_FILE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_FILE))
#define GWY_FILE_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_FILE, GwyFileClass))

typedef struct _GwyFile      GwyFile;
typedef struct _GwyFileClass GwyFileClass;

G_END_DECLS

#include <libgwyapp/data-list.h>

G_BEGIN_DECLS

struct _GwyFile {
    GObject g_object;
    struct _GwyFilePrivate *priv;
};

struct _GwyFileClass {
    /*<private>*/
    GObjectClass g_object_class;
};

GType        gwy_file_get_type     (void)                G_GNUC_CONST;
GwyFile*     gwy_file_new          (void)                G_GNUC_MALLOC;
guint        gwy_file_get_id       (const GwyFile *file) G_GNUC_PURE;
GwyDataList* gwy_file_get_data_list(const GwyFile *file,
                                    GwyDataKind kind)    G_GNUC_PURE;
GwyFile*     gwy_files_find_file   (guint file_id)       G_GNUC_PURE;

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
