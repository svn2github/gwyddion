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

#ifndef __LIBGWYAPP_MODULE_H__
#define __LIBGWYAPP_MODULE_H__

#include <libgwy/error-list.h>
#include <glib-object.h>
#include <gmodule.h>

G_BEGIN_DECLS

#define GWY_MODULE_ABI_VERSION 3

#ifdef  __cplusplus
#define __GWY_MODULE_EXTERN_C extern "C"
#else
#define __GWY_MODULE_EXTERN_C /* */
#endif

#ifndef GWY_MODULE_BUILDING_LIBRARY
#define GWY_DEFINE_MODULE(mod_info,name) \
    __GWY_MODULE_EXTERN_C G_MODULE_EXPORT const GwyModuleInfo* \
    gwy_module_query_##name(void) { return &mod_info; }
#else
#define GWY_DEFINE_MODULE(mod_info,name) \
    __GWY_MODULE_EXTERN_C G_GNUC_INTERNAL const GwyModuleInfo* \
    gwy_module_query_##name(void) { return &mod_info; }
#endif

#define GWY_DEFINE_MODULE_LIBRARY(mod_query_list) \
    __GWY_MODULE_EXTERN_C G_MODULE_EXPORT const GwyModuleQuery** \
    gwy_module_query(void) { return mod_query_list; }

#define GWY_MODULE_ERROR gwy_module_error_quark()

typedef enum {
    GWY_MODULE_ERROR_NAME,
    GWY_MODULE_ERROR_DUPLICATE,
    GWY_MODULE_ERROR_OPEN,
    GWY_MODULE_ERROR_QUERY,
    GWY_MODULE_ERROR_ABI,
    GWY_MODULE_ERROR_INFO,
    GWY_MODULE_ERROR_REGISTER
} GwyModuleError;

typedef struct _GwyModuleInfo GwyModuleInfo;

typedef GType                (*GwyGetTypeFunc)       (void);
typedef const GwyModuleInfo* (*GwyModuleQueryFunc)   (void);
typedef gboolean             (*GwyModuleRegisterFunc)(GError **error);

struct _GwyModuleInfo {
    guint32 abi_version;
    GwyModuleRegisterFunc register_func;
    const gchar *blurb;
    const gchar *author;
    const gchar *version;
    const gchar *copyright;
    const gchar *date;
};

typedef struct {
    const gchar *name;
    GwyGetTypeFunc get_type;
} GwyModuleProvidedType;

GQuark               gwy_module_error_quark   (void)                               G_GNUC_CONST;
gboolean             gwy_module_provide_type  (const gchar *name,
                                               GwyGetTypeFunc gettype,
                                               GError **error);
guint                gwy_module_provide       (GwyErrorList **errorlist,
                                               const gchar *name,
                                               ...)                                G_GNUC_NULL_TERMINATED;
guint                gwy_module_providev      (const GwyModuleProvidedType *types,
                                               guint ntypes,
                                               GwyErrorList **errorlist);
const GwyModuleInfo* gwy_module_load          (const gchar *filename,
                                               GError **error);
guint                gwy_module_load_library  (const gchar *filename,
                                               GwyErrorList **errorlist);
guint                gwy_module_load_directory(const gchar *path,
                                               GwyErrorList **errorlist);
void                 gwy_module_register_types(void);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
