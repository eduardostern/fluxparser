/*
 * BLAS Wrapper - Optional acceleration for matrix operations
 * Falls back to pure C if BLAS not available
 */

#ifndef BLAS_WRAPPER_H
#define BLAS_WRAPPER_H

#include <stddef.h>

/* Check if we have BLAS available */
#ifdef __APPLE__
    #define HAS_BLAS 1
    #include <Accelerate/Accelerate.h>
#elif defined(USE_OPENBLAS)
    #define HAS_BLAS 1
    #include <cblas.h>
#else
    #define HAS_BLAS 0
#endif

/* Matrix multiplication: C = A * B
 * A: m×k, B: k×n, C: m×n
 * If use_blas=1 and BLAS available, uses optimized BLAS
 * Otherwise falls back to pure C implementation
 */
void matmul_optimized(const double *A, const double *B, double *C,
                     int m, int k, int n, int use_blas);

/* Matrix transpose: B = A^T
 * A: m×n, B: n×m
 */
void transpose_optimized(const double *A, double *B, int m, int n, int use_blas);

/* Check if BLAS is available */
int has_blas(void);

/* Get implementation name */
const char* get_blas_impl(void);

#endif /* BLAS_WRAPPER_H */
