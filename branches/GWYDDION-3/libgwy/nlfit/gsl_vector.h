/*
 *  $Id$
 *  Vector object from GNU Scientific Library.
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

#define NULL_VECTOR {0, 0, 0, 0, 0}
#define NULL_VECTOR_VIEW {{0, 0, 0, 0, 0}}

typedef struct
{
  size_t size;
  size_t stride;
  double *data;
  gsl_block *block;
  int owner;
}
gsl_vector;

typedef struct
{
  gsl_vector vector;
} _gsl_vector_view;

typedef _gsl_vector_view gsl_vector_view;

typedef struct
{
  gsl_vector vector;
} _gsl_vector_const_view;

typedef const _gsl_vector_const_view gsl_vector_const_view;

static gsl_vector *
gsl_vector_alloc (const size_t n)
{
  gsl_block * block;
  gsl_vector * v;

  if (n == 0)
    {
      GSL_ERROR_VAL ("vector length n must be positive integer",
                        GSL_EINVAL, 0);
    }

  v = (gsl_vector *) malloc (sizeof (gsl_vector));

  if (v == 0)
    {
      GSL_ERROR_VAL ("failed to allocate space for vector struct",
                        GSL_ENOMEM, 0);
    }

  block = gsl_block_alloc (n);

  if (block == 0)
    {
      free (v) ;

      GSL_ERROR_VAL ("failed to allocate space for block",
                        GSL_ENOMEM, 0);
    }

  v->data = block->data ;
  v->size = n;
  v->stride = 1;
  v->block = block;
  v->owner = 1;

  return v;
}

static gsl_vector *
gsl_vector_calloc (const size_t n)
{
  size_t i;

  gsl_vector * v = gsl_vector_alloc (n);

  if (v == 0)
    return 0;

  /* initialize vector to zero */

  for (i = 0; i < n; i++)
    {
      v->data[i] = 0;
    }

  return v;
}

static void
gsl_vector_free (gsl_vector * v)
{
  if (v->owner)
    {
      gsl_block_free (v->block) ;
    }
  free (v);
}


static void
gsl_vector_set_all (gsl_vector * v, double x)
{
  double * const data = v->data;
  const size_t n = v->size;
  const size_t stride = v->stride;

  size_t i;

  for (i = 0; i < n; i++)
    {
      *(double *) (data + i * stride) = x;
    }
}

static void
gsl_vector_set_zero (gsl_vector * v)
{
  double * const data = v->data;
  const size_t n = v->size;
  const size_t stride = v->stride;
  const double zero = 0.0 ;

  size_t i;

  for (i = 0; i < n; i++)
    {
      *(double *) (data + i * stride) = zero;
    }
}

static int
gsl_vector_memcpy (gsl_vector * dest,
                               const gsl_vector * src)
{
  const size_t src_size = src->size;
  const size_t dest_size = dest->size;

  if (src_size != dest_size)
    {
      GSL_ERROR ("vector lengths are not equal", GSL_EBADLEN);
    }

  {
    const size_t src_stride = src->stride ;
    const size_t dest_stride = dest->stride ;
    size_t j;

    for (j = 0; j < src_size; j++)
      {
        dest->data[dest_stride * j] = src->data[src_stride * j];
      }
  }

  return GSL_SUCCESS;
}

static int
gsl_vector_swap_elements (gsl_vector * v, const size_t i, const size_t j)
{
  double * data = v->data ;
  const size_t size = v->size ;
  const size_t stride = v->stride ;

  if (i >= size)
    {
      GSL_ERROR("first index is out of range", GSL_EINVAL);
    }

  if (j >= size)
    {
      GSL_ERROR("second index is out of range", GSL_EINVAL);
    }

  if (i != j)
    {
      const size_t s = stride ;
      double tmp = data[j*s];
      data[j*s] = data[i*s];
      data[i*s] = tmp;
    }

  return GSL_SUCCESS;
}

static int
gsl_vector_add (gsl_vector * a, const gsl_vector * b)
{
  const size_t N = a->size;

  if (b->size != N)
    {
      GSL_ERROR ("vectors must have same length", GSL_EBADLEN);
    }
  else
    {
      const size_t stride_a = a->stride;
      const size_t stride_b = b->stride;

      size_t i;

      for (i = 0; i < N; i++)
        {
          a->data[i * stride_a] += b->data[i * stride_b];
        }

      return GSL_SUCCESS;
    }
}

