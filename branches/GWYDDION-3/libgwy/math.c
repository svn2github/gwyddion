/*
 *  $Id$
 *  Copyright (C) 2009 David Necas (Yeti).
 *  E-mail: yeti@gwyddion.net.
 *
 *  The quicksort algorithm was copied from GNU C library,
 *  Copyright (C) 1991, 1992, 1996, 1997, 1999 Free Software Foundation, Inc.
 *  See below.
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

#include "libgwy/macros.h"
#include "libgwy/math.h"
#include "libgwy/libgwy-aliases.h"

#define DSWAP(x, y) GWY_SWAP(gdouble, x, y)
#define ISWAP(x, y) GWY_SWAP(guint, x, y)
#define SLi gwy_lower_triangular_matrix_index

static void sort_plain(gdouble *array,
                       gsize n);
static void sort_with_index(gdouble *array,
                            guint *index_array,
                            gsize n);

/* Quickly find median value in an array
 * based on public domain code by Nicolas Devillard */
/**
 * gwy_math_median:
 * @array: Array of doubles.  It is modified by this function.  All values are
 *         kept, but their positions in the array change.
 * @n: Number of items in @array.
 *
 * Finds median of an array of values using Quick select algorithm.
 *
 * Returns: The median value of @array.
 **/
gdouble
gwy_math_median(gdouble *array, gsize n)
{
    gsize lo, hi;
    gsize median;
    gsize middle, ll, hh;

    lo = 0;
    hi = n - 1;
    median = n/2;
    while (TRUE) {
        if (hi <= lo)        /* One element only */
            return array[median];

        if (hi == lo + 1) {  /* Two elements only */
            if (array[lo] > array[hi])
                DSWAP(array[lo], array[hi]);
            return array[median];
        }

        /* Find median of lo, middle and hi items; swap into position lo */
        middle = (lo + hi)/2;
        if (array[middle] > array[hi])
            DSWAP(array[middle], array[hi]);
        if (array[lo] > array[hi])
            DSWAP(array[lo], array[hi]);
        if (array[middle] > array[lo])
            DSWAP(array[middle], array[lo]);

        /* Swap low item (now in position middle) into position (lo+1) */
        DSWAP(array[middle], array[lo + 1]);

        /* Nibble from each end towards middle, swapping items when stuck */
        ll = lo + 1;
        hh = hi;
        while (TRUE) {
            do {
                ll++;
            } while (array[lo] > array[ll]);
            do {
                hh--;
            } while (array[hh] > array[lo]);

            if (hh < ll)
                break;

            DSWAP(array[ll], array[hh]);
        }

        /* Swap middle item (in position lo) back into correct position */
        DSWAP(array[lo], array[hh]);

        /* Re-set active partition */
        if (hh <= median)
            lo = ll;
        if (hh >= median)
            hi = hh - 1;
    }
}

/**
 * gwy_cholesky_decompose:
 * @matrix: Lower triangular part of a symmetric matrix, see
 *          gwy_lower_triangular_matrix_index() for storage details.
 * @n: Dimension of @matrix.
 *
 * Decomposes a symmetric positive definite matrix in place.
 *
 * Regardless of the return value, the contents of @matrix is overwritten.
 * In case of failure, it will not contain any meaningful values.
 *
 * Returns: %TRUE if the decomposition succeeded, %FALSE if the matrix was not
 *          numerically positive definite.
 **/
gboolean
gwy_cholesky_decompose(gdouble *a, guint n)
{
    for (guint k = 0; k < n; k++) {
        /* diagonal element */
        gdouble s = SLi(a, k, k);
        for (guint i = 0; i < k; i++)
            s -= SLi(a, k, i) * SLi(a, k, i);
        if (s <= 0.0)
            return FALSE;
        SLi(a, k, k) = s = sqrt(s);

        /* nondiagonal elements */
        for (guint j = k+1; j < n; j++) {
            gdouble r = SLi(a, j, k);
            for (guint i = 0; i < k; i++)
                r -= SLi(a, k, i) * SLi(a, j, i);
            SLi(a, j, k) = r/s;
        }
    }

    return TRUE;
}

