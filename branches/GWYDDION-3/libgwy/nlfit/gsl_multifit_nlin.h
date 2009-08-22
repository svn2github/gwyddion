/*
 *  $Id$
 *  Non-linear least squares fitting algorithms from GNU Scientific Library.
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

/* Definition of vector-valued functions and gradient with parameters
   based on gsl_vector */

struct gsl_multifit_function_fdf_struct
{
  int (* f) (const gsl_vector * x, void * params, gsl_vector * f);
  int (* df) (const gsl_vector * x, void * params, gsl_matrix * df);
  int (* fdf) (const gsl_vector * x, void * params, gsl_vector * f, gsl_matrix *df);
  size_t n;   /* number of functions */
  size_t p;   /* number of independent variables */
  void * params;
};

typedef struct gsl_multifit_function_fdf_struct gsl_multifit_function_fdf ;

#define GSL_MULTIFIT_FN_EVAL_F(F,x,y) ((*((F)->f))(x,(F)->params,(y)))
#define GSL_MULTIFIT_FN_EVAL_DF(F,x,dy) ((*((F)->df))(x,(F)->params,(dy)))
#define GSL_MULTIFIT_FN_EVAL_F_DF(F,x,y,dy) ((*((F)->fdf))(x,(F)->params,(y),(dy)))

typedef struct
  {
    const char *name;
    size_t size;
    int (*alloc) (void *state, size_t n, size_t p);
    int (*set) (void *state, gsl_multifit_function_fdf * fdf, gsl_vector * x, gsl_vector * f, gsl_matrix * J, gsl_vector * dx);
    int (*iterate) (void *state, gsl_multifit_function_fdf * fdf, gsl_vector * x, gsl_vector * f, gsl_matrix * J, gsl_vector * dx);
    void (*free) (void *state);
  }
gsl_multifit_fdfsolver_type;

typedef struct
  {
    const gsl_multifit_fdfsolver_type * type;
    gsl_multifit_function_fdf * fdf ;
    gsl_vector * x;
    gsl_vector * f;
    gsl_matrix * J;
    gsl_vector * dx;
    void *state;
  }
gsl_multifit_fdfsolver;

static inline int
gsl_multifit_gradient (const gsl_matrix * J, const gsl_vector * f,
                       gsl_vector * g)
{
  int status = gsl_blas_dgemv (CblasTrans, 1.0, J, f, 0.0, g);
  return status;
}

/* Compute the covariance matrix

   cov = inv (J^T J)

   by QRP^T decomposition of J
*/

static int
gsl_multifit_covar (const gsl_matrix * J, double epsrel, gsl_matrix * covar)
{
  double tolr;

  size_t i, j, k;
  size_t kmax = 0;

  gsl_matrix * r;
  gsl_vector * tau;
  gsl_vector * norm;
  gsl_permutation * perm;

  size_t m = J->size1, n = J->size2 ;

  if (m < n)
    {
      GSL_ERROR ("Jacobian be rectangular M x N with M >= N", GSL_EBADLEN);
    }

  if (covar->size1 != covar->size2 || covar->size1 != n)
    {
      GSL_ERROR ("covariance matrix must be square and match second dimension of jacobian", GSL_EBADLEN);
    }

  r = gsl_matrix_alloc (m, n);
  tau = gsl_vector_alloc (n);
  perm = gsl_permutation_alloc (n) ;
  norm = gsl_vector_alloc (n) ;

  {
    int signum = 0;
    gsl_matrix_memcpy (r, J);
    gsl_linalg_QRPT_decomp (r, tau, perm, &signum, norm);
  }


  /* Form the inverse of R in the full upper triangle of R */

  tolr = epsrel * fabs(gsl_matrix_get(r, 0, 0));

  for (k = 0 ; k < n ; k++)
    {
      double rkk = gsl_matrix_get(r, k, k);

      if (fabs(rkk) <= tolr)
        {
          break;
        }

      gsl_matrix_set(r, k, k, 1.0/rkk);

      for (j = 0; j < k ; j++)
        {
          double t = gsl_matrix_get(r, j, k) / rkk;
          gsl_matrix_set (r, j, k, 0.0);

          for (i = 0; i <= j; i++)
            {
              double rik = gsl_matrix_get (r, i, k);
              double rij = gsl_matrix_get (r, i, j);

              gsl_matrix_set (r, i, k, rik - t * rij);
            }
        }
      kmax = k;
    }

  /* Form the full upper triangle of the inverse of R^T R in the full
     upper triangle of R */

  for (k = 0; k <= kmax ; k++)
    {
      for (j = 0; j < k; j++)
        {
          double rjk = gsl_matrix_get (r, j, k);

          for (i = 0; i <= j ; i++)
            {
              double rij = gsl_matrix_get (r, i, j);
              double rik = gsl_matrix_get (r, i, k);

              gsl_matrix_set (r, i, j, rij + rjk * rik);
            }
        }

      {
        double t = gsl_matrix_get (r, k, k);

        for (i = 0; i <= k; i++)
          {
            double rik = gsl_matrix_get (r, i, k);

            gsl_matrix_set (r, i, k, t * rik);
          };
      }
    }

  /* Form the full lower triangle of the covariance matrix in the
     strict lower triangle of R and in w */

  for (j = 0 ; j < n ; j++)
    {
      size_t pj = gsl_permutation_get (perm, j);

      for (i = 0; i <= j; i++)
        {
          size_t pi = gsl_permutation_get (perm, i);

          double rij;

          if (j > kmax)
            {
              gsl_matrix_set (r, i, j, 0.0);
              rij = 0.0 ;
            }
          else
            {
              rij = gsl_matrix_get (r, i, j);
            }

          if (pi > pj)
            {
              gsl_matrix_set (r, pi, pj, rij);
            }
          else if (pi < pj)
            {
              gsl_matrix_set (r, pj, pi, rij);
            }

        }

      {
        double rjj = gsl_matrix_get (r, j, j);
        gsl_matrix_set (covar, pj, pj, rjj);
      }
    }


  /* symmetrize the covariance matrix */

  for (j = 0 ; j < n ; j++)
    {
      for (i = 0; i < j ; i++)
        {
          double rji = gsl_matrix_get (r, j, i);

          gsl_matrix_set (covar, j, i, rji);
          gsl_matrix_set (covar, i, j, rji);
        }
    }

  gsl_matrix_free (r);
  gsl_permutation_free (perm);
  gsl_vector_free (tau);
  gsl_vector_free (norm);

  return GSL_SUCCESS;
}

