/*
 *  @(#) $Id$
 *  Copyright (C) 2003,2005 David Necas (Yeti), Petr Klapetek.
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

#include "config.h"
#include <string.h>
#include <stdlib.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <gdk/gdkevents.h>
#include <glib-object.h>

#ifdef HAVE_GTKGLEXT
#include <gtk/gtkgl.h>
#endif

#ifdef G_OS_WIN32
#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>
#endif

#ifdef HAVE_GTKGLEXT
#include <GL/gl.h>
#endif

#include <pango/pangoft2.h>

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwydebugobjects.h>
#include <libprocess/stats.h>
#include <libgwydgets/gwydgets.h>

#define DEG_2_RAD (G_PI / 180.0)
#define RAD_2_DEG (180.0 / G_PI)

#define BITS_PER_SAMPLE 8

#define  GWY_3D_ORTHO_CORRECTION   2.0
#define  GWY_3D_Z_DEFORMATION     1.01
#define  GWY_3D_Z_TRANSFORMATION  0.5
#define  GWY_3D_Z_DISPLACEMENT    -0.2

#define GWY_3D_TIMEOUT_DELAY      1000

#define connect_swapped_after(obj, signal, cb, data) \
    g_signal_connect_object(obj, signal, G_CALLBACK(cb), data, \
                            G_CONNECT_SWAPPED | G_CONNECT_AFTER);

enum {
    GWY_3D_VIEW_DEFAULT_SIZE_X = 260,
    GWY_3D_VIEW_DEFAULT_SIZE_Y = 260
};

enum {
    GWY_3D_SHAPE_AFM = 0,
    GWY_3D_SHAPE_REDUCED = 1,
    GWY_3D_N_LISTS
};

/* Changed components */
enum {
    GWY_3D_GRADIENT   = 1 << 0,
    GWY_3D_MATERIAL   = 1 << 1,
    GWY_3D_DATA_FIELD = 1 << 2,
    GWY_3D_MASK       = 0x07
};

enum {
    PROP_0,
    PROP_MOVEMENT,
    PROP_PROJECTION,
    PROP_SHOW_AXES,
    PROP_SHOW_LABELS,
    PROP_VISUALIZATION,
    PROP_REDUCED_SIZE,
};

#ifdef HAVE_GTKGLEXT
typedef struct {
    GLfloat x, y, z;
} Gwy3DVector;

typedef struct {
    guint base;
    guint size;
    guint64 pool;
} Gwy3DListPool;

static void     gwy_3d_view_destroy              (GtkObject *object);
static void     gwy_3d_view_finalize             (GObject *object);
static void     gwy_3d_view_set_property         (GObject *object,
                                                  guint prop_id,
                                                  const GValue *value,
                                                  GParamSpec *pspec);
static void     gwy_3d_view_get_property         (GObject*object,
                                                  guint prop_id,
                                                  GValue *value,
                                                  GParamSpec *pspec);
static void     gwy_3d_view_realize              (GtkWidget *widget);
static void     gwy_3d_view_unrealize            (GtkWidget *widget);
static GtkAdjustment* gwy_3d_view_create_adjustment(Gwy3DView *gwy3dview,
                                                    const gchar *key,
                                                    gdouble value,
                                                    gdouble lower,
                                                    gdouble upper,
                                                    gdouble step,
                                                    gdouble page);
static void     gwy_3d_view_container_connect    (Gwy3DView *gwy3dview,
                                                  const gchar *data_key_string,
                                                  gulong *id,
                                                  GCallback callback);
static void     gwy_3d_view_data_item_changed    (Gwy3DView *gwy3dview);
static void     gwy_3d_view_data_field_connect   (Gwy3DView *gwy3dview);
static void     gwy_3d_view_data_field_disconnect(Gwy3DView *gwy3dview);
static void     gwy_3d_view_data_field_changed   (Gwy3DView *gwy3dview);
static void     gwy_3d_view_gradient_item_changed(Gwy3DView *gwy3dview);
static void     gwy_3d_view_gradient_connect     (Gwy3DView *gwy3dview);
static void     gwy_3d_view_gradient_disconnect  (Gwy3DView *gwy3dview);
static void     gwy_3d_view_gradient_changed     (Gwy3DView *gwy3dview);
static void     gwy_3d_view_material_item_changed(Gwy3DView *gwy3dview);
static void     gwy_3d_view_material_connect     (Gwy3DView *gwy3dview);
static void     gwy_3d_view_material_disconnect  (Gwy3DView *gwy3dview);
static void     gwy_3d_view_material_changed     (Gwy3DView *gwy3dview);
static void     gwy_3d_view_update_lists         (Gwy3DView *gwy3dview);
static void     gwy_3d_view_downsample_data      (Gwy3DView *gwy3dview);
static void     gwy_3d_view_realize_gl           (Gwy3DView *widget);
static void     gwy_3d_view_size_request         (GtkWidget *widget,
                                                  GtkRequisition *requisition);
static void     gwy_3d_view_size_allocate        (GtkWidget *widget,
                                                  GtkAllocation *allocation);
static gboolean gwy_3d_view_expose               (GtkWidget *widget,
                                                  GdkEventExpose *event);
static gboolean gwy_3d_view_configure            (GtkWidget *widget,
                                                  GdkEventConfigure *event);
static void     gwy_3d_view_send_configure       (Gwy3DView *gwy3dview);
static gboolean gwy_3d_view_button_press         (GtkWidget *widget,
                                                  GdkEventButton *event);
static gboolean gwy_3d_view_button_release       (GtkWidget *widget,
                                                  GdkEventButton *event);
static Gwy3DVector* gwy_3d_make_normals          (GwyDataField *dfield,
                                                  Gwy3DVector *normals);
static gboolean gwy_3d_view_motion_notify        (GtkWidget *widget,
                                                  GdkEventMotion *event);
static void     gwy_3d_make_list                 (Gwy3DView *gwy3D,
                                                  GwyDataField *dfield,
                                                  gint shape);
static void     gwy_3d_draw_axes                 (Gwy3DView *gwy3dview);
static void     gwy_3d_draw_light_position       (Gwy3DView *gwy3dview);
static void     gwy_3d_set_projection            (Gwy3DView *gwy3dview);
static void     gwy_3d_view_update_labels        (Gwy3DView *gwy3dview);
static void     gwy_3d_adjustment_value_changed  (GtkAdjustment *adjustment,
                                                  Gwy3DView *gwy3dview);
static void     gwy_3d_label_changed             (Gwy3DView *gwy3dview);
static void     gwy_3d_timeout_start             (Gwy3DView *gwy3dview,
                                                  gboolean immediate,
                                                  gboolean invalidate_now);
static gboolean gwy_3d_timeout_func              (gpointer user_data);
static void     gwy_3d_pango_ft2_render_layout   (PangoLayout *layout);
static void     gwy_3d_print_text                (Gwy3DView *gwy3dview,
                                                  Gwy3DViewLabel id,
                                                  GLfloat raster_x,
                                                  GLfloat raster_y,
                                                  GLfloat raster_z,
                                                  guint size,
                                                  gint vjustify,
                                                  gint hjustify);
static void     gwy_3d_view_class_make_list_pool (Gwy3DListPool *pool);
static void     gwy_3d_view_assign_lists         (Gwy3DView *gwy3dview);
static void     gwy_3d_view_release_lists        (Gwy3DView *gwy3dview);

static GQuark container_key_quark = 0;

/* Must match Gwy3DViewLabel */
static const struct {
    const gchar *key;
    const gchar *default_text;
}
labels[] = {
    { "/0/3d/label/x",    "x: $x" },
    { "/0/3d/label/y",    "y: $y" },
    { "/0/3d/label/min",  "$min"  },
    { "/0/3d/label/max",  "$max"  },
};
#endif /* HAVE_GTKGLEXT */

G_DEFINE_TYPE(Gwy3DView, gwy_3d_view, GTK_TYPE_WIDGET)

#ifdef HAVE_GTKGLEXT
static void
gwy_3d_view_class_init(Gwy3DViewClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkObjectClass *object_class;
    GtkWidgetClass *widget_class;

    gwy_debug(" ");

    object_class = (GtkObjectClass*)klass;
    widget_class = (GtkWidgetClass*)klass;

    gobject_class->finalize = gwy_3d_view_finalize;
    gobject_class->set_property = gwy_3d_view_set_property;
    gobject_class->get_property = gwy_3d_view_get_property;

    object_class->destroy = gwy_3d_view_destroy;

    widget_class->realize = gwy_3d_view_realize;
    widget_class->unrealize = gwy_3d_view_unrealize;
    widget_class->expose_event = gwy_3d_view_expose;
    widget_class->configure_event = gwy_3d_view_configure;
    widget_class->size_request = gwy_3d_view_size_request;
    widget_class->size_allocate = gwy_3d_view_size_allocate;
    widget_class->button_press_event = gwy_3d_view_button_press;
    widget_class->button_release_event = gwy_3d_view_button_release;
    widget_class->motion_notify_event = gwy_3d_view_motion_notify;

    klass->list_pool = g_new0(Gwy3DListPool, 1);

    /**
     * Gwy3DView:movement-type:
     *
     * The :movement-type property represents type of action on user pointer
     * drag.
     **/
    g_object_class_install_property
        (gobject_class,
         PROP_MOVEMENT,
         g_param_spec_enum("movement-type",
                           "Movement type",
                           "What quantity is changed when uses moves pointer",
                           GWY_TYPE_3D_MOVEMENT,
                           GWY_3D_MOVEMENT_NONE,
                           G_PARAM_READWRITE));

    /**
     * Gwy3DView:projection:
     *
     * The :projection property represents type of 3D to 2D projection.
     **/
    g_object_class_install_property
        (gobject_class,
         PROP_PROJECTION,
         g_param_spec_enum("projection",
                           "Projection type",
                           "The type of 3D to 2D projection",
                           GWY_TYPE_3D_PROJECTION,
                           GWY_3D_PROJECTION_ORTHOGRAPHIC,
                           G_PARAM_READWRITE));

    /**
     * Gwy3DView:show-axes:
     *
     * The :show-axes property determines whether axes around data are shown.
     **/
    g_object_class_install_property
        (gobject_class,
         PROP_SHOW_AXES,
         g_param_spec_boolean("show-axes",
                              "Show axes",
                              "Whether to show axies around data",
                              TRUE, G_PARAM_READWRITE));

    /**
     * Gwy3DView:show-labels:
     *
     * The :show-labels property determines whether axis labels are shown.
     * Note when axes themselves are not shown, neither are labels.
     **/
    g_object_class_install_property
        (gobject_class,
         PROP_SHOW_LABELS,
         g_param_spec_boolean("show-labels",
                              "Show labels",
                              "Whether to show axis labels",
                              TRUE, G_PARAM_READWRITE));

    g_object_class_install_property
        (gobject_class,
         PROP_REDUCED_SIZE,
         g_param_spec_uint("reduced-size",
                           "Reduced size",
                           "The size of downsampled data in quick view",
                           4, 4096, 100, G_PARAM_READWRITE));

    g_object_class_install_property
        (gobject_class,
         PROP_VISUALIZATION,
         g_param_spec_enum("visualization",
                           "Visualization type",
                           "The type of data visualization",
                           GWY_TYPE_3D_VISUALIZATION,
                           GWY_3D_VISUALIZATION_GRADIENT,
                           G_PARAM_READWRITE));

}

static void
gwy_3d_view_init(Gwy3DView *gwy3dview)
{
    gwy3dview->reduced_size          = 100;

    gwy3dview->view_scale_max        = 3.0f;
    gwy3dview->view_scale_min        = 0.5f;
    gwy3dview->movement              = GWY_3D_MOVEMENT_NONE;
    gwy3dview->show_axes             = TRUE;
    gwy3dview->show_labels           = TRUE;

    gwy3dview->variables = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                 NULL, g_free);
}

static void
gwy_3d_view_destroy(GtkObject *object)
{
    Gwy3DView *gwy3dview;
    guint i;

    gwy3dview = GWY_3D_VIEW(object);

    gwy_signal_handler_disconnect(gwy3dview->data, gwy3dview->data_item_id);
    gwy_signal_handler_disconnect(gwy3dview->data, gwy3dview->gradient_item_id);
    gwy_signal_handler_disconnect(gwy3dview->data, gwy3dview->material_item_id);

    gwy_3d_view_data_field_disconnect(gwy3dview);
    gwy_3d_view_gradient_disconnect(gwy3dview);
    gwy_3d_view_material_disconnect(gwy3dview);

    if (gwy3dview->shape_list_base > 0)
        gwy_3d_view_release_lists(gwy3dview);

    if (gwy3dview->labels) {
        for (i = 0; i < G_N_ELEMENTS(labels); i++) {
            g_signal_handler_disconnect(gwy3dview->labels[i],
                                        gwy3dview->label_signal_ids[2*i]);
            g_signal_handler_disconnect(gwy3dview->labels[i],
                                        gwy3dview->label_signal_ids[2*i + 1]);
            gwy_object_unref(gwy3dview->labels[i]);
        }
        g_free(gwy3dview->labels);
        g_free(gwy3dview->label_signal_ids);
        gwy3dview->labels = NULL;
        gwy3dview->label_signal_ids = NULL;
    }

    gwy_object_unref(gwy3dview->data_field);
    gwy_object_unref(gwy3dview->downsampled);
    gwy_object_unref(gwy3dview->data);
    gwy_object_unref(gwy3dview->rot_x);
    gwy_object_unref(gwy3dview->rot_y);
    gwy_object_unref(gwy3dview->view_scale);
    gwy_object_unref(gwy3dview->deformation_z);
    gwy_object_unref(gwy3dview->light_z);
    gwy_object_unref(gwy3dview->light_y);

    GTK_OBJECT_CLASS(gwy_3d_view_parent_class)->destroy(object);
}