/**
 * gwy_cholesky_solve:
 * @decomp: Lower triangular part of Cholesky decomposition as computed
 *          by gwy_math_cholesky_decompose().
 * @rhs: Right hand side vector.  It is modified in place, on return it
 *       contains the solution.
 * @n: Dimension of @decomp and length of @rhs.
 *
 * Solves a system of linear equations with predecomposed symmetric positive
 * definite matrix.
 *
 * Once the matrix is decomposed, this function can be used repeatedly to
 * calculate the solution of the system with different right-hand sides.
 **/
void
gwy_cholesky_solve(const gdouble *a, gdouble *b, guint n)
{
    guint i, j;

    /* back-substitution with the lower triangular matrix */
    for (j = 0; j < n; j++) {
        for (i = 0; i < j; i++)
            b[j] -= SLi(a, j, i)*b[i];
        b[j] /= SLi(a, j, j);
    }

    /* back-substitution with the upper triangular matrix */
    for (j = n; j > 0; j--) {
        for (i = j; i < n; i++)
            b[j-1] -= SLi(a, i, j-1)*b[i];
        b[j-1] /= SLi(a, j-1, j-1);
    }
}

/**
 * gwy_cholesky_invert:
 * @matrix: Lower triangular part of a symmetric matrix, see
 *          gwy_lower_triangular_matrix_index() for storage details.
 * @n: Dimension of @matrix.
 *
 * Inverts a positive definite matrix in place.
 *
 * Regardless of the return value, the contents of @matrix is overwritten.
 * In case of failure, it will not contain any meaningful values.
 *
 * Returns: %TRUE if the inversion succeeded, %FALSE if the matrix was not
 *          numerically positive definite.
 **/
gboolean
gwy_cholesky_invert(gdouble *a, guint n)
{
    gdouble x[n];
    for (guint k = n; k > 0; k--) {
        gdouble s = a[0];
        if (s <= 0)
            return FALSE;
        guint m = 0, q = 0;
        for (guint i = 0; i < n-1; i++) {
            q = m+1;
            m += i+2;
            gdouble t = a[q];
            x[i] = -t/s;      /* note use temporary x */
            if (i >= k-1)
                x[i] = -x[i];
            for (guint j = q; j < m; j++)
                a[j - (i+1)] = a[j+1] + t * x[j - q];
        }
        a[m] = 1.0/s;
        for (guint i = 0; i < n-1; i++)
            a[q + i] = x[i];
    }
    return TRUE;
}

/**
 * gwy_math_sort:
 * @n: Number of items in @array.
 * @array: Array of doubles to sort in place.
 * @index_array: Array of integer identifiers of the items that are permuted
 *               simultaneously with @array.  Pass %NULL if the identity
 *               of the items need not be preserved.
 *
 * Sorts an array of doubles using a quicksort algorithm.
 *
 * With %NULL @index_array this is usually about twice as fast as the generic
 * quicksort function thanks to specialization for doubles.
 *
 * The simplest and probably most common use of @index_array is to fill it with
 * numbers 0 to @n-1 before calling gwy_math_sort().  After sorting,
 * @index_array[@i] then contains the original position of the @i-th item of
 * the sorted array.
 **/
void
gwy_math_sort(gdouble *array,
              guint *index_array,
              gsize n)
{
    if (index_array)
        sort_with_index(array, index_array, n);
    else
        sort_plain(array, n);
}

/****************************************************************************
 * Quicksort
 ****************************************************************************/

/* Note: The implementation was specialized for doubles and a few things were
 * renamed, otherwise it has not changed.  It is about twice as fast as the
 * generic version. */

/* Copyright (C) 1991, 1992, 1996, 1997, 1999 Free Software Foundation, Inc.
   This file is part of the GNU C Library.
   Written by Douglas C. Schmidt (schmidt@ics.uci.edu).

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

/* If you consider tuning this algorithm, you should consult first:
   Engineering a sort function; Jon Bentley and M. Douglas McIlroy;
   Software - Practice and Experience; Vol. 23 (11), 1249-1265, 1993.  */

/* Discontinue quicksort algorithm when partition gets below this size.
   This particular magic number was chosen to work best on a Sun 4/260. */
