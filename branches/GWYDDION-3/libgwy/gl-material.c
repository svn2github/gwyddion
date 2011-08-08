/*
 *  $Id$
 *  Copyright (C) 2009 David Nečas (Yeti).
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

#include <string.h>
#include "libgwy/macros.h"
#include "libgwy/strfuncs.h"
#include "libgwy/serializable-boxed.h"
#include "libgwy/serialize.h"
#include "libgwy/gl-material.h"
#include "libgwy/object-internal.h"

/* FIXME
#define BITS_PER_SAMPLE 8
#define MAX_CVAL (0.99999999*(1 << (BITS_PER_SAMPLE)))
*/

enum { N_ITEMS = 5 };

// NB: Below we assume this is plain old data that can be bit-wise assigned.
// If this ceases to hold, the code must be reviewed.
struct _GwyGLMaterialPrivate {
    GwyRGBA ambient;
    GwyRGBA diffuse;
    GwyRGBA specular;
    GwyRGBA emission;
    gdouble shininess;
};

typedef struct _GwyGLMaterialPrivate Material;

static void         gwy_gl_material_serializable_init(GwySerializableInterface *iface);
static gsize        gwy_gl_material_n_items          (GwySerializable *serializable);
static gsize        gwy_gl_material_itemize          (GwySerializable *serializable,
                                                      GwySerializableItems *items);
static gboolean     gwy_gl_material_construct        (GwySerializable *serializable,
                                                      GwySerializableItems *items,
                                                      GwyErrorList **error_list);
static GObject*     gwy_gl_material_duplicate_impl   (GwySerializable *serializable);
static void         gwy_gl_material_assign_impl      (GwySerializable *destination,
                                                      GwySerializable *source);
static GwyResource* gwy_gl_material_copy             (GwyResource *resource);
static void         gwy_gl_material_sanitize         (GwyGLMaterial *gl_material);
static void         gwy_gl_material_changed          (GwyGLMaterial *gl_material);
static void         gwy_gl_material_setup_inventory  (GwyInventory *inventory);
static gchar*       gwy_gl_material_dump             (GwyResource *resource);
static gboolean     gwy_gl_material_parse            (GwyResource *resource,
                                                      gchar *text,
                                                      GError **error);

static const GwyRGBA black = { 0, 0, 0, 0 };

/* OpenGL reference states these: */
static const Material opengl_default = {
    .ambient  = { 0.2, 0.2, 0.2, 1.0 },
    .diffuse  = { 0.8, 0.8, 0.8, 1.0 },
    .specular = { 0.0, 0.0, 0.0, 1.0 },
    .emission = { 0.0, 0.0, 0.0, 1.0 },
    .shininess = 0.0,
};

static const Material null_material = {
    .ambient  = { 0.0, 0.0, 0.0, 0.0 },
    .diffuse  = { 0.0, 0.0, 0.0, 0.0 },
    .specular = { 0.0, 0.0, 0.0, 0.0 },
    .emission = { 0.0, 0.0, 0.0, 0.0 },
    .shininess = 0.0,
};

static const GwySerializableItem serialize_items[N_ITEMS] = {
    /*0*/ { .name = "ambient",   .ctype = GWY_SERIALIZABLE_BOXED,  },
    /*1*/ { .name = "diffuse",   .ctype = GWY_SERIALIZABLE_BOXED,  },
    /*2*/ { .name = "specular",  .ctype = GWY_SERIALIZABLE_BOXED,  },
    /*3*/ { .name = "emission",  .ctype = GWY_SERIALIZABLE_BOXED,  },
    /*4*/ { .name = "shininess", .ctype = GWY_SERIALIZABLE_DOUBLE, },
};

G_DEFINE_TYPE_EXTENDED
    (GwyGLMaterial, gwy_gl_material, GWY_TYPE_RESOURCE, 0,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_gl_material_serializable_init));

GwySerializableInterface *gwy_gl_material_parent_serializable = NULL;

static void
gwy_gl_material_serializable_init(GwySerializableInterface *iface)
{
    gwy_gl_material_parent_serializable = g_type_interface_peek_parent(iface);
    iface->n_items   = gwy_gl_material_n_items;
    iface->itemize   = gwy_gl_material_itemize;
    iface->construct = gwy_gl_material_construct;
    iface->duplicate = gwy_gl_material_duplicate_impl;
    iface->assign    = gwy_gl_material_assign_impl;
}