static void
gwy_3d_view_finalize(GObject *object)
{
    Gwy3DView *gwy3dview;

    gwy3dview = GWY_3D_VIEW(object);

    g_hash_table_destroy(gwy3dview->variables);

    G_OBJECT_CLASS(gwy_3d_view_parent_class)->finalize(object);
}

static void
gwy_3d_view_set_property(GObject *object,
                         guint prop_id,
                         const GValue *value,
                         GParamSpec *pspec)
{
    Gwy3DView *view = GWY_3D_VIEW(object);

    switch (prop_id) {
        case PROP_MOVEMENT:
        gwy_3d_view_set_movement_type(view, g_value_get_enum(value));
        break;

        case PROP_PROJECTION:
        gwy_3d_view_set_projection(view, g_value_get_enum(value));
        break;

        case PROP_SHOW_AXES:
        gwy_3d_view_set_show_axes(view, g_value_get_boolean(value));
        break;

        case PROP_SHOW_LABELS:
        gwy_3d_view_set_show_labels(view, g_value_get_boolean(value));
        break;

        case PROP_REDUCED_SIZE:
        gwy_3d_view_set_reduced_size(view, g_value_get_uint(value));
        break;

        case PROP_VISUALIZATION:
        gwy_3d_view_set_visualization(view, g_value_get_enum(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_3d_view_get_property(GObject*object,
                          guint prop_id,
                          GValue *value,
                          GParamSpec *pspec)
{
    Gwy3DView *view = GWY_3D_VIEW(object);

    switch (prop_id) {
        case PROP_MOVEMENT:
        g_value_set_enum(value, view->movement);
        break;

        case PROP_PROJECTION:
        g_value_set_enum(value, view->projection);
        break;

        case PROP_SHOW_AXES:
        g_value_set_boolean(value, view->show_axes);
        break;

        case PROP_SHOW_LABELS:
        g_value_set_boolean(value, view->show_labels);
        break;

        case PROP_REDUCED_SIZE:
        g_value_set_boolean(value, view->reduced_size);
        break;

        case PROP_VISUALIZATION:
        g_value_set_enum(value, view->visual);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_3d_view_unrealize(GtkWidget *widget)
{
    Gwy3DView *gwy3dview;

    gwy3dview = GWY_3D_VIEW(widget);

    gwy_3d_view_release_lists(gwy3dview);
    g_object_unref(gwy3dview->ft2_context);
    g_object_unref(gwy3dview->ft2_font_map);

    if (GTK_WIDGET_CLASS(gwy_3d_view_parent_class)->unrealize)
        GTK_WIDGET_CLASS(gwy_3d_view_parent_class)->unrealize(widget);
}

/**
 * gwy_3d_view_new:
 * @data: A #GwyContainer containing the data to display.
 *
 * Creates a new threedimensional OpenGL display of @data.
 *
 * The widget is initialized from container @data.
 *
 * Returns: The new OpenGL 3D widget as a #GtkWidget.
 **/
GtkWidget*
gwy_3d_view_new(GwyContainer *data)
{
    GdkGLConfig *glconfig;
    GtkWidget *widget;
    Gwy3DView *gwy3dview;
    guint i;

    g_return_val_if_fail(GWY_IS_CONTAINER(data), NULL);
    glconfig = gwy_widgets_get_gl_config();
    g_return_val_if_fail(glconfig, NULL);

    g_object_ref(data);

    gwy3dview = (Gwy3DView*)g_object_new(GWY_TYPE_3D_VIEW, NULL);
    widget = GTK_WIDGET(gwy3dview);

    gwy3dview->data = data;

    gwy3dview->rot_x
        = gwy_3d_view_create_adjustment(gwy3dview, "/0/3d/rot_x",
                                        45.0, -G_MAXDOUBLE, G_MAXDOUBLE,
                                         5.0, 30.0);
    gwy3dview->rot_y
        = gwy_3d_view_create_adjustment(gwy3dview, "/0/3d/rot_y",
                                        -45.0, -270.0, 180.0,
                                        5.0, 15.0);
    gwy3dview->view_scale
        = gwy_3d_view_create_adjustment(gwy3dview, "/0/3d/view_scale",
                                        1.0, 0.0, G_MAXDOUBLE,
                                        0.05, 0.5);
    gwy3dview->deformation_z
        = gwy_3d_view_create_adjustment(gwy3dview, "/0/3d/deformation_z",
                                        1.0, 0.0, G_MAXDOUBLE,
                                        0.05, 0.5);
    gwy3dview->light_z
        = gwy_3d_view_create_adjustment(gwy3dview, "/0/3d/light_z",
                                        0.0, -G_MAXDOUBLE, G_MAXDOUBLE,
                                        1.0, 15.0);
    gwy3dview->light_y
        = gwy_3d_view_create_adjustment(gwy3dview, "/0/3d/light_y",
                                        0.0, -G_MAXDOUBLE, G_MAXDOUBLE,
                                        1.0, 15.0);

    gwy_3d_view_container_connect
                            (gwy3dview,
                             g_quark_to_string(gwy3dview->data_key),
                             &gwy3dview->data_item_id,
                             G_CALLBACK(gwy_3d_view_data_item_changed));
    gwy_3d_view_data_field_connect(gwy3dview);
    gwy_3d_view_container_connect
                            (gwy3dview,
                             g_quark_to_string(gwy3dview->gradient_key),
                             &gwy3dview->gradient_item_id,
                             G_CALLBACK(gwy_3d_view_gradient_item_changed));
    gwy_3d_view_gradient_connect(gwy3dview);
    gwy_3d_view_container_connect
                            (gwy3dview,
                             g_quark_to_string(gwy3dview->material_key),
                             &gwy3dview->material_item_id,
                             G_CALLBACK(gwy_3d_view_material_item_changed));
    gwy_3d_view_material_connect(gwy3dview);

    gwy_container_gis_int32_by_name(data, "/0/3d/reduced_size",
                                    &gwy3dview->reduced_size);
    gwy_container_gis_double_by_name(data, "/0/3d/view_scale_max",
                                     &gwy3dview->view_scale_max);
    gwy_container_gis_double_by_name(data, "/0/3d/view_scale_min",
                                     &gwy3dview->view_scale_min);
    gwy_container_gis_enum_by_name(data, "/0/3d/projection",
                                   &gwy3dview->projection);
    gwy_container_gis_enum_by_name(data, "/0/3d/visualization",
                                   &gwy3dview->visual);
    gwy_container_gis_boolean_by_name(data, "/0/3d/show_axes",
                                      &gwy3dview->show_axes);
    gwy_container_gis_boolean_by_name(data, "/0/3d/show_labels",
                                      &gwy3dview->show_labels);

    gtk_widget_set_gl_capability(GTK_WIDGET(gwy3dview),
                                 glconfig,
                                 NULL,
                                 TRUE,
                                 GDK_GL_RGBA_TYPE);

    gwy3dview->labels = g_new0(Gwy3DLabel*, G_N_ELEMENTS(labels));
    gwy3dview->label_signal_ids = g_new0(gulong, 2*G_N_ELEMENTS(labels));
    for (i = 0; i < G_N_ELEMENTS(labels); i++) {
        if (gwy_container_gis_object_by_name(data, labels[i].key,
                                             &gwy3dview->labels[i]))
            g_object_ref(gwy3dview->labels[i]);
        else {
            gwy3dview->labels[i] = gwy_3d_label_new(labels[i].default_text);
            gwy_container_set_object_by_name(data, labels[i].key,
                                             gwy3dview->labels[i]);
        }

        gwy3dview->label_signal_ids[2*i]
            = g_signal_connect_swapped(gwy3dview->labels[i], "value-changed",
                                       G_CALLBACK(gwy_3d_label_changed),
                                       gwy3dview);
        gwy3dview->label_signal_ids[2*i + 1]
            = g_signal_connect_swapped(gwy3dview->labels[i], "notify",
                                       G_CALLBACK(gwy_3d_label_changed),
                                       gwy3dview);
    }

    return widget;
}

static void
gwy_3d_view_update_labels(Gwy3DView *gwy3dview)
{
    GwySIValueFormat *format;
    GwySIUnit *unit;
    gdouble xreal, yreal, data_min, data_max, range, maximum;
    gchar buffer[32], *s;

    xreal = gwy_data_field_get_xreal(gwy3dview->data_field);
    yreal = gwy_data_field_get_yreal(gwy3dview->data_field);
    gwy_data_field_get_min_max(gwy3dview->data_field, &data_min, &data_max);
    range = fabs(data_max - data_min);
    maximum = MAX(fabs(data_min), fabs(data_max));

    /* $x */
    unit = gwy_data_field_get_si_unit_xy(gwy3dview->data_field);
    format = gwy_si_unit_get_format_with_resolution(unit,
                                                    GWY_SI_UNIT_FORMAT_VFMARKUP,
                                                    xreal, xreal/12, NULL);
    g_snprintf(buffer, sizeof(buffer), "%.*f %s",
               format->precision, xreal/format->magnitude, format->units);
    s = g_hash_table_lookup(gwy3dview->variables, "x");
    if (!s || strcmp(s, buffer))
        g_hash_table_insert(gwy3dview->variables, "x", g_strdup(buffer));

    /* $y */
    gwy_si_unit_get_format_with_resolution(unit, GWY_SI_UNIT_FORMAT_VFMARKUP,
                                           yreal, yreal/12, format);
    g_snprintf(buffer, sizeof(buffer), "%.*f %s",
               format->precision, yreal/format->magnitude, format->units);
    s = g_hash_table_lookup(gwy3dview->variables, "y");
    if (!s || strcmp(s, buffer))
        g_hash_table_insert(gwy3dview->variables, "y", g_strdup(buffer));

    /* $max */
    unit = gwy_data_field_get_si_unit_z(gwy3dview->data_field);
    gwy_si_unit_get_format_with_resolution(unit, GWY_SI_UNIT_FORMAT_VFMARKUP,
                                           maximum, range/12, format);
    g_snprintf(buffer, sizeof(buffer), "%.*f %s",
               format->precision, data_max/format->magnitude, format->units);
    s = g_hash_table_lookup(gwy3dview->variables, "max");
    if (!s || strcmp(s, buffer))
        g_hash_table_insert(gwy3dview->variables, "max", g_strdup(buffer));

    /* $min */
    g_snprintf(buffer, sizeof(buffer), "%.*f %s",
               format->precision, data_min/format->magnitude, format->units);
    s = g_hash_table_lookup(gwy3dview->variables, "min");
    if (!s || strcmp(s, buffer))
        g_hash_table_insert(gwy3dview->variables, "min", g_strdup(buffer));

    gwy_si_unit_value_format_free(format);
}

static GtkAdjustment*
gwy_3d_view_create_adjustment(Gwy3DView *gwy3dview,
                              const gchar *key,
                              gdouble value,
                              gdouble lower,
                              gdouble upper,
                              gdouble step,
                              gdouble page)
{
    GtkObject *adj;
    GQuark quark;

    if (!container_key_quark)
        container_key_quark = g_quark_from_string("gwy3dview-container-key");

    quark = g_quark_from_string(key);
    gwy_container_gis_double(gwy3dview->data, quark, &value);
    adj = gtk_adjustment_new(value, lower, upper, step, page, 0.0);
    g_object_ref(adj);
    gtk_object_sink(adj);
    g_object_set_qdata(G_OBJECT(adj), container_key_quark,
                       GUINT_TO_POINTER(quark));
    g_signal_connect(adj, "value-changed",
                     G_CALLBACK(gwy_3d_adjustment_value_changed), gwy3dview);

    return (GtkAdjustment*)adj;
}

static void
gwy_3d_view_container_connect(Gwy3DView *gwy3dview,
                              const gchar *data_key_string,
                              gulong *id,
                              GCallback callback)
{
    gchar *detailed_signal;

    if (!data_key_string || !gwy3dview->data) {
        *id = 0;
        return;
    }
    detailed_signal = g_newa(gchar, sizeof("item-changed::")
                                    + strlen(data_key_string));
    g_stpcpy(g_stpcpy(detailed_signal, "item-changed::"), data_key_string);
    *id = connect_swapped_after(gwy3dview->data, detailed_signal,
                                callback, gwy3dview);
}

void
gwy_3d_view_set_data_key(Gwy3DView *gwy3dview,
                         const gchar *key)
{
    GQuark quark;

    g_return_if_fail(GWY_IS_3D_VIEW(gwy3dview));

    quark = key ? g_quark_from_string(key) : 0;
    if (gwy3dview->data_key == quark)
        return;

    gwy_signal_handler_disconnect(gwy3dview->data, gwy3dview->data_item_id);
    gwy_3d_view_data_field_disconnect(gwy3dview);
    gwy3dview->data_key = quark;
    gwy_3d_view_data_field_connect(gwy3dview);
    gwy_3d_view_container_connect(gwy3dview, key,
                                  &gwy3dview->data_item_id,
                                  G_CALLBACK(gwy_3d_view_data_item_changed));
    /*g_object_notify(G_OBJECT(gwy3dview), "data-key");*/
    gwy_3d_view_data_field_changed(gwy3dview);
}

const gchar*
gwy_3d_view_get_data_key(Gwy3DView *gwy3dview)
{
    g_return_val_if_fail(GWY_IS_LAYER_BASIC(gwy3dview), NULL);
    return g_quark_to_string(gwy3dview->data_key);
}

static void
gwy_3d_view_data_item_changed(Gwy3DView *gwy3dview)
{
    gwy_3d_view_data_field_disconnect(gwy3dview);
    gwy_3d_view_data_field_connect(gwy3dview);
    gwy_3d_view_data_field_changed(gwy3dview);
}

static void
gwy_3d_view_data_field_connect(Gwy3DView *gwy3dview)
{
    g_return_if_fail(!gwy3dview->data_field);
    if (!gwy3dview->data_key)
        return;

    if (!gwy_container_gis_object(gwy3dview->data, gwy3dview->data_key,
                                  &gwy3dview->data_field))
        return;

    g_object_ref(gwy3dview->data_field);
    gwy3dview->data_id
        = g_signal_connect_swapped(gwy3dview->data_field,
                                   "data-changed",
                                   G_CALLBACK(gwy_3d_view_data_field_changed),
                                   gwy3dview);
}

static void
gwy_3d_view_data_field_disconnect(Gwy3DView *gwy3dview)
{
    gwy_signal_handler_disconnect(gwy3dview->data_field, gwy3dview->data_id);
    gwy_object_unref(gwy3dview->data_field);
}

static void
gwy_3d_view_data_field_changed(Gwy3DView *gwy3dview)
{
    gwy3dview->changed |= GWY_3D_DATA_FIELD;
    gwy_3d_view_update_labels(gwy3dview);
    gwy_3d_view_update_lists(gwy3dview);
}

/**
 * gwy_3d_view_get_gradient_key:
 * @gwy3dview: A 3D data view widget.
 *
 * Gets key identifying color gradient.
 *
 * Returns: The string key, or %NULL if it isn't set.
 **/
const gchar*
gwy_3d_view_get_gradient_key(Gwy3DView *gwy3dview)
{
    g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), NULL);
    return g_quark_to_string(gwy3dview->gradient_key);
}

/**
 * gwy_3d_view_set_gradient_key:
 * @gwy3dview: A 3D data view widget.
 * @key: Container string key identifying the color gradient to use for
 *       gradient visualization mode.
 *
 * Sets the color gradient to use to visualize data in a 3D view.
 **/
void
gwy_3d_view_set_gradient_key(Gwy3DView *gwy3dview,
                             const gchar *key)
{
    GQuark quark;

    g_return_if_fail(GWY_IS_3D_VIEW(gwy3dview));

    quark = key ? g_quark_from_string(key) : 0;
    if (gwy3dview->gradient_key == quark)
        return;

    gwy_signal_handler_disconnect(gwy3dview->data, gwy3dview->gradient_item_id);
    gwy_3d_view_gradient_disconnect(gwy3dview);
    gwy3dview->gradient_key = quark;
    gwy_3d_view_gradient_connect(gwy3dview);
    gwy_3d_view_container_connect
                            (gwy3dview, key,
                             &gwy3dview->gradient_item_id,
                             G_CALLBACK(gwy_3d_view_gradient_item_changed));
    /*g_object_notify(G_OBJECT(gwy3dview), "gradient-key");*/
    gwy_3d_view_gradient_changed(gwy3dview);
}

static void
gwy_3d_view_gradient_item_changed(Gwy3DView *gwy3dview)
{
    gwy_3d_view_gradient_disconnect(gwy3dview);
    gwy_3d_view_gradient_connect(gwy3dview);
    gwy_3d_view_gradient_changed(gwy3dview);
}

static void
gwy_3d_view_gradient_connect(Gwy3DView *gwy3dview)
{
    const guchar *s = NULL;

    g_return_if_fail(!gwy3dview->gradient);
    if (gwy3dview->gradient_key)
        gwy_container_gis_string(gwy3dview->data, gwy3dview->gradient_key, &s);
    gwy3dview->gradient = gwy_gradients_get_gradient(s);
    gwy_resource_use(GWY_RESOURCE(gwy3dview->gradient));
    gwy3dview->gradient_id
        = g_signal_connect_swapped(gwy3dview->gradient, "data-changed",
                                   G_CALLBACK(gwy_3d_view_gradient_changed),
                                   gwy3dview);
}

static void
gwy_3d_view_gradient_disconnect(Gwy3DView *gwy3dview)
{
    if (!gwy3dview->gradient)
        return;

    gwy_signal_handler_disconnect(gwy3dview->gradient, gwy3dview->gradient_id);
    gwy_resource_release(GWY_RESOURCE(gwy3dview->gradient));
    gwy3dview->gradient = NULL;
}

static void
gwy_3d_view_gradient_changed(Gwy3DView *gwy3dview)
{
    gwy3dview->changed |= GWY_3D_GRADIENT;
    if (gwy3dview->visual == GWY_3D_VISUALIZATION_GRADIENT)
        gwy_3d_view_update_lists(gwy3dview);
}

/**
 * gwy_3d_view_get_material_key:
 * @gwy3dview: A 3D data view widget.
 *
 * Gets key identifying GL material.
 *
 * Returns: The string key, or %NULL if it isn't set.
 **/
const gchar*
gwy_3d_view_get_material_key(Gwy3DView *gwy3dview)
{
    g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), NULL);
    return g_quark_to_string(gwy3dview->material_key);
}

/**
 * gwy_3d_view_set_material_key:
 * @gwy3dview: A 3D data view widget.
 * @key: Container string key identifying the color material to use for
 *       material visualization mode.
 *
 * Sets the GL material to use to visualize data in a 3D view.
 **/
void
gwy_3d_view_set_material_key(Gwy3DView *gwy3dview,
                            const gchar *key)
{
    GQuark quark;

    g_return_if_fail(GWY_IS_3D_VIEW(gwy3dview));

    quark = key ? g_quark_from_string(key) : 0;
    if (gwy3dview->material_key == quark)
        return;

    gwy_signal_handler_disconnect(gwy3dview->data, gwy3dview->material_item_id);
    gwy_3d_view_material_disconnect(gwy3dview);
    gwy3dview->material_key = quark;
    gwy_3d_view_material_connect(gwy3dview);
    gwy_3d_view_container_connect
                            (gwy3dview, key,
                             &gwy3dview->material_item_id,
                             G_CALLBACK(gwy_3d_view_material_item_changed));
    /*g_object_notify(G_OBJECT(gwy3dview), "material-key");*/
    gwy_3d_view_material_changed(gwy3dview);
}

static void
gwy_3d_view_material_item_changed(Gwy3DView *gwy3dview)
{
    gwy_3d_view_material_disconnect(gwy3dview);
    gwy_3d_view_material_connect(gwy3dview);
    gwy_3d_view_material_changed(gwy3dview);
}

static void
gwy_3d_view_material_connect(Gwy3DView *gwy3dview)
{
    const guchar *s = NULL;

    g_return_if_fail(!gwy3dview->material);
    if (gwy3dview->material_key)
        gwy_container_gis_string(gwy3dview->data, gwy3dview->material_key, &s);
    gwy3dview->material = gwy_gl_materials_get_gl_material(s);
    gwy_resource_use(GWY_RESOURCE(gwy3dview->material));
    gwy3dview->material_id
        = g_signal_connect_swapped(gwy3dview->material, "data-changed",
                                   G_CALLBACK(gwy_3d_view_material_changed),
                                   gwy3dview);
}

static void
gwy_3d_view_material_disconnect(Gwy3DView *gwy3dview)
{
    if (!gwy3dview->material)
        return;

    gwy_signal_handler_disconnect(gwy3dview->material, gwy3dview->material_id);
    gwy_resource_release(GWY_RESOURCE(gwy3dview->material));
    gwy3dview->material = NULL;
}

static void
gwy_3d_view_material_changed(Gwy3DView *gwy3dview)
{
    gwy3dview->changed |= GWY_3D_MATERIAL;
    if (gwy3dview->visual == GWY_3D_VISUALIZATION_LIGHTING)
        gwy_3d_view_update_lists(gwy3dview);
}

static void
gwy_3d_view_update_lists(Gwy3DView *gwy3dview)
{
    gwy_3d_view_downsample_data(gwy3dview);

    if (!GTK_WIDGET_REALIZED(gwy3dview))
        return;

    gwy_3d_make_list(gwy3dview, gwy3dview->downsampled, GWY_3D_SHAPE_REDUCED);
    gwy_3d_make_list(gwy3dview, gwy3dview->data_field, GWY_3D_SHAPE_AFM);
    gwy_3d_timeout_start(gwy3dview, FALSE, TRUE);
}

/**
 * gwy_3d_view_get_movement_type:
 * @gwy3dview: A 3D data view widget.
 *
 * Returns a movement type describing actual type of response on
 * the mouse motion event.
 *
 * Returns: actual type of response on the mouse motion event
 **/
Gwy3DMovement
gwy_3d_view_get_movement_type(Gwy3DView *gwy3dview)
{
    g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), GWY_3D_MOVEMENT_NONE);
    return gwy3dview->movement;
}

