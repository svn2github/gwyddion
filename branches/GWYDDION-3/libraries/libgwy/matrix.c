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

#include "libgwy/matrix.h"

struct _GwyMatrix {
    guint ref_count;
    guint nrows;
    guint ncols;
    gpointer model;
    GDestroyNotify destroy;
    GwyMatrixMultiplyFunc multiply;
    GwyMatrixMultiplyFunc inv_multiply;
    GwyMatrixGetDiagFunc get_diag;
    GwyMatrixSetDiagFunc set_diag;
};

/**
 * gwy_matrix_new:
 * @nrows: Number of rows.
 * @ncols: Number of columns.
 * @model: (allow-none):
 *         Convenience pointer for representing the data.
 * @destroy: Function to call on @model
 *
 * Creates a new abstract matrix.
 *
 * A newly created matrix cannot be used in any operations.  It is necessary to
 * set the functions actually performing algebraic operations using functions
 * such as gwy_matrix_set_multiply_func().
 *
 * The number of rows and columns cannot change during the matrix lifetime.
 * Note they row and column argument order is usual for matrices, but opposite
 * to the common convention in Gwyddion for images.
 *
 * Returns: A newly created abstract matrix.
 **/
GwyMatrix*
gwy_matrix_new(guint nrows, guint ncols,
               gpointer model,
               GDestroyNotify destroy)
{
    g_assert(nrows);
    g_assert(ncols);

    GwyMatrix *matrix = g_slice_new0(GwyMatrix);
    matrix->ref_count = 1;
    matrix->model = model;
    matrix->destroy = destroy;
    matrix->ncols = ncols;
    matrix->nrows = nrows;

    return matrix;
}

/**
 * gwy_matrix_ref:
 * @matrix: An abstract matrix.
 *
 * Increases the reference count of an abstract matrix.
 **/
void
gwy_matrix_ref(GwyMatrix *matrix)
{
    g_return_if_fail(matrix);
    matrix->ref_count++;
}

/**
 * gwy_matrix_unref:
 * @matrix: An abstract matrix.
 *
 * Decreases the reference count of an abstract matrix, possibly freeing it.
 **/
void
gwy_matrix_unref(GwyMatrix *matrix)
{
    g_return_if_fail(matrix);
    g_return_if_fail(matrix->ref_count);
    if (--matrix->ref_count)
        return;

    if (matrix->destroy)
        matrix->destroy(matrix->model);

    g_slice_free(GwyMatrix, matrix);
}

/**
 * gwy_matrix_set_multiply_func:
 * @matrix: An abstract matrix.
 * @func: Multiplication function.
 *
 * Sets the function performing multiplication of a vector for an abstract
 * matrix.
 **/
void
gwy_matrix_set_multiply_func(GwyMatrix *matrix,
                             GwyMatrixMultiplyFunc func)
{
    g_return_if_fail(matrix);
    matrix->multiply = func;
}

/**
 * gwy_matrix_set_inv_multiply_func:
 * @matrix: An abstract matrix.
 * @func: Inverse multiplication function.
 *
 * Sets the function performing multiplication of a vector by inverse matrix
 * for an abstract matrix.
 *
 * Obviously, this function only makes sense if the matrix is square.
 **/
void
gwy_matrix_set_inv_multiply_func(GwyMatrix *matrix,
                                 GwyMatrixMultiplyFunc func)
{
    g_return_if_fail(matrix);
    g_warn_if_fail(matrix->nrows == matrix->ncols);
    matrix->inv_multiply = func;
}

/**
 * gwy_matrix_set_diagonal_funcs:
 * @matrix: An abstract matrix.
 * @getdiag: Diagonal extraction function.
 * @setdiag: Diagonal replacement function.
 *
 * Sets the diagonal manipulation functions for an abstract matrix.
 *
 * Obviously, this function only makes sense if the matrix is square.
 **/
void
gwy_matrix_set_diagonal_funcs(GwyMatrix *matrix,
                              GwyMatrixGetDiagFunc getdiag,
                              GwyMatrixSetDiagFunc setdiag)
{
    g_return_if_fail(matrix);
    g_warn_if_fail(matrix->nrows == matrix->ncols);
    matrix->set_diag = setdiag;
    matrix->get_diag = getdiag;
}

/**
 * gwy_matrix_model:
 * @matrix: An abstract matrix.
 *
 * Obtains the model pointer of an abstract matrix.
 *
 * Returns: The model pointer given to gwy_matrix_new().
 **/
gpointer
gwy_matrix_model(const GwyMatrix *matrix)
{
    g_return_val_if_fail(matrix, NULL);
    return matrix->model;
}

/**
 * gwy_matrix_n_rows:
 * @matrix: An abstract matrix.
 *
 * Obtains the number of rows of an abstract matrix.
 *
 * Returns: The number of rows.
 **/
guint
gwy_matrix_n_rows(const GwyMatrix *matrix)
{
    g_return_val_if_fail(matrix, 0);
    return matrix->nrows;
}