static void
gwy_gl_material_class_init(GwyGLMaterialClass *klass)
{
    GwyResourceClass *res_class = GWY_RESOURCE_CLASS(klass);

    g_type_class_add_private(klass, sizeof(Material));

    res_class->setup_inventory = gwy_gl_material_setup_inventory;
    res_class->copy = gwy_gl_material_copy;
    res_class->dump = gwy_gl_material_dump;
    res_class->parse = gwy_gl_material_parse;

    gwy_resource_class_register(res_class, "glmaterials", NULL);
}

static void
gwy_gl_material_init(GwyGLMaterial *gl_material)
{
    gl_material->priv = G_TYPE_INSTANCE_GET_PRIVATE(gl_material,
                                                    GWY_TYPE_GL_MATERIAL,
                                                    Material);
    *gl_material->priv = opengl_default;
}

static gsize
gwy_gl_material_n_items(GwySerializable *serializable)
{
    return N_ITEMS+1 + 4*gwy_serializable_boxed_n_items(GWY_TYPE_RGBA)
           + gwy_gl_material_parent_serializable->n_items(serializable);
}

static gsize
gwy_gl_material_itemize(GwySerializable *serializable,
                        GwySerializableItems *items)
{

    GwyGLMaterial *gl_material = GWY_GL_MATERIAL(serializable);
    Material *material = gl_material->priv;
    GwySerializableItem it;

    // Our own data
    g_return_val_if_fail(items->len - items->n, 0);
    it = serialize_items[0];
    it.value.v_boxed = &material->ambient;
    items->items[items->n++] = it;
    gwy_serializable_boxed_itemize(GWY_TYPE_RGBA, it.value.v_boxed, items);

    g_return_val_if_fail(items->len - items->n, 0);
    it = serialize_items[1];
    it.value.v_boxed = &material->diffuse;
    items->items[items->n++] = it;
    gwy_serializable_boxed_itemize(GWY_TYPE_RGBA, it.value.v_boxed, items);

    g_return_val_if_fail(items->len - items->n, 0);
    it = serialize_items[2];
    it.value.v_boxed = &material->specular;
    items->items[items->n++] = it;
    gwy_serializable_boxed_itemize(GWY_TYPE_RGBA, it.value.v_boxed, items);

    g_return_val_if_fail(items->len - items->n, 0);
    it = serialize_items[3];
    it.value.v_boxed = &material->emission;
    items->items[items->n++] = it;
    gwy_serializable_boxed_itemize(GWY_TYPE_RGBA, it.value.v_boxed, items);

    g_return_val_if_fail(items->len - items->n, 0);
    it = serialize_items[4];
    it.value.v_double = material->shininess;
    items->items[items->n++] = it;

    return _gwy_itemize_chain_to_parent(serializable, GWY_TYPE_RESOURCE,
                                        gwy_gl_material_parent_serializable,
                                        items, N_ITEMS);
}

static gboolean
gwy_gl_material_construct(GwySerializable *serializable,
                          GwySerializableItems *items,
                          GwyErrorList **error_list)
{
    GwySerializableItem its[N_ITEMS];
    memcpy(its, serialize_items, sizeof(serialize_items));
    for (guint i = 0; i < 4; i++)
        its[i].array_size = GWY_TYPE_RGBA;

    gboolean ok = FALSE;
    GwySerializableItems parent_items;
    if (gwy_deserialize_filter_items(its, N_ITEMS, items, &parent_items,
                                     "GwyGLMaterial", error_list)) {
        if (!gwy_gl_material_parent_serializable->construct(serializable,
                                                            &parent_items,
                                                            error_list))
            goto fail;
    }

    // Our own data
    GwyGLMaterial *gl_material = GWY_GL_MATERIAL(serializable);
    Material *material = gl_material->priv;
    const GwyRGBA *color;

    if ((color = its[0].value.v_boxed))
        material->ambient = *color;
    if ((color = its[1].value.v_boxed))
        material->diffuse = *color;
    if ((color = its[2].value.v_boxed))
        material->specular = *color;
    if ((color = its[3].value.v_boxed))
        material->emission = *color;
    material->shininess = its[4].value.v_double;

    gwy_gl_material_sanitize(gl_material);

    ok = TRUE;

fail:
    // If the items were of another type filter_items() would catch it.
    for (guint i = 0; i < 4; i++) {
        if (its[i].value.v_boxed)
            gwy_rgba_free(its[i].value.v_boxed);
    }
    return ok;
}