/**
 * gwy_3d_view_set_movement_type:
 * @gwy3dview: A 3D data view widget.
 * @movement: A new type of response on the mouse motion event.
 *
 * Sets the type of widget response on the mouse motion event.
 **/
void
gwy_3d_view_set_movement_type(Gwy3DView *gwy3dview,
                              Gwy3DMovement movement)
{
    g_return_if_fail(GWY_IS_3D_VIEW(gwy3dview));
    g_return_if_fail(movement <= GWY_3D_MOVEMENT_LIGHT);

    gwy3dview->movement = movement;
}

/**
 * gwy_3d_view_get_projection:
 * @gwy3dview: A 3D data view widget.
 *
 * Gets projection a 3D data view uses.
 *
 * Returns: Projection type used by @gwy3dview.
 **/
Gwy3DProjection
gwy_3d_view_get_projection(Gwy3DView *gwy3dview)
{
    g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), FALSE);
    return gwy3dview->projection;
}

/**
 * gwy_3d_view_set_projection:
 * @gwy3dview: A 3D data view widget.
 * @projection: Proejction type to use.
 *
 * Sets the type of projection of a 3D data to the screen.
 **/
void
gwy_3d_view_set_projection(Gwy3DView *gwy3dview,
                           Gwy3DProjection projection)
{
    g_return_if_fail(GWY_IS_3D_VIEW(gwy3dview));
    g_return_if_fail((gint)projection >= GWY_3D_PROJECTION_ORTHOGRAPHIC
                     && (gint)projection <= GWY_3D_PROJECTION_PERSPECTIVE);

    if (projection == gwy3dview->projection)
        return;
    gwy3dview->projection = projection;
    gwy_container_set_enum_by_name(gwy3dview->data, "/0/3d/projection",
                                   projection);

    gwy_3d_timeout_start(gwy3dview, FALSE, TRUE);
}

/**
 * gwy_3d_view_get_show_axes:
 * @gwy3dview: A 3D data view widget.
 *
 * Returns whether the axes are shown within the widget.
 *
 * Returns: visibility of the axes
 **/
gboolean
gwy_3d_view_get_show_axes(Gwy3DView *gwy3dview)
{
    g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), FALSE);
    return gwy3dview->show_axes;
}

/**
 * gwy_3d_view_set_show_axes:
 * @gwy3dview: A 3D data view widget.
 * @show_axes: Show/hide axes
 *
 * Show/hide axes within @gwy3dview.
 **/
void
gwy_3d_view_set_show_axes(Gwy3DView *gwy3dview,
                          gboolean show_axes)
{
    g_return_if_fail(GWY_IS_3D_VIEW(gwy3dview));

    if (show_axes == gwy3dview->show_axes)
        return;
    gwy3dview->show_axes = show_axes;
    gwy_container_set_boolean_by_name(gwy3dview->data,
                                      "/0/3d/show_axes",
                                      show_axes);

    gwy_3d_timeout_start(gwy3dview, FALSE, TRUE);
}

/**
 * gwy_3d_view_get_show_labels:
 * @gwy3dview: A 3D data view widget.
 *
 * Returns whether the axes labels are shown within the @gwy3dview.
 * The labels are visible only if #show_axes is TRUE.
 *
 * Returns: Whwteher the axes labels are visible.
 **/
gboolean
gwy_3d_view_get_show_labels(Gwy3DView *gwy3dview)
{
    g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), FALSE);
    return gwy3dview->show_labels;
}

/**
 * gwy_3d_view_set_show_labels:
 * @gwy3dview: A 3D data view widget.
 * @show_labels: Show/hide axes labels
 *
 * Show/hide labels of the axes within @gwy3dview.
 * Widget is invalidated if necessary.
 * The labels of the axes are visible only if #show_axes is TRUE.
 **/
