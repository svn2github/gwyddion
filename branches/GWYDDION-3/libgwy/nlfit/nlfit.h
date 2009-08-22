#ifndef __GWY_GSL_NLFIT_H__
#define __GWY_GSL_NLFIT_H__ 1

#include <stdlib.h>
#include <math.h>

#define GSL_RANGE_CHECK 0

#define GSL_DBL_EPSILON        2.2204460492503131e-16
#define GSL_SQRT_DBL_EPSILON   1.4901161193847656e-08
#define GSL_DBL_MIN        2.2250738585072014e-308
#define GSL_SQRT_DBL_MIN   1.4916681462400413e-154
#define GSL_SQRT_DBL_MAX   1.3407807929942596e+154

#define GSL_VAR extern

static inline double
GSL_MAX_DBL(double a, double b)
{
  return MAX(a, b);
}

static inline double
GSL_MIN_DBL(double a, double b)
{
  return MIN(a, b);
}

#include "gsl_errno.h"
#include "gsl_block.h"
#include "gsl_vector.h"
#include "gsl_matrix.h"
#include "gsl_blas.h"
#include "gsl_permutation.h"
#include "gsl_linalg.h"
#include "gsl_multifit_nlin.h"

#endif
