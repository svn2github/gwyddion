/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net.
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

#ifndef __GWY_GWY3DVIEW_H__
#define __GWY_GWY3DVIEW_H__

#include <gdk/gdk.h>
#include <gtk/gtkadjustment.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkdrawingarea.h>
#include <gtk/gtkadjustment.h>
#include <pango/pangoft2.h>

#include <gtk/gtkgl.h>

#ifdef G_OS_WIN32
#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>
#endif

#include <GL/gl.h>
#include <GL/glu.h>

#include <libprocess/datafield.h>
#include <libdraw/gwypalette.h>
#include <libdraw/gwygradient.h>
#include <libgwyddion/gwysiunit.h>

#include "gwyglmaterial.h"
#include "gwy3dlabels.h"

G_BEGIN_DECLS

#define GWY_TYPE_3D_VIEW              (gwy_3d_view_get_type())
#define GWY_3D_VIEW(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_3D_VIEW, Gwy3DView))
#define GWY_3D_VIEW_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_3D_VIEW, Gwy3DViewClass))
#define GWY_IS_3D_VIEW(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_3D_VIEW))
#define GWY_IS_3D_VIEW_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_3D_VIEW))
#define GWY_3D_VIEW_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_3D_VIEW, Gwy3DViewClass))

typedef struct _Gwy3DView      Gwy3DView;
typedef struct _Gwy3DViewClass Gwy3DViewClass;

typedef enum {
    GWY_3D_NONE           = 0,
    GWY_3D_ROTATION       = 1,
    GWY_3D_SCALE          = 2,
    GWY_3D_DEFORMATION    = 3,
    GWY_3D_LIGHT_MOVEMENT = 4
} Gwy3DMovement;


struct _Gwy3DView {
    GtkDrawingArea drawing_area;

    GwyContainer * container;       /* Container with data */
    GwyDataField * data;            /* Data to be shown */
    GwyDataField * downsampled;     /* Downsampled data for faster rendering */
    GwyPalette   * palette;         /* Color palette of heights (if lights are
                                       off): XXX remove in 2.0 */

    gdouble data_min;               /* minimal z-value of the heights */
    gdouble data_max;               /* maximal z-value od the heights */
    gdouble data_mean;              /* mean z-value od the heights */

    Gwy3DMovement movement_status;  /* What to do, if mouse is moving */

    GLint shape_list_base;          /* Base index of scene display lists */
    GLuint shape_current;           /* Actually shown shape in the scene (full or reduced data) */


    guint reduced_size;             /* Resolution of the surface while rotations etc. */

    GtkAdjustment *rot_x;           /* First angle of ratation of the scene */
    GtkAdjustment *rot_y;           /* Second angle of ratation of the scene */
    GtkAdjustment *view_scale;      /* Actual zoom*/
    GtkAdjustment *deformation_z;   /* Deformation of the z axis within the scene */
    GtkAdjustment *light_z;         /* First angle describing position of light */
    GtkAdjustment *light_y;         /* Second angle describing position of light */
    GLfloat view_scale_max;         /* Maximum zoom of the scene */
    GLfloat view_scale_min;         /* Minimum zoom of the scene */

    gboolean orthogonal_projection; /* Whether use orthographic or perspectine projection */
    gboolean show_axes;             /* Whether show axes wihin the scene */
    gboolean show_labels;           /* Whwther show axes labels, only if axes are shown */
    gboolean enable_lights;         /* Enable lightning */

    GwyGLMaterial * mat_current;    /* Current material (influences the color of the object, lights must be on) */

    gdouble mouse_begin_x;          /* Start x-coordinate of mouse */
    gdouble mouse_begin_y;          /* Start y-coordinate of mouse */

    gboolean timeout;               /* Is running timeot for redrawing in full scale */
    guint timeout_id;               /* Timeout id */

    PangoContext * ft2_context;     /* For text rendering */
    PangoFT2FontMap *ft2_font_map;  /* Font map for text rendering */
    GwySIUnit    * si_unit;         /* [m] for axis labels */
    Gwy3DLabels* labels;            /* labels text, displacement etc */

    gboolean b_reserved1;           /* resreved for thread creating of display-lists */
    gboolean b_reserved2;
    gboolean b_reserved3;
    gboolean b_reserved4;

    gint     i_reserved1;           /* reserved for axis-labels display-list base  */
    gint     i_reserved2;