static GObject*
gwy_gl_material_duplicate_impl(GwySerializable *serializable)
{
    GwyGLMaterial *gl_material = GWY_GL_MATERIAL(serializable);
    GwyGLMaterial *duplicate = g_object_newv(GWY_TYPE_GL_MATERIAL, 0, NULL);

    gwy_gl_material_parent_serializable->assign(GWY_SERIALIZABLE(duplicate),
                                                serializable);
    duplicate->priv = gl_material->priv;

    return G_OBJECT(duplicate);
}

static void
gwy_gl_material_assign_impl(GwySerializable *destination,
                            GwySerializable *source)
{
    GwyGLMaterial *dest = GWY_GL_MATERIAL(destination);
    GwyGLMaterial *src = GWY_GL_MATERIAL(source);
    gboolean emit_changed = FALSE;

    g_object_freeze_notify(G_OBJECT(dest));
    gwy_gl_material_parent_serializable->assign(destination, source);
    if (memcmp(dest->priv, src->priv, sizeof(Material)) != 0) {
        dest->priv = src->priv;
        emit_changed = TRUE;
    }
    g_object_thaw_notify(G_OBJECT(dest));
    if (emit_changed)
        gwy_gl_material_changed(dest);
}

static GwyResource*
gwy_gl_material_copy(GwyResource *resource)
{
    return GWY_RESOURCE(gwy_gl_material_duplicate_impl(GWY_SERIALIZABLE(resource)));
}

/* This is an internal function and does NOT call gwy_gl_material_changed(). */
static void
gwy_gl_material_sanitize(GwyGLMaterial *gl_material)
{
    Material *material = gl_material->priv;

    gwy_rgba_fix(&material->ambient);
    gwy_rgba_fix(&material->diffuse);
    gwy_rgba_fix(&material->specular);
    gwy_rgba_fix(&material->emission);
    material->shininess = CLAMP(material->shininess, 0.0, 1.0);
}

static gboolean
gwy_gl_material_set_rgba(GwyRGBA *dest,
                         const GwyRGBA *src)
{
    GwyRGBA color = *src;
    gwy_rgba_fix(&color);
    if (memcmp(dest, &color, sizeof(GwyRGBA) == 0))
        return FALSE;
    *dest = color;
    return TRUE;
}

/**
 * gwy_gl_material_get_ambient:
 * @gl_material: A GL material.
 *
 * Gets the ambient reflectance of a GL material.
 *
 * Returns: Ambient reflectance.
 **/
GwyRGBA
gwy_gl_material_get_ambient(GwyGLMaterial *gl_material)
{
    g_return_val_if_fail(GWY_IS_GL_MATERIAL(gl_material), black);
    return gl_material->priv->ambient;
}

/**
 * gwy_gl_material_set_ambient:
 * @gl_material: A GL material.
 * @ambient: Ambient reflectance.
 *
 * Sets the ambient reflectance of a GL material.
 **/
void
gwy_gl_material_set_ambient(GwyGLMaterial *gl_material,
                            const GwyRGBA *ambient)
{
    g_return_if_fail(GWY_IS_GL_MATERIAL(gl_material));
    g_return_if_fail(gwy_resource_is_modifiable(GWY_RESOURCE(gl_material)));
    if (gwy_gl_material_set_rgba(&gl_material->priv->ambient, ambient))
        gwy_gl_material_changed(gl_material);
}

/**
 * gwy_gl_material_get_diffuse:
 * @gl_material: A GL material.
 *
 * Gets the diffuse reflectance of a GL material.
 *
 * Returns: Diffuse reflectance.
 **/