static gsl_multifit_fdfsolver *
gsl_multifit_fdfsolver_alloc (const gsl_multifit_fdfsolver_type * T,
                              size_t n, size_t p)
{
  int status;

  gsl_multifit_fdfsolver * s;

  if (n < p)
    {
      GSL_ERROR_VAL ("insufficient data points, n < p", GSL_EINVAL, 0);
    }

  s = (gsl_multifit_fdfsolver *) malloc (sizeof (gsl_multifit_fdfsolver));

  if (s == 0)
    {
      GSL_ERROR_VAL ("failed to allocate space for multifit solver struct",
                     GSL_ENOMEM, 0);
    }

  s->x = gsl_vector_calloc (p);

  if (s->x == 0)
    {
      free (s);
      GSL_ERROR_VAL ("failed to allocate space for x", GSL_ENOMEM, 0);
    }

  s->f = gsl_vector_calloc (n);

  if (s->f == 0)
    {
      gsl_vector_free (s->x);
      free (s);
      GSL_ERROR_VAL ("failed to allocate space for f", GSL_ENOMEM, 0);
    }

  s->J = gsl_matrix_calloc (n,p);

  if (s->J == 0)
    {
      gsl_vector_free (s->x);
      gsl_vector_free (s->f);
      free (s);
      GSL_ERROR_VAL ("failed to allocate space for g", GSL_ENOMEM, 0);
    }

  s->dx = gsl_vector_calloc (p);

  if (s->dx == 0)
    {
      gsl_matrix_free (s->J);
      gsl_vector_free (s->x);
      gsl_vector_free (s->f);
      free (s);
      GSL_ERROR_VAL ("failed to allocate space for dx", GSL_ENOMEM, 0);
    }

  s->state = malloc (T->size);

  if (s->state == 0)
    {
      gsl_vector_free (s->dx);
      gsl_vector_free (s->x);
      gsl_vector_free (s->f);
      gsl_matrix_free (s->J);
      free (s);         /* exception in constructor, avoid memory leak */

      GSL_ERROR_VAL ("failed to allocate space for multifit solver state",
                        GSL_ENOMEM, 0);
    }

  s->type = T ;

  status = (s->type->alloc)(s->state, n, p);

  if (status != GSL_SUCCESS)
    {
      free (s->state);
      gsl_vector_free (s->dx);
      gsl_vector_free (s->x);
      gsl_vector_free (s->f);
      gsl_matrix_free (s->J);
      free (s);         /* exception in constructor, avoid memory leak */

      GSL_ERROR_VAL ("failed to set solver", status, 0);
    }

  s->fdf = NULL;

  return s;
}

static int
gsl_multifit_fdfsolver_set (gsl_multifit_fdfsolver * s,
                             gsl_multifit_function_fdf * f,
                             const gsl_vector * x)
{
  if (s->f->size != f->n)
    {
      GSL_ERROR ("function size does not match solver", GSL_EBADLEN);
    }

  if (s->x->size != x->size)
    {
      GSL_ERROR ("vector length does not match solver", GSL_EBADLEN);
    }

  s->fdf = f;
  gsl_vector_memcpy(s->x,x);

  return (s->type->set) (s->state, s->fdf, s->x, s->f, s->J, s->dx);
}

static int
gsl_multifit_fdfsolver_iterate (gsl_multifit_fdfsolver * s)
{
  return (s->type->iterate) (s->state, s->fdf, s->x, s->f, s->J, s->dx);
}

static void
gsl_multifit_fdfsolver_free (gsl_multifit_fdfsolver * s)
{
  (s->type->free) (s->state);
  free (s->state);
  gsl_vector_free (s->dx);
  gsl_vector_free (s->x);
  gsl_vector_free (s->f);
  gsl_matrix_free (s->J);
  free (s);
}

static const char *
gsl_multifit_fdfsolver_name (const gsl_multifit_fdfsolver * s)
{
  return s->type->name;
}

static gsl_vector *
gsl_multifit_fdfsolver_position (const gsl_multifit_fdfsolver * s)
{
  return s->x;
}


static int
gsl_multifit_test_delta (const gsl_vector * dx, const gsl_vector * x,
                         double epsabs, double epsrel)
{
  size_t i;
  int ok = 1;
  const size_t n = x->size ;

  if (epsrel < 0.0)
    {
      GSL_ERROR ("relative tolerance is negative", GSL_EBADTOL);
    }

  for (i = 0 ; i < n ; i++)
    {
      double xi = gsl_vector_get(x,i);
      double dxi = gsl_vector_get(dx,i);
      double tolerance = epsabs + epsrel * fabs(xi)  ;

      if (fabs(dxi) < tolerance)
        {
          ok = 1;
        }
      else
        {
          ok = 0;
          break;
        }
    }

  if (ok)
    return GSL_SUCCESS ;

  return GSL_CONTINUE;
}

static int
gsl_multifit_test_gradient (const gsl_vector * g, double epsabs)
{
  size_t i;

  double residual = 0;

  const size_t n = g->size;

  if (epsabs < 0.0)
    {
      GSL_ERROR ("absolute tolerance is negative", GSL_EBADTOL);
    }

  for (i = 0 ; i < n ; i++)
    {
      double gi = gsl_vector_get(g, i);

      residual += fabs(gi);
    }


  if (residual < epsabs)
    {
      return GSL_SUCCESS;
    }

  return GSL_CONTINUE ;
}

typedef struct
  {
    size_t iter;
    double xnorm;
    double fnorm;
    double delta;
    double par;
    gsl_matrix *r;
    gsl_vector *tau;
    gsl_vector *diag;
    gsl_vector *qtf;
    gsl_vector *newton;
    gsl_vector *gradient;
    gsl_vector *x_trial;
    gsl_vector *f_trial;
    gsl_vector *df;
    gsl_vector *sdiag;
    gsl_vector *rptdx;
    gsl_vector *w;
    gsl_vector *work1;
    gsl_permutation * perm;
  }