/**
 * gwy_matrix_n_cols:
 * @matrix: An abstract matrix.
 *
 * Obtains the number of columns of an abstract matrix.
 *
 * Returns: The number of columns.
 **/
guint
gwy_matrix_n_cols(const GwyMatrix *matrix)
{
    g_return_val_if_fail(matrix, 0);
    return matrix->ncols;
}

/**
 * gwy_matrix_multiply:
 * @matrix: An abstract matrix.
 * @vector: Vector to multiply.  The array must have the same number of
 *          elements as the matrix columns.
 * @result: Vector to put the result to.  The array must have the same number
 *          of elements as the matrix rows.
 *
 * Multiplies a vector from left by an abstract matrix.
 *
 * Returns: %TRUE if the operation succeeded, %FALSE on failure (including
 *          the case when no multiplication function is set).
 **/
gboolean
gwy_matrix_multiply(GwyMatrix *matrix,
                    const gdouble *vector,
                    gdouble *result)
{
    g_return_val_if_fail(matrix, FALSE);
    g_return_val_if_fail(vector, FALSE);
    g_return_val_if_fail(result, FALSE);
    if (!matrix->multiply)
        return FALSE;
    return matrix->multiply(matrix, vector, result);
}

/**
 * gwy_matrix_inv_multiply:
 * @matrix: An abstract matrix.
 * @vector: Vector to multiply.  The array must have the same number of
 *          elements as the matrix rows and columns.
 * @result: Vector to put the result to.  The array must have the same number
 *          of elements as the matrix rows and columns.
 *
 * Multiplies a vector from left by the inverse of an abstract matrix.
 *
 * This essentially means solving a system of linear equations with the
 * abstract matrix being the matrix of the system.
 *
 * Returns: %TRUE if the operation succeeded, %FALSE on failure (including
 *          the case when no inverse multiplication function is set).
 **/
gboolean
gwy_matrix_inv_multiply(GwyMatrix *matrix,
                        const gdouble *vector,
                        gdouble *result)
{
    g_return_val_if_fail(matrix, FALSE);
    g_return_val_if_fail(vector, FALSE);
    g_return_val_if_fail(result, FALSE);
    if (!matrix->inv_multiply)
        return FALSE;
    return matrix->inv_multiply(matrix, vector, result);
}

/**
 * gwy_matrix_get_diagonal:
 * @matrix: An abstract matrix.
 * @diag: Vector to put the diagonal elements to.  The array must have the same
 *        number of elements as the matrix rows and columns.
 *
 * Extracts the diagonal elements of an abstract matrix.
 *
 * Returns: %TRUE if the operation succeeded, %FALSE on failure (including
 *          the case when no diagonal extraction function is set).
 **/
gboolean
gwy_matrix_get_diagonal(GwyMatrix *matrix,
                        gdouble *diag)
{
    g_return_val_if_fail(matrix, FALSE);
    g_return_val_if_fail(diag, FALSE);
    if (!matrix->get_diag)
        return FALSE;
    matrix->get_diag(matrix, diag);
    return TRUE;
}

/**
 * gwy_matrix_set_diagonal:
 * @matrix: An abstract matrix.
 * @diag: Vector to set the diagonal elements to.  The array must have the same
 *        number of elements as the matrix rows and columns.
 *
 * Replaces the diagonal elements of an abstract matrix.
 *
 * Returns: %TRUE if the operation succeeded, %FALSE on failure (including
 *          the case when no diagonal replacement function is set).
 **/
gboolean
gwy_matrix_set_diagonal(GwyMatrix *matrix,
                        const gdouble *diag)
{
    g_return_val_if_fail(matrix, FALSE);
    g_return_val_if_fail(diag, FALSE);
    if (!matrix->set_diag)
        return FALSE;
    matrix->set_diag(matrix, diag);
    return TRUE;
}

/************************** Documentation ****************************/

/**
 * SECTION: matrix
 * @title: GwyMatrix
 * @short_description: Abstraction of a matrix
 **/

/**
 * GwyMatrix:
 *
 * Abstract matrix.
 *
 * The #GwyMatrix struct is opaque and can only be accessed through the
 * <function>gwy_matrix_foo<!-- -->()</function> functions.
 **/

/**
 * GwyMatrixMultiplyFunc:
 * @matrix: An abstract matrix.
 * @vector: Vector to multiply.
 * @result: Vector to put the result to.
 *
 * Multiplication function type for an abstract matrix.
 *
 * Returns: %TRUE on success, %FALSE on failure.
 **/

/**
 * GwyMatrixGetDiagFunc:
 * @matrix: An abstract matrix.
 * @diag: Vector to put the diagonal elements to.
 *
 * Diagonal extraction function type for an abstract matrix.
 *
 * The function has no return value.  It is assumed that if it is defined it
 * can succeed.
 **/

/**
 * GwyMatrixGetDiagFunc:
 * @matrix: An abstract matrix.
 * @diag: Vector to set the diagonal elements to.
 *
 * Diagonal replacement function type for an abstract matrix.
 *
 * The function has no return value.  It is assumed that if it is defined it
 * can succeed.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
