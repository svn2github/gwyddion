/*
 *  @(#) $Id$
 *  Copyright (C) 2004 Martin Siler.
 *  E-mail: silerm@physics.muni.cz.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

#include <string.h>
#include <stdio.h>
#include <math.h>

#include <libgwyddion/gwymacros.h>
#include "gwyglmaterial.h"

#define GWY_GLMATERIAL_TYPE_NAME "GwyGLMaterial"

typedef struct {
    GLfloat ambient[4];
    GLfloat diffuse[4];
    GLfloat specular[4];
    GLfloat shininess;
} GwyGLMaterialPreset;

static void     gwy_glmaterial_class_init        (GwyGLMaterialClass *klass);
static void     gwy_glmaterial_init              (GwyGLMaterial *glmaterial);
static void     gwy_glmaterial_finalize          (GObject *object);
static gchar*   gwy_glmaterial_invent_name       (GHashTable *materials,
                                                      const gchar *prefix);
static void     gwy_glmaterial_create_preset     (GwyGLMaterialPreset *entries,
                                                      const gchar *name);

static GObjectClass *parent_class = NULL;

GType
gwy_glmaterial_get_type(void)
{
    static GType gwy_glmaterial_type = 0;

    if (!gwy_glmaterial_type) {
        static const GTypeInfo gwy_glmaterial_info = {
            sizeof(GwyGLMaterialClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_glmaterial_class_init,
            NULL,
            NULL,
            sizeof(GwyGLMaterial),
            0,
            (GInstanceInitFunc)gwy_glmaterial_init,
            NULL,
        };


        gwy_debug("");
        gwy_glmaterial_type = g_type_register_static(G_TYPE_OBJECT,
                                                   GWY_GLMATERIAL_TYPE_NAME,
                                                   &gwy_glmaterial_info,
                                                   0);
    }

    return gwy_glmaterial_type;
}


static void
gwy_glmaterial_class_init(GwyGLMaterialClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gwy_debug("");

    parent_class = g_type_class_peek_parent(klass);
    gobject_class->finalize = gwy_glmaterial_finalize;
    /* static classes are never finalized, so this is never freed */
    klass->materials = g_hash_table_new(g_str_hash, g_str_equal);
    /*XXX: too early gwy_glmaterial_setup_presets(klass->materials);*/
}

static void
gwy_glmaterial_init(GwyGLMaterial *glmaterial)
{
    gwy_debug("");
    glmaterial->ambient[0]  = 0.0f;
    glmaterial->ambient[1]  = 0.0f;
    glmaterial->ambient[2]  = 0.0f;
    glmaterial->ambient[3]  = 1.0f;
    glmaterial->diffuse[0]  = 0.0f;
    glmaterial->diffuse[1]  = 0.0f;
    glmaterial->diffuse[2]  = 0.0f;
    glmaterial->diffuse[3]  = 1.0f;
    glmaterial->specular[0] = 0.0f;
    glmaterial->specular[1] = 0.0f;
    glmaterial->specular[2] = 0.0f;
    glmaterial->specular[3] = 1.0f;
    glmaterial->shininess   = 1.0f;

    glmaterial->name = NULL;
}

