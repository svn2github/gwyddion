/*
 *  $Id$
 *  BLAS routines from GNU Scientific Library.
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
#define BLAS_ERROR(x) /* */

#define CBLAS_INDEX size_t  /* this may vary between platforms */
#define OFFSET(N, incX) ((incX) > 0 ?  0 : ((N) - 1) * (-(incX)))
#define INT(X) ((int)(X))
#define INDEX int

typedef  CBLAS_INDEX  CBLAS_INDEX_t;
typedef  enum CBLAS_TRANSPOSE   CBLAS_TRANSPOSE_t;
typedef  enum CBLAS_ORDER       CBLAS_ORDER_t;

enum CBLAS_TRANSPOSE {CblasNoTrans=111, CblasTrans=112, CblasConjTrans=113};
enum CBLAS_ORDER {CblasRowMajor=101, CblasColMajor=102};

static inline double
cblas_dnrm2 (const int N, const double *X, const int incX)
{
  double scale = 0.0;
  double ssq = 1.0;
  INDEX i;
  INDEX ix = 0;

  if (N <= 0 || incX <= 0) {
    return 0;
  } else if (N == 1) {
    return fabs(X[0]);
  }

  for (i = 0; i < N; i++) {
    const double x = X[ix];

    if (x != 0.0) {
      const double ax = fabs(x);

      if (scale < ax) {
        ssq = 1.0 + ssq * (scale / ax) * (scale / ax);
        scale = ax;
      } else {
        ssq += (ax / scale) * (ax / scale);
      }
    }

    ix += incX;
  }

  return scale * sqrt(ssq);
}

static double
gsl_blas_dnrm2 (const gsl_vector * X)
{
  return cblas_dnrm2 (INT (X->size), X->data, INT (X->stride));
}

static inline void
cblas_dgemv (const enum CBLAS_ORDER order, const enum CBLAS_TRANSPOSE TransA,
             const int M, const int N, const double alpha, const double *A,
             const int lda, const double *X, const int incX,
             const double beta, double *Y, const int incY)
{
  INDEX i, j;
  INDEX lenX, lenY;

  const int Trans = (TransA != CblasConjTrans) ? TransA : CblasTrans;

  if (M == 0 || N == 0)
    return;

  if (alpha == 0.0 && beta == 1.0)
    return;

  if (Trans == CblasNoTrans) {
    lenX = N;
    lenY = M;
  } else {
    lenX = M;
    lenY = N;
  }

  /* form  y := beta*y */
  if (beta == 0.0) {
    INDEX iy = OFFSET(lenY, incY);
    for (i = 0; i < lenY; i++) {
      Y[iy] = 0.0;
      iy += incY;
    }
  } else if (beta != 1.0) {
    INDEX iy = OFFSET(lenY, incY);
    for (i = 0; i < lenY; i++) {
      Y[iy] *= beta;
      iy += incY;
    }
  }

  if (alpha == 0.0)
    return;

  if ((order == CblasRowMajor && Trans == CblasNoTrans)
      || (order == CblasColMajor && Trans == CblasTrans)) {
    /* form  y := alpha*A*x + y */
    INDEX iy = OFFSET(lenY, incY);
    for (i = 0; i < lenY; i++) {
      double temp = 0.0;
      INDEX ix = OFFSET(lenX, incX);
      for (j = 0; j < lenX; j++) {
        temp += X[ix] * A[lda * i + j];
        ix += incX;
      }
      Y[iy] += alpha * temp;
      iy += incY;
    }
  } else if ((order == CblasRowMajor && Trans == CblasTrans)
             || (order == CblasColMajor && Trans == CblasNoTrans)) {
    /* form  y := alpha*A'*x + y */
    INDEX ix = OFFSET(lenX, incX);
    for (j = 0; j < lenX; j++) {
      const double temp = alpha * X[ix];
      if (temp != 0.0) {
        INDEX iy = OFFSET(lenY, incY);
        for (i = 0; i < lenY; i++) {
          Y[iy] += temp * A[lda * j + i];
          iy += incY;
        }
      }
      ix += incX;
    }
  } else {
    BLAS_ERROR("unrecognized operation");
  }
}

static int
gsl_blas_dgemv (CBLAS_TRANSPOSE_t TransA, double alpha, const gsl_matrix * A,
                const gsl_vector * X, double beta, gsl_vector * Y)
{
  const size_t M = A->size1;
  const size_t N = A->size2;

  if ((TransA == CblasNoTrans && N == X->size && M == Y->size)
      || (TransA == CblasTrans && M == X->size && N == Y->size))
    {
      cblas_dgemv (CblasRowMajor, TransA, INT (M), INT (N), alpha, A->data,
                   INT (A->tda), X->data, INT (X->stride), beta, Y->data,
                   INT (Y->stride));
      return GSL_SUCCESS;
    }
  else
    {
      GSL_ERROR ("invalid length", GSL_EBADLEN);
    }
}