lmder_state_t;

static double
enorm (const gsl_vector * f)
{
  return gsl_blas_dnrm2 (f);
}

static double
scaled_enorm (const gsl_vector * d, const gsl_vector * f)
{
  double e2 = 0;
  size_t i, n = f->size;
  for (i = 0; i < n; i++)
    {
      double fi = gsl_vector_get (f, i);
      double di = gsl_vector_get (d, i);
      double u = di * fi;
      e2 += u * u;
    }
  return sqrt (e2);
}

static double
compute_delta (gsl_vector * diag, gsl_vector * x)
{
  double Dx = scaled_enorm (diag, x);
  double factor = 100;  /* generally recommended value from MINPACK */

  return (Dx > 0) ? factor * Dx : factor;
}

static double
compute_actual_reduction (double fnorm, double fnorm1)
{
  double actred;

  if (0.1 * fnorm1 < fnorm)
    {
      double u = fnorm1 / fnorm;
      actred = 1 - u * u;
    }
  else
    {
      actred = -1;
    }

  return actred;
}

static void
compute_diag (const gsl_matrix * J, gsl_vector * diag)
{
  size_t i, j, n = J->size1, p = J->size2;

  for (j = 0; j < p; j++)
    {
      double sum = 0;

      for (i = 0; i < n; i++)
        {
          double Jij = gsl_matrix_get (J, i, j);
          sum += Jij * Jij;
        }
      if (sum == 0)
        sum = 1.0;

      gsl_vector_set (diag, j, sqrt (sum));
    }
}

static void
update_diag (const gsl_matrix * J, gsl_vector * diag)
{
  size_t i, j, n = diag->size;

  for (j = 0; j < n; j++)
    {
      double cnorm, diagj, sum = 0;
      for (i = 0; i < n; i++)
        {
          double Jij = gsl_matrix_get (J, i, j);
          sum += Jij * Jij;
        }
      if (sum == 0)
        sum = 1.0;

      cnorm = sqrt (sum);
      diagj = gsl_vector_get (diag, j);

      if (cnorm > diagj)
        gsl_vector_set (diag, j, cnorm);
    }
}

static void
compute_rptdx (const gsl_matrix * r, const gsl_permutation * p,
               const gsl_vector * dx, gsl_vector * rptdx)
{
  size_t i, j, N = dx->size;

  for (i = 0; i < N; i++)
    {
      double sum = 0;

      for (j = i; j < N; j++)
        {
          size_t pj = gsl_permutation_get (p, j);

          sum += gsl_matrix_get (r, i, j) * gsl_vector_get (dx, pj);
        }

      gsl_vector_set (rptdx, i, sum);
    }
}


static void
compute_trial_step (gsl_vector * x, gsl_vector * dx, gsl_vector * x_trial)
{
  size_t i, N = x->size;

  for (i = 0; i < N; i++)
    {
      double pi = gsl_vector_get (dx, i);
      double xi = gsl_vector_get (x, i);
      gsl_vector_set (x_trial, i, xi + pi);
    }
}

static int
qrsolv (gsl_matrix * r, const gsl_permutation * p, const double lambda,
        const gsl_vector * diag, const gsl_vector * qtb,
        gsl_vector * x, gsl_vector * sdiag, gsl_vector * wa)
{
  size_t n = r->size2;

  size_t i, j, k, nsing;

  /* Copy r and qtb to preserve input and initialise s. In particular,
     save the diagonal elements of r in x */

  for (j = 0; j < n; j++)
    {
      double rjj = gsl_matrix_get (r, j, j);
      double qtbj = gsl_vector_get (qtb, j);

      for (i = j + 1; i < n; i++)
        {
          double rji = gsl_matrix_get (r, j, i);
          gsl_matrix_set (r, i, j, rji);
        }

      gsl_vector_set (x, j, rjj);
      gsl_vector_set (wa, j, qtbj);
    }

  /* Eliminate the diagonal matrix d using a Givens rotation */

  for (j = 0; j < n; j++)
    {
      double qtbpj;

      size_t pj = gsl_permutation_get (p, j);

      double diagpj = lambda * gsl_vector_get (diag, pj);

      if (diagpj == 0)
        {
          continue;
        }

      gsl_vector_set (sdiag, j, diagpj);

      for (k = j + 1; k < n; k++)
        {
          gsl_vector_set (sdiag, k, 0.0);
        }

      /* The transformations to eliminate the row of d modify only a
         single element of qtb beyond the first n, which is initially
         zero */

      qtbpj = 0;

      for (k = j; k < n; k++)
        {
          /* Determine a Givens rotation which eliminates the
             appropriate element in the current row of d */

          double sine, cosine;

          double wak = gsl_vector_get (wa, k);
          double rkk = gsl_matrix_get (r, k, k);
          double sdiagk = gsl_vector_get (sdiag, k);

          if (sdiagk == 0)
            {
              continue;
            }

          if (fabs (rkk) < fabs (sdiagk))
            {
              double cotangent = rkk / sdiagk;
              sine = 0.5 / sqrt (0.25 + 0.25 * cotangent * cotangent);
              cosine = sine * cotangent;
            }
          else
            {
              double tangent = sdiagk / rkk;
              cosine = 0.5 / sqrt (0.25 + 0.25 * tangent * tangent);
              sine = cosine * tangent;
            }

          /* Compute the modified diagonal element of r and the
             modified element of [qtb,0] */

          {
            double new_rkk = cosine * rkk + sine * sdiagk;
            double new_wak = cosine * wak + sine * qtbpj;

            qtbpj = -sine * wak + cosine * qtbpj;

            gsl_matrix_set(r, k, k, new_rkk);
            gsl_vector_set(wa, k, new_wak);
          }

          /* Accumulate the transformation in the row of s */

          for (i = k + 1; i < n; i++)
            {
              double rik = gsl_matrix_get (r, i, k);
              double sdiagi = gsl_vector_get (sdiag, i);

              double new_rik = cosine * rik + sine * sdiagi;
              double new_sdiagi = -sine * rik + cosine * sdiagi;

              gsl_matrix_set(r, i, k, new_rik);
              gsl_vector_set(sdiag, i, new_sdiagi);
            }
        }

      /* Store the corresponding diagonal element of s and restore the
         corresponding diagonal element of r */

      {
        double rjj = gsl_matrix_get (r, j, j);
        double xj = gsl_vector_get(x, j);

        gsl_vector_set (sdiag, j, rjj);
        gsl_matrix_set (r, j, j, xj);
      }

    }

  /* Solve the triangular system for z. If the system is singular then
     obtain a least squares solution */

  nsing = n;

  for (j = 0; j < n; j++)
    {
      double sdiagj = gsl_vector_get (sdiag, j);

      if (sdiagj == 0)
        {
          nsing = j;
          break;
        }
    }

  for (j = nsing; j < n; j++)
    {
      gsl_vector_set (wa, j, 0.0);
    }

  for (k = 0; k < nsing; k++)
    {
      double sum = 0;

      j = (nsing - 1) - k;

      for (i = j + 1; i < nsing; i++)
        {
          sum += gsl_matrix_get(r, i, j) * gsl_vector_get(wa, i);
        }

      {
        double waj = gsl_vector_get (wa, j);
        double sdiagj = gsl_vector_get (sdiag, j);

        gsl_vector_set (wa, j, (waj - sum) / sdiagj);
      }
    }

  /* Permute the components of z back to the components of x */

  for (j = 0; j < n; j++)
    {
      size_t pj = gsl_permutation_get (p, j);
      double waj = gsl_vector_get (wa, j);

      gsl_vector_set (x, pj, waj);
    }

  return GSL_SUCCESS;
}

