/*
 *  $Id$
 *  Copyright (C) 2009-2011 David Nečas (Yeti).
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

#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "timer.h"
#include "libgwy/macros.h"

#define DSWAP(x, y) GWY_SWAP(gdouble, x, y)

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

static void
sort_plain(gdouble *array,
           gsize MAX_THRESH,
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

int
main(int argc, char *argv[])
{
    guint size_min = 4, size_max = 65536,
          threshold_min = 2, threshold_max = 32,
          rand_seed = 42;
    gdouble size_step = 1.2;

    GOptionEntry entries[] = {
        { "size-min",      's', 0, G_OPTION_ARG_INT,    &size_min,      "Smallest array size to test", "N",  },
        { "size-max",      'S', 0, G_OPTION_ARG_INT,    &size_max,      "Largest array size to test",  "N",  },
        { "size-step",     'q', 0, G_OPTION_ARG_DOUBLE, &size_step,     "Array size step factor",      "Q",  },
        { "threshold-min", 't', 0, G_OPTION_ARG_INT,    &threshold_min, "Smallest threshold to try",   "N",  },
        { "threshold-max", 'T', 0, G_OPTION_ARG_INT,    &threshold_max, "Largest threshold to try",    "N",  },
        { "random-seed",   'r', 0, G_OPTION_ARG_INT,    &rand_seed,     "Random seed",                 "R",  },
        { NULL,            0,   0, 0,                   NULL,           NULL,                          NULL, },
    };

    GError *error = NULL;
    GOptionContext *context = g_option_context_new("- optimise insert-sort threshold of quicksort");
    g_option_context_add_main_entries(context, entries, NULL);
    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        g_printerr("Arguments parsing failed: %s\n", error->message);
        return 1;
    }
    g_option_context_free(context);
    setvbuf(stdout, (char*)NULL, _IOLBF, 0);

    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, rand_seed);

    gdouble *data = g_new(gdouble, 2*size_max);
    gdouble *workspace = g_new(gdouble, size_max);
    for (guint i = 0; i < 2*size_max; i++)
        data[i] = g_rand_double(rng);

    for (guint size = size_min;
         size <= size_max;
         size = MAX(size + 1, (guint)(size_step*size + 0.5))) {
        for (guint threshold = threshold_min; threshold <= threshold_max; threshold++) {
            gulong niters = 2, i = 0;
            gdouble t;

            gwy_benchmark_timer_start();
            do {
                niters *= 2;
                while (i < niters) {
                    guint pos = g_rand_int_range(rng, 0, 2*size_max - size);
                    memcpy(workspace, data + pos, size);
                    sort_plain(workspace, threshold, size);
                    i++;
                }
                gwy_benchmark_timer_stop();

            } while ((t = gwy_benchmark_timer_get_total()) < 1.0);
            printf("%u %u %g\n", size, threshold, t/niters);
        }
        // For gnuplot
        printf("\n");
    }

    g_free(data);
    g_rand_free(rng);

    return 0;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
