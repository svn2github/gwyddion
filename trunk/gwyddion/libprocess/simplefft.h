/* @(#) $Id$ */

#ifndef __GWY_SIMPLEFFT_H__
#define __GWY_SIMPLEFFT_H__

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef enum {
  GWY_WINDOWING_NONE       = 0,
  GWY_WINDOWING_HANN       = 1,
  GWY_WINDOWING_HAMMING    = 2,
  GWY_WINDOWING_BLACKMANN  = 3,
  GWY_WINDOWING_LANCZOS    = 4,
  GWY_WINDOWING_WELCH      = 5,
  GWY_WINDOWING_RECT       = 6
} GwyWindowingType;

typedef enum {
  GWY_FFT_OUTPUT_REAL_IMG   = 0,
  GWY_FFT_OUTPUT_MOD_PHASE  = 1,
  GWY_FFT_OUTPUT_REAL       = 2,
  GWY_FFT_OUTPUT_IMG        = 3,
  GWY_FFT_OUTPUT_MOD        = 4,
  GWY_FFT_OUTPUT_PHASE      = 5
} GwyFFTOutputType;
  
  

/*2^N fft algorithm*/
gint gwy_fft_hum(gint dir, gdouble *re_in, gdouble *im_in,
                 gdouble *re_out, gdouble *im_out, gint n);

/*apply windowing*/
void gwy_fft_window(gdouble *data, gint n, GwyWindowingType windowing);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /*__GWY_SIPLEFFT__*/