GwyRGBA
gwy_gl_material_get_diffuse(GwyGLMaterial *gl_material)
{
    g_return_val_if_fail(GWY_IS_GL_MATERIAL(gl_material), black);
    return gl_material->priv->diffuse;
}

/**
 * gwy_gl_material_set_diffuse:
 * @gl_material: A GL material.
 * @diffuse: Diffuse reflectance.
 *
 * Sets the diffuse reflectance of a GL material.
 **/
void
gwy_gl_material_set_diffuse(GwyGLMaterial *gl_material,
                            const GwyRGBA *diffuse)
{
    g_return_if_fail(GWY_IS_GL_MATERIAL(gl_material));
    g_return_if_fail(gwy_resource_is_modifiable(GWY_RESOURCE(gl_material)));
    if (gwy_gl_material_set_rgba(&gl_material->priv->diffuse, diffuse))
        gwy_gl_material_changed(gl_material);
}

/**
 * gwy_gl_material_get_specular:
 * @gl_material: A GL material.
 *
 * Gets the specular reflectance of a GL material.
 *
 * Returns: Specular reflectance.
 **/
GwyRGBA
gwy_gl_material_get_specular(GwyGLMaterial *gl_material)
{
    g_return_val_if_fail(GWY_IS_GL_MATERIAL(gl_material), black);
    return gl_material->priv->specular;
}

/**
 * gwy_gl_material_set_specular:
 * @gl_material: A GL material.
 * @specular: Specular reflectance.
 *
 * Sets the specular reflectance of a GL material.
 **/
void
gwy_gl_material_set_specular(GwyGLMaterial *gl_material,
                             const GwyRGBA *specular)
{
    g_return_if_fail(GWY_IS_GL_MATERIAL(gl_material));
    g_return_if_fail(gwy_resource_is_modifiable(GWY_RESOURCE(gl_material)));
    if (gwy_gl_material_set_rgba(&gl_material->priv->specular, specular))
        gwy_gl_material_changed(gl_material);
}

/**
 * gwy_gl_material_get_emission:
 * @gl_material: A GL material.
 *
 * Gets the emission component of a GL material.
 *
 * Returns: Emission component.
 **/
GwyRGBA
gwy_gl_material_get_emission(GwyGLMaterial *gl_material)
{
    g_return_val_if_fail(GWY_IS_GL_MATERIAL(gl_material), black);
    return gl_material->priv->emission;
}

/**
 * gwy_gl_material_set_emission:
 * @gl_material: A GL material.
 * @emission: Emission component.
 *
 * Sets the emission component of a GL material.
 **/
void
gwy_gl_material_set_emission(GwyGLMaterial *gl_material,
                             const GwyRGBA *emission)
{
    g_return_if_fail(GWY_IS_GL_MATERIAL(gl_material));
    g_return_if_fail(gwy_resource_is_modifiable(GWY_RESOURCE(gl_material)));
    if (gwy_gl_material_set_rgba(&gl_material->priv->emission, emission))
        gwy_gl_material_changed(gl_material);
}

/**
 * gwy_gl_material_get_shininess:
 * @gl_material: A GL material.
 *
 * Gets the shininess value of a GL material.
 *
 * Returns: The shininess value (in normalised range [0,1], not [0,128]).
 **/
gdouble
gwy_gl_material_get_shininess(GwyGLMaterial *gl_material)
{
    g_return_val_if_fail(GWY_IS_GL_MATERIAL(gl_material), 0.0);
    return gl_material->priv->shininess;
}

/**
 * gwy_gl_material_set_shininess:
 * @gl_material: A GL material.
 * @shininess: Shinniness value (in normalised range [0,1], not [0,128]).
 *
 * Sets the shininess value of a GL material.
 **/
void
gwy_gl_material_set_shininess(GwyGLMaterial *gl_material,
                              gdouble shininess)
{
    g_return_if_fail(GWY_IS_GL_MATERIAL(gl_material));
    g_return_if_fail(gwy_resource_is_modifiable(GWY_RESOURCE(gl_material)));
    shininess = CLAMP(shininess, 0.0, 1.0);
    if (shininess != gl_material->priv->shininess) {
        gl_material->priv->shininess = shininess;
        gwy_gl_material_changed(gl_material);
    }
}