static size_t
count_nsing (const gsl_matrix * r)
{
  /* Count the number of nonsingular entries. Returns the index of the
     first entry which is singular. */

  size_t n = r->size2;
  size_t i;

  for (i = 0; i < n; i++)
    {
      double rii = gsl_matrix_get (r, i, i);

      if (rii == 0)
        {
          break;
        }
    }

  return i;
}


static void
compute_newton_direction (const gsl_matrix * r, const gsl_permutation * perm,
                          const gsl_vector * qtf, gsl_vector * x)
{

  /* Compute and store in x the Gauss-Newton direction. If the
     Jacobian is rank-deficient then obtain a least squares
     solution. */

  const size_t n = r->size2;
  size_t i, j, nsing;

  for (i = 0 ; i < n ; i++)
    {
      double qtfi = gsl_vector_get (qtf, i);
      gsl_vector_set (x, i,  qtfi);
    }

  nsing = count_nsing (r);

#ifdef DEBUG
  printf("nsing = %d\n", nsing);
  printf("r = "); gsl_matrix_fprintf(stdout, r, "%g"); printf("\n");
  printf("qtf = "); gsl_vector_fprintf(stdout, x, "%g"); printf("\n");
#endif

  for (i = nsing; i < n; i++)
    {
      gsl_vector_set (x, i, 0.0);
    }

  if (nsing > 0)
    {
      for (j = nsing; j > 0 && j--;)
        {
          double rjj = gsl_matrix_get (r, j, j);
          double temp = gsl_vector_get (x, j) / rjj;

          gsl_vector_set (x, j, temp);

          for (i = 0; i < j; i++)
            {
              double rij = gsl_matrix_get (r, i, j);
              double xi = gsl_vector_get (x, i);
              gsl_vector_set (x, i, xi - rij * temp);
            }
        }
    }

  gsl_permute_vector_inverse (perm, x);
}

static void
compute_newton_correction (const gsl_matrix * r, const gsl_vector * sdiag,
                           const gsl_permutation * p, gsl_vector * x,
                           double dxnorm,
                           const gsl_vector * diag, gsl_vector * w)
{
  size_t n = r->size2;
  size_t i, j;

  for (i = 0; i < n; i++)
    {
      size_t pi = gsl_permutation_get (p, i);

      double dpi = gsl_vector_get (diag, pi);
      double xpi = gsl_vector_get (x, pi);

      gsl_vector_set (w, i, dpi * (dpi * xpi) / dxnorm);
    }

  for (j = 0; j < n; j++)
    {
      double sj = gsl_vector_get (sdiag, j);
      double wj = gsl_vector_get (w, j);

      double tj = wj / sj;

      gsl_vector_set (w, j, tj);

      for (i = j + 1; i < n; i++)
        {
          double rij = gsl_matrix_get (r, i, j);
          double wi = gsl_vector_get (w, i);

          gsl_vector_set (w, i, wi - rij * tj);
        }
    }
}

static void
compute_newton_bound (const gsl_matrix * r, const gsl_vector * x,
                      double dxnorm,  const gsl_permutation * perm,
                      const gsl_vector * diag, gsl_vector * w)
{
  /* If the jacobian is not rank-deficient then the Newton step
     provides a lower bound for the zero of the function. Otherwise
     set this bound to zero. */

  size_t n = r->size2;

  size_t i, j;

  size_t nsing = count_nsing (r);

  if (nsing < n)
    {
      gsl_vector_set_zero (w);
      return;
    }

  for (i = 0; i < n; i++)
    {
      size_t pi = gsl_permutation_get (perm, i);

      double dpi = gsl_vector_get (diag, pi);
      double xpi = gsl_vector_get (x, pi);

      gsl_vector_set (w, i, dpi * (dpi * xpi / dxnorm));
    }

  for (j = 0; j < n; j++)
    {
      double sum = 0;

      for (i = 0; i < j; i++)
        {
          sum += gsl_matrix_get (r, i, j) * gsl_vector_get (w, i);
        }

      {
        double rjj = gsl_matrix_get (r, j, j);
        double wj = gsl_vector_get (w, j);

        gsl_vector_set (w, j, (wj - sum) / rjj);
      }
    }
}

static void
compute_gradient_direction (const gsl_matrix * r, const gsl_permutation * p,
                            const gsl_vector * qtf, const gsl_vector * diag,
                            gsl_vector * g)
{
  const size_t n = r->size2;

  size_t i, j;

  for (j = 0; j < n; j++)
    {
      double sum = 0;

      for (i = 0; i <= j; i++)
        {
          sum += gsl_matrix_get (r, i, j) * gsl_vector_get (qtf, i);
        }

      {
        size_t pj = gsl_permutation_get (p, j);
        double dpj = gsl_vector_get (diag, pj);

        gsl_vector_set (g, j, sum / dpj);
      }
    }
}