static void
gwy_glmaterial_finalize(GObject *object)
{
    GwyGLMaterial *glmaterial = (GwyGLMaterial*)object;
    GwyGLMaterialClass *klass;
    gboolean removed;

    gwy_debug("%s", glmaterial->name);

    klass = GWY_GLMATERIAL_GET_CLASS(glmaterial);
    removed = g_hash_table_remove(klass->materials, glmaterial->name);
    g_assert(removed);
    g_free(glmaterial->name);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

/**
 * gwy_glmaterial_new:
 * @name: Open GL Material name.
 *
 * Returns a Open GL material called @name.
 *
 * Open GL Materials of the same name are singletons, thus if a Open GL material
 * definitions called @name already exists, it is returned instead (with
 * reference count incremented).
 *
 * @name can be %NULL, a new unique name is invented then.
 *
 * A newly created Open GL material is non-reflecting non-transparent. 
 * The graphical object is black independently of the light settings. 
 *
 * Returns: The new Open GL material definition as a #GObject.
 * 
 * Since 1.5  
 **/
GObject*
gwy_glmaterial_new(const gchar *name)
{
    GwyGLMaterial *glmaterial;
    GwyGLMaterialClass *klass;

    gwy_debug("");

    /* when g_type_class_peek() returns NULL we are constructing the very
     * first Open GL material definition and thus no other can exist yet */
    if ((klass = g_type_class_peek(GWY_TYPE_GLMATERIAL))
        && name
        && (glmaterial = g_hash_table_lookup(klass->materials, name))) {
        g_object_ref(glmaterial);
        return (GObject*)glmaterial;
    }

    glmaterial = g_object_new(GWY_TYPE_GLMATERIAL, NULL);
    /* now it has to be defined */
    klass = g_type_class_peek(GWY_TYPE_GLMATERIAL);
    g_assert(klass);
    glmaterial->name = gwy_glmaterial_invent_name(klass->materials, name);
    g_hash_table_insert(klass->materials, glmaterial->name, glmaterial);

    return (GObject*)(glmaterial);
}


/**
 * gwy_glmaterial_new_as_copy:
 * @src_glmaterial: An existing #GwyGLMaterial.
 *
 * Creates a new Open GL material  as a copy of an existing one.
 *
 * A new name is invented based on the existing one.
 *
 * Returns: The new Open GL material definition as a #GObject.
 * 
 * Since 1.5  
 **/
GObject*
gwy_glmaterial_new_as_copy(GwyGLMaterial *src_glmaterial)
{
    GwyGLMaterial *glmaterial;
    guint i;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_GLMATERIAL(src_glmaterial), NULL);

    glmaterial = (GwyGLMaterial*)gwy_glmaterial_new(src_glmaterial->name);

    for (i = 0; i < 4; i++) {
        glmaterial->ambient[i]  = src_glmaterial->ambient[i];
        glmaterial->diffuse[i]  = src_glmaterial->diffuse[i];
        glmaterial->specular[i] = src_glmaterial->specular[i];
    }
    glmaterial->shininess = src_glmaterial->shininess;

    return (GObject*)(glmaterial);
}


/**
 * gwy_glmaterial_get_name:
 * @glmaterial: A #GwyGLMaterial.
 *
 * Returns the name of Open GL material @glmaterial.
 *
 * Returns: The name. It should be considered constant and not modifier or
 *          freed.
 *          
 * Since 1.5  
 **/
G_CONST_RETURN gchar*
gwy_glmaterial_get_name(GwyGLMaterial *glmaterial)
{
    g_return_val_if_fail(GWY_IS_GLMATERIAL(glmaterial), NULL);
    return glmaterial->name;
}

/**
 * gwy_glmaterial_set_name:
 * @glmaterial: A #GwyGLMaterial.
 * @name: A new name of the Open GL material 
 *
 * Sets the name of a Open GL material.
 *
 * This function fails when a Open GL material definition of given name already
 * exists.
 *
 * Returns: Whether the rename was successfull.
 * 
 * Since 1.5  
 **/
gboolean
gwy_glmaterial_set_name(GwyGLMaterial *glmaterial,
                         const gchar *name)
{
    GwyGLMaterialClass *klass;
    gchar *oldname;

    g_return_val_if_fail(GWY_IS_GLMATERIAL(glmaterial), FALSE);
    g_return_val_if_fail(name, FALSE);

    klass = GWY_GLMATERIAL_GET_CLASS(glmaterial);
    if (name == glmaterial->name)
        return TRUE;
    if (g_hash_table_lookup(klass->materials, name))
        return FALSE;

    oldname = glmaterial->name;
    g_hash_table_steal(klass->materials, glmaterial->name);
    glmaterial->name = g_strdup(name);
    g_hash_table_insert(klass->materials, glmaterial->name, glmaterial);
    g_free(oldname);

    return TRUE;
}


/**
 * gwy_glmaterial_get_by_name:
 * @name: A new name of the Open GL material 
 *
 * Returns an Open GL material given by @name. 
 * The function does not incement reference count of material. 
 *
 * Returns: Open GL material ginven by @name or %NULL if material 
 * does not exists.
 * 
 * Since 1.5  
 **/
GwyGLMaterial*
gwy_glmaterial_get_by_name(const gchar *name)
{
    GwyGLMaterialClass *klass;

    g_return_val_if_fail(name, NULL);

    klass = g_type_class_peek(GWY_TYPE_GLMATERIAL);
    return g_hash_table_lookup(klass->materials, name);
}

/*
 * Copied from <libdraw/gwypalettedef.c>
 */ 
static gchar*
gwy_glmaterial_invent_name(GHashTable *materials,
                            const gchar *prefix)
{
    gchar *str;
    gint n, i;

    if (!prefix)
        prefix = _("Untitled");
    n = strlen(prefix);
    str = g_new(gchar, n + 9);
    strcpy(str, prefix);
    if (!g_hash_table_lookup(materials, str))
        return str;
    for (i = 1; i < 100000; i++) {
        g_snprintf(str + n, 9, "%d", i);
        if (!g_hash_table_lookup(materials, str))
            return str;
    }
    g_assert_not_reached();
    return NULL;
}