void
gwy_3d_view_set_show_labels(Gwy3DView *gwy3dview,
                            gboolean show_labels)
{
     g_return_if_fail(GWY_IS_3D_VIEW(gwy3dview));

     if (show_labels == gwy3dview->show_labels)
         return;
     gwy3dview->show_labels = show_labels;
     gwy_container_set_boolean_by_name(gwy3dview->data, "/0/3d/show_labels",
                                       show_labels);

     gwy_3d_timeout_start(gwy3dview, FALSE, TRUE);
}

/**
 * gwy_3d_view_get_visualization:
 * @gwy3dview: A 3D data view widget.
 *
 * Returns visualization method a 3D view uses.
 *
 * Returns: The visualization type.
 **/
Gwy3DVisualization
gwy_3d_view_get_visualization(Gwy3DView *gwy3dview)
{
    g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), FALSE);

    return gwy3dview->visual;
}

/**
 * gwy_3d_view_set_visualization:
 * @gwy3dview: A 3D data view widget.
 * @visual: Visualization method to use.
 *
 * Sets the visualization type a 3D view should use.
 **/
void
gwy_3d_view_set_visualization(Gwy3DView *gwy3dview,
                              Gwy3DVisualization visual)
{
    g_return_if_fail(GWY_IS_3D_VIEW(gwy3dview));

    if (visual == gwy3dview->visual)
        return;
    gwy3dview->visual = visual;
    gwy_container_set_enum_by_name(gwy3dview->data, "/0/3d/visualization",
                                   visual);

    gwy_3d_timeout_start(gwy3dview, FALSE, TRUE);
}

/**
 * gwy_3d_view_get_reduced_size:
 * @gwy3dview: A 3D data view widget.
 *
 * Returns the size of the downsampled data. The downampled
 * data are used when fast rendering of surface is needed
 * (in the situations like mouse rotations etc.)
 *
 * Returns: The size of the downsampled data.
 **/
guint
gwy_3d_view_get_reduced_size(Gwy3DView *gwy3dview)
{
    g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), FALSE);

    return gwy3dview->reduced_size;
}

/**
 * gwy_3d_view_set_reduced_size:
 * @gwy3dview: A 3D data view widget.
 * @reduced_size: The size of the downsampled data.
 *
 * Sets the size of the downsampled data. In case of the original
 * data are not square, the @reduced_size is the greater size of the
 * downsampled data, the other dimension is proportional to the original
 * size.
 *
 * If necessary a display list is recreated and widget is invalidated
 **/
void
gwy_3d_view_set_reduced_size(Gwy3DView *gwy3dview,
                             guint reduced_size)
{
    g_return_if_fail(GWY_IS_3D_VIEW(gwy3dview));

    if (reduced_size == gwy3dview->reduced_size)
        return;

    gwy3dview->reduced_size = reduced_size;
    gwy_3d_view_downsample_data(gwy3dview);
    if (GTK_WIDGET_REALIZED(gwy3dview)) {
        gwy_3d_make_list(gwy3dview, gwy3dview->downsampled,
                         GWY_3D_SHAPE_REDUCED);
        gwy_3d_timeout_start(gwy3dview, FALSE, TRUE);
    }
    g_object_notify(G_OBJECT(gwy3dview), "reduced-size");
}

static void
gwy_3d_view_downsample_data(Gwy3DView *gwy3dview)
{
    gint rx, ry;

    if (!gwy3dview->downsampled)
        gwy3dview->downsampled
            = gwy_data_field_duplicate(gwy3dview->data_field);
    else
        gwy_serializable_clone(G_OBJECT(gwy3dview->data_field),
                               G_OBJECT(gwy3dview->downsampled));

    rx = gwy_data_field_get_xres(gwy3dview->downsampled);
    ry = gwy_data_field_get_yres(gwy3dview->downsampled);
    if (rx > ry) {
        ry = (guint)((gdouble)ry / (gdouble)rx
                     * (gdouble)gwy3dview->reduced_size);
        rx = gwy3dview->reduced_size;
    } else {
        rx = (guint) ((gdouble)rx / (gdouble)ry
                      * (gdouble)gwy3dview->reduced_size);
        ry = gwy3dview->reduced_size;
    }
    gwy_data_field_resample(gwy3dview->downsampled, rx, ry,
                            GWY_INTERPOLATION_BILINEAR);
}

Gwy3DLabel*
gwy_3d_view_get_label(Gwy3DView *gwy3dview,
                      Gwy3DViewLabel label)
{
    g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), NULL);
    g_return_val_if_fail(label < G_N_ELEMENTS(labels), NULL);

    return gwy3dview->labels[label];
}

/**
 * gwy_3d_view_get_pixbuf:
 * @gwy3dview: A 3D data view widget.
 *
 * Copies the contents of the framebuffer to the GdkPixbuf.
 *
 * The size of the pixbuf is currently indentical with the size of the widget.
 * @xres and @yres will be implemented to set the resolution of the pixbuf.
 *
 * Returns: A newly allocated GdkPixbuf with copy of the framebuffer.
 **/
GdkPixbuf*
gwy_3d_view_get_pixbuf(Gwy3DView *gwy3dview)
{
    int width, height, rowstride, n_channels, i, j;
    guchar *pixels, *a, *b, z;
    GdkPixbuf * pixbuf;

    g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), NULL);
    g_return_val_if_fail(GTK_WIDGET_REALIZED(gwy3dview), NULL);

    width  = GTK_WIDGET(gwy3dview)->allocation.width;
    height = GTK_WIDGET(gwy3dview)->allocation.height;

    pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, width, height);
    gwy_debug_objects_creation(G_OBJECT(pixbuf));
    n_channels = gdk_pixbuf_get_n_channels(pixbuf);

    rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    pixels = gdk_pixbuf_get_pixels(pixbuf);

    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels);

    a = pixels;
    b = pixels + (height -1)  * rowstride;
    for (i = 0; i < height/2; i ++, b = pixels + (height - 1 - i) * rowstride) {
        for (j = 0; j < rowstride; j++, a++, b++) {
             z = *a;
            *a = *b;
            *b =  z;
        }
    }

    return pixbuf;
}

/**
 * gwy_3d_view_get_data:
 * @gwy3dview: A 3D data view widget.
 *
 * Returns the data container this 3D view displays.
 *
 * Returns: The container as a #GwyContainer.
 **/
GwyContainer*
gwy_3d_view_get_data(Gwy3DView *gwy3dview)
{
    g_return_val_if_fail(GTK_WIDGET_REALIZED(gwy3dview), NULL);
    return gwy3dview->data;
}

/**
 * gwy_3d_view_reset_view:
 * @gwy3dview: A 3D data view widget.
 *
 * Resets angle of the view, scale, deformation ant the position
 * of the light to the "default" values. Invalidates the widget.
 **/
void
gwy_3d_view_reset_view(Gwy3DView *gwy3dview)
{
   g_return_if_fail(GWY_IS_3D_VIEW(gwy3dview));

   gtk_adjustment_set_value(gwy3dview->rot_x, 45.0);
   gtk_adjustment_set_value(gwy3dview->rot_y, -45.0);
   gtk_adjustment_set_value(gwy3dview->view_scale, 1.0);
   gtk_adjustment_set_value(gwy3dview->deformation_z, 1.0);
   gtk_adjustment_set_value(gwy3dview->light_z, 0.0);
   gtk_adjustment_set_value(gwy3dview->light_y, 0.0f);

   gwy_3d_timeout_start(gwy3dview, FALSE, TRUE);
}

/**
 * gwy_3d_view_get_rot_x_adjustment:
 * @gwy3dview: A 3D data view widget.
 *
 * Returns a GtkAdjustment with settings of the Phi angle of rotation.
 *
 * Returns: a GtkAdjustment with settings of the Phi angle of rotation
 **/
GtkAdjustment*
gwy_3d_view_get_rot_x_adjustment(Gwy3DView *gwy3dview)
{
    g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), NULL);
    return gwy3dview->rot_x;
}

/**
 * gwy_3d_view_get_rot_y_adjustment:
 * @gwy3dview: A 3D data view widget.
 *
 * Returns a GtkAdjustment with settings of the Theta angle of rotation
 *
 * Returns: a GtkAdjustment with settings of the Theta angle of rotation
 **/
GtkAdjustment*
gwy_3d_view_get_rot_y_adjustment(Gwy3DView *gwy3dview)
{
    g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), NULL);
    return gwy3dview->rot_y;
}

/**
 * gwy_3d_view_get_view_scale_adjustment:
 * @gwy3dview: A 3D data view widget.
 *
 * Returns a GtkAdjustment with settings of the view zoom
 *
 * Returns: a GtkAdjustment with settings of the view zoom
 **/
GtkAdjustment*
gwy_3d_view_get_view_scale_adjustment(Gwy3DView *gwy3dview)
{
    g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), NULL);
    return gwy3dview->view_scale;
}

/**
 * gwy_3d_view_get_z_deformation_adjustment:
 * @gwy3dview: A 3D data view widget.
 *
 * Returns a GtkAdjustment with settings of the zoom of the z-axis
 *
 * Returns: a GtkAdjustment with settings of the zoom of the z-axis
 **/
GtkAdjustment*
gwy_3d_view_get_z_deformation_adjustment(Gwy3DView *gwy3dview)
{
    g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), NULL);
    return gwy3dview->deformation_z;
}

/**
 * gwy_3d_view_get_light_z_adjustment:
 * @gwy3dview: A 3D data view widget.
 *
 * Returns a GtkAdjustment with settings of the Phi angle of light position.
 *
 * Returns: a GtkAdjustment with settings of the Phi angle of light position
 **/
GtkAdjustment*
gwy_3d_view_get_light_z_adjustment(Gwy3DView *gwy3dview)
{
    g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), NULL);
    return gwy3dview->light_z;
}

/**
 * gwy_3d_view_get_light_y_adjustment:
 * @gwy3dview: A 3D data view widget.
 *
 * Returns a GtkAdjustment with settings of the Theta angle of light position.
 *
 * Returns: a GtkAdjustment with settings of the Theta angle of light position
 **/
GtkAdjustment*
gwy_3d_view_get_light_y_adjustment(Gwy3DView *gwy3dview)
{
    g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), NULL);
    return gwy3dview->light_y;
}

/**
 * gwy_3d_view_get_max_view_scale:
 * @gwy3dview: A 3D data view widget.
 *
 * Returns the maximal zoom of the 3D data view
 *
 * Returns: the maximal zoom of the 3D data view
 **/
gdouble
gwy_3d_view_get_max_view_scale(Gwy3DView *gwy3dview)
{
    g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), G_MAXDOUBLE);
    return gwy3dview->view_scale_max;
}

/**
 * gwy_3d_view_get_min_view_scale:
 * @gwy3dview: A 3D data view widget.
 *
 * Returns the minimal zoom of the 3D data view
 *
 * Returns: the minimal zoom of the 3D data view
 **/
gdouble
gwy_3d_view_get_min_view_scale(Gwy3DView *gwy3dview)
{
    g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), G_MAXDOUBLE);
    return gwy3dview->view_scale_min;
}

/**
 * gwy_3d_view_set_max_view_scale:
 * @gwy3dview: A 3D data view widget.
 * @new_max_scale: maximal zoom of the 3D data view
 *
 * Sets the new maximal zoom of 3D data view. Recommended values are 0.5 - 5.0.
 **/
void
gwy_3d_view_set_max_view_scale(Gwy3DView *gwy3dview,
                               gdouble new_max_scale)
{
    g_return_if_fail(GWY_IS_3D_VIEW(gwy3dview));

    new_max_scale = fabs(new_max_scale);
    if (new_max_scale != gwy3dview->view_scale_max) {
        gwy3dview->view_scale_max = new_max_scale;
        if (gwy3dview->view_scale_max < gwy3dview->view_scale_min)
            GWY_SWAP(gdouble,
                     gwy3dview->view_scale_max,
                     gwy3dview->view_scale_min);
        if (gwy3dview->view_scale->value > gwy3dview->view_scale_max)
           gtk_adjustment_set_value(gwy3dview->view_scale,
                                    gwy3dview->view_scale_max);
        if (gwy3dview->view_scale->value < gwy3dview->view_scale_min)
           gtk_adjustment_set_value(gwy3dview->view_scale,
                                    gwy3dview->view_scale_min);
    }
}

/**
 * gwy_3d_view_set_min_view_scale:
 * @gwy3dview: A 3D data view widget.
 * @new_min_scale: minimal zoom of the 3D data view
 *
 * Sets the new manimal zoom of 3D data view. Recommended values are 0.5 - 5.0.
 **/
void
gwy_3d_view_set_min_view_scale(Gwy3DView *gwy3dview,
                               gdouble new_min_scale)
{
    g_return_if_fail(GWY_IS_3D_VIEW(gwy3dview));

    new_min_scale = fabs(new_min_scale);
    if (new_min_scale != gwy3dview->view_scale_min) {
        gwy3dview->view_scale_min = new_min_scale;
        if (gwy3dview->view_scale_max < gwy3dview->view_scale_min)
            GWY_SWAP(gdouble,
                     gwy3dview->view_scale_max,
                     gwy3dview->view_scale_min);
        if (gwy3dview->view_scale->value < gwy3dview->view_scale_min)
           gtk_adjustment_set_value(gwy3dview->view_scale,
                                    gwy3dview->view_scale_min);
        if (gwy3dview->view_scale->value > gwy3dview->view_scale_max)
           gtk_adjustment_set_value(gwy3dview->view_scale,
                                    gwy3dview->view_scale_max);
    }
}