static int
lmpar (gsl_matrix * r, const gsl_permutation * perm, const gsl_vector * qtf,
       const gsl_vector * diag, double delta, double * par_inout,
       gsl_vector * newton, gsl_vector * gradient, gsl_vector * sdiag,
       gsl_vector * x, gsl_vector * w)
{
  double dxnorm, gnorm, fp, fp_old, par_lower, par_upper, par_c;

  double par = *par_inout;

  size_t iter = 0;

#ifdef DEBUG
  printf("ENTERING lmpar\n");
#endif


  compute_newton_direction (r, perm, qtf, newton);

#ifdef DEBUG
  printf ("newton = ");
  gsl_vector_fprintf (stdout, newton, "%g");
  printf ("\n");

  printf ("diag = ");
  gsl_vector_fprintf (stdout, diag, "%g");
  printf ("\n");
#endif

  /* Evaluate the function at the origin and test for acceptance of
     the Gauss-Newton direction. */

  dxnorm = scaled_enorm (diag, newton);

  fp = dxnorm - delta;

#ifdef DEBUG
  printf ("dxnorm = %g, delta = %g, fp = %g\n", dxnorm, delta, fp);
#endif

  if (fp <= 0.1 * delta)
    {
      gsl_vector_memcpy (x, newton);
#ifdef DEBUG
      printf ("took newton (fp = %g, delta = %g)\n", fp, delta);
#endif

      *par_inout = 0;

      return GSL_SUCCESS;
    }

#ifdef DEBUG
  printf ("r = ");
  gsl_matrix_fprintf (stdout, r, "%g");
  printf ("\n");

  printf ("newton = ");
  gsl_vector_fprintf (stdout, newton, "%g");
  printf ("\n");

  printf ("dxnorm = %g\n", dxnorm);
#endif


  compute_newton_bound (r, newton, dxnorm, perm, diag, w);

#ifdef DEBUG
  printf("perm = "); gsl_permutation_fprintf(stdout, perm, "%d");

  printf ("diag = ");
  gsl_vector_fprintf (stdout, diag, "%g");
  printf ("\n");

  printf ("w = ");
  gsl_vector_fprintf (stdout, w, "%g");
  printf ("\n");
#endif


  {
    double wnorm = enorm (w);
    double phider = wnorm * wnorm;

    /* w == zero if r rank-deficient,
       then set lower bound to zero form MINPACK, lmder.f
       Hans E. Plesser 2002-02-25 (hans.plesser@itf.nlh.no) */
    if ( wnorm > 0 )
      par_lower = fp / (delta * phider);
    else
      par_lower = 0.0;
  }

#ifdef DEBUG
  printf("par       = %g\n", par      );
  printf("par_lower = %g\n", par_lower);
#endif

  compute_gradient_direction (r, perm, qtf, diag, gradient);

  gnorm = enorm (gradient);

#ifdef DEBUG
  printf("gradient = "); gsl_vector_fprintf(stdout, gradient, "%g"); printf("\n");
  printf("gnorm = %g\n", gnorm);
#endif

  par_upper =  gnorm / delta;

  if (par_upper == 0)
    {
      par_upper = GSL_DBL_MIN/GSL_MIN_DBL(delta, 0.1);
    }

#ifdef DEBUG
  printf("par_upper = %g\n", par_upper);
#endif

  if (par > par_upper)
    {
#ifdef DEBUG
  printf("set par to par_upper\n");
#endif

      par = par_upper;
    }
  else if (par < par_lower)
    {
#ifdef DEBUG
  printf("set par to par_lower\n");
#endif

      par = par_lower;
    }

  if (par == 0)
    {
      par = gnorm / dxnorm;
#ifdef DEBUG
      printf("set par to gnorm/dxnorm = %g\n", par);
#endif

    }

  /* Beginning of iteration */

iteration:

  iter++;

#ifdef DEBUG
  printf("lmpar iteration = %d\n", iter);
#endif

#ifdef BRIANSFIX
  /* Seems like this is described in the paper but not in the MINPACK code */

  if (par < par_lower || par > par_upper)
    {
      par = GSL_MAX_DBL (0.001 * par_upper, sqrt(par_lower * par_upper));
    }
#endif

  /* Evaluate the function at the current value of par */

  if (par == 0)
    {
      par = GSL_MAX_DBL (0.001 * par_upper, GSL_DBL_MIN);
#ifdef DEBUG
      printf("par = 0, set par to  = %g\n", par);
#endif

    }

  /* Compute the least squares solution of [ R P x - Q^T f, sqrt(par) D x]
     for A = Q R P^T */

#ifdef DEBUG
  printf ("calling qrsolv with par = %g\n", par);
#endif

  {
    double sqrt_par = sqrt(par);

    qrsolv (r, perm, sqrt_par, diag, qtf, x, sdiag, w);
  }

  dxnorm = scaled_enorm (diag, x);

  fp_old = fp;

  fp = dxnorm - delta;

#ifdef DEBUG
  printf ("After qrsolv dxnorm = %g, delta = %g, fp = %g\n", dxnorm, delta, fp);
  printf ("sdiag = ") ; gsl_vector_fprintf(stdout, sdiag, "%g"); printf("\n");
  printf ("x = ") ; gsl_vector_fprintf(stdout, x, "%g"); printf("\n");
  printf ("r = ") ; gsl_matrix_fprintf(stdout, r, "%g"); printf("\nXXX\n");
#endif

  /* If the function is small enough, accept the current value of par */

  if (fabs (fp) <= 0.1 * delta)
    goto line220;

  if (par_lower == 0 && fp <= fp_old && fp_old < 0)
    goto line220;

  /* Check for maximum number of iterations */

  if (iter == 10)
    goto line220;

  /* Compute the Newton correction */

  compute_newton_correction (r, sdiag, perm, x, dxnorm, diag, w);

#ifdef DEBUG
  printf ("newton_correction = ");
  gsl_vector_fprintf(stdout, w, "%g"); printf("\n");
#endif

  {
    double wnorm = enorm (w);
    par_c = fp / (delta * wnorm * wnorm);
  }

#ifdef DEBUG
  printf("fp = %g\n", fp);
  printf("par_lower = %g\n", par_lower);
  printf("par_upper = %g\n", par_upper);
  printf("par_c = %g\n", par_c);
#endif


  /* Depending on the sign of the function, update par_lower or par_upper */

  if (fp > 0)
    {
      if (par > par_lower)
        {
          par_lower = par;
#ifdef DEBUG
      printf("fp > 0: set par_lower = par = %g\n", par);
#endif

        }
    }
  else if (fp < 0)
    {
      if (par < par_upper)
        {
#ifdef DEBUG
      printf("fp < 0: set par_upper = par = %g\n", par);
#endif
          par_upper = par;
        }
    }

  /* Compute an improved estimate for par */

#ifdef DEBUG
      printf("improved estimate par = MAX(%g, %g) \n", par_lower, par+par_c);
#endif

  par = GSL_MAX_DBL (par_lower, par + par_c);

#ifdef DEBUG
      printf("improved estimate par = %g \n", par);
#endif


  goto iteration;

line220:

#ifdef DEBUG
  printf("LEAVING lmpar, par = %g\n", par);
#endif

  *par_inout = par;

  return GSL_SUCCESS;
}

