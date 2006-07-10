/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net.
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

#include "config.h"
#include <libdraw/gwydraw.h>
#include <libgwydgets/gwydgets.h>

static GdkGLConfig *glconfig = NULL;

/************************** Initialization ****************************/

/**
 * gwy_widgets_type_init:
 *
 * Makes libgwydgets types safe for deserialization and performs other
 * initialization.  You have to call this function before using widgets and
 * objects from libgwydgets.
 *
 * Calls gwy_draw_type_init() first to make sure libgwydraw is initialized.
 *
 * It is safe to call this function more than once, subsequent calls are no-op.
 **/
void
gwy_widgets_type_init(void)
{
    static guint types_initialized = FALSE;

    if (types_initialized)
        return;

    gwy_draw_type_init();

    g_type_class_peek(GWY_TYPE_GRAPH_CURVE_MODEL);
    g_type_class_peek(GWY_TYPE_GRAPH_MODEL);
    g_type_class_peek(GWY_TYPE_3D_LABEL);
    g_type_class_peek(GWY_TYPE_3D_SETUP);
    types_initialized = 1;

    gtk_rc_parse_string(/* graph window statusbar */
                        "style \"flatstatusbar\" {\n"
                        "GtkStatusbar::shadow_type = 0\n"
                        "}\n"
                        "widget \"*.flatstatusbar\" style \"flatstatusbar\"\n"
                        "\n");
}

/**
 * gwy_widgets_gl_init:
 *
 * Configures an OpenGL-capable visual for 3D widgets.
 *
 * Use gwy_widgets_get_gl_config() to get the framebuffer configuration.
 *
 * This function must be called before OpenGL widgets can be used.
 *
 * Returns: %TRUE if an appropriate visual was found.  If Gwyddion was compiled
 *          without OpenGL support, it always returns %FALSE.
 **/
gboolean
gwy_widgets_gl_init(void)
{
    /* when called twice, fail but successfully :o) */
    g_return_val_if_fail(glconfig == NULL, TRUE);

#ifdef HAVE_GTKGLEXT
    glconfig = gdk_gl_config_new_by_mode(GDK_GL_MODE_RGB
                                         | GDK_GL_MODE_DEPTH
                                         | GDK_GL_MODE_DOUBLE);
    /* Try double-buffered visual */
    if (!glconfig) {
        g_warning("Cannot find a double-buffered OpenGL visual, "
                  "Trying single-buffered visual.");

        /* Try single-buffered visual */
        glconfig = gdk_gl_config_new_by_mode(GDK_GL_MODE_RGB
                                             | GDK_GL_MODE_DEPTH);
        if (!glconfig) {
            g_warning("No appropriate OpenGL-capable visual found.");
        }
    }
#endif

    return glconfig != NULL;
}

/**
 * gwy_widgets_get_gl_config:
 *
 * Returns OpenGL framebuffer configuration for 3D widgets.
 *
 * Call gwy_widgets_gl_init() first.
 *
 * Returns: The OpenGL framebuffer configuration, %NULL if OpenGL
 *          initialization was not successfull.
 **/
GdkGLConfig*
gwy_widgets_get_gl_config(void)
{
    return glconfig;
}

/************************** Documentation ****************************/

/**
 * SECTION:gwydgets
 * @title: gwydgets
 * @short_description: Base functions
 *
 * Gwyddion classes has to be initialized before they can be safely
 * deserialized. The function gwy_type_init() performs this initialization.
 *
 * Before 3D widgets (#Gwy3DView) can be used, OpenGL must be initialized with
 * gwy_widgets_gl_init().
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
