/*
 *  $Id$
 *  Copyright (C) 2009-2011 David Nečas (Yeti).
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

#ifndef __TESTLIBGWY_H__
#define __TESTLIBGWY_H__ 1

#undef G_DISABLE_ASSERT
#include "config.h"
#include "libgwy/libgwy.h"

#ifndef HAVE_EXP10
#define exp10(x) pow(10.0, (x))
#endif

#define TEST_DATA_DIR "_testdata"

typedef void (*CompareObjectDataFunc)(GObject *object, GObject *reference);

// Helpers
void          base62_format                 (guint x,
                                             gchar *out,
                                             guint outsize);
void          assert_error_list             (GwyErrorList *error_list,
                                             GwyErrorList *expected_errors);
void          dump_error_list               (GwyErrorList *error_list);
gboolean      values_are_equal              (const GValue *value1,
                                             const GValue *value2);
GObject*      serialize_and_back            (GObject *object,
                                             CompareObjectDataFunc compare);
gboolean      compare_properties            (GObject *object,
                                             GObject *reference);
void          serializable_duplicate        (GwySerializable *serializable,
                                             CompareObjectDataFunc compare);
void          serializable_assign           (GwySerializable *serializable,
                                             CompareObjectDataFunc compare);
gpointer      serialize_boxed_and_back      (gpointer boxed,
                                             GType type);
void          deserialize_assert_failure    (GMemoryOutputStream *stream,
                                             GwyErrorList *expected_errors);
gboolean      data_stream_put_double        (GDataOutputStream *stream,
                                             gdouble data,
                                             GCancellable *cancellable,
                                             GError **error);
gboolean      data_stream_put_string0       (GDataOutputStream *stream,
                                             const gchar *str,
                                             GCancellable *cancellable,
                                             GError **error);
void          record_item_change            (GObject *object,
                                             guint pos,
                                             guint64 *counter);
void          record_signal                 (guint *counter);
void          int_set_randomize_range       (GwyIntSet *intset,
                                             GRand *rng,
                                             gint min,
                                             gint max);
void          int_set_assert_equal          (const GwyIntSet *result,
                                             const GwyIntSet *reference);
void          unit_randomize                (GwyUnit *unit,
                                             GRand *rng);
void          mask_line_print               (const gchar *name,
                                             const GwyMaskLine *maskline);
GwyMaskField* random_mask_field             (guint xres,
                                             guint yres,
                                             GRand *rng);
GwyMaskField* random_mask_field_prob        (guint xres,
                                             guint yres,
                                             GRand *rng,
                                             gdouble probability);
void          mask_field_print              (const gchar *name,
                                             const GwyMaskField *maskfield);
void          mask_field_print_grains       (const gchar *name,
                                             const GwyMaskField *maskfield,
                                             guint grain_id);
void          curve_randomize               (GwyCurve *field,
                                             GRand *rng);
void          field_randomize               (GwyField *field,
                                             GRand *rng);
void          field_print                   (const gchar *name,
                                             const GwyField *field);
void          field_print_row               (const gchar *name,
                                             const gdouble *data,
                                             gsize size);
void          field_assert_equal            (const GwyField *result,
                                             const GwyField *reference);
void          field_assert_numerically_equal(const GwyField *result,
                                             const GwyField *reference,
                                             gdouble eps);
void          line_randomize                (GwyLine *field,
                                             GRand *rng);
void          line_print                    (const gchar *name,
                                             const GwyLine *line);
void          line_assert_equal             (const GwyLine *result,
                                             const GwyLine *reference);
void          line_assert_numerically_equal (const GwyLine *result,
                                             const GwyLine *reference,
                                             gdouble eps);
void          brick_print                   (const gchar *name,
                                             const GwyBrick *brick);
void          brick_part_assert_equal       (const GwyBrickPart *part,
                                             const GwyBrickPart *refpart);
void          field_part_assert_equal       (const GwyFieldPart *part,
                                             const GwyFieldPart *refpart);
void          line_part_assert_equal        (const GwyLinePart *part,
                                             const GwyLinePart *refpart);
void          check_congruence_group_sanity (void);
void          resource_check_file           (GwyResource *resource,
                                             const gchar *filename);
void          resource_assert_load_error    (const gchar *string,
                                             GType type,
                                             guint errdomain,
                                             gint errcode);

extern const guint plane_congruence_group[8][8];

GType gwy_ser_test_get_type(void) G_GNUC_CONST;

typedef struct _GwySerTest      GwySerTest;
typedef struct _GwySerTestClass GwySerTestClass;

#define GWY_TYPE_SER_TEST \
    (gwy_ser_test_get_type())
#define GWY_SER_TEST(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_SER_TEST, GwySerTest))
#define GWY_IS_SER_TEST(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_SER_TEST))
#define GWY_SER_TEST_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_SER_TEST, GwySerTestClass))

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
