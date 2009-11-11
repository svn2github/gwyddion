#ifndef __WAVELET_H__
#define __WAVELET_H__ 1

#include <assert.h>
#include <stdlib.h>

int coefficients_daubechies(const long int n,
                            double *c);
int scaling_function       (const long int scaling_steps,
                            double *phi,
                            double *psi,
                            const long int K,
                            const double *c);

static inline void *
xmalloc(size_t size)
{
    void *p = malloc(size);
    assert(p != NULL);
    return p;
}

static inline void *
xrealloc(void *ptr, size_t size)
{
    void *p = realloc(ptr, size);
    assert(p != NULL);
    return p;
}

static inline void*
xfree(void *ptr)
{
    free(ptr);
    return NULL;
}

#endif
/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