/* #define MAX_THRESH 4 */
/* Note: Specialization makes the insertion sort part relatively more
 * efficient, after some benchmarking this seems be about the best value
 * on Athlon 64. */
#define MAX_THRESH 12

/* Stack node declarations used to store unfulfilled partition obligations. */
typedef struct {
    gdouble *lo;
    gdouble *hi;
} stack_node;

/* The next 4 #defines implement a very fast in-line stack abstraction. */
/* The stack needs log (total_elements) entries (we could even subtract
   log(MAX_THRESH)).  Since total_elements has type size_t, we get as
   upper bound for log (total_elements):
   bits per byte (CHAR_BIT) * sizeof(size_t).  */
#define STACK_SIZE      (CHAR_BIT * sizeof(gsize))
#define PUSH(low, high) ((void) ((top->lo = (low)), (top->hi = (high)), ++top))
#define POP(low, high)  ((void) (--top, (low = top->lo), (high = top->hi)))
#define STACK_NOT_EMPTY (stack < top)

/* Order size using quicksort.  This implementation incorporates
   four optimizations discussed in Sedgewick:

   1. Non-recursive, using an explicit stack of pointer that store the
   next array partition to sort.  To save time, this maximum amount
   of space required to store an array of SIZE_MAX is allocated on the
   stack.  Assuming a 32-bit (64 bit) integer for size_t, this needs
   only 32 * sizeof(stack_node) == 256 bytes (for 64 bit: 1024 bytes).
   Pretty cheap, actually.

   2. Chose the pivot element using a median-of-three decision tree.
   This reduces the probability of selecting a bad pivot value and
   eliminates certain extraneous comparisons.

   3. Only quicksorts TOTAL_ELEMS / MAX_THRESH partitions, leaving
   insertion sort to order the MAX_THRESH items within each partition.
   This is a big win, since insertion sort is faster for small, mostly
   sorted array segments.

   4. The larger of the two sub-partitions is always pushed onto the
   stack first, with the algorithm then concentrating on the
   smaller partition.  This *guarantees* no more than log(n)
   stack size is needed (actually O(1) in this case)!  */