static void
gwy_glmaterial_create_preset(GwyGLMaterialPreset *entry,
                              const gchar *name)
{
    GwyGLMaterial *glmaterial;
    gint i;

    glmaterial = (GwyGLMaterial*)gwy_glmaterial_new(name);
    for (i = 0; i < 4; i++) {
        glmaterial->ambient[i]  = entry->ambient[i];
        glmaterial->diffuse[i]  = entry->diffuse[i];
        glmaterial->specular[i] = entry->specular[i];
    }
    glmaterial->shininess = entry->shininess;
}

/**
 * gwy_glmaterial_setup_presets:
 *
 * Set up built-in Open GL material definitions.  To be used in Gwyddion initialization
 * and eventually replaced by loading Open GL material definitions from external files.
 * 
 * Sice 1.5  
 **/
void
gwy_glmaterial_setup_presets(void)
{
    static GwyGLMaterialPreset mat_emerald = {
        {0.0215, 0.1745, 0.0215, 1.0},
        {0.07568, 0.61424, 0.07568, 1.0},
        {0.633, 0.727811, 0.633, 1.0},
        0.6
    };
    static GwyGLMaterialPreset mat_jade = {
        {0.135, 0.2225, 0.1575, 1.0},
        {0.54, 0.89, 0.63, 1.0},
        {0.316228, 0.316228, 0.316228, 1.0},
        0.1
    };
    static GwyGLMaterialPreset mat_obsidian = {
        {0.05375, 0.05, 0.06625, 1.0},
        {0.18275, 0.17, 0.22525, 1.0},
        {0.332741, 0.328634, 0.346435, 1.0},
        0.3
    };
    static GwyGLMaterialPreset mat_pearl = {
        {0.25, 0.20725, 0.20725, 1.0},
        {1.0, 0.829, 0.829, 1.0},
        {0.296648, 0.296648, 0.296648, 1.0},
        0.088
    };
    static GwyGLMaterialPreset mat_ruby = {
        {0.1745, 0.01175, 0.01175, 1.0},
        {0.61424, 0.04136, 0.04136, 1.0},
        {0.727811, 0.626959, 0.626959, 1.0},
        0.6
    };
    static GwyGLMaterialPreset mat_turquoise = {
        {0.1, 0.18725, 0.1745, 1.0},
        {0.396, 0.74151, 0.69102, 1.0},
        {0.297254, 0.30829, 0.306678, 1.0},
        0.1
    };
    static GwyGLMaterialPreset mat_brass = {
        {0.329412, 0.223529, 0.027451, 1.0},
        {0.780392, 0.568627, 0.113725, 1.0},
        {0.992157, 0.941176, 0.807843, 1.0},
        0.21794872
    };
    static GwyGLMaterialPreset mat_bronze = {
        {0.2125, 0.1275, 0.054, 1.0},
        {0.714, 0.4284, 0.18144, 1.0},
        {0.393548, 0.271906, 0.166721, 1.0},
        0.2
    };
    static GwyGLMaterialPreset mat_chrome = {
        {0.25, 0.25, 0.25, 1.0},
        {0.4, 0.4, 0.4, 1.0},
        {0.774597, 0.774597, 0.774597, 1.0},
        0.6
    };
    static GwyGLMaterialPreset mat_copper = {
        {0.19125, 0.0735, 0.0225, 1.0},
        {0.7038, 0.27048, 0.0828, 1.0},
        {0.256777, 0.137622, 0.086014, 1.0},
        0.1
    };
    static GwyGLMaterialPreset mat_gold = {
        {0.24725, 0.1995, 0.0745, 1.0},
        {0.75164, 0.60648, 0.22648, 1.0},
        {0.628281, 0.555802, 0.366065, 1.0},
        0.4
    };
    static GwyGLMaterialPreset mat_silver = {
        {0.19225, 0.19225, 0.19225, 1.0},
        {0.50754, 0.50754, 0.50754, 1.0},
        {0.508273, 0.508273, 0.508273, 1.0},
        0.4
    };
    static GwyGLMaterialPreset mat_none = {
        {0.0, 0.0, 0.0, 0.0},
        {0.0, 0.0, 0.0, 0.0},
        {0.0, 0.0, 0.0, 0.0},
        0.0
    };

    gwy_glmaterial_create_preset(&mat_none,       GWY_GLMATERIAL_NONE     );
    gwy_glmaterial_create_preset(&mat_emerald,    GWY_GLMATERIAL_EMERALD  );
    gwy_glmaterial_create_preset(&mat_jade,       GWY_GLMATERIAL_JADE     );
    gwy_glmaterial_create_preset(&mat_obsidian,   GWY_GLMATERIAL_OBSIDIAN );
    gwy_glmaterial_create_preset(&mat_pearl,      GWY_GLMATERIAL_PEARL    );
    gwy_glmaterial_create_preset(&mat_ruby,       GWY_GLMATERIAL_RUBY     );
    gwy_glmaterial_create_preset(&mat_turquoise,  GWY_GLMATERIAL_TURQUOISE);
    gwy_glmaterial_create_preset(&mat_brass,      GWY_GLMATERIAL_BRASS    );
    gwy_glmaterial_create_preset(&mat_bronze,     GWY_GLMATERIAL_BRONZE   );
    gwy_glmaterial_create_preset(&mat_chrome,     GWY_GLMATERIAL_CHROME   );
    gwy_glmaterial_create_preset(&mat_copper,     GWY_GLMATERIAL_COPPER   );
    gwy_glmaterial_create_preset(&mat_gold,       GWY_GLMATERIAL_GOLD     );
    gwy_glmaterial_create_preset(&mat_silver,     GWY_GLMATERIAL_SILVER   );
}


