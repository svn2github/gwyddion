/*
 *  $Id$
 *  Matrix object from GNU Scientific Library.
 *
 *  Copyright (C) 1996, 1997, 1998, 1999, 2000, 2007 Gerard Jungman, Brian Gough.
 *  Copyright (C) 2009 David Necas (Yeti).
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

#define NULL_MATRIX {0, 0, 0, 0, 0, 0}
#define NULL_MATRIX_VIEW {{0, 0, 0, 0, 0, 0}}

typedef struct
{
  size_t size1;
  size_t size2;
  size_t tda;
  double * data;
  gsl_block * block;
  int owner;
} gsl_matrix;

typedef struct
{
  gsl_matrix matrix;
} _gsl_matrix_view;

typedef _gsl_matrix_view gsl_matrix_view;

typedef struct
{
  gsl_matrix matrix;
} _gsl_matrix_const_view;

typedef const _gsl_matrix_const_view gsl_matrix_const_view;

static void
gsl_matrix_free (gsl_matrix * m)
{
  if (m->owner)
    {
      gsl_block_free (m->block);
    }

  free (m);
}

static gsl_matrix*
gsl_matrix_alloc(const size_t n1, const size_t n2)
{
  gsl_block * block;
  gsl_matrix * m;

  if (n1 == 0)
    {
      GSL_ERROR_VAL ("matrix dimension n1 must be positive integer",
                        GSL_EINVAL, 0);
    }
  else if (n2 == 0)
    {
      GSL_ERROR_VAL ("matrix dimension n2 must be positive integer",
                        GSL_EINVAL, 0);
    }

  m = (gsl_matrix *) malloc (sizeof (gsl_matrix));

  if (m == 0)
    {
      GSL_ERROR_VAL ("failed to allocate space for matrix struct",
                        GSL_ENOMEM, 0);
    }

  /* FIXME: n1*n2 could overflow for large dimensions */

  block = gsl_block_alloc(n1 * n2) ;

  if (block == 0)
    {
      GSL_ERROR_VAL ("failed to allocate space for block",
                        GSL_ENOMEM, 0);
    }

  m->data = block->data;
  m->size1 = n1;
  m->size2 = n2;
  m->tda = n2;
  m->block = block;
  m->owner = 1;

  return m;
}

static gsl_matrix*
gsl_matrix_calloc (const size_t n1, const size_t n2)
{
  size_t i;

  gsl_matrix * m = gsl_matrix_alloc (n1, n2);

  if (m == 0)
    return 0;

  /* initialize matrix to zero */

  for (i = 0; i < n1 * n2; i++)
    {
      m->data[i] = 0;
    }

  return m;
}


static int
gsl_matrix_memcpy(gsl_matrix *dest, const gsl_matrix *src)
{
  const size_t src_size1 = src->size1;
  const size_t src_size2 = src->size2;
  const size_t dest_size1 = dest->size1;
  const size_t dest_size2 = dest->size2;

  if (src_size1 != dest_size1 || src_size2 != dest_size2)
    {
      GSL_ERROR ("matrix sizes are different", GSL_EBADLEN);
    }

  {
    const size_t src_tda = src->tda ;
    const size_t dest_tda = dest->tda ;
    size_t i, j;

    for (i = 0; i < src_size1 ; i++)
      {
        for (j = 0; j < src_size2; j++)
          {
            dest->data[dest_tda * i + j] = src->data[src_tda * i + j];
          }
      }
  }

  return GSL_SUCCESS;
}

static inline _gsl_vector_view
gsl_matrix_row(gsl_matrix *m, const size_t i)
{
  _gsl_vector_view view = NULL_VECTOR_VIEW;

  if (i >= m->size1)
    {
      GSL_ERROR_VAL ("row index is out of range", GSL_EINVAL, view);
    }

  {
    gsl_vector v = NULL_VECTOR;

    v.data = m->data + i * m->tda;
    v.size = m->size2;
    v.stride = 1;
    v.block = m->block;
    v.owner = 0;

    view.vector = v;
    return view;
  }
}

static inline _gsl_vector_const_view
gsl_matrix_const_row(const gsl_matrix *m, const size_t i)
{
  _gsl_vector_const_view view = NULL_VECTOR_VIEW;

  if (i >= m->size1)
    {
      GSL_ERROR_VAL ("row index is out of range", GSL_EINVAL, view);
    }

  {
    gsl_vector v = NULL_VECTOR;

    v.data = m->data + i * m->tda;
    v.size = m->size2;
    v.stride = 1;
    v.block = m->block;
    v.owner = 0;

    view.vector = v;
    return view;
  }
}

