/*
 *  $Id$
 *  Copyright (C) 2009 David Necas (Yeti).
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

#ifndef __LIBGWY_GL_MATERIAL_H__
#define __LIBGWY_GL_MATERIAL_H__

#include <libgwy/rgba.h>
#include <libgwy/serializable.h>
#include <libgwy/resource.h>

G_BEGIN_DECLS

#define GWY_GL_MATERIAL_DEFAULT "OpenGL-Default"
#define GWY_GL_MATERIAL_NONE    "None"

#define GWY_TYPE_GL_MATERIAL \
    (gwy_gl_material_get_type())
#define GWY_GL_MATERIAL(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_GL_MATERIAL, GwyGLMaterial))
#define GWY_GL_MATERIAL_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_GL_MATERIAL, GwyGLMaterialClass))
#define GWY_IS_GL_MATERIAL(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_GL_MATERIAL))
#define GWY_IS_GL_MATERIAL_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_GL_MATERIAL))
#define GWY_GL_MATERIAL_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_GL_MATERIAL, GwyGLMaterialClass))

typedef struct _GwyGLMaterial      GwyGLMaterial;
typedef struct _GwyGLMaterialClass GwyGLMaterialClass;

struct _GwyGLMaterial {
    GwyResource resource;
    struct _GwyGLMaterialPrivate *priv;
};

struct _GwyGLMaterialClass {
    /*<private>*/
    GwyResourceClass resource_class;
};

#define gwy_gl_material_duplicate(gl_material) \
        (GWY_GL_MATERIAL(gwy_serializable_duplicate(GWY_SERIALIZABLE(gl_material))))
#define gwy_gl_material_assign(dest, src) \
        (gwy_serializable_assign(GWY_SERIALIZABLE(dest), GWY_SERIALIZABLE(src)))

GType          gwy_gl_material_get_type     (void)                       G_GNUC_CONST;
GwyGLMaterial* gwy_gl_material_new          (void)                       G_GNUC_MALLOC;
GwyRGBA        gwy_gl_material_get_ambient  (GwyGLMaterial *gl_material) G_GNUC_PURE;
void           gwy_gl_material_set_ambient  (GwyGLMaterial *gl_material,
                                             const GwyRGBA *ambient);
GwyRGBA        gwy_gl_material_get_diffuse  (GwyGLMaterial *gl_material) G_GNUC_PURE;
void           gwy_gl_material_set_diffuse  (GwyGLMaterial *gl_material,
                                             const GwyRGBA *diffuse);
GwyRGBA        gwy_gl_material_get_specular (GwyGLMaterial *gl_material) G_GNUC_PURE;
void           gwy_gl_material_set_specular (GwyGLMaterial *gl_material,
                                             const GwyRGBA *specular);
GwyRGBA        gwy_gl_material_get_emission (GwyGLMaterial *gl_material) G_GNUC_PURE;
void           gwy_gl_material_set_emission (GwyGLMaterial *gl_material,
                                             const GwyRGBA *emission);
gdouble        gwy_gl_material_get_shininess(GwyGLMaterial *gl_material) G_GNUC_PURE;
void           gwy_gl_material_set_shininess(GwyGLMaterial *gl_material,
                                             gdouble shininess);

#define gwy_gl_materials() \
    (gwy_resource_type_get_inventory(GWY_TYPE_GL_MATERIAL))
#define gwy_gl_materials_get(name) \
    ((GwyGLMaterial*)gwy_inventory_get_or_default(gwy_gl_materials(), (name)))

G_END_DECLS

#endif /*__GWY_GL_MATERIAL_H__*/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