/**
 * gwy_glmaterial_exists:
 * @name: A Open GL material name.
 *
 * Tests whether a Open GL material definition of given name exists.
 *
 * Returns: %TRUE if such a Open GL material definition exists, %FALSE otherwise.
 * 
 * Since 1.5  
 **/
gboolean
gwy_glmaterial_exists(const gchar *name)
{
    GwyGLMaterialClass *klass;

    g_return_val_if_fail(name, FALSE);
    klass = g_type_class_peek(GWY_TYPE_GLMATERIAL);
    g_return_val_if_fail(klass, FALSE);
    return g_hash_table_lookup(klass->materials, name) != 0;
}

/**
 * GwyGLMaterialFunc:
 * @name: Open GL Material definition name.
 * @glmaterial: Open GL Material definition.
 * @user_data: A user-specified pointer.
 *
 * Callback function type for gwy_glmaterial_foreach().
 * 
 * Since 1.5  
 **/

/**
 * gwy_glmaterial_foreach:
 * @callback: A callback.
 * @user_data: User data passed to the callback.
 *
 * Runs @callback for each existing Open GL material definition.
 * 
 * Since 1.5  
 **/
void
gwy_glmaterial_foreach(GwyGLMaterialFunc callback,
                        gpointer user_data)
{
    GwyGLMaterialClass *klass;

    klass = g_type_class_peek(GWY_TYPE_GLMATERIAL);
    g_hash_table_foreach(klass->materials, (GHFunc)callback, user_data);
}

/************************** Documentation ****************************/

/**
 * GwyGLMaterial:
 *
 * The #GwyGLMaterial struct contains informations about the material properties
 * of OpenGL objects. For details see OpenGL documentation, specifically the light 
 * settings and glMaterialfv and glMaterialf functions.
 * 
 * @name: name of OpenGL material
 * @ambient: vector of the reflectivity of color components (RGBA) of ambient light. 
 * All values should be in interval 0-1. 
 * @diffuse: vector of the reflectivity of color components (RGBA) of diffuse light
 * @specular: vector of the reflectivity of color components (RGBA) of specular light
 * @shininess: intrisic shininess of the material. Values should be in inteval 0-1. This 
 * value is multiplied by factor 128 before passing to the glMaterialf function. 
 * 
 * Since 1.5  
 **/


/**
 * GWY_GLMATERIAL_NONE:
 * 
 * Black material independent on light settings.
 * 
 * This is the default material.
 **/
   
/**
 * GWY_GLMATERIAL_EMERALD:
 * 
 * Emerald like mateial. Mainly light green.
 **/ 

/**
 * GWY_GLMATERIAL_JADE:
 * 
 * Jade
 **/ 

/**
 * GWY_GLMATERIAL_OBSIDIAN:
 * 
 * Obsidian, very dark material.
 **/ 

/**
 * GWY_GLMATERIAL_PEARL:
 * 
 * Pearl
 **/
 
/** 
 * GWY_GLMATERIAL_RUBY:
 * 
 * Ruby
 **/

/**
 * GWY_GLMATERIAL_TURQUOISE:
 * 
 * Turquoise
 **/
  
/**
 * GWY_GLMATERIAL_BRASS:
 * 
 * Brass
 **/
   
/**
 * GWY_GLMATERIAL_BRONZE:
 * 
 * Bronze
 **/
  
/**
 * GWY_GLMATERIAL_CHROME:
 * 
 * Chrome
 **/ 

/**
 * GWY_GLMATERIAL_COPPER:
 * 
 * Copper
 **/ 

/**
 * GWY_GLMATERIAL_GOLD:
 * 
 * Gold
 **/
  
/**
 * GWY_GLMATERIAL_SILVER:
 *  
 * Silver
 **/