static int
gsl_vector_sub (gsl_vector * a, const gsl_vector * b)
{
  const size_t N = a->size;

  if (b->size != N)
    {
      GSL_ERROR ("vectors must have same length", GSL_EBADLEN);
    }
  else
    {
      const size_t stride_a = a->stride;
      const size_t stride_b = b->stride;

      size_t i;

      for (i = 0; i < N; i++)
        {
          a->data[i * stride_a] -= b->data[i * stride_b];
        }

      return GSL_SUCCESS;
    }
}

static int
gsl_vector_mul (gsl_vector * a, const gsl_vector * b)
{
  const size_t N = a->size;

  if (b->size != N)
    {
      GSL_ERROR ("vectors must have same length", GSL_EBADLEN);
    }
  else
    {
      const size_t stride_a = a->stride;
      const size_t stride_b = b->stride;

      size_t i;

      for (i = 0; i < N; i++)
        {
          a->data[i * stride_a] *= b->data[i * stride_b];
        }

      return GSL_SUCCESS;
    }
}

static int
gsl_vector_div (gsl_vector * a, const gsl_vector * b)
{
  const size_t N = a->size;

  if (b->size != N)
    {
      GSL_ERROR ("vectors must have same length", GSL_EBADLEN);
    }
  else
    {
      const size_t stride_a = a->stride;
      const size_t stride_b = b->stride;

      size_t i;

      for (i = 0; i < N; i++)
        {
          a->data[i * stride_a] /= b->data[i * stride_b];
        }

      return GSL_SUCCESS;
    }
}

static int
gsl_vector_scale (gsl_vector * a, const double x)
{
  const size_t N = a->size;
  const size_t stride = a->stride;

  size_t i;

  for (i = 0; i < N; i++)
    {
      a->data[i * stride] *= x;
    }

  return GSL_SUCCESS;
}

static _gsl_vector_view
gsl_vector_view_array(double *base, size_t n)
{
  _gsl_vector_view view = NULL_VECTOR_VIEW;

  if (n == 0)
    {
      GSL_ERROR_VAL ("vector length n must be positive integer", 
                     GSL_EINVAL, view);
    }

  {
    gsl_vector v = NULL_VECTOR;

    v.data = base;
    v.size = n;
    v.stride = 1;
    v.block = 0;
    v.owner = 0;

    view.vector = v;
    return view;
  }
}

static _gsl_vector_view
gsl_vector_subvector (gsl_vector * v, size_t offset, size_t n)
{
  _gsl_vector_view view = NULL_VECTOR_VIEW;

  if (n == 0)
    {
      GSL_ERROR_VAL ("vector length n must be positive integer",
                     GSL_EINVAL, view);
    }

  if (offset + (n - 1) >= v->size)
    {
      GSL_ERROR_VAL ("view would extend past end of vector",
                     GSL_EINVAL, view);
    }

  {
    gsl_vector s = NULL_VECTOR;

    s.data = v->data +  v->stride * offset ;
    s.size = n;
    s.stride = v->stride;
    s.block = v->block;
    s.owner = 0;

    view.vector = s;
    return view;
  }
}

static _gsl_vector_const_view
gsl_vector_const_subvector (const gsl_vector * v, size_t offset, size_t n)
{
  _gsl_vector_const_view view = NULL_VECTOR_VIEW;

  if (n == 0)
    {
      GSL_ERROR_VAL ("vector length n must be positive integer",
                     GSL_EINVAL, view);
    }

  if (offset + (n - 1) >= v->size)
    {
      GSL_ERROR_VAL ("view would extend past end of vector",
                     GSL_EINVAL, view);
    }

  {
    gsl_vector s = NULL_VECTOR;

    s.data = v->data +  v->stride * offset ;
    s.size = n;
    s.stride = v->stride;
    s.block = v->block;
    s.owner = 0;

    view.vector = s;
    return view;
  }
}

static inline double
gsl_vector_get (const gsl_vector * v, const size_t i)
{
#if GSL_RANGE_CHECK
  if (GSL_RANGE_COND(i >= v->size))
    {
      GSL_ERROR_VAL ("index out of range", GSL_EINVAL, 0);
    }
#endif
  return v->data[i * v->stride];
}

static inline void
gsl_vector_set (gsl_vector * v, const size_t i, double x)
{
#if GSL_RANGE_CHECK
  if (GSL_RANGE_COND(i >= v->size))
    {
      GSL_ERROR_VOID ("index out of range", GSL_EINVAL);
    }
#endif
  v->data[i * v->stride] = x;
}