/******************************************************************************/
static void
gwy_3d_timeout_start(Gwy3DView *gwy3dview,
                     gboolean immediate,
                     gboolean invalidate_now)
{
    gwy_debug(" ");

    if (gwy3dview->timeout)
         g_source_remove(gwy3dview->timeout_id);

    if (!GTK_WIDGET_REALIZED(gwy3dview))
        return;

    if (gwy_data_field_get_xres(gwy3dview->data_field) <= gwy3dview->reduced_size
        && gwy_data_field_get_yres(gwy3dview->data_field) <= gwy3dview->reduced_size)
        immediate = TRUE;

    if (immediate)
        gwy3dview->shape_current = GWY_3D_SHAPE_AFM;
    else
        gwy3dview->shape_current = GWY_3D_SHAPE_REDUCED;

    if (invalidate_now || immediate)
        gtk_widget_queue_draw(GTK_WIDGET(gwy3dview));

    if (!immediate) {
        gwy3dview->timeout_id = g_timeout_add(GWY_3D_TIMEOUT_DELAY,
                                              (GSourceFunc)gwy_3d_timeout_func,
                                              (gpointer)gwy3dview);
        gwy3dview->timeout = TRUE;
    }
}

static gboolean
gwy_3d_timeout_func(gpointer user_data)
{
    Gwy3DView * gwy3dview;

    gwy_debug(" ");
    g_return_val_if_fail(GWY_IS_3D_VIEW(user_data), FALSE);

    gwy3dview = (Gwy3DView *) user_data;
    gwy3dview->shape_current = GWY_3D_SHAPE_AFM;
    gwy3dview->timeout = FALSE;
    if (GTK_WIDGET_REALIZED(gwy3dview))
        gdk_window_invalidate_rect(GTK_WIDGET(gwy3dview)->window,
                                   &GTK_WIDGET(gwy3dview)->allocation, TRUE);

    return FALSE;
}

static void
gwy_3d_adjustment_value_changed(GtkAdjustment *adjustment,
                                Gwy3DView *gwy3dview)
{
    GQuark quark;

    if ((quark = GPOINTER_TO_UINT(g_object_get_qdata(G_OBJECT(adjustment),
                                                     container_key_quark))))
        gwy_container_set_double(gwy3dview->data, quark,
                                 gtk_adjustment_get_value(adjustment));
    gwy_3d_timeout_start(gwy3dview, FALSE, TRUE);
}

static void
gwy_3d_label_changed(Gwy3DView *gwy3dview)
{
    gwy_3d_timeout_start(gwy3dview, FALSE, TRUE);
}

static void
gwy_3d_view_realize(GtkWidget *widget)
{
    Gwy3DView *gwy3dview;
    GdkWindowAttr attributes;
    gint attributes_mask;

    gwy_debug("realizing a Gwy3DView (%ux%u)",
              widget->allocation.width, widget->allocation.height);

    GTK_WIDGET_SET_FLAGS(widget, GTK_REALIZED);
    gwy3dview = GWY_3D_VIEW(widget);

    attributes.window_type = GDK_WINDOW_CHILD;
    attributes.x = widget->allocation.x;
    attributes.y = widget->allocation.y;
    attributes.width = widget->allocation.width;
    attributes.height = widget->allocation.height;
    attributes.wclass = GDK_INPUT_OUTPUT;
    attributes.visual = gtk_widget_get_visual(widget);
    attributes.colormap = gtk_widget_get_colormap(widget);
    attributes.event_mask = gtk_widget_get_events(widget)
                           | GDK_EXPOSURE_MASK
                           | GDK_BUTTON1_MOTION_MASK
                           | GDK_BUTTON2_MOTION_MASK
                           | GDK_BUTTON_PRESS_MASK
                           | GDK_BUTTON_RELEASE_MASK
                           | GDK_VISIBILITY_NOTIFY_MASK;

    attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

    widget->window = gdk_window_new(gtk_widget_get_parent_window(widget),
                                    &attributes, attributes_mask);
    gdk_window_set_user_data(widget->window, widget);

    widget->style = gtk_style_attach(widget->style, widget->window);
    gtk_style_set_background(widget->style, widget->window, GTK_STATE_NORMAL);

    gwy_3d_view_send_configure(gwy3dview);
    gwy_3d_view_realize_gl(gwy3dview);

    /* Get PangoFT2 context. */
    gwy3dview->ft2_font_map = gwy_get_pango_ft2_font_map(FALSE);
    g_object_ref(gwy3dview->ft2_font_map);
    gwy3dview->ft2_context = pango_ft2_font_map_create_context
                                 (PANGO_FT2_FONT_MAP(gwy3dview->ft2_font_map));
}

static gboolean
gwy_3d_view_configure(GtkWidget *widget,
                      G_GNUC_UNUSED GdkEventConfigure *event)
{
    Gwy3DView *gwy3dview;


    GdkGLContext *glcontext = gtk_widget_get_gl_context(widget);
    GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable(widget);

    GLfloat w = widget->allocation.width;
    GLfloat h = widget->allocation.height;

    gwy_debug("width: %f, height: %f", w, h);
    g_return_val_if_fail(GWY_IS_3D_VIEW(widget), FALSE);
    gwy3dview = GWY_3D_VIEW(widget);

    /*** OpenGL BEGIN ***/
    if (!gdk_gl_drawable_gl_begin(gldrawable, glcontext))
        return FALSE;

    glViewport(0, 0, w, h);
    gwy_3d_set_projection(gwy3dview);

    glMatrixMode(GL_MODELVIEW);

    gdk_gl_drawable_gl_end(gldrawable);
    /*** OpenGL END ***/

    return FALSE;
}

static void
gwy_3d_view_send_configure(Gwy3DView *gwy3dview)
{
    GtkWidget *widget;
    GdkEvent *event;

    event = gdk_event_new(GDK_CONFIGURE);

    widget = GTK_WIDGET(gwy3dview);
    event->configure.window = g_object_ref(widget->window);
    event->configure.send_event = TRUE;
    event->configure.x = widget->allocation.x;
    event->configure.y = widget->allocation.y;
    event->configure.width = widget->allocation.width;
    event->configure.height = widget->allocation.height;

    gtk_widget_event(widget, event);
    gdk_event_free(event);
}

static void
gwy_3d_view_size_request(G_GNUC_UNUSED GtkWidget *widget,
                         GtkRequisition *requisition)
{
    requisition->width = GWY_3D_VIEW_DEFAULT_SIZE_X;
    requisition->height = GWY_3D_VIEW_DEFAULT_SIZE_Y;
}

static void
gwy_3d_view_size_allocate(GtkWidget *widget,
                          GtkAllocation *allocation)
{
    g_return_if_fail(allocation != NULL);

    widget->allocation = *allocation;

    if (GTK_WIDGET_REALIZED(widget)) {
        gdk_window_move_resize(widget->window,
                               allocation->x, allocation->y,
                               allocation->width, allocation->height);

        gwy_3d_view_send_configure(GWY_3D_VIEW(widget));
    }
}

/* Convert GwyRGBA to GLfloat[4] array */
static void
gwy_3d_view_rgba_dv(GLenum face,
                    GLenum pname,
                    const GwyRGBA *rgba)
{
    gfloat fparams[4];

    fparams[0] = rgba->r;
    fparams[1] = rgba->g;
    fparams[2] = rgba->b;
    fparams[3] = rgba->a;
    glMaterialfv(face, pname, fparams);
}

static gboolean
gwy_3d_view_expose(GtkWidget *widget,
                   G_GNUC_UNUSED GdkEventExpose *event)
{
    GwyGLMaterial *material;
    GdkGLContext  *glcontext;
    GdkGLDrawable *gldrawable;
    Gwy3DView *gwy3dview;

    GLfloat light_position[] = { 0.0, 0.0, 4.0, 1.0 };

    gwy_debug(" ");

    g_return_val_if_fail(GWY_IS_3D_VIEW(widget), FALSE);
    gwy3dview = GWY_3D_VIEW(widget);

    glcontext  = gtk_widget_get_gl_context(widget);
    gldrawable = gtk_widget_get_gl_drawable(widget);
    gwy_debug("GLContext: %p, GLDrawable: %p", glcontext, gldrawable);

    /*** OpenGL BEGIN ***/
    if (!gdk_gl_drawable_gl_begin(gldrawable, glcontext))
        return FALSE;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glLoadIdentity();

    /* View transformation. */
    gwy_3d_set_projection(gwy3dview);
    glTranslatef(0.0, 0.0, -10.0);
    glScalef(gwy3dview->view_scale->value,
             gwy3dview->view_scale->value,
             gwy3dview->view_scale->value);

    glRotatef(gwy3dview->rot_y->value, 1.0, 0.0, 0.0);
    glRotatef(gwy3dview->rot_x->value, 0.0,  0.0, 1.0);
    glScalef(1.0f, 1.0f, gwy3dview->deformation_z->value);

    /* Render shape */
    if (gwy3dview->visual == GWY_3D_VISUALIZATION_LIGHTING) {
        material = gwy3dview->material;

        glEnable(GL_LIGHTING);
        gwy_3d_view_rgba_dv(GL_FRONT, GL_AMBIENT,
                            gwy_gl_material_get_ambient(material));
        gwy_3d_view_rgba_dv(GL_FRONT, GL_DIFFUSE,
                            gwy_gl_material_get_diffuse(material));
        gwy_3d_view_rgba_dv(GL_FRONT, GL_SPECULAR,
                            gwy_gl_material_get_specular(material));
        glMaterialf(GL_FRONT, GL_SHININESS,
                    (GLfloat)gwy_gl_material_get_shininess(material)*128.0f);
        glPushMatrix();
        glRotatef(gwy3dview->light_z->value, 0.0f, 0.0f, 1.0f);
        glRotatef(gwy3dview->light_y->value, 0.0f, 1.0f, 0.0f);
        glLightfv(GL_LIGHT0, GL_POSITION, light_position);
        glPopMatrix();
    }
    else {
        glDisable(GL_LIGHTING);
    }


    glCallList(gwy3dview->shape_list_base + gwy3dview->shape_current);
    if (gwy3dview->show_axes)
        gwy_3d_draw_axes(gwy3dview);

    if (gwy3dview->movement == GWY_3D_MOVEMENT_LIGHT
        && gwy3dview->shape_current == GWY_3D_SHAPE_REDUCED)
        gwy_3d_draw_light_position(gwy3dview);

    /* Swap buffers */
    if (gdk_gl_drawable_is_double_buffered(gldrawable))
        gdk_gl_drawable_swap_buffers(gldrawable);
    else
        glFlush();

    gdk_gl_drawable_gl_end(gldrawable);
    /*** OpenGL END ***/

    return FALSE;
}

static gboolean
gwy_3d_view_button_press(GtkWidget *widget,
                         GdkEventButton *event)
{
    Gwy3DView *gwy3dview;

    gwy_debug(" ");

    g_return_val_if_fail(GWY_IS_3D_VIEW(widget), FALSE);
    g_return_val_if_fail(event != NULL, FALSE);

    gwy3dview = GWY_3D_VIEW(widget);

    gwy3dview->mouse_begin_x = event->x;
    gwy3dview->mouse_begin_y = event->y;

    return FALSE;
}

static gboolean
gwy_3d_view_button_release(GtkWidget *widget,
                           GdkEventButton *event)
{
    Gwy3DView *gwy3dview;

    gwy_debug(" ");

    g_return_val_if_fail(GWY_IS_3D_VIEW(widget), FALSE);
    g_return_val_if_fail(event != NULL, FALSE);

    gwy3dview = GWY_3D_VIEW(widget);

    return FALSE;
}

static gboolean
gwy_3d_view_motion_notify(GtkWidget *widget,
                          GdkEventMotion *event)
{
    Gwy3DView *gwy3dview;
    float h = widget->allocation.height;
    float x = event->x;
    float y = event->y;

    g_return_val_if_fail(GWY_IS_3D_VIEW(widget), FALSE);
    g_return_val_if_fail(event, FALSE);

    gwy3dview = GWY_3D_VIEW(widget);
    gwy_debug("motion event: (%f, %f), shape=%d",
              event->x, event->y, gwy3dview->shape_current);

    /* Rotation. */
    if (event->state & GDK_BUTTON1_MASK)
        switch (gwy3dview->movement) {
            case GWY_3D_MOVEMENT_NONE:
            break;

            case GWY_3D_MOVEMENT_ROTATION:
                gtk_adjustment_set_value(gwy3dview->rot_x,
                                         gwy3dview->rot_x->value
                                         + x - gwy3dview->mouse_begin_x);
                gtk_adjustment_set_value(gwy3dview->rot_y,
                                         gwy3dview->rot_y->value
                                         + y - gwy3dview->mouse_begin_y);
                break;

            case GWY_3D_MOVEMENT_SCALE:
                gtk_adjustment_set_value(gwy3dview->view_scale,
                                         gwy3dview->view_scale->value
                                         *(1.0
                                           + (y - gwy3dview->mouse_begin_y)/h));
                if (gwy3dview->view_scale->value > gwy3dview->view_scale_max)
                    gtk_adjustment_set_value(gwy3dview->view_scale,
                                             gwy3dview->view_scale_max);
                else if (gwy3dview->view_scale->value
                         < gwy3dview->view_scale_min)
                    gtk_adjustment_set_value(gwy3dview->view_scale,
                                             gwy3dview->view_scale_min);
                break;

            case GWY_3D_MOVEMENT_DEFORMATION:
            {
                int i;
                double dz = gwy3dview->deformation_z->value;

                if (y - gwy3dview->mouse_begin_y > 0)
                    for (i = 0; i < y - gwy3dview->mouse_begin_y; i++)
                        dz /= GWY_3D_Z_DEFORMATION;
                else
                    for (i = 0; i < gwy3dview->mouse_begin_y - y; i++)
                        dz *= GWY_3D_Z_DEFORMATION;
                gtk_adjustment_set_value(gwy3dview->deformation_z, dz);
                break;
            }
            case GWY_3D_MOVEMENT_LIGHT:
                gtk_adjustment_set_value(gwy3dview->light_z,
                                         gwy3dview->light_z->value
                                         + x - gwy3dview->mouse_begin_x);
                gtk_adjustment_set_value(gwy3dview->light_y,
                                         gwy3dview->light_y->value
                                         + y - gwy3dview->mouse_begin_y);
                break;
        }

    gwy3dview->mouse_begin_x = x;
    gwy3dview->mouse_begin_y = y;
    return FALSE;
}



