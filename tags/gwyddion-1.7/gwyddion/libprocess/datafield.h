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

#ifndef __GWY_DATAFIELD_H__
#define __GWY_DATAFIELD_H__

#include <glib.h>
/* XXX: GtkOrientation */
#include <gtk/gtkenums.h>
#include <libprocess/interpolation.h>
#include <libprocess/dataline.h>
#include <libgwyddion/gwysiunit.h>

G_BEGIN_DECLS

#define GWY_TYPE_DATA_FIELD                  (gwy_data_field_get_type())
#define GWY_DATA_FIELD(obj)                  (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_DATA_FIELD, GwyDataField))
#define GWY_DATA_FIELD_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_DATA_FIELD, GwyDataFieldClass))
#define GWY_IS_DATA_FIELD(obj)               (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_DATA_FIELD))
#define GWY_IS_DATA_FIELD_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_DATA_FIELD))
#define GWY_DATA_FIELD_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_DATA_FIELD, GwyDataFieldClass))

typedef struct _GwyDataField      GwyDataField;
typedef struct _GwyDataFieldClass GwyDataFieldClass;

struct _GwyDataField {
    GObject parent_instance;

    gint xres;
    gint yres;
    gdouble xreal;
    gdouble yreal;
    gdouble *data;
    gint N;  /* FIXME: remove, not used, not even updated */
    GwySIUnit *si_unit_xy;
    GwySIUnit *si_unit_z;
};

struct _GwyDataFieldClass {
    GObjectClass parent_class;
};

typedef enum {
    GWY_COMP_INIT         = 0, /*start initializations*/
    GWY_COMP_ITERATE      = 1, /*locate steps*/
    GWY_COMP_FINISHED     = 2
} GwyComputationStateType;

G_END_DECLS

/* XXX: This is here to allow people #include just datafield.h and get all
 * datafield-related functions as before, will be removed in 2.0.
 * Do NOT add new headers here! Add them to gwyprocess.h. */
#include <libprocess/correct.h>
#include <libprocess/correlation.h>
#include <libprocess/cwt.h>
#include <libprocess/fractals.h>
#include <libprocess/filters.h>
#include <libprocess/grains.h>
#include <libprocess/inttrans.h>
#include <libprocess/level.h>
#include <libprocess/stats.h>

G_BEGIN_DECLS

GType gwy_data_field_get_type  (void) G_GNUC_CONST;

/* FIXME: in 2.0 it should actually invalidate cached statistics.
 * Valid cached values will be probably bits in a bit field, so it will
 * simply expand to data_field->valid = 0 to clear them all. */
#define gwy_data_field_invalidate(data_field) /* */

GObject*         gwy_data_field_new                  (gint xres,
                                                      gint yres,
                                                      gdouble xreal,
                                                      gdouble yreal,
                                                      gboolean nullme);
void              gwy_data_field_resample  (GwyDataField *data_field,
                                            gint xres,
                                            gint yres,
                                            GwyInterpolationType interpolation);
gboolean          gwy_data_field_resize              (GwyDataField *data_field,
                                                      gint ulcol,
                                                      gint ulrow,
                                                      gint brcol,
                                                      gint brrow);
gboolean          gwy_data_field_copy                (GwyDataField *a,
                                                      GwyDataField *b);
gboolean          gwy_data_field_area_copy           (GwyDataField *src,
                                                      GwyDataField *dest,
                                                      gint ulcol,
                                                      gint ulrow,
                                                      gint brcol,
                                                      gint brrow,
                                                      gint destcol,
                                                      gint destrow);
gdouble*          gwy_data_field_get_data            (GwyDataField *data_field);
const gdouble*    gwy_data_field_get_data_const      (GwyDataField *data_field);
gint              gwy_data_field_get_xres            (GwyDataField *data_field);
gint              gwy_data_field_get_yres            (GwyDataField *data_field);
gdouble           gwy_data_field_get_xreal           (GwyDataField *data_field);
gdouble           gwy_data_field_get_yreal           (GwyDataField *data_field);
void              gwy_data_field_set_xreal           (GwyDataField *data_field,
                                                      gdouble xreal);
void              gwy_data_field_set_yreal           (GwyDataField *data_field,
                                                      gdouble yreal);
GwySIUnit*        gwy_data_field_get_si_unit_xy      (GwyDataField *data_field);
GwySIUnit*        gwy_data_field_get_si_unit_z       (GwyDataField *data_field);
void              gwy_data_field_set_si_unit_xy      (GwyDataField *data_field,
                                                      GwySIUnit *si_unit);
void              gwy_data_field_set_si_unit_z       (GwyDataField *data_field,
                                                      GwySIUnit *si_unit);
GwySIValueFormat* gwy_data_field_get_value_format_xy (GwyDataField *data_field,
                                                      GwySIValueFormat *format);
GwySIValueFormat* gwy_data_field_get_value_format_z  (GwyDataField *data_field,
                                                      GwySIValueFormat *format);

gdouble           gwy_data_field_itor                (GwyDataField *data_field,
                                                      gdouble row);
gdouble           gwy_data_field_jtor                (GwyDataField *data_field,
                                                      gdouble col);
gdouble           gwy_data_field_rtoi                (GwyDataField *data_field,
                                                      gdouble realy);
gdouble           gwy_data_field_rtoj                (GwyDataField *data_field,
                                                      gdouble realx);