static void
sort_plain(gdouble *array,
           gsize n)
{
    if (n < 2)
        /* Avoid lossage with unsigned arithmetic below.  */
        return;

    if (n > MAX_THRESH) {
        gdouble *lo = array;
        gdouble *hi = lo + (n - 1);
        stack_node stack[STACK_SIZE];
        stack_node *top = stack + 1;

        while (STACK_NOT_EMPTY) {
            gdouble *left_ptr;
            gdouble *right_ptr;

            /* Select median value from among LO, MID, and HI. Rearrange
               LO and HI so the three values are sorted. This lowers the
               probability of picking a pathological pivot value and
               skips a comparison for both the LEFT_PTR and RIGHT_PTR in
               the while loops. */

            gdouble *mid = lo + ((hi - lo) >> 1);

            if (*mid < *lo)
                DSWAP(*mid, *lo);
            if (*hi < *mid)
                DSWAP(*mid, *hi);
            else
                goto jump_over;
            if (*mid < *lo)
                DSWAP(*mid, *lo);

jump_over:
          left_ptr  = lo + 1;
          right_ptr = hi - 1;

          /* Here's the famous ``collapse the walls'' section of quicksort.
             Gotta like those tight inner loops!  They are the main reason
             that this algorithm runs much faster than others. */
          do {
              while (*left_ptr < *mid)
                  left_ptr++;

              while (*mid < *right_ptr)
                  right_ptr--;

              if (left_ptr < right_ptr) {
                  DSWAP(*left_ptr, *right_ptr);
                  if (mid == left_ptr)
                      mid = right_ptr;
                  else if (mid == right_ptr)
                      mid = left_ptr;
                  left_ptr++;
                  right_ptr--;
              }
              else if (left_ptr == right_ptr) {
                  left_ptr++;
                  right_ptr--;
                  break;
              }
          }
          while (left_ptr <= right_ptr);

          /* Set up pointers for next iteration.  First determine whether
             left and right partitions are below the threshold size.  If so,
             ignore one or both.  Otherwise, push the larger partition's
             bounds on the stack and continue sorting the smaller one. */

          if ((gsize)(right_ptr - lo) <= MAX_THRESH) {
              if ((gsize)(hi - left_ptr) <= MAX_THRESH)
                  /* Ignore both small partitions. */
                  POP(lo, hi);
              else
                  /* Ignore small left partition. */
                  lo = left_ptr;
          }
          else if ((gsize)(hi - left_ptr) <= MAX_THRESH)
              /* Ignore small right partition. */
              hi = right_ptr;
          else if ((right_ptr - lo) > (hi - left_ptr)) {
              /* Push larger left partition indices. */
              PUSH(lo, right_ptr);
              lo = left_ptr;
          }
          else {
              /* Push larger right partition indices. */
              PUSH(left_ptr, hi);
              hi = right_ptr;
          }
        }
    }

    /* Once the BASE_PTR array is partially sorted by quicksort the rest
       is completely sorted using insertion sort, since this is efficient
       for partitions below MAX_THRESH size. BASE_PTR points to the beginning
       of the array to sort, and END_PTR points at the very last element in
       the array (*not* one beyond it!). */

    {
        gdouble *const end_ptr = array + (n - 1);
        gdouble *tmp_ptr = array;
        gdouble *thresh = MIN(end_ptr, array + MAX_THRESH);
        gdouble *run_ptr;

        /* Find smallest element in first threshold and place it at the
           array's beginning.  This is the smallest array element,
           and the operation speeds up insertion sort's inner loop. */

        for (run_ptr = tmp_ptr + 1; run_ptr <= thresh; run_ptr++) {
            if (*run_ptr < *tmp_ptr)
                tmp_ptr = run_ptr;
        }

        if (tmp_ptr != array)
            DSWAP(*tmp_ptr, *array);

        /* Insertion sort, running from left-hand-side up to right-hand-side.
         */

        run_ptr = array + 1;
        while (++run_ptr <= end_ptr) {
            tmp_ptr = run_ptr - 1;
            while (*run_ptr < *tmp_ptr)
                tmp_ptr--;

            tmp_ptr++;
            if (tmp_ptr != run_ptr) {
                gdouble *hi, *lo;
                gdouble d;

                d = *run_ptr;
                for (hi = lo = run_ptr; --lo >= tmp_ptr; hi = lo)
                    *hi = *lo;
                *hi = d;
            }
        }
    }
}

typedef struct {
    gdouble *lo;
    gdouble *hi;
    guint *loi;
    guint *hii;
} stacki_node;

/* FIXME: It is questionable whether it is still more efficient to use pointers
 * instead of array indices when it effectively doubles the number of
 * variables.  This might force some variables from registers to memory... */