static Gwy3DVector*
gwy_3d_make_normals(GwyDataField *dfield,
                    Gwy3DVector *normals)
{
   typedef struct { Gwy3DVector A, B; } RectangleNorm;
   const gdouble *data;
   gint i, j, xres, yres;
   RectangleNorm * norms;

   g_return_val_if_fail(normals, NULL);

   xres = gwy_data_field_get_xres(dfield);
   yres = gwy_data_field_get_yres(dfield);
   data = gwy_data_field_get_data_const(dfield);

   /* memory for normals of triangles
    * total count of rectangels is (xres-1)*(yres-1), each one has 2n triangles
    * 3 componernts per vector
    */
   norms = g_new(RectangleNorm, (xres - 1) * (yres - 1));
   if (norms == NULL)
       return NULL;

   /* Calculation of nornals of triangles */
   for (j = 0; j < yres-1; j++) {
      for (i = 0; i < xres-1; i++) {
         GLfloat a, b, c, n;

         a = data[(yres-1 - j)*xres + i];
         b = data[(yres-2 - j)*xres + i];
         c = data[(yres-1 - j)*xres + i+1];
         n = 1.0 / sqrt((a-c)*(a-c) + (b-a)*(b-a) + 1.0);
         norms[j*(xres-1) + i].A.x = (a-c)*n;
         norms[j*(xres-1) + i].A.y = (b-a)*n;
         norms[j*(xres-1)+ i].A.z = n;

         a = b;
         b = c;
         c = data[(yres-2 - j)*xres + i+1];
         n = 1.0 / sqrt((a-c)*(a-c) + (c-b)*(c-b) + 1.0);
         norms[j*(xres-1) + i].B.x = (a-c)*n;
         norms[j*(xres-1) + i].B.y = (c-b)*n;
         norms[j*(xres-1) + i].B.z = n;
      }
   }

   /* two of corner vertecies have only one triangle adjacent
    * (first and last vertex) */
   normals[0] = norms[0].A;
   normals[xres * yres - 1] = norms[(xres-1) * (yres-1) - 1].B;
   /*the other two corner vertecies have two triangles adjacent*/
   normals[xres - 1].x = (norms[xres - 2].A.x + norms[xres - 2].B.x)/2.0f;
   normals[xres - 1].y = (norms[xres - 2].A.y + norms[xres - 2].B.y)/2.0f;
   normals[xres - 1].z = (norms[xres - 2].A.z + norms[xres - 2].B.z)/2.0f;
   normals[xres * (yres -1)].x = (norms[(xres - 1)*(yres -2)].A.x
                                  + norms[(xres - 1)*(yres -2)].B.x)/2.0f;
   normals[xres * (yres -1)].y = (norms[xres - 2].A.y
                                  + norms[(xres - 1)*(yres -2)].B.y)/2.0f;
   normals[xres * (yres -1)].z = (norms[xres - 2].A.z
                                  + norms[(xres - 1)*(yres -2)].B.z)/2.0f;
   /*the vertexies on the edge og the matrix have three adjacent triangles*/
   for (i = 1; i < xres - 1; i ++) {
       const Gwy3DVector *a = &(norms[i-1].A);
       const Gwy3DVector *b = &(norms[i-1].B);
       const Gwy3DVector *c = &(norms[i].A);

       normals[i].x = (a->x + b->x + c->x)/3.0;
       normals[i].y = (a->y + b->y + c->y)/3.0;
       normals[i].z = (a->z + b->z + c->z)/3.0;
   }

   for (i = 1; i < xres - 1; i ++) {
       const Gwy3DVector *a = &(norms[(xres - 1)*(yres - 2) + i-1].B);
       const Gwy3DVector *b = &(norms[(xres - 1)*(yres - 2) + i-1].A);
       const Gwy3DVector *c = &(norms[(xres - 1)*(yres - 2) + i].B);

       normals[xres * (yres - 1) + i].x = (a->x + b->x + c->x)/3.0;
       normals[xres * (yres - 1) + i].y = (a->y + b->y + c->y)/3.0;
       normals[xres * (yres - 1) + i].z = (a->z + b->z + c->z)/3.0;
   }

   for (i = 1; i < yres - 1; i ++) {
       const Gwy3DVector *a = &(norms[(i-1) * (xres-1)].A);
       const Gwy3DVector *b = &(norms[(i-1) * (xres-1)].B);
       const Gwy3DVector *c = &(norms[i *     (xres-1)].A);

       normals[i * xres].x = (a->x + b->x + c->x)/3.0;
       normals[i * xres].y = (a->y + b->y + c->y)/3.0;
       normals[i * xres].z = (a->z + b->z + c->z)/3.0;
   }

   for (i = 1; i < yres - 1; i ++) {
       const Gwy3DVector *a = &(norms[i     * (xres-1) - 1].B);
       const Gwy3DVector *b = &(norms[(i+1) * (xres-1) - 1].A);
       const Gwy3DVector *c = &(norms[(i+1) * (xres-1) - 1].B);

       normals[i * xres - 1].x = (a->x + b->x + c->x)/3.0;
       normals[i * xres - 1].y = (a->y + b->y + c->y)/3.0;
       normals[i * xres - 1].z = (a->z + b->z + c->z)/3.0;
   }

   /* The vertecies inside of matrix have six adjacent triangles*/
   for (j = 1; j < yres - 1; j++) {
       for (i = 1; i < xres - 1; i++) {
           Gwy3DVector *adj_tri[6];
           int k;

           adj_tri[0] = &(norms[(j-1)*(xres-1) + i - 1].B);
           adj_tri[1] = &(norms[(j-1)*(xres-1) + i    ].A);
           adj_tri[2] = &(norms[(j-1)*(xres-1) + i    ].B);
           adj_tri[3] = &(norms[j    *(xres-1) + i    ].A);
           adj_tri[4] = &(norms[j    *(xres-1) + i - 1].A);
           adj_tri[5] = &(norms[j    *(xres-1) + i - 1].B);
           normals[j*xres + i].x = 0.0f;
           normals[j*xres + i].y = 0.0f;
           normals[j*xres + i].z = 0.0f;
           for (k = 0; k < 6; k++) {
               normals[j*xres + i].x += adj_tri[k]->x;
               normals[j*xres + i].y += adj_tri[k]->y;
               normals[j*xres + i].z += adj_tri[k]->z;
           }
           normals[j*xres + i].x /= 6.0f;
           normals[j*xres + i].y /= 6.0f;
           normals[j*xres + i].z /= 6.0f;
       }
   }

   g_free(norms);

   return normals;
}

static void
gwy_3d_make_list(Gwy3DView *gwy3dview,
                 GwyDataField *dfield,
                 gint shape)
{
    gint i, j, xres, yres, res;
    gdouble data_min, data_max;
    GLdouble zdifr;
    Gwy3DVector *normals;
    const gdouble *data;
    GwyGradient *grad;
    GwyRGBA color;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    gwy_data_field_get_min_max(gwy3dview->data_field, &data_min, &data_max);
    data = gwy_data_field_get_data_const(dfield);
    res  = xres > yres ? xres : yres;
    grad = gwy3dview->gradient;

    glNewList(gwy3dview->shape_list_base + shape, GL_COMPILE);
    glPushMatrix();
    glTranslatef(-(xres/(double)res), -(yres/(double)res),
                 GWY_3D_Z_DISPLACEMENT);
    glScalef(2.0/res, 2.0/res, GWY_3D_Z_TRANSFORMATION/(data_max - data_min));
    glTranslatef(0.0, 0.0, -data_min);
    zdifr = 1.0/(data_max - data_min);
    normals = g_new(Gwy3DVector, xres * yres);
    if (!gwy_3d_make_normals(dfield, normals)) {
        /*TODO solve not enough momory problem*/
    }

    for (j = 0; j < yres-1; j++) {
        glBegin(GL_TRIANGLE_STRIP);
        for (i = 0; i < xres-1; i++) {
            gdouble a, b;

            a = data[(yres-1 - j)*xres + i];
            b = data[(yres-2 - j)*xres + i];
            glNormal3d(normals[j*xres+i].x,
                       normals[j*xres+i].y,
                       normals[j*xres+i].z);
            gwy_gradient_get_color(grad, (a - data_min)*zdifr, &color);
            glColor3d(color.r , color.g, color.b);
            glVertex3d((double)i, (double)j, a);
            glNormal3d(normals[(j+1)*xres+i].x,
                       normals[(j+1)*xres+i].y,
                       normals[(j+1)*xres+i].z);
            gwy_gradient_get_color(grad, (b - data_min)*zdifr, &color);
            glColor3d(color.r , color.g, color.b);
            glVertex3d((double)i, (double)(j+1), b);
        }
        glEnd();
    }
    g_free(normals);
    glPopMatrix();
    glEndList();

}

static void
gwy_3d_draw_axes(Gwy3DView *widget)
{
    GLfloat rx, Ax, Ay, Bx, By, Cx, Cy;
    gdouble data_min, data_max;
    gint xres, yres, res;
    gboolean yfirst;
    GwyGLMaterial *mat_none;

    gwy_debug(" ");

    xres = gwy_data_field_get_xres(widget->data_field);
    yres = gwy_data_field_get_yres(widget->data_field);
    gwy_data_field_get_min_max(widget->data_field, &data_min, &data_max);
    res  = xres > yres ? xres : yres;

    Ax = Ay = Bx = By = Cx = Cy = 0.0f;
    yfirst = TRUE;
    rx = widget->rot_x->value
                 - ((int)(widget->rot_x->value/360.0)) * 360.0;
    if (rx < 0.0)
        rx += 360.0;

    mat_none = gwy_gl_materials_get_gl_material(GWY_GL_MATERIAL_NONE);

    glPushMatrix();
    glTranslatef(-(xres/(double)res), -(yres/(double)res),
                 GWY_3D_Z_DISPLACEMENT);
    glScalef(2.0/res, 2.0/res, GWY_3D_Z_TRANSFORMATION/(data_max - data_min));
    gwy_3d_view_rgba_dv(GL_FRONT, GL_AMBIENT,
                        gwy_gl_material_get_ambient(mat_none));
    gwy_3d_view_rgba_dv(GL_FRONT, GL_DIFFUSE,
                        gwy_gl_material_get_diffuse(mat_none));
    gwy_3d_view_rgba_dv(GL_FRONT, GL_SPECULAR,
                        gwy_gl_material_get_specular(mat_none));
    glMaterialf(GL_FRONT, GL_SHININESS,
                (GLfloat)gwy_gl_material_get_shininess(mat_none)*128.0f);

    if (rx >= 0.0 && rx <= 90.0) {
        Ay = yres;
        Cx = xres;
        yfirst = TRUE;
    } else if (rx > 90.0 && rx <= 180.0) {
        Ax = xres;
        Ay = yres;
        By = yres;
        yfirst = FALSE;
    } else if (rx > 180.0 && rx <= 270.0) {
        Ax = xres;
        Bx = xres;
        By = yres;
        Cy = yres;
       yfirst = TRUE;
    } else if (rx >= 270.0 && rx <= 360.0) {
        Bx = xres;
        Cx = xres;
        Cy = yres;
        yfirst = FALSE;
    }
    glBegin(GL_LINE_STRIP);
        glColor3f(0.0, 0.0, 0.0);
        glVertex3f(Ax, Ay, 0.0f);
        glVertex3f(Bx, By, 0.0f);
        glVertex3f(Cx, Cy, 0.0f);
        glVertex3f(Cx, Cy, data_max - data_min);
    glEnd();
    glBegin(GL_LINES);
        glVertex3f(Ax, Ay, 0.0f);
        glVertex3f(Ax - (Cx-Bx)*0.02, Ay - (Cy-By)*0.02, 0.0f );
        glVertex3f((Ax+Bx)/2, (Ay+By)/2, 0.0f);
        glVertex3f((Ax+Bx)/2 - (Cx-Bx)*0.02,
                   (Ay+By)/2 - (Cy-By)*0.02, 0.0f );
        glVertex3f(Bx , By, 0.0f);
        glVertex3f(Bx - (Cx-Bx)*0.02, By - (Cy-By)*0.02, 0.0f );
        glVertex3f(Bx, By, 0.0f);
        glVertex3f(Bx - (Ax-Bx)*0.02, By - (Ay-By)*0.02, 0.0f );
        glVertex3f((Cx+Bx)/2, (Cy+By)/2, 0.0f);
        glVertex3f((Cx+Bx)/2 - (Ax-Bx)*0.02,
                   (Cy+By)/2 - (Ay-By)*0.02, 0.0f );
        glVertex3f(Cx , Cy, 0.0f);
        glVertex3f(Cx - (Ax-Bx)*0.02, Cy - (Ay-By)*0.02, 0.0f );
    glEnd();

    glBegin(GL_LINES);
        glVertex3f(Cx, Cy, data_max - data_min);
        glVertex3f(Cx - (Ax-Bx)*0.02, Cy - (Ay-By)*0.02,
                 data_max - data_min);
        glVertex3f(Cx, Cy, (data_max - data_min)/2);
        glVertex3f(Cx - (Ax-Bx)*0.02, Cy - (Ay-By)*0.02,
                   (data_max - data_min)/2);
    glEnd();

    /*
    TODO: create bitmaps with labels in the beginning (possibly in init_gl)
          into display lists and draw here
    */
    if (widget->show_labels) {
        guint view_size;
        gint size;

        view_size = MIN(GTK_WIDGET(widget)->allocation.width,
                        GTK_WIDGET(widget)->allocation.height);
        size = (gint)(sqrt(view_size)*0.8);

        glColor3f(0.0, 0.0, 0.0);
        gwy_3d_print_text(widget, yfirst ? GWY_3D_VIEW_LABEL_Y
                                         : GWY_3D_VIEW_LABEL_X,
                          (Ax+2*Bx)/3 - (Cx-Bx)*0.1,
                          (Ay+2*By)/3 - (Cy-By)*0.1, -0.0f,
                          size, 1, 1);

        gwy_3d_print_text(widget, yfirst ? GWY_3D_VIEW_LABEL_X
                                         : GWY_3D_VIEW_LABEL_Y,
                          (2*Bx+Cx)/3 - (Ax-Bx)*0.1,
                          (2*By+Cy)/3 - (Ay-By)*0.1, -0.0f,
                          size, 1, -1);

        gwy_3d_print_text(widget, GWY_3D_VIEW_LABEL_MAX,
                          Cx - (Ax-Bx)*0.1, Cy - (Ay-By)*0.1,
                          (data_max - data_min),
                          size, 0, -1);

        gwy_3d_print_text(widget, GWY_3D_VIEW_LABEL_MIN,
                          Cx - (Ax-Bx)*0.1, Cy - (Ay-By)*0.1, 0.0f,
                          size, 0, -1);
    }

   glPopMatrix();
}