#if 0
/**
 * gwy_gl_material_sample_to_pixbuf:
 * @gl_material: A GL material to sample.
 * @pixbuf: A pixbuf to sample gl_material to (in horizontal direction).
 *
 * Samples GL material to a provided pixbuf.
 **/
void
gwy_gl_material_sample_to_pixbuf(GwyGLMaterial *gl_material,
                                 GdkPixbuf *pixbuf)
{
    GwyGLMaterial *glm = gl_material;
    gint width, height, rowstride, i, j, bpp;
    gboolean has_alpha;
    guchar *row, *pdata;
    guchar alpha;
    gdouble p, q;

    g_return_if_fail(GWY_IS_GL_MATERIAL(gl_material));
    g_return_if_fail(GDK_IS_PIXBUF(pixbuf));

    width = gdk_pixbuf_get_width(pixbuf);
    height = gdk_pixbuf_get_height(pixbuf);
    rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    has_alpha = gdk_pixbuf_get_has_alpha(pixbuf);
    pdata = gdk_pixbuf_get_pixels(pixbuf);
    bpp = 3 + (has_alpha ? 1 : 0);

    q = (width == 1) ? 0.0 : 1.0/(width - 1.0);
    p = (height == 1) ? 0.0 : 1.0/(height - 1.0);
    alpha = (guchar)CLAMP(MAX_CVAL*glm->ambient.a, 0.0, 255.0);

    for (j = 0; j < width; j++) {
        gdouble VRp = j*q*(2.0 - j*q);
        gdouble s = pow(VRp, 128.0*glm->shininess);
        GwyRGBA s0;

        s0.r = glm->emission.r + 0.3*glm->ambient.r + glm->specular.r*s;
        s0.g = glm->emission.g + 0.3*glm->ambient.g + glm->specular.g*s;
        s0.b = glm->emission.b + 0.3*glm->ambient.b + glm->specular.b*s;

        for (i = 0; i < height; i++) {
            gdouble LNp = 1.0 - i*p;
            gdouble v;

            row = pdata + i*rowstride + j*bpp;

            v = s0.r + glm->diffuse.r*LNp;
            *(row++) = (guchar)CLAMP(MAX_CVAL*v, 0.0, 255.0);

            v = s0.g + glm->diffuse.g*LNp;
            *(row++) = (guchar)CLAMP(MAX_CVAL*v, 0.0, 255.0);

            v = s0.b + glm->diffuse.b*LNp;
            *(row++) = (guchar)CLAMP(MAX_CVAL*v, 0.0, 255.0);

            if (has_alpha)
                *(row++) = alpha;
        }
    }
}
#endif

static void
gwy_gl_material_changed(GwyGLMaterial *gl_material)
{
    gwy_resource_data_changed(GWY_RESOURCE(gl_material));
}

static void
gwy_gl_material_setup_inventory(GwyInventory *inventory)
{
    gwy_inventory_set_default_name(inventory, GWY_GL_MATERIAL_DEFAULT);
    GwyGLMaterial *gl_material;
    // OpenGL-Default
    gl_material = g_object_new(GWY_TYPE_GL_MATERIAL,
                               "is-modifiable", FALSE,
                               "name", GWY_GL_MATERIAL_DEFAULT,
                               NULL);
    gwy_inventory_insert(inventory, gl_material);
    g_object_unref(gl_material);
    // None
    gl_material = g_object_new(GWY_TYPE_GL_MATERIAL,
                               "is-modifiable", FALSE,
                               "name", GWY_GL_MATERIAL_NONE,
                               NULL);
    *gl_material->priv = null_material;
    gwy_inventory_insert(inventory, gl_material);
    g_object_unref(gl_material);
}

/**
 * gwy_gl_material_new:
 *
 * Creates a new GL material.
 *
 * Returns: A new free-standing GL material.
 **/
GwyGLMaterial*
gwy_gl_material_new(void)
{
    return g_object_newv(GWY_TYPE_GL_MATERIAL, 0, NULL);
}