static int
set (void *vstate, gsl_multifit_function_fdf * fdf, gsl_vector * x, gsl_vector * f, gsl_matrix * J, gsl_vector * dx, int scale)
{
  lmder_state_t *state = (lmder_state_t *) vstate;

  gsl_matrix *r = state->r;
  gsl_vector *tau = state->tau;
  gsl_vector *diag = state->diag;
  gsl_vector *work1 = state->work1;
  gsl_permutation *perm = state->perm;

  int signum;

  GSL_MULTIFIT_FN_EVAL_F_DF (fdf, x, f, J);

  state->par = 0;
  state->iter = 1;
  state->fnorm = enorm (f);

  gsl_vector_set_all (dx, 0.0);

  /* store column norms in diag */

  if (scale)
    {
      compute_diag (J, diag);
    }
  else
    {
      gsl_vector_set_all (diag, 1.0);
    }

  /* set delta to 100 |D x| or to 100 if |D x| is zero */

  state->xnorm = scaled_enorm (diag, x);
  state->delta = compute_delta (diag, x);

  /* Factorize J into QR decomposition */

  gsl_matrix_memcpy (r, J);
  gsl_linalg_QRPT_decomp (r, tau, perm, &signum, work1);

  gsl_vector_set_zero (state->rptdx);
  gsl_vector_set_zero (state->w);

  /* Zero the trial vector, as in the alloc function */

  gsl_vector_set_zero (state->f_trial);

#ifdef DEBUG
  printf("r = "); gsl_matrix_fprintf(stdout, r, "%g");
  printf("perm = "); gsl_permutation_fprintf(stdout, perm, "%d");
  printf("tau = "); gsl_vector_fprintf(stdout, tau, "%g");
#endif

  return GSL_SUCCESS;
}