static void
gwy_3d_draw_light_position(Gwy3DView *widget)
{
    GwyGLMaterial *mat_none;
    GLfloat plane_z;
    gdouble data_min, data_max, data_mean;
    int i;

    gwy_debug(" ");

    gwy_data_field_get_min_max(widget->data_field, &data_min, &data_max);
    data_mean = gwy_data_field_get_avg(widget->data_field);

    mat_none = gwy_gl_materials_get_gl_material(GWY_GL_MATERIAL_NONE);
    gwy_3d_view_rgba_dv(GL_FRONT, GL_AMBIENT,
                        gwy_gl_material_get_ambient(mat_none));
    gwy_3d_view_rgba_dv(GL_FRONT, GL_DIFFUSE,
                        gwy_gl_material_get_diffuse(mat_none));
    gwy_3d_view_rgba_dv(GL_FRONT, GL_SPECULAR,
                        gwy_gl_material_get_specular(mat_none));
    glMaterialf(GL_FRONT, GL_SHININESS,
                (GLfloat)gwy_gl_material_get_shininess(mat_none)*128.0f);
    glPushMatrix();
    plane_z = GWY_3D_Z_TRANSFORMATION
              *(data_mean - data_min)/(data_max  - data_min)
              + GWY_3D_Z_DISPLACEMENT;

    glTranslatef(0.0f, 0.0f, plane_z);
    glRotatef(widget->light_z->value, 0.0f, 0.0f, 1.0f);
    glRotatef(-widget->light_y->value, 0.0f, 1.0f, 0.0f);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glBegin(GL_QUAD_STRIP);
        for (i = -180; i <= 180; i += 5) {
            GLfloat x = cos(i * DEG_2_RAD) * G_SQRT2;
            GLfloat z = sin(i * DEG_2_RAD) * G_SQRT2;
            glVertex3f(x,  0.05f, z);
            glVertex3f(x, -0.05f, z);
        }
    glEnd();
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glBegin(GL_LINE_STRIP);
        glVertex3f(0.0f,  0.0f,  0.0f);
        glVertex3f(0.0f,  0.05f, G_SQRT2);
        glVertex3f(0.0f, -0.05f, G_SQRT2);
        glVertex3f(0.0f,  0.0f,  0.0f);
    glEnd();
    glPopMatrix();
}


static void
gwy_3d_view_realize_gl(Gwy3DView *widget)
{
    GdkGLContext *glcontext;
    GdkGLDrawable *gldrawable;

    GLfloat ambient[] = { 0.1, 0.1, 0.1, 1.0 };
    GLfloat diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
    GLfloat position[] = { 0.0, 3.0, 3.0, 1.0 };

    GLfloat lmodel_ambient[] = { 0.2, 0.2, 0.2, 1.0 };
    GLfloat local_view[] = { 0.0 } ;

    gwy_debug("GL capable %d", gtk_widget_is_gl_capable(GTK_WIDGET(widget)));

    glcontext = gtk_widget_get_gl_context(GTK_WIDGET(widget));
    gldrawable = gtk_widget_get_gl_drawable(GTK_WIDGET(widget));
    /*** OpenGL BEGIN ***/
    gwy_debug("drawable %p, context %p", gldrawable, glcontext);

    if (!gdk_gl_drawable_gl_begin(gldrawable, glcontext))
        return;

    glClearColor(1.0, 1.0, 1.0, 1.0);
    glClearDepth(1.0);

    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    glLightfv(GL_LIGHT0, GL_POSITION, position);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, lmodel_ambient);
    glLightModelfv(GL_LIGHT_MODEL_LOCAL_VIEWER, local_view);

    glFrontFace(GL_CW);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_AUTO_NORMAL);
    glEnable(GL_NORMALIZE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    /* Shape display lists */
    gwy_3d_view_assign_lists(widget);
    gwy_3d_make_list(widget, widget->data_field, GWY_3D_SHAPE_AFM);
    gwy_3d_make_list(widget, widget->downsampled, GWY_3D_SHAPE_REDUCED);

    gdk_gl_drawable_gl_end(gldrawable);
    /*** OpenGL END ***/

  return;
}

static void
gwy_3d_set_projection(Gwy3DView *widget)
{
    GLfloat w, h;
    GLfloat aspect;

    gwy_debug(" ");

    w = GTK_WIDGET(widget)->allocation.width;
    h = GTK_WIDGET(widget)->allocation.height;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    if (w > h) {
        aspect = w / h;
        switch (widget->projection) {
            case GWY_3D_PROJECTION_ORTHOGRAPHIC:
            glOrtho(-aspect * GWY_3D_ORTHO_CORRECTION,
                     aspect * GWY_3D_ORTHO_CORRECTION,
                     -1.0   * GWY_3D_ORTHO_CORRECTION /* * deformation_z*/,
                      1.0   * GWY_3D_ORTHO_CORRECTION /* * deformation_z*/,
                      5.0,
                      60.0);
            break;

            case GWY_3D_PROJECTION_PERSPECTIVE:
            glFrustum(-aspect, aspect, -1.0 , 1.0, 5.0, 60.0);
            break;
        }
    }
    else {
        aspect = h / w;
        switch (widget->projection) {
            case GWY_3D_PROJECTION_ORTHOGRAPHIC:
            glOrtho( -1.0   * GWY_3D_ORTHO_CORRECTION,
                      1.0   * GWY_3D_ORTHO_CORRECTION,
                    -aspect * GWY_3D_ORTHO_CORRECTION /* * deformation_z*/,
                     aspect * GWY_3D_ORTHO_CORRECTION /* * deformation_z*/,
                      5.0,
                      60.0);
            break;

            case GWY_3D_PROJECTION_PERSPECTIVE:
            glFrustum(-1.0, 1.0, -aspect, aspect, 5.0, 60.0);
            break;
        }
    }
    glMatrixMode(GL_MODELVIEW);
}

static void
gwy_3d_pango_ft2_render_layout(PangoLayout *layout)
{
    PangoRectangle logical_rect;
    FT_Bitmap bitmap;
    GLvoid *pixels;
    guint32 *p;
    GLfloat color[4];
    guint32 rgb;
    GLfloat a;
    guint8 *row, *row_end;
    int i;

    pango_layout_get_pixel_extents(layout, NULL, &logical_rect);
    if (logical_rect.width == 0 || logical_rect.height == 0)
        return;

    bitmap.rows = logical_rect.height;
    bitmap.width = logical_rect.width;
    bitmap.pitch = bitmap.width;
    bitmap.pixel_mode = FT_PIXEL_MODE_GRAY;
    bitmap.num_grays = 256;
    bitmap.buffer = g_malloc0(bitmap.rows * bitmap.pitch);

    pango_ft2_render_layout(&bitmap, layout, -logical_rect.x, 0);

    pixels = g_malloc(bitmap.rows * bitmap.width * 4);
    p = (guint32 *)pixels;

    glGetFloatv(GL_CURRENT_COLOR, color);
#if !defined(GL_VERSION_1_2) && G_BYTE_ORDER == G_LITTLE_ENDIAN
    rgb = ((guint32)(color[0] * 255.0))
          | (((guint32)(color[1] * 255.0)) << 8)
          | (((guint32)(color[2] * 255.0)) << 16);
#else
    rgb = (((guint32)(color[0] * 255.0)) << 24)
          | (((guint32)(color[1] * 255.0)) << 16)
          | (((guint32)(color[2] * 255.0)) << 8);
#endif
    a = color[3];

    row = bitmap.buffer + bitmap.rows * bitmap.width; /* past-the-end */
    row_end = bitmap.buffer;      /* beginning */

    if (a == 1.0) {
        do {
            row -= bitmap.width;
            for (i = 0; i < bitmap.width; i++)
#if !defined(GL_VERSION_1_2) && G_BYTE_ORDER == G_LITTLE_ENDIAN
                *p++ = rgb | (((guint32) row[i]) << 24);
#else
            *p++ = rgb | ((guint32) row[i]);
#endif
        }
        while (row != row_end);
    }
    else {
        do {
            row -= bitmap.width;
            for (i = 0; i < bitmap.width; i++)
#if !defined(GL_VERSION_1_2) && G_BYTE_ORDER == G_LITTLE_ENDIAN
                *p++ = rgb | (((guint32) (a * row[i])) << 24);
#else
            *p++ = rgb | ((guint32) (a * row[i]));
#endif
        }
        while (row != row_end);
    }

    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

#if !defined(GL_VERSION_1_2)
    glDrawPixels(bitmap.width, bitmap.rows,
                 GL_RGBA, GL_UNSIGNED_BYTE,
                 pixels);
#else
    glDrawPixels(bitmap.width, bitmap.rows,
                 GL_RGBA, GL_UNSIGNED_INT_8_8_8_8,
                 pixels);
#endif

    glDisable(GL_BLEND);

    g_free(bitmap.buffer);
    g_free(pixels);
}

static void
gwy_3d_print_text(Gwy3DView     *gwy3dview,
                  Gwy3DViewLabel id,
                  GLfloat        raster_x,
                  GLfloat        raster_y,
                  GLfloat        raster_z,
                  guint          size,
                  gint           vjustify,
                  gint           hjustify)
{
    PangoContext *widget_context;
    PangoFontDescription *font_desc;
    PangoLayout *layout;
    PangoRectangle logical_rect;
    GLfloat text_w, text_h;
    guint hlp = 0;
    Gwy3DLabel *label;
    gint displacement_x, displacement_y;
    GtkAdjustment *adj;
    gchar *text;

    label = gwy3dview->labels[id];
    text = gwy_3d_label_expand_text(label, gwy3dview->variables);
    size = gwy_3d_label_user_size(label, size);
    adj = gwy_3d_label_get_delta_x_adjustment(label);
    displacement_x = gwy_adjustment_get_int(adj);
    adj = gwy_3d_label_get_delta_y_adjustment(label);
    displacement_y = gwy_adjustment_get_int(adj);

    /* Font */
    /* FIXME: is it possible for pango to write on trasnparent background? */
    widget_context = gtk_widget_get_pango_context(GTK_WIDGET(gwy3dview));
    font_desc = pango_context_get_font_description(widget_context);
    pango_font_description_set_size(font_desc, size * PANGO_SCALE);
    pango_context_set_font_description(gwy3dview->ft2_context, font_desc);

    /* Text layout */
    layout = pango_layout_new(gwy3dview->ft2_context);
    pango_layout_set_width(layout, -1);
    pango_layout_set_alignment(layout, PANGO_ALIGN_LEFT);
    pango_layout_set_markup(layout, text, -1);
    /* TODO: use Pango to rotate text, after Pango is capable doing it */

    /* Text position */
    pango_layout_get_pixel_extents(layout, NULL, &logical_rect);
    text_w = logical_rect.width;
    text_h = logical_rect.height;

    glRasterPos3f(raster_x, raster_y, raster_z);
    glBitmap(0, 0, 0, 0, displacement_x, displacement_y, (GLubyte *)&hlp);
    if (vjustify < 0) {
       /* vertically justified to the bottom */
    }
    else if (vjustify == 0) {
       /* vertically justified to the middle */
       glBitmap(0, 0, 0, 0, 0, -text_h/2, (GLubyte *)&hlp);
    }
    else {
       /* vertically justified to the top */
       glBitmap(0, 0, 0, 0, 0, -text_h, (GLubyte *)&hlp);
    }

    if (hjustify < 0) {
       /* horizontally justified to the left */
    }
    else if (hjustify == 0) {
       /* horizontally justified to the middle */
       glBitmap(0, 0, 0, 0, -text_w/2, 0, (GLubyte *)&hlp);
    }
    else {
       /* horizontally justified to the right */
       glBitmap(0, 0, 0, 0, -text_w, 0, (GLubyte *)&hlp);
    }

    /* Render text */
    gwy_3d_pango_ft2_render_layout(layout);

    g_object_unref(G_OBJECT(layout));

    return ;
}