    GwyGradient *gradient;
    gpointer p_reserved2;           /* reserved for future use   */
    gpointer p_reserved3;
    gpointer p_reserved4;
};

struct _Gwy3DViewClass {
    GtkDrawingAreaClass parent_class;

    gpointer reserved1;             /* reserved for future use (signals) */
    gpointer reserved2;
    gpointer reserved3;
    gpointer reserved4;
};

GtkWidget*       gwy_3d_view_new               (GwyContainer * data);
GType            gwy_3d_view_get_type          (void) G_GNUC_CONST;

void             gwy_3d_view_update            (Gwy3DView *gwy3dview);

#ifndef GWY_DISABLE_DEPRECATED
GwyPalette*      gwy_3d_view_get_palette       (Gwy3DView *gwy3dview);
void             gwy_3d_view_set_palette       (Gwy3DView *gwy3dview,
                                                GwyPalette *palette);
#endif

const gchar*     gwy_3d_view_get_gradient      (Gwy3DView *gwy3dview);
void             gwy_3d_view_set_gradient      (Gwy3DView *gwy3dview,
                                                const gchar *gradient);

Gwy3DMovement    gwy_3d_view_get_status        (Gwy3DView * gwy3dview);
void             gwy_3d_view_set_status        (Gwy3DView * gwy3dview,
                                                Gwy3DMovement mv);

gboolean         gwy_3d_view_get_orthographic  (Gwy3DView *gwy3dview);
void             gwy_3d_view_set_orthographic  (Gwy3DView *gwy3dview,
                                                gboolean  orthographic);
gboolean         gwy_3d_view_get_show_axes     (Gwy3DView *gwy3dview);
void             gwy_3d_view_set_show_axes     (Gwy3DView *gwy3dview,
                                                gboolean  show_axes);
gboolean         gwy_3d_view_get_show_labels   (Gwy3DView *gwy3dview);
void             gwy_3d_view_set_show_labels   (Gwy3DView *gwy3dview,
                                                gboolean  show_labels);

gboolean         gwy_3d_view_get_use_lights    (Gwy3DView *gwy3dview);
void             gwy_3d_view_set_use_lights    (Gwy3DView *gwy3dview,
                                                gboolean  use_lights);

guint            gwy_3d_view_get_reduced_size  (Gwy3DView *gwy3dview);
void             gwy_3d_view_set_reduced_size  (Gwy3DView *gwy3dview,
                                                guint  reduced_size);

GwyGLMaterial*   gwy_3d_view_get_material      (Gwy3DView *gwy3dview);
void             gwy_3d_view_set_material      (Gwy3DView *gwy3dview,
                                                GwyGLMaterial *material);

GdkPixbuf*       gwy_3d_view_get_pixbuf        (Gwy3DView *gwy3dview,
                                                guint xres,
                                                guint yres);

GwyContainer*    gwy_3d_view_get_data          (Gwy3DView *gwy3dview);

void             gwy_3d_view_reset_view        (Gwy3DView *gwy3dview);

GtkAdjustment*   gwy_3d_view_get_rot_x_adjustment          (Gwy3DView *gwy3dview);
GtkAdjustment*   gwy_3d_view_get_rot_y_adjustment          (Gwy3DView *gwy3dview);
GtkAdjustment*   gwy_3d_view_get_view_scale_adjustment     (Gwy3DView *gwy3dview);
GtkAdjustment*   gwy_3d_view_get_z_deformation_adjustment  (Gwy3DView *gwy3dview);
GtkAdjustment*   gwy_3d_view_get_light_z_adjustment        (Gwy3DView *gwy3dview);
GtkAdjustment*   gwy_3d_view_get_light_y_adjustment        (Gwy3DView *gwy3dview);

gdouble          gwy_3d_view_get_max_view_scale(Gwy3DView *gwy3dview);
gdouble          gwy_3d_view_get_min_view_scale(Gwy3DView *gwy3dview);
gboolean         gwy_3d_view_set_max_view_scale(Gwy3DView *gwy3dview,
                                                gdouble new_max_scale);
gboolean         gwy_3d_view_set_min_view_scale(Gwy3DView *gwy3dview,
                                                gdouble new_min_scale);
Gwy3DLabelDescription * gwy_3d_view_get_label_description(Gwy3DView * gwy3dview,
                                                        Gwy3DLabelName label_name);

G_END_DECLS

#endif  /* __GWY_GWY3DVIEW_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