static gchar*
gwy_gl_material_dump(GwyResource *resource)
{
    GwyGLMaterial *gl_material = GWY_GL_MATERIAL(resource);
    Material *material = gl_material->priv;
    gchar *amb, *dif, *spec, *emi;

    amb = gwy_resource_dump_data_line((gdouble*)&material->ambient, 4);
    dif = gwy_resource_dump_data_line((gdouble*)&material->diffuse, 4);
    spec = gwy_resource_dump_data_line((gdouble*)&material->specular, 4);
    emi = gwy_resource_dump_data_line((gdouble*)&material->emission, 4);

    gchar buffer[G_ASCII_DTOSTR_BUF_SIZE];
    g_ascii_formatd(buffer, sizeof(buffer), "%g", material->shininess);

    gchar *retval = g_strjoin("\n", amb, dif, spec, emi, buffer, "", NULL);
    g_free(amb);
    g_free(dif);
    g_free(spec);
    g_free(emi);

    return retval;
}

static gboolean
parse_component(gchar **text, GwyRGBA *color)
{
    GwyResourceLineType ltype;
    do {
        gchar *line = gwy_str_next_line(text);
        if (!line)
            return FALSE;
        ltype = gwy_resource_parse_data_line(line, 4, (gdouble*)color);
        if (ltype == GWY_RESOURCE_LINE_OK)
            return TRUE;
    } while (ltype == GWY_RESOURCE_LINE_EMPTY);
    return FALSE;
}

static gboolean
gwy_gl_material_parse(GwyResource *resource,
                      gchar *text,
                      G_GNUC_UNUSED GError **error)
{
    GwyGLMaterial *gl_material = GWY_GL_MATERIAL(resource);

    if (!parse_component(&text, &gl_material->priv->ambient)
        || !parse_component(&text, &gl_material->priv->diffuse)
        || !parse_component(&text, &gl_material->priv->specular)
        || !parse_component(&text, &gl_material->priv->emission))
        return FALSE;

    gchar *end;
    gl_material->priv->shininess = g_ascii_strtod(text, &end);
    if (end == text)
        return FALSE;

    gwy_gl_material_sanitize(gl_material);
    return TRUE;
}


/************************** Documentation ****************************/

/**
 * SECTION: gl-material
 * @title: GwyGLMaterial
 * @short_description: OpenGL material representation
 *
 * #GwyGLMaterial represents an OpenGL material.  Its properties directly map
 * to corresponding OpenGL material characteristics.  Note however that all
 * are #GwyGLMaterial properties are normalised to the range [0,1].
 *
 * GL material objects can be obtained from gwy_gl_materials_get(). New GL
 * materials can be created with gwy_inventory_copy() on the #GwyInventory
 * returned by gwy_gl_materials().
 **/

/**
 * GwyGLMaterial:
 *
 * Object represnting an OpenGL material.
 *
 * The #GwyGLMaterial struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GwyGLMaterialClass:
 *
 * Class of OpenGL materials.
 *
 * #GwyGLMaterialClass does not contain any public members.
 **/

/**
 * GWY_GL_MATERIAL_DEFAULT:
 *
 * Name of the default OpenGL material.
 *
 * It is guaranteed to always exist.
 *
 * Note this is not the same as user's default material which corresponds to
 * the default item in gwy_gl_materials() inventory and it change over time.
 **/

/**
 * GWY_GL_MATERIAL_NONE:
 *
 * Name of the special void material with all characteristics zero.
 *
 * It is guaranteed to exist, but you should rarely actually need it.
 **/

/**
 * gwy_gl_material_duplicate:
 * @gl_material: A GL material.
 *
 * Duplicates a GL material.
 *
 * This is a convenience wrapper of gwy_serializable_duplicate().
 **/

/**
 * gwy_gl_material_assign:
 * @dest: Destination GL material.
 * @src: Source GL material.
 *
 * Copies the value of a GL material.
 *
 * This is a convenience wrapper of gwy_serializable_assign().
 **/

/**
 * gwy_gl_materials:
 *
 * Gets inventory with all the GL materials.
 *
 * Returns: GL material inventory.
 **/

/**
 * gwy_gl_materials_get:
 * @name: GL material name.  May be %NULL to get the default GL material.
 *
 * Convenience function to get a GL material from gwy_gl_materials() by name.
 *
 * Returns: GL material identified by @name or the default GL material if @name
 *          does not exist.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