static inline CBLAS_INDEX
cblas_idamax (const int N, const double *X, const int incX)
{
  double max = 0.0;
  INDEX ix = 0;
  INDEX i;
  CBLAS_INDEX result = 0;

  if (incX <= 0) {
    return 0;
  }

  for (i = 0; i < N; i++) {
    if (fabs(X[ix]) > max) {
      max = fabs(X[ix]);
      result = i;
    }
    ix += incX;
  }

  return result;
}

static CBLAS_INDEX_t
gsl_blas_idamax (const gsl_vector * X)
{
  return cblas_idamax (INT (X->size), X->data, INT (X->stride));
}

static inline double
cblas_dasum (const int N, const double *X, const int incX)
{
  double r = 0.0;
  INDEX i;
  INDEX ix = 0;

  if (incX <= 0) {
    return 0;
  }

  for (i = 0; i < N; i++) {
    r += fabs(X[ix]);
    ix += incX;
  }
  return r;
}

static double
gsl_blas_dasum (const gsl_vector * X)
{
  return cblas_dasum (INT (X->size), X->data, INT (X->stride));
}

static void
cblas_dscal (const int N, const double alpha, double *X, const int incX)
{
  INDEX i;
  INDEX ix;

  if (incX <= 0) {
    return;
  }

  ix = OFFSET(N, incX);

  for (i = 0; i < N; i++) {
    X[ix] *= alpha;
    ix += incX;
  }
}

static void
gsl_blas_dscal (double alpha, gsl_vector * X)
{
  cblas_dscal (INT (X->size), alpha, X->data, INT (X->stride));
}

static inline void
cblas_daxpy (const int N, const double alpha, const double *X, const int incX,
             double *Y, const int incY)
{
  INDEX i;

  if (alpha == 0.0) {
    return;
  }

  if (incX == 1 && incY == 1) {
    const INDEX m = N % 4;

    for (i = 0; i < m; i++) {
      Y[i] += alpha * X[i];
    }

    for (i = m; i + 3 < N; i += 4) {
      Y[i] += alpha * X[i];
      Y[i + 1] += alpha * X[i + 1];
      Y[i + 2] += alpha * X[i + 2];
      Y[i + 3] += alpha * X[i + 3];
    }
  } else {
    INDEX ix = OFFSET(N, incX);
    INDEX iy = OFFSET(N, incY);

    for (i = 0; i < N; i++) {
      Y[iy] += alpha * X[ix];
      ix += incX;
      iy += incY;
    }
  }
}

static int
gsl_blas_daxpy (double alpha, const gsl_vector * X, gsl_vector * Y)
{
  if (X->size == Y->size)
    {
      cblas_daxpy (INT (X->size), alpha, X->data, INT (X->stride), Y->data,
                   INT (Y->stride));
      return GSL_SUCCESS;
    }
  else
    {
      GSL_ERROR ("invalid length", GSL_EBADLEN);
    }
}

static inline void
cblas_drot (const int N, double *X, const int incX, double *Y, const int incY,
            const double c, const double s)
{
  INDEX i;
  INDEX ix = OFFSET(N, incX);
  INDEX iy = OFFSET(N, incY);
  for (i = 0; i < N; i++) {
    const double x = X[ix];
    const double y = Y[iy];
    X[ix] = c * x + s * y;
    Y[iy] = -s * x + c * y;
    ix += incX;
    iy += incY;
  }
}

static int
gsl_blas_drot (gsl_vector * X, gsl_vector * Y, const double c, const double s)
{
  if (X->size == Y->size)
    {
      cblas_drot (INT (X->size), X->data, INT (X->stride), Y->data,
                  INT (Y->stride), c, s);
      return GSL_SUCCESS;
    }
  else
    {
      GSL_ERROR ("invalid length", GSL_EBADLEN);
    }
}

static inline double
cblas_ddot (const int N, const double *X, const int incX, const double *Y,
            const int incY)
{
  double r = 0.0;
  INDEX i;
  INDEX ix = OFFSET(N, incX);
  INDEX iy = OFFSET(N, incY);

  for (i = 0; i < N; i++) {
    r += X[ix] * Y[iy];
    ix += incX;
    iy += incY;
  }

  return r;
}

static int
gsl_blas_ddot (const gsl_vector * X, const gsl_vector * Y, double *result)
{
  if (X->size == Y->size)
    {
      *result =
        cblas_ddot (INT (X->size), X->data, INT (X->stride), Y->data,
                    INT (Y->stride));
      return GSL_SUCCESS;
    }
  else
    {
      GSL_ERROR ("invalid length", GSL_EBADLEN);
    }
}