static int
iterate (void *vstate, gsl_multifit_function_fdf * fdf, gsl_vector * x, gsl_vector * f, gsl_matrix * J, gsl_vector * dx, int scale)
{
  lmder_state_t *state = (lmder_state_t *) vstate;

  gsl_matrix *r = state->r;
  gsl_vector *tau = state->tau;
  gsl_vector *diag = state->diag;
  gsl_vector *qtf = state->qtf;
  gsl_vector *x_trial = state->x_trial;
  gsl_vector *f_trial = state->f_trial;
  gsl_vector *rptdx = state->rptdx;
  gsl_vector *newton = state->newton;
  gsl_vector *gradient = state->gradient;
  gsl_vector *sdiag = state->sdiag;
  gsl_vector *w = state->w;
  gsl_vector *work1 = state->work1;
  gsl_permutation *perm = state->perm;

  double prered, actred;
  double pnorm, fnorm1, fnorm1p, gnorm;
  double ratio;
  double dirder;

  int iter = 0;

  double p1 = 0.1, p25 = 0.25, p5 = 0.5, p75 = 0.75, p0001 = 0.0001;

  if (state->fnorm == 0.0)
    {
      return GSL_SUCCESS;
    }

  /* Compute qtf = Q^T f */

  gsl_vector_memcpy (qtf, f);
  gsl_linalg_QR_QTvec (r, tau, qtf);

  /* Compute norm of scaled gradient */

  compute_gradient_direction (r, perm, qtf, diag, gradient);

  {
    size_t iamax = gsl_blas_idamax (gradient);

    gnorm = fabs(gsl_vector_get (gradient, iamax) / state->fnorm);
  }

  /* Determine the Levenberg-Marquardt parameter */

lm_iteration:

  iter++ ;

  {
    int status = lmpar (r, perm, qtf, diag, state->delta, &(state->par), newton, gradient, sdiag, dx, w);
    if (status)
      return status;
  }

  /* Take a trial step */

  gsl_vector_scale (dx, -1.0); /* reverse the step to go downhill */

  compute_trial_step (x, dx, state->x_trial);

  pnorm = scaled_enorm (diag, dx);

  if (state->iter == 1)
    {
      if (pnorm < state->delta)
        {
#ifdef DEBUG
          printf("set delta = pnorm = %g\n" , pnorm);
#endif
          state->delta = pnorm;
        }
    }

  /* Evaluate function at x + p */
  /* return immediately if evaluation raised error */
  {
    int status = GSL_MULTIFIT_FN_EVAL_F (fdf, x_trial, f_trial);
    if (status)
      return status;
  }

  fnorm1 = enorm (f_trial);

  /* Compute the scaled actual reduction */

  actred = compute_actual_reduction (state->fnorm, fnorm1);

#ifdef DEBUG
  printf("lmiterate: fnorm = %g fnorm1 = %g  actred = %g\n", state->fnorm, fnorm1, actred);
  printf("r = "); gsl_matrix_fprintf(stdout, r, "%g");
  printf("perm = "); gsl_permutation_fprintf(stdout, perm, "%d");
  printf("dx = "); gsl_vector_fprintf(stdout, dx, "%g");
#endif

  /* Compute rptdx = R P^T dx, noting that |J dx| = |R P^T dx| */

  compute_rptdx (r, perm, dx, rptdx);

#ifdef DEBUG
  printf("rptdx = "); gsl_vector_fprintf(stdout, rptdx, "%g");
#endif

  fnorm1p = enorm (rptdx);

  /* Compute the scaled predicted reduction = |J dx|^2 + 2 par |D dx|^2 */

  {
    double t1 = fnorm1p / state->fnorm;
    double t2 = (sqrt(state->par) * pnorm) / state->fnorm;

    prered = t1 * t1 + t2 * t2 / p5;
    dirder = -(t1 * t1 + t2 * t2);
  }

  /* compute the ratio of the actual to predicted reduction */

  if (prered > 0)
    {
      ratio = actred / prered;
    }
  else
    {
      ratio = 0;
    }

#ifdef DEBUG
  printf("lmiterate: prered = %g dirder = %g ratio = %g\n", prered, dirder,ratio);
#endif


  /* update the step bound */

  if (ratio > p25)
    {
#ifdef DEBUG
      printf("ratio > p25\n");
#endif
      if (state->par == 0 || ratio >= p75)
        {
          state->delta = pnorm / p5;
          state->par *= p5;
#ifdef DEBUG
          printf("updated step bounds: delta = %g, par = %g\n", state->delta, state->par);
#endif
        }
    }
  else
    {
      double temp = (actred >= 0) ? p5 : p5*dirder / (dirder + p5 * actred);

#ifdef DEBUG
      printf("ratio < p25\n");
#endif

      if (p1 * fnorm1 >= state->fnorm || temp < p1 )
        {
          temp = p1;
        }

      state->delta = temp * GSL_MIN_DBL(state->delta, pnorm/p1);

      state->par /= temp;
#ifdef DEBUG
      printf("updated step bounds: delta = %g, par = %g\n", state->delta, state->par);
#endif
    }


  /* test for successful iteration, termination and stringent tolerances */

  if (ratio >= p0001)
    {
      gsl_vector_memcpy (x, x_trial);
      gsl_vector_memcpy (f, f_trial);

      /* return immediately if evaluation raised error */
      {
        int status = GSL_MULTIFIT_FN_EVAL_DF (fdf, x_trial, J);
        if (status)
          return status;
      }

      /* wa2_j  = diag_j * x_j */
      state->xnorm = scaled_enorm(diag, x);
      state->fnorm = fnorm1;
      state->iter++;

      /* Rescale if necessary */

      if (scale)
        {
          update_diag (J, diag);
        }

      {
        int signum;
        gsl_matrix_memcpy (r, J);
        gsl_linalg_QRPT_decomp (r, tau, perm, &signum, work1);
      }

      return GSL_SUCCESS;
    }
  else if (fabs(actred) <= GSL_DBL_EPSILON  && prered <= GSL_DBL_EPSILON
           && p5 * ratio <= 1.0)
    {
      return GSL_ETOLF ;
    }
  else if (state->delta <= GSL_DBL_EPSILON * state->xnorm)
    {
      return GSL_ETOLX;
    }
  else if (gnorm <= GSL_DBL_EPSILON)
    {
      return GSL_ETOLG;
    }
  else if (iter < 10)
    {
      /* Repeat inner loop if unsuccessful */
      goto lm_iteration;
    }

  return GSL_CONTINUE;
}