static void
sort_with_index(gdouble *array,
                guint *index_array,
                gsize n)
{
    if (n < 2)
        /* Avoid lossage with unsigned arithmetic below.  */
        return;

    if (n > MAX_THRESH) {
        gdouble *lo = array;
        gdouble *hi = lo + (n - 1);
        guint *loi = index_array;
        guint *hii = loi + (n - 1);
        stacki_node stack[STACK_SIZE];
        stacki_node *top = stack + 1;

        while (STACK_NOT_EMPTY) {
            gdouble *left_ptr;
            gdouble *right_ptr;
            guint *left_ptri;
            guint *right_ptri;

            /* Select median value from among LO, MID, and HI. Rearrange
               LO and HI so the three values are sorted. This lowers the
               probability of picking a pathological pivot value and
               skips a comparison for both the LEFT_PTR and RIGHT_PTR in
               the while loops. */

            gdouble *mid = lo + ((hi - lo) >> 1);
            guint *midi = loi + ((hii - loi) >> 1);

            if (*mid < *lo) {
                DSWAP(*mid, *lo);
                ISWAP(*midi, *loi);
            }
            if (*hi < *mid) {
                DSWAP(*mid, *hi);
                ISWAP(*midi, *hii);
            }
            else
                goto jump_over;

            if (*mid < *lo) {
                DSWAP(*mid, *lo);
                ISWAP(*midi, *loi);
            }

jump_over:
          left_ptr  = lo + 1;
          right_ptr = hi - 1;
          left_ptri  = loi + 1;
          right_ptri = hii - 1;

          /* Here's the famous ``collapse the walls'' section of quicksort.
             Gotta like those tight inner loops!  They are the main reason
             that this algorithm runs much faster than others. */
          do {
              while (*left_ptr < *mid) {
                  left_ptr++;
                  left_ptri++;
              }

              while (*mid < *right_ptr) {
                  right_ptr--;
                  right_ptri--;
              }

              if (left_ptr < right_ptr) {
                  DSWAP(*left_ptr, *right_ptr);
                  ISWAP(*left_ptri, *right_ptri);
                  if (mid == left_ptr) {
                      mid = right_ptr;
                      midi = right_ptri;
                  }
                  else if (mid == right_ptr) {
                      midi = left_ptri;
                  }
                  left_ptr++;
                  left_ptri++;
                  right_ptr--;
                  right_ptri--;
              }
              else if (left_ptr == right_ptr) {
                  left_ptr++;
                  left_ptri++;
                  right_ptr--;
                  right_ptri--;
                  break;
              }
          }
          while (left_ptr <= right_ptr);

          /* Set up pointers for next iteration.  First determine whether
             left and right partitions are below the threshold size.  If so,
             ignore one or both.  Otherwise, push the larger partition's
             bounds on the stack and continue sorting the smaller one. */

          if ((gsize)(right_ptr - lo) <= MAX_THRESH) {
              if ((gsize)(hi - left_ptr) <= MAX_THRESH) {
                  /* Ignore both small partitions. */
                  --top;
                  lo = top->lo;
                  hi = top->hi;
                  loi = top->loi;
                  hii = top->hii;
              }
              else {
                  /* Ignore small left partition. */
                  lo = left_ptr;
                  loi = left_ptri;
              }
          }
          else if ((gsize)(hi - left_ptr) <= MAX_THRESH) {
              /* Ignore small right partition. */
              hi = right_ptr;
              hii = right_ptri;
          }
          else if ((right_ptr - lo) > (hi - left_ptr)) {
              /* Push larger left partition indices. */
              top->lo = lo;
              top->loi = loi;
              top->hi = right_ptr;
              top->hii = right_ptri;
              ++top;
              lo = left_ptr;
              loi = left_ptri;
          }
          else {
              /* Push larger right partition indices. */
              top->lo = left_ptr;
              top->loi = left_ptri;
              top->hi = hi;
              top->hii = hii;
              ++top;
              hi = right_ptr;
              hii = right_ptri;
          }
        }
    }

    /* Once the BASE_PTR array is partially sorted by quicksort the rest
       is completely sorted using insertion sort, since this is efficient
       for partitions below MAX_THRESH size. BASE_PTR points to the beginning
       of the array to sort, and END_PTR points at the very last element in
       the array (*not* one beyond it!). */

    {
        gdouble *const end_ptr = array + (n - 1);
        gdouble *tmp_ptr = array;
        guint *tmp_ptri = index_array;
        gdouble *thresh = MIN(end_ptr, array + MAX_THRESH);
        gdouble *run_ptr;
        guint *run_ptri;

        /* Find smallest element in first threshold and place it at the
           array's beginning.  This is the smallest array element,
           and the operation speeds up insertion sort's inner loop. */

        for (run_ptr = tmp_ptr + 1, run_ptri = tmp_ptri + 1;
             run_ptr <= thresh;
             run_ptr++, run_ptri++) {
            if (*run_ptr < *tmp_ptr) {
                tmp_ptr = run_ptr;
                tmp_ptri = run_ptri;
            }
        }

        if (tmp_ptr != array) {
            DSWAP(*tmp_ptr, *array);
            ISWAP(*tmp_ptri, *index_array);
        }

        /* Insertion sort, running from left-hand-side up to right-hand-side.
         */

        run_ptr = array + 1;
        run_ptri = index_array + 1;
        while (++run_ptr <= end_ptr) {
            tmp_ptr = run_ptr - 1;
            tmp_ptri = run_ptri;
            ++run_ptri;
            while (*run_ptr < *tmp_ptr) {
                tmp_ptr--;
                tmp_ptri--;
            }

            tmp_ptr++;
            tmp_ptri++;
            if (tmp_ptr != run_ptr) {
                gdouble *hi, *lo;
                guint *hii, *loi;
                gdouble d;
                guint i;

                d = *run_ptr;
                for (hi = lo = run_ptr; --lo >= tmp_ptr; hi = lo)
                    *hi = *lo;
                *hi = d;

                i = *run_ptri;
                for (hii = loi = run_ptri; --loi >= tmp_ptri; hii = loi)
                    *hii = *loi;
                *hii = i;
            }
        }
    }
}

