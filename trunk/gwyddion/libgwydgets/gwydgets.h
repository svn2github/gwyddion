/* @(#) $Id$ */

#ifndef __GWY_GWYDGETS_H__
#define __GWY_GWYDGETS_H__

#include <libgwydgets/gwyaxis.h>
#include <libgwydgets/gwyaxisdialog.h>
#include <libgwydgets/gwycoloraxis.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwydataviewlayer.h>
#include <libgwydgets/gwydatawindow.h>
#include <libgwydgets/gwygradsphere.h>
#include <libgwydgets/gwygraph.h>
#include <libgwydgets/gwygrapharea.h>
#include <libgwydgets/gwygraphcorner.h>
#include <libgwydgets/gwygraphlabel.h>
#include <libgwydgets/gwyhruler.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwylayer-lines.h>
#include <libgwydgets/gwylayer-mask.h>
#include <libgwydgets/gwylayer-pointer.h>
#include <libgwydgets/gwylayer-points.h>
#include <libgwydgets/gwylayer-select.h>
#include <libgwydgets/gwyruler.h>
#include <libgwydgets/gwyscitext.h>
#include <libgwydgets/gwyspherecoords.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwyvectorshade.h>
#include <libgwydgets/gwyvruler.h>
#include <libprocess/cwt.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct {
    const gchar *name;
    gint value;
} GwyOptionMenuEntry;

GtkWidget* gwy_palette_menu               (GCallback callback,
                                           gpointer cbdata);
GtkWidget* gwy_palette_option_menu        (GCallback callback,
                                           gpointer cbdata,
                                           const gchar *current);
GtkWidget* gwy_interpolation_option_menu  (GCallback callback,
                                           gpointer cbdata,
                                           GwyInterpolationType current);
GtkWidget* gwy_windowing_option_menu      (GCallback callback,
                                           gpointer cbdata,
                                           GwyWindowingType current);
GtkWidget* gwy_zoom_mode_option_menu      (GCallback callback,
                                           gpointer cbdata,
                                           GwyZoomMode current);
GtkWidget* gwy_2dcwt_option_menu          (GCallback callback,
                                           gpointer cbdata,
                                           Gwy2DCWTWaveletType current);
GtkWidget* gwy_option_menu_create         (const GwyOptionMenuEntry *entries,
                                           gint nentries,
                                           const gchar *key,
                                           GCallback callback,
                                           gpointer cbdata,
                                           gint current);
gboolean   gwy_option_menu_set_history    (GtkWidget *option_menu,
                                           const gchar *key,
                                           gint current);

GtkWidget* gwy_table_attach_spinbutton    (GtkWidget *table,
                                           gint row,
                                           const gchar *name,
                                           const gchar *units,
                                           GtkObject *adj);
void       gwy_table_attach_row           (GtkWidget *table,
                                           gint row,
                                           const gchar *name,
                                           const gchar *units,
                                           GtkWidget *middle_widget);

gboolean   gwy_dialog_prevent_delete_cb   (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GWY_GWYDGETS_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