static int
lmder_alloc (void *vstate, size_t n, size_t p)
{
  lmder_state_t *state = (lmder_state_t *) vstate;
  gsl_matrix *r;
  gsl_vector *tau, *diag, *qtf, *newton, *gradient, *x_trial, *f_trial,
   *df, *sdiag, *rptdx, *w, *work1;
  gsl_permutation *perm;

  r = gsl_matrix_calloc (n, p);

  if (r == 0)
    {
      GSL_ERROR ("failed to allocate space for r", GSL_ENOMEM);
    }

  state->r = r;

  tau = gsl_vector_calloc (MIN(n, p));

  if (tau == 0)
    {
      gsl_matrix_free (r);

      GSL_ERROR ("failed to allocate space for tau", GSL_ENOMEM);
    }

  state->tau = tau;

  diag = gsl_vector_calloc (p);

  if (diag == 0)
    {
      gsl_matrix_free (r);
      gsl_vector_free (tau);

      GSL_ERROR ("failed to allocate space for diag", GSL_ENOMEM);
    }

  state->diag = diag;

  qtf = gsl_vector_calloc (n);

  if (qtf == 0)
    {
      gsl_matrix_free (r);
      gsl_vector_free (tau);
      gsl_vector_free (diag);

      GSL_ERROR ("failed to allocate space for qtf", GSL_ENOMEM);
    }

  state->qtf = qtf;

  newton = gsl_vector_calloc (p);

  if (newton == 0)
    {
      gsl_matrix_free (r);
      gsl_vector_free (tau);
      gsl_vector_free (diag);
      gsl_vector_free (qtf);

      GSL_ERROR ("failed to allocate space for newton", GSL_ENOMEM);
    }

  state->newton = newton;

  gradient = gsl_vector_calloc (p);

  if (gradient == 0)
    {
      gsl_matrix_free (r);
      gsl_vector_free (tau);
      gsl_vector_free (diag);
      gsl_vector_free (qtf);
      gsl_vector_free (newton);

      GSL_ERROR ("failed to allocate space for gradient", GSL_ENOMEM);
    }

  state->gradient = gradient;

  x_trial = gsl_vector_calloc (p);

  if (x_trial == 0)
    {
      gsl_matrix_free (r);
      gsl_vector_free (tau);
      gsl_vector_free (diag);
      gsl_vector_free (qtf);
      gsl_vector_free (newton);
      gsl_vector_free (gradient);

      GSL_ERROR ("failed to allocate space for x_trial", GSL_ENOMEM);
    }

  state->x_trial = x_trial;

  f_trial = gsl_vector_calloc (n);

  if (f_trial == 0)
    {
      gsl_matrix_free (r);
      gsl_vector_free (tau);
      gsl_vector_free (diag);
      gsl_vector_free (qtf);
      gsl_vector_free (newton);
      gsl_vector_free (gradient);
      gsl_vector_free (x_trial);

      GSL_ERROR ("failed to allocate space for f_trial", GSL_ENOMEM);
    }

  state->f_trial = f_trial;

  df = gsl_vector_calloc (n);

  if (df == 0)
    {
      gsl_matrix_free (r);
      gsl_vector_free (tau);
      gsl_vector_free (diag);
      gsl_vector_free (qtf);
      gsl_vector_free (newton);
      gsl_vector_free (gradient);
      gsl_vector_free (x_trial);
      gsl_vector_free (f_trial);

      GSL_ERROR ("failed to allocate space for df", GSL_ENOMEM);
    }

  state->df = df;

  sdiag = gsl_vector_calloc (p);

  if (sdiag == 0)
    {
      gsl_matrix_free (r);
      gsl_vector_free (tau);
      gsl_vector_free (diag);
      gsl_vector_free (qtf);
      gsl_vector_free (newton);
      gsl_vector_free (gradient);
      gsl_vector_free (x_trial);
      gsl_vector_free (f_trial);
      gsl_vector_free (df);

      GSL_ERROR ("failed to allocate space for sdiag", GSL_ENOMEM);
    }

  state->sdiag = sdiag;


  rptdx = gsl_vector_calloc (n);

  if (rptdx == 0)
    {
      gsl_matrix_free (r);
      gsl_vector_free (tau);
      gsl_vector_free (diag);
      gsl_vector_free (qtf);
      gsl_vector_free (newton);
      gsl_vector_free (gradient);
      gsl_vector_free (x_trial);
      gsl_vector_free (f_trial);
      gsl_vector_free (df);
      gsl_vector_free (sdiag);

      GSL_ERROR ("failed to allocate space for rptdx", GSL_ENOMEM);
    }

  state->rptdx = rptdx;

  w = gsl_vector_calloc (n);

  if (w == 0)
    {
      gsl_matrix_free (r);
      gsl_vector_free (tau);
      gsl_vector_free (diag);
      gsl_vector_free (qtf);
      gsl_vector_free (newton);
      gsl_vector_free (gradient);
      gsl_vector_free (x_trial);
      gsl_vector_free (f_trial);
      gsl_vector_free (df);
      gsl_vector_free (sdiag);
      gsl_vector_free (rptdx);

      GSL_ERROR ("failed to allocate space for w", GSL_ENOMEM);
    }

  state->w = w;

  work1 = gsl_vector_calloc (p);

  if (work1 == 0)
    {
      gsl_matrix_free (r);
      gsl_vector_free (tau);
      gsl_vector_free (diag);
      gsl_vector_free (qtf);
      gsl_vector_free (newton);
      gsl_vector_free (gradient);
      gsl_vector_free (x_trial);
      gsl_vector_free (f_trial);
      gsl_vector_free (df);
      gsl_vector_free (sdiag);
      gsl_vector_free (rptdx);
      gsl_vector_free (w);

      GSL_ERROR ("failed to allocate space for work1", GSL_ENOMEM);
    }

  state->work1 = work1;

  perm = gsl_permutation_calloc (p);

  if (perm == 0)
    {
      gsl_matrix_free (r);
      gsl_vector_free (tau);
      gsl_vector_free (diag);
      gsl_vector_free (qtf);
      gsl_vector_free (newton);
      gsl_vector_free (gradient);
      gsl_vector_free (x_trial);
      gsl_vector_free (f_trial);
      gsl_vector_free (df);
      gsl_vector_free (sdiag);
      gsl_vector_free (rptdx);
      gsl_vector_free (w);
      gsl_vector_free (work1);

      GSL_ERROR ("failed to allocate space for perm", GSL_ENOMEM);
    }

  state->perm = perm;

  return GSL_SUCCESS;
}

static int
lmder_set (void *vstate, gsl_multifit_function_fdf * fdf, gsl_vector * x, gsl_vector * f, gsl_matrix * J, gsl_vector * dx)
{
  int status = set (vstate, fdf, x, f, J, dx, 0);
  return status ;
}

static int
lmsder_set (void *vstate, gsl_multifit_function_fdf * fdf, gsl_vector * x, gsl_vector * f, gsl_matrix * J, gsl_vector * dx)
{
  int status = set (vstate, fdf, x, f, J, dx, 1);
  return status ;
}

static int
lmder_iterate (void *vstate, gsl_multifit_function_fdf * fdf, gsl_vector * x, gsl_vector * f, gsl_matrix * J, gsl_vector * dx)
{
  int status = iterate (vstate, fdf, x, f, J, dx, 0);
  return status;
}

static int
lmsder_iterate (void *vstate, gsl_multifit_function_fdf * fdf, gsl_vector * x, gsl_vector * f, gsl_matrix * J, gsl_vector * dx)
{
  int status = iterate (vstate, fdf, x, f, J, dx, 1);
  return status;
}

static void
lmder_free (void *vstate)
{
  lmder_state_t *state = (lmder_state_t *) vstate;

  gsl_permutation_free (state->perm);
  gsl_vector_free (state->work1);
  gsl_vector_free (state->w);
  gsl_vector_free (state->rptdx);
  gsl_vector_free (state->sdiag);
  gsl_vector_free (state->df);
  gsl_vector_free (state->f_trial);
  gsl_vector_free (state->x_trial);
  gsl_vector_free (state->gradient);
  gsl_vector_free (state->newton);
  gsl_vector_free (state->qtf);
  gsl_vector_free (state->diag);
  gsl_vector_free (state->tau);
  gsl_matrix_free (state->r);
}

static const gsl_multifit_fdfsolver_type lmder_type =
{
  "lmder",                      /* name */
  sizeof (lmder_state_t),
  &lmder_alloc,
  &lmder_set,
  &lmder_iterate,
  &lmder_free
};

static const gsl_multifit_fdfsolver_type lmsder_type =
{
  "lmsder",                     /* name */
  sizeof (lmder_state_t),
  &lmder_alloc,
  &lmsder_set,
  &lmsder_iterate,
  &lmder_free
};