static inline _gsl_vector_view
gsl_matrix_column(gsl_matrix *m, const size_t j)
{
  _gsl_vector_view view = NULL_VECTOR_VIEW;

  if (j >= m->size2)
    {
      GSL_ERROR_VAL ("column index is out of range", GSL_EINVAL, view);
    }

  {
    gsl_vector v = NULL_VECTOR;

    v.data = m->data + j;
    v.size = m->size1;
    v.stride = m->tda;
    v.block = m->block;
    v.owner = 0;

    view.vector = v;
    return view;
  }
}

static inline _gsl_vector_const_view
gsl_matrix_const_column(const gsl_matrix *m, const size_t j)
{
  _gsl_vector_const_view view = NULL_VECTOR_VIEW;

  if (j >= m->size2)
    {
      GSL_ERROR_VAL ("column index is out of range", GSL_EINVAL, view);
    }

  {
    gsl_vector v = NULL_VECTOR;

    v.data = m->data + j;
    v.size = m->size1;
    v.stride = m->tda;
    v.block = m->block;
    v.owner = 0;

    view.vector = v;
    return view;
  }
}

static _gsl_matrix_view
gsl_matrix_submatrix (gsl_matrix * m,
                                  const size_t i, const size_t j,
                                  const size_t n1, const size_t n2)
{
  _gsl_matrix_view view = NULL_MATRIX_VIEW;

  if (i >= m->size1)
    {
      GSL_ERROR_VAL ("row index is out of range", GSL_EINVAL, view);
    }
  else if (j >= m->size2)
    {
      GSL_ERROR_VAL ("column index is out of range", GSL_EINVAL, view);
    }
  else if (n1 == 0)
    {
      GSL_ERROR_VAL ("first dimension must be non-zero", GSL_EINVAL, view);
    }
  else if (n2 == 0)
    {
      GSL_ERROR_VAL ("second dimension must be non-zero", GSL_EINVAL, view);
    }
  else if (i + n1 > m->size1)
    {
      GSL_ERROR_VAL ("first dimension overflows matrix", GSL_EINVAL, view);
    }
  else if (j + n2 > m->size2)
    {
      GSL_ERROR_VAL ("second dimension overflows matrix", GSL_EINVAL, view);
    }

  {
     gsl_matrix s = NULL_MATRIX;

     s.data = m->data + (i * m->tda + j);
     s.size1 = n1;
     s.size2 = n2;
     s.tda = m->tda;
     s.block = m->block;
     s.owner = 0;

     view.matrix = s;
     return view;
  }
}

static void
gsl_matrix_set_identity (gsl_matrix * m)
{
  size_t i, j;
  double * const data = m->data;
  const size_t p = m->size1 ;
  const size_t q = m->size2 ;
  const size_t tda = m->tda ;

  const double zero = 0.0;
  const double one = 1.0;

  for (i = 0; i < p; i++)
    {
      for (j = 0; j < q; j++)
        {
          *(double *) (data + (i * tda + j)) = ((i == j) ? one : zero);
        }
    }
}

static int
gsl_matrix_swap_columns (gsl_matrix * m,
                                     const size_t i, const size_t j)
{
  const size_t size1 = m->size1;
  const size_t size2 = m->size2;

  if (i >= size2)
    {
      GSL_ERROR ("first column index is out of range", GSL_EINVAL);
    }

  if (j >= size2)
    {
      GSL_ERROR ("second column index is out of range", GSL_EINVAL);
    }

  if (i != j)
    {
      double *col1 = m->data + i;
      double *col2 = m->data + j;

      size_t p;

      for (p = 0; p < size1; p++)
        {
          size_t n = p * m->tda;
          double tmp = col1[n] ;
          col1[n] = col2[n] ;
          col2[n] = tmp ;
        }
    }

  return GSL_SUCCESS;
}

static inline double
gsl_matrix_get(const gsl_matrix * m, const size_t i, const size_t j)
{
#if GSL_RANGE_CHECK
  if (GSL_RANGE_COND(1))
    {
      if (i >= m->size1)
        {
          GSL_ERROR_VAL("first index out of range", GSL_EINVAL, 0) ;
        }
      else if (j >= m->size2)
        {
          GSL_ERROR_VAL("second index out of range", GSL_EINVAL, 0) ;
        }
    }
#endif
  return m->data[i * m->tda + j] ;
}

static inline void
gsl_matrix_set(gsl_matrix * m, const size_t i, const size_t j, const double x)
{
#if GSL_RANGE_CHECK
  if (GSL_RANGE_COND(1))
    {
      if (i >= m->size1)
        {
          GSL_ERROR_VOID("first index out of range", GSL_EINVAL) ;
        }
      else if (j >= m->size2)
        {
          GSL_ERROR_VOID("second index out of range", GSL_EINVAL) ;
        }
    }
#endif
  m->data[i * m->tda + j] = x ;
}