/**
 * gwy_3d_view_class_make_list_pool:
 * @klass: 3D view class.
 *
 * Allocates a pool of OpenGL lists to assign list numbers from.
 *
 * We are only able to get GLContext-unique list id's from glGenLists(),
 * but lists with the same id's seem to interfere across GLContexts.  See
 * bug #53 for more.
 *
 * We store the pool in bits of one 64bit integer.
 **/
static void
gwy_3d_view_class_make_list_pool(Gwy3DListPool *pool)
{
    guint try_size = 64;

    glGetError();
    while (try_size >= 1) {
        pool->base = glGenLists(GWY_3D_N_LISTS*try_size);
        if (!glGetError()) {
            gwy_debug("Allocated a pool with %u items (%u lists)",
                      try_size, GWY_3D_N_LISTS*try_size);
            pool->size = try_size;
            return;
        }
        try_size = (try_size*2)/3;
    }
    g_warning("Cannot get any OpenGL lists");
    pool->base = 0;
}

/**
 * gwy_3d_view_allocate_lists:
 * @gwy3dview: A 3D data view widget.
 *
 * Allocates a block of free OpenGL lists from pool for this 3D view to use.
 **/
static void
gwy_3d_view_assign_lists(Gwy3DView *gwy3dview)
{
    Gwy3DListPool *pool;
    guint64 b;
    guint i;

    pool = GWY_3D_VIEW_GET_CLASS(gwy3dview)->list_pool;
    if (!pool->size) {
        g_return_if_fail(GTK_WIDGET_REALIZED(gwy3dview));
        gwy_3d_view_class_make_list_pool(pool);
    }
    g_return_if_fail(pool->size > 0);

    b = 1;
    for (i = 0; i < pool->size && (pool->pool & b); i++)
        b <<= 1;
    if (i == pool->size) {
        g_critical("No more free OpenGL lists");
        return;
    }

    gwy_debug("Assigned list #%u", i);
    pool->pool |= b;
    gwy3dview->shape_list_base = pool->base + i*GWY_3D_N_LISTS;
}

/**
 * gwy_3d_view_release_lists:
 * @gwy3dview: A 3D data view widget.
 *
 * Returns OpenGL lists in use by this 3D view back to the pool.
 **/
static void
gwy_3d_view_release_lists(Gwy3DView *gwy3dview)
{
    Gwy3DListPool *pool;
    guint i;
    guint64 b = 1;

    pool = GWY_3D_VIEW_GET_CLASS(gwy3dview)->list_pool;
    g_return_if_fail(gwy3dview->shape_list_base >= pool->base);

    i = (gwy3dview->shape_list_base - pool->base)/GWY_3D_N_LISTS;
    gwy_debug("Released list #%u", i);
    b <<= i;
    pool->pool &= ~b;
    gwy3dview->shape_list_base = 0;
}

#else /* HAVE_GTKGLEXT */
/* Export the same set of symbols if we don't have OpenGL support.  But let
 * them all fail. */
static void
gwy_3d_view_init(G_GNUC_UNUSED Gwy3DView *gwy3dview)
{
}

static void
gwy_3d_view_class_init(G_GNUC_UNUSED Gwy3DViewClass *klass)
{
}

GtkWidget*
gwy_3d_view_new(G_GNUC_UNUSED GwyContainer *data)
{
    g_critical("OpenGL support was not compiled in.");
    return NULL;
}

void
gwy_3d_view_update(G_GNUC_UNUSED Gwy3DView *gwy3dview)
{
    g_critical("OpenGL support was not compiled in.");
}

const gchar*
gwy_3d_view_get_data_key(G_GNUC_UNUSED Gwy3DView *gwy3dview)
{
    g_critical("OpenGL support was not compiled in.");
    return NULL;
}

void
gwy_3d_view_set_data_key(G_GNUC_UNUSED Gwy3DView *gwy3dview,
                         G_GNUC_UNUSED const gchar *key)
{
    g_critical("OpenGL support was not compiled in.");
}

const gchar*
gwy_3d_view_get_gradient_key(G_GNUC_UNUSED Gwy3DView *gwy3dview)
{
    g_critical("OpenGL support was not compiled in.");
    return NULL;
}

void
gwy_3d_view_set_gradient_key(G_GNUC_UNUSED Gwy3DView *gwy3dview,
                             G_GNUC_UNUSED const gchar *key)
{
    g_critical("OpenGL support was not compiled in.");
}

const gchar*
gwy_3d_view_get_material_key(G_GNUC_UNUSED Gwy3DView *gwy3dview)
{
    g_critical("OpenGL support was not compiled in.");
    return NULL;
}

void
gwy_3d_view_set_material_key(G_GNUC_UNUSED Gwy3DView *gwy3dview,
                             G_GNUC_UNUSED const gchar *key)
{
    g_critical("OpenGL support was not compiled in.");
}

Gwy3DMovement
gwy_3d_view_get_movement_type(G_GNUC_UNUSED Gwy3DView *gwy3dview)
{
    g_critical("OpenGL support was not compiled in.");
    return 0;
}

void
gwy_3d_view_set_movement_type(G_GNUC_UNUSED Gwy3DView *gwy3dview,
                              G_GNUC_UNUSED Gwy3DMovement movement)
{
    g_critical("OpenGL support was not compiled in.");
}

Gwy3DProjection
gwy_3d_view_get_projection(G_GNUC_UNUSED Gwy3DView *gwy3dview)
{
    g_critical("OpenGL support was not compiled in.");
    return 0;
}

void
gwy_3d_view_set_projection(G_GNUC_UNUSED Gwy3DView *gwy3dview,
                           G_GNUC_UNUSED Gwy3DProjection projection)
{
    g_critical("OpenGL support was not compiled in.");
}

gboolean
gwy_3d_view_get_show_axes(G_GNUC_UNUSED Gwy3DView *gwy3dview)
{
    g_critical("OpenGL support was not compiled in.");
    return FALSE;
}

void
gwy_3d_view_set_show_axes(G_GNUC_UNUSED Gwy3DView *gwy3dview,
                          G_GNUC_UNUSED gboolean show_axes)
{
    g_critical("OpenGL support was not compiled in.");
}

gboolean
gwy_3d_view_get_show_labels(G_GNUC_UNUSED Gwy3DView *gwy3dview)
{
    g_critical("OpenGL support was not compiled in.");
    return FALSE;
}

void
gwy_3d_view_set_show_labels(G_GNUC_UNUSED Gwy3DView *gwy3dview,
                            G_GNUC_UNUSED gboolean show_labels)
{
    g_critical("OpenGL support was not compiled in.");
}

Gwy3DVisualization
gwy_3d_view_get_visualization(G_GNUC_UNUSED Gwy3DView *gwy3dview)
{
    g_critical("OpenGL support was not compiled in.");
    return 0;
}

void
gwy_3d_view_set_visualization(G_GNUC_UNUSED Gwy3DView *gwy3dview,
                              G_GNUC_UNUSED Gwy3DVisualization visual)
{
    g_critical("OpenGL support was not compiled in.");
}

guint
gwy_3d_view_get_reduced_size(G_GNUC_UNUSED Gwy3DView *gwy3dview)
{
    g_critical("OpenGL support was not compiled in.");
    return 0;
}

void
gwy_3d_view_set_reduced_size(G_GNUC_UNUSED Gwy3DView *gwy3dview,
                             G_GNUC_UNUSED guint reduced_size)
{
    g_critical("OpenGL support was not compiled in.");
}

GdkPixbuf*
gwy_3d_view_get_pixbuf(G_GNUC_UNUSED Gwy3DView *gwy3dview)
{
    g_critical("OpenGL support was not compiled in.");
    return NULL;
}

Gwy3DLabel*
gwy_3d_view_get_label(G_GNUC_UNUSED Gwy3DView *gwy3dview,
                      G_GNUC_UNUSED Gwy3DViewLabel label)
{
    g_critical("OpenGL support was not compiled in.");
    return NULL;
}

GwyContainer*
gwy_3d_view_get_data(G_GNUC_UNUSED Gwy3DView *gwy3dview)
{
    g_critical("OpenGL support was not compiled in.");
    return NULL;
}

void
gwy_3d_view_reset_view(G_GNUC_UNUSED Gwy3DView *gwy3dview)
{
    g_critical("OpenGL support was not compiled in.");
}

GtkAdjustment*
gwy_3d_view_get_rot_x_adjustment(G_GNUC_UNUSED Gwy3DView *gwy3dview)
{
    g_critical("OpenGL support was not compiled in.");
    return NULL;
}

GtkAdjustment*
gwy_3d_view_get_rot_y_adjustment(G_GNUC_UNUSED Gwy3DView *gwy3dview)
{
    g_critical("OpenGL support was not compiled in.");
    return NULL;
}

GtkAdjustment*
gwy_3d_view_get_view_scale_adjustment(G_GNUC_UNUSED Gwy3DView *gwy3dview)
{
    g_critical("OpenGL support was not compiled in.");
    return NULL;
}

GtkAdjustment*
gwy_3d_view_get_z_deformation_adjustment(G_GNUC_UNUSED Gwy3DView *gwy3dview)
{
    g_critical("OpenGL support was not compiled in.");
    return NULL;
}

GtkAdjustment*
gwy_3d_view_get_light_z_adjustment(G_GNUC_UNUSED Gwy3DView *gwy3dview)
{
    g_critical("OpenGL support was not compiled in.");
    return NULL;
}

GtkAdjustment*
gwy_3d_view_get_light_y_adjustment(G_GNUC_UNUSED Gwy3DView *gwy3dview)
{
    g_critical("OpenGL support was not compiled in.");
    return NULL;
}

gdouble
gwy_3d_view_get_max_view_scale(G_GNUC_UNUSED Gwy3DView *gwy3dview)
{
    g_critical("OpenGL support was not compiled in.");
    return 0.0;
}

gdouble
gwy_3d_view_get_min_view_scale(G_GNUC_UNUSED Gwy3DView *gwy3dview)
{
    g_critical("OpenGL support was not compiled in.");
    return 0.0;
}

void
gwy_3d_view_set_max_view_scale(G_GNUC_UNUSED Gwy3DView *gwy3dview,
                               G_GNUC_UNUSED gdouble new_max_scale)
{
    g_critical("OpenGL support was not compiled in.");
}

void
gwy_3d_view_set_min_view_scale(G_GNUC_UNUSED Gwy3DView *gwy3dview,
                               G_GNUC_UNUSED gdouble new_min_scale)
{
    g_critical("OpenGL support was not compiled in.");
}
#endif /* HAVE_GTKGLEXT */

/************************** Documentation ****************************/

/**
 * SECTION:gwy3dview
 * @title: Gwy3DView
 * @short_description: OpenGL 3D data display
 * @see_also: #Gwy3DWindow -- window combining 3D view with controls,
 *            #GwyGLMaterial -- OpenGL materials
 *
 * #Gwy3DView displays a data field as a threedimensional heightfield using
 * OpenGL. You can create a new 3D view for a data container with
 * gwy_3d_view_new().  By default, it inherits properties like palette from
 * <link linkend="GwyDataView">data view</link> settings, but supports separate
 * settings -- see gwy_3d_view_set_palette() et al.
 *
 * #Gwy3DView allows the user to interactively rotate, scale, z-scale the data
 * or move lights, depending on its <link linkend="Gwy3DMovement">movement
 * state</link>. There are no controls provided for mode change, you have to
 * provide some yourself and set the movement mode with
 * gwy_3d_view_set_movement_type(). There are #GtkAdjustment's for each view
 * parameter, you can fetch them with gwy_3d_view_get_rot_x_adjustment(),
 * gwy_3d_view_get_rot_y_adjustment(), etc.
 *
 * You have initialize GtkGLExt with gtk_gl_init_check() and then Gwyddion's
 * OpenGL with gwy_widgets_gl_init() before you can use #Gwy3DView.  These
 * functions may not always succeed, see their description for more.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