#define __LIBGWY_MATH_C__
#include "libgwy/libgwy-aliases.c"

/**
 * SECTION: math
 * @title: math
 * @short_description: Mathematical functions
 *
 * A number of less standard but useful mathematical functions is provided to
 * ensure they are available on all platforms.
 * 
 * These functions exist in two flavours:
 * <itemizedlist>
 *   <listitem>
 *     Namespace-clean, that is prefixed with <literal>gwy_</literal>.
 *     For instance, gwy_exp10().  These are defined always.
 *   </listitem>
 *   <listitem>
 *     Bare-named, i.e. with the same name as the C library function, for
 *     instance exp10().  There are defined only when explicitly
 *     requested with:
 * |[#define GWY_MATH_POLLUTE_NAMESPACE]|
 *     to avoid problems when Gwyddion headers are included indirectly or
 *     in combination with other libraries.
 *   </listitem>
 * </itemizedlist>
 *
 * Both kinds of symbols can be either functions or macros expanding to a
 * function name so it it always possible to take their addresses.  The bare
 * names are in no case exported.  If the system C library provides a specific
 * function, both kinds of symbols are defined so that the system
 * implementation is directly used.
 **/

/**
 * gwy_exp10:
 * @x: Exponent of 10.
 *
 * Calculates the value of 10 raised to given power.
 *
 * Returns: Value of 10 raised to @x.
 **/

/**
 * exp10:
 *
 * System function <literal>exp10</literal> or alias of gwy_exp10().
 *
 * This macro expands to an identifier.
 **/

/**
 * gwy_powi:
 * @x: Base.
 * @i: Integer exponent.
 *
 * Calculates the integer power of a number.
 *
 * Returns: Value of @x raised to the integer power @i.
 **/

/**
 * powi:
 *
 * GNU C function <literal>__builtin_powi</literal> or alias of gwy_powi().
 *
 * This macro expands to an identifier.
 **/

/**
 * gwy_round:
 * @x: Double value.
 *
 * Rounds a number to nearest integer.
 *
 * This macro evaluates its argument only once.
 **/

/**
 * gwy_triangular_matrix_length:
 * @n: Matrix dimension.
 *
 * Evaluates to the length of an array necessary to store a triangular matrix.
 *
 * This macro may evaluate its arguments several times.
 *
 * The length is the same for triangular matrices of both kinds and also for
 * symmetrical matrices although the element interpretation differs.
 **/

/**
 * gwy_lower_triangular_matrix_index:
 * @a: Lower triangular matrix stored by rows.
 * @i: Row index.
 * @j: Column index, it must be at most equal to @i.
 *
 * Accesses an element of lower triangular matrix.
 *
 * This macro may evaluate its arguments several times.
 * This macro expands to a left-hand side expression.
 *
 * The matrix is assumed to be stored as follows:
 * [a_00 a_10 a_11 a_20 a_21 a_22 a_30 ...] which is suitable also for
 * symmetrical matrices as only half of the matrix needs to be stored.
 * However, as triangular matrices are complemented with zeroes while
 * symmetrical matrices with reflected values one has to be careful and not
 * confuse them despite the same representation.
 *
 * For instance, to multiply rows and columns of @a by the corresponding
 * elements of vector @v one can do:
 * |[
 * for (guint i = 0; i < n; i++) {
 *     for (guint j = 0; j <= i; j++)
 *         gwy_lower_triangular_matrix_index(a, i, j) *= v[i] * v[j];
 * }
 * ]|
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
