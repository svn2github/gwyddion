/*
 *  $Id$
 *  Permutation object from GNU Scientific Library.
 *
 *  Copyright (C) 1996, 1997, 1998, 1999, 2000, 2007 Brian Gough.
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

struct gsl_permutation_struct
{
  size_t size;
  size_t *data;
};

typedef struct gsl_permutation_struct gsl_permutation;

static gsl_permutation *
gsl_permutation_alloc (const size_t n)
{
  gsl_permutation * p;

  if (n == 0)
    {
      GSL_ERROR_VAL ("permutation length n must be positive integer",
                        GSL_EDOM, 0);
    }

  p = (gsl_permutation *) malloc (sizeof (gsl_permutation));

  if (p == 0)
    {
      GSL_ERROR_VAL ("failed to allocate space for permutation struct",
                        GSL_ENOMEM, 0);
    }

  p->data = (size_t *) malloc (n * sizeof (size_t));

  if (p->data == 0)
    {
      free (p);         /* exception in constructor, avoid memory leak */

      GSL_ERROR_VAL ("failed to allocate space for permutation data",
                        GSL_ENOMEM, 0);
    }

  p->size = n;

  return p;
}

static gsl_permutation *
gsl_permutation_calloc (const size_t n)
{
  size_t i;

  gsl_permutation * p =  gsl_permutation_alloc (n);

  if (p == 0)
    return 0;

  /* initialize permutation to identity */

  for (i = 0; i < n; i++)
    {
      p->data[i] = i;
    }

  return p;
}

static void
gsl_permutation_init (gsl_permutation * p)
{
  const size_t n = p->size ;
  size_t i;

  /* initialize permutation to identity */

  for (i = 0; i < n; i++)
    {
      p->data[i] = i;
    }
}

static void
gsl_permutation_free (gsl_permutation * p)
{
  free (p->data);
  free (p);
}

static int
gsl_permutation_swap (gsl_permutation * p, const size_t i, const size_t j)
{
  const size_t size = p->size ;

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
      size_t tmp = p->data[i];
      p->data[i] = p->data[j];
      p->data[j] = tmp;
    }

  return GSL_SUCCESS;
}

static inline size_t
gsl_permutation_get (const gsl_permutation * p, const size_t i)
{
#if GSL_RANGE_CHECK
  if (GSL_RANGE_COND(i >= p->size))
    {
      GSL_ERROR_VAL ("index out of range", GSL_EINVAL, 0);
    }
#endif
  return p->data[i];
}

static int
gsl_permute (const size_t * p, double * data, const size_t stride, const size_t n)
{
  size_t i, k, pk;

  for (i = 0; i < n; i++)
    {
      k = p[i];

      while (k > i)
        k = p[k];

      if (k < i)
        continue ;

      /* Now have k == i, i.e the least in its cycle */

      pk = p[k];

      if (pk == i)
        continue ;

      /* shuffle the elements of the cycle */

      {
        double t = data[i*stride];

        while (pk != i)
          {
            double r1 = data[pk*stride];
            data[k*stride] = r1;
            k = pk;
            pk = p[k];
          };

          data[k*stride] = t;
      }
    }

  return GSL_SUCCESS;
}

static int
gsl_permute_inverse (const size_t * p, double * data, const size_t stride, const size_t n)
{
  size_t i, k, pk;

  for (i = 0; i < n; i++)
    {
      k = p[i];

      while (k > i)
        k = p[k];

      if (k < i)
        continue ;

      /* Now have k == i, i.e the least in its cycle */

      pk = p[k];

      if (pk == i)
        continue ;

      /* shuffle the elements of the cycle in the inverse direction */

      {
        double t = data[k*stride];

        while (pk != i)
          {
            double r1 = data[pk*stride];
            data[pk*stride] = t;
            t = r1;
            k = pk;
            pk = p[k];
          };

        data[pk*stride] = t;
      }
    }

  return GSL_SUCCESS;
}

static int
gsl_permute_vector (const gsl_permutation * p, gsl_vector * v)
{
  if (v->size != p->size)
    {
      GSL_ERROR ("vector and permutation must be the same length", GSL_EBADLEN);
    }

  gsl_permute (p->data, v->data, v->stride, v->size) ;

  return GSL_SUCCESS;
}

static int
gsl_permute_vector_inverse (const gsl_permutation * p, gsl_vector * v)
{
  if (v->size != p->size)
    {
      GSL_ERROR ("vector and permutation must be the same length", GSL_EBADLEN);
    }

  gsl_permute_inverse (p->data, v->data, v->stride, v->size) ;

  return GSL_SUCCESS;
}
