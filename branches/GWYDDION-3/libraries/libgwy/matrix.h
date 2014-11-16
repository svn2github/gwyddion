/*
 *  $Id$
 *  Copyright (C) 2014 David Nečas (Yeti).
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

#ifndef __LIBGWY_MATRIX_H__
#define __LIBGWY_MATRIX_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef enum {
    GWY_MATRIX_MULTIPLY         = 1 << 0,
    GWY_MATRIX_INV_MULTIPLY     = 1 << 1,
    GWY_MATRIX_GET_DIAGONAL     = 1 << 2,
    GWY_MATRIX_GET_INV_DIAGONAL = 1 << 3,
    GWY_MATRIX_SET_DIAGONAL     = 1 << 4,
} GwyMatrixOperation;

typedef struct _GwyMatrix GwyMatrix;

typedef gboolean (*GwyMatrixMultiplyFunc)(GwyMatrix *matrix,
                                          const gdouble *vector,
                                          gdouble *result);
typedef gboolean (*GwyMatrixGetDiagFunc) (GwyMatrix *matrix,
                                          gdouble *diag);
typedef gboolean (*GwyMatrixSetDiagFunc) (GwyMatrix *matrix,
                                          const gdouble *diag);

GwyMatrix* gwy_matrix_new                  (guint nrows,
                                            guint ncols,
                                            gpointer model,
                                            GDestroyNotify destroy)        G_GNUC_MALLOC;
void       gwy_matrix_ref                  (GwyMatrix *matrix);
void       gwy_matrix_unref                (GwyMatrix *matrix);
gboolean   gwy_matrix_check_operations     (const GwyMatrix *matrix,
                                            GwyMatrixOperation ops);
void       gwy_matrix_set_multiply_func    (GwyMatrix *matrix,
                                            GwyMatrixMultiplyFunc func);
void       gwy_matrix_set_inv_multiply_func(GwyMatrix *matrix,
                                            GwyMatrixMultiplyFunc func);
void       gwy_matrix_set_diagonal_funcs   (GwyMatrix *matrix,
                                            GwyMatrixGetDiagFunc getdiag,
                                            GwyMatrixSetDiagFunc setdiag);
void       gwy_matrix_set_inv_diagonal_func(GwyMatrix *matrix,
                                            GwyMatrixGetDiagFunc getdiag);
gpointer   gwy_matrix_model                (const GwyMatrix *matrix)       G_GNUC_PURE;
guint      gwy_matrix_n_rows               (const GwyMatrix *matrix)       G_GNUC_PURE;
guint      gwy_matrix_n_cols               (const GwyMatrix *matrix)       G_GNUC_PURE;
gboolean   gwy_matrix_multiply             (GwyMatrix *matrix,
                                            const gdouble *vector,
                                            gdouble *result);
gboolean   gwy_matrix_inv_multiply         (GwyMatrix *matrix,
                                            const gdouble *vector,
                                            gdouble *result);
gboolean   gwy_matrix_get_diagonal         (GwyMatrix *matrix,
                                            gdouble *diag);
gboolean   gwy_matrix_get_inv_diagonal     (GwyMatrix *matrix,
                                            gdouble *invdiag);
gboolean   gwy_matrix_set_diagonal         (GwyMatrix *matrix,
                                            const gdouble *diag);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
