/*
 * BLAS Wrapper Implementation
 */

#include "blas_wrapper.h"
#include <string.h>

/* Pure C fallback implementation */
static void matmul_pure_c(const double *A, const double *B, double *C,
                         int m, int k, int n) {
    /* C = A * B where A is m×k, B is k×n, C is m×n */
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) {
            double sum = 0.0;
            for (int l = 0; l < k; l++) {
                sum += A[i * k + l] * B[l * n + j];
            }
            C[i * n + j] = sum;
        }
    }
}

static void transpose_pure_c(const double *A, double *B, int m, int n) {
    /* B = A^T where A is m×n, B is n×m */
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) {
            B[j * m + i] = A[i * n + j];
        }
    }
}

/* Optimized matrix multiplication */
void matmul_optimized(const double *A, const double *B, double *C,
                     int m, int k, int n, int use_blas) {
#if HAS_BLAS
    if (use_blas) {
        /* Use BLAS dgemm: C = alpha*A*B + beta*C
         * We want C = A*B, so alpha=1, beta=0
         *
         * BLAS dgemm signature:
         * cblas_dgemm(Order, TransA, TransB, M, N, K,
         *             alpha, A, lda, B, ldb, beta, C, ldc)
         *
         * For C = A*B (no transpose):
         * - Order: CblasRowMajor
         * - TransA: CblasNoTrans
         * - TransB: CblasNoTrans
         * - M, N, K: dimensions
         * - lda = K (leading dimension of A)
         * - ldb = N (leading dimension of B)
         * - ldc = N (leading dimension of C)
         */
        cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans,
                   m, n, k,
                   1.0, A, k, B, n,
                   0.0, C, n);
        return;
    }
#endif
    /* Fallback to pure C */
    matmul_pure_c(A, B, C, m, k, n);
}

/* Optimized transpose */
void transpose_optimized(const double *A, double *B, int m, int n, int use_blas) {
#if HAS_BLAS
    if (use_blas) {
        /* BLAS doesn't have a dedicated transpose, but we can use dgemm
         * with TransA = CblasTrans to effectively transpose
         * B = 1.0 * A^T * I + 0.0 * B
         * But simpler: just use the pure C version (it's fast enough for transpose)
         */
    }
#endif
    /* Always use pure C for transpose (simple enough, memory-bound anyway) */
    transpose_pure_c(A, B, m, n);
}

/* Check if BLAS is available */
int has_blas(void) {
    return HAS_BLAS;
}

/* Get implementation name */
const char* get_blas_impl(void) {
#if HAS_BLAS
    #ifdef __APPLE__
        return "Apple Accelerate";
    #else
        return "OpenBLAS";
    #endif
#else
    return "Pure C (no BLAS)";
#endif
}