gboolean          gwy_data_field_outside             (GwyDataField *data_field,
                                                      gint col,
                                                      gint row);
gdouble           gwy_data_field_get_val             (GwyDataField *data_field,
                                                      gint col,
                                                      gint row);
void              gwy_data_field_set_val             (GwyDataField *data_field,
                                                      gint col,
                                                      gint row,
                                                      gdouble value);
gdouble  gwy_data_field_get_dval           (GwyDataField *data_field,
                                            gdouble x,
                                            gdouble y,
                                            GwyInterpolationType interpolation);
gdouble  gwy_data_field_get_dval_real      (GwyDataField *data_field,
                                            gdouble x,
                                            gdouble y,
                                            GwyInterpolationType interpolation);
void     gwy_data_field_rotate             (GwyDataField *data_field,
                                            gdouble angle,
                                            GwyInterpolationType interpolation);
void     gwy_data_field_invert             (GwyDataField *data_field,
                                            gboolean x,
                                            gboolean y,
                                            gboolean z);
void     gwy_data_field_fill               (GwyDataField *data_field,
                                            gdouble value);
void     gwy_data_field_multiply           (GwyDataField *data_field,
                                            gdouble value);
void     gwy_data_field_add                (GwyDataField *data_field,
                                            gdouble value);
void     gwy_data_field_area_fill          (GwyDataField *data_field,
                                            gint ulcol,
                                            gint ulrow,
                                            gint brcol,
                                            gint brrow,
                                            gdouble value);
void     gwy_data_field_area_multiply      (GwyDataField *data_field,
                                            gint ulcol,
                                            gint ulrow,
                                            gint brcol,
                                            gint brrow,
                                            gdouble value);
void     gwy_data_field_area_add           (GwyDataField *data_field,
                                            gint ulcol,
                                            gint ulrow,
                                            gint brcol,
                                            gint brrow,
                                            gdouble value);
gint     gwy_data_field_threshold          (GwyDataField *data_field,
                                            gdouble threshval,
                                            gdouble bottom,
                                            gdouble top);
gint     gwy_data_field_area_threshold     (GwyDataField *data_field,
                                            gint ulcol,
                                            gint ulrow,
                                            gint brcol,
                                            gint brrow,
                                            gdouble threshval,
                                            gdouble bottom,
                                            gdouble top);
gint     gwy_data_field_clamp              (GwyDataField *data_field,
                                            gdouble bottom,
                                            gdouble top);
gint     gwy_data_field_area_clamp         (GwyDataField *data_field,
                                            gint ulcol,
                                            gint ulrow,
                                            gint brcol,
                                            gint brrow,
                                            gdouble bottom,
                                            gdouble top);
gboolean gwy_data_field_get_data_line      (GwyDataField *data_field,
                                            GwyDataLine* data_line,
                                            gint ulcol,
                                            gint ulrow,
                                            gint brcol,
                                            gint brrow,
                                            gint res,
                                            GwyInterpolationType interpolation);
gboolean gwy_data_field_get_data_line_averaged(GwyDataField *data_field,
                                            GwyDataLine* data_line,
                                            gint ulcol,
                                            gint ulrow,
                                            gint brcol,
                                            gint brrow,
                                            gint res,
                                            gint thickness,
                                            GwyInterpolationType interpolation);
void     gwy_data_field_get_row            (GwyDataField *data_field,
                                            GwyDataLine* data_line,
                                            gint row);
void     gwy_data_field_get_column         (GwyDataField *data_field,
                                            GwyDataLine* data_line,
                                            gint col);
void     gwy_data_field_set_row            (GwyDataField *data_field,
                                            GwyDataLine* data_line,
                                            gint row);
void     gwy_data_field_set_column         (GwyDataField *data_field,
                                            GwyDataLine* data_line,
                                            gint col);
void     gwy_data_field_get_row_part       (GwyDataField *data_field,
                                            GwyDataLine* data_line,
                                            gint row,
                                            gint from,
                                            gint to);
void     gwy_data_field_get_column_part    (GwyDataField *data_field,
                                            GwyDataLine* data_line,
                                            gint col,
                                            gint from,
                                            gint to);
void     gwy_data_field_set_row_part       (GwyDataField *data_field,
                                            GwyDataLine* data_line,
                                            gint row,
                                            gint from,
                                            gint to);
void     gwy_data_field_set_column_part    (GwyDataField *data_field,
                                            GwyDataLine* data_line,
                                            gint col,
                                            gint from,
                                            gint to);
gdouble  gwy_data_field_get_xder           (GwyDataField *data_field,
                                            gint col,
                                            gint row);
gdouble  gwy_data_field_get_yder           (GwyDataField *data_field,
                                            gint col,
                                            gint row);
gdouble  gwy_data_field_get_angder         (GwyDataField *data_field,
                                            gint col,
                                            gint row,
                                            gdouble theta);
void     gwy_data_field_shade              (GwyDataField *data_field,
                                            GwyDataField *target_field,
                                            gdouble theta,
                                            gdouble phi);
void     gwy_data_field_fit_lines          (GwyDataField *data_field,
                                            gint ulcol,
                                            gint ulrow,
                                            gint brcol,
                                            gint brrow,
                                            GwyFitLineType fit_type,
                                            gboolean exclude,
                                            GtkOrientation orientation);

G_END_DECLS

#endif /*__GWY_DATAFIELD_H__*/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
