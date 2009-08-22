/*
 *  $Id$
 *  Data block routines from GNU Scientific Library.
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

struct gsl_block_struct
{
  size_t size;
  double *data;
};

typedef struct gsl_block_struct gsl_block;

static gsl_block *
gsl_block_alloc (const size_t n)
{
  gsl_block * b;

  if (n == 0)
    {
      GSL_ERROR_VAL ("block length n must be positive integer",
                        GSL_EINVAL, 0);
    }

  b = (gsl_block *) malloc (sizeof (gsl_block));

  if (b == 0)
    {
      GSL_ERROR_VAL ("failed to allocate space for block struct",
                        GSL_ENOMEM, 0);
    }

  b->data = (double *) malloc (n * sizeof (double));

  if (b->data == 0)
    {
      free (b);         /* exception in constructor, avoid memory leak */

      GSL_ERROR_VAL ("failed to allocate space for block data",
                        GSL_ENOMEM, 0);
    }

  b->size = n;

  return b;
}

static void
gsl_block_free (gsl_block * b)
{
  free (b->data);
  free (b);
}
