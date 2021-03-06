#ifndef __GWY_CONTAINER_H__
#define __GWY_CONTAINER_H__

#include <gdk/gdk.h>
#include <gtk/gtkobject.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GWY_TYPE_CONTAINER                  (gwy_container_get_type ())
#define GWY_CONTAINER(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GWY_TYPE_CONTAINER, GwyContainer))
#define GWY_CONTAINER_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GWY_TYPE_CONTAINER, GwyContainerClass))
#define GWY_IS_CONTAINER(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GWY_TYPE_CONTAINER))
#define GWY_IS_CONTAINER_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GWY_TYPE_CONTAINER))
#define GWY_CONTAINER_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GWY_TYPE_CONTAINER, GwyContainerClass))

#define GWY_CONTAINER_PATHSEP      '/'
#define GWY_CONTAINER_PATHSEP_STR  "/"

typedef struct _GwyContainer GwyContainer;
typedef struct _GwyContainerClass GwyContainerClass;

typedef struct {
    GQuark key;
    GValue *value;
    gboolean changed;
} GwyKeyVal;

struct _GwyContainer {
    GObject parent_instance;

    GHashTable *values;
    GHashTable *watching;
};

struct _GwyContainerClass {
    GObjectClass parent_class;
};


GType      gwy_container_get_type           (void) G_GNUC_CONST;
GObject*   gwy_container_new                (void);
GType      gwy_container_value_type         (GwyContainer *container,
                                             GQuark key);
GValue     gwy_container_get_value          (GwyContainer *container,
                                             GQuark key);
void       gwy_container_set_value          (GwyContainer *container,
                                             ...);
void       gwy_container_set_value_by_name  (GwyContainer *container,
                                             ...);
gboolean   gwy_container_delete             (GwyContainer *container,
                                             GQuark key);
gboolean   gwy_container_foreach            (GwyContainer *container,
                                             const guchar *prefix,
                                             gpointer foo);

void       gwy_container_set_boolean        (GwyContainer *container,
                                             GQuark key,
                                             gboolean value);
gboolean   gwy_container_get_boolean        (GwyContainer *container,
                                             GQuark key);
void       gwy_container_set_char           (GwyContainer *container,
                                             GQuark key,
                                             guchar value);
guchar     gwy_container_get_char           (GwyContainer *container,
                                             GQuark key);
void       gwy_container_set_int32          (GwyContainer *container,
                                             GQuark key,
                                             gint32 value);
gint32     gwy_container_get_int32          (GwyContainer *container,
                                             GQuark key);
void       gwy_container_set_int64          (GwyContainer *container,
                                             GQuark key,
                                             gint64 value);
gint64     gwy_container_get_int64          (GwyContainer *container,
                                             GQuark key);
void       gwy_container_set_double         (GwyContainer *container,
                                             GQuark key,
                                             gdouble value);
gdouble    gwy_container_get_double         (GwyContainer *container,
                                             GQuark key);

#define gwy_container_value_type_by_name(c,n) gwy_container_value_type(c,g_quark_try_string(n))
#define gwy_container_get_value_by_name(c,n) gwy_container_get_value(c,g_quark_try_string(n))
#define gwy_container_delete_by_name(c,n) gwy_container_delete(c,g_quark_try_string(n))
#define gwy_container_set_boolean_by_name(c,n,v) gwy_container_set_boolean(c,g_quark_try_string(n),v)
#define gwy_container_get_boolean_by_name(c,n) gwy_container_get_boolean(c,g_quark_try_string(n))
#define gwy_container_set_char_by_name(c,n,v) gwy_container_set_char(c,g_quark_try_string(n),v)
#define gwy_container_get_char_by_name(c,n) gwy_container_get_char(c,g_quark_try_string(n))
#define gwy_container_set_int32_by_name(c,n,v) gwy_container_set_int32(c,g_quark_try_string(n),v)
#define gwy_container_get_int32_by_name(c,n) gwy_container_get_int32(c,g_quark_try_string(n))
#define gwy_container_set_int64_by_name(c,n,v) gwy_container_set_int64(c,g_quark_try_string(n),v)
#define gwy_container_get_int64_by_name(c,n) gwy_container_get_int64(c,g_quark_try_string(n))
#define gwy_container_set_double_by_name(c,n,v) gwy_container_set_double(c,g_quark_try_string(n),v)
#define gwy_container_get_double_by_name(c,n) gwy_container_get_double(c,g_quark_try_string(n))

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GWY_CONTAINER_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
