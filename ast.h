/*
 * FluxParser - Research-Grade C Math Parser
 * Copyright (C) 2025 Eduardo Stern
 *
 * Dual Licensed:
 * - GPL-3.0 for open-source/non-commercial use
 * - Commercial license available - see LICENSE-COMMERCIAL.md
 *
 * SPDX-License-Identifier: GPL-3.0-or-later OR Commercial
 */

#ifndef AST_H
#define AST_H

#include <stdbool.h>
#include "parser.h"

/* AST Node Types */
typedef enum {
    AST_NUMBER,
    AST_VARIABLE,
    AST_BINARY_OP,
    AST_UNARY_OP,
    AST_FUNCTION_CALL,
    AST_TENSOR              /* Tensor/Matrix (for ML operations) */
} ASTNodeType;

/* Binary operators */
typedef enum {
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_POWER,
    OP_AND,
    OP_OR,
    OP_GREATER,
    OP_LESS,
    OP_GREATER_EQ,
    OP_LESS_EQ,
    OP_EQUAL,
    OP_NOT_EQUAL
} BinaryOp;

/* Unary operators */
typedef enum {
    OP_NEGATE,
    OP_NOT
} UnaryOp;

/* ============================================================================
 * TENSOR/MATRIX SUPPORT (Phase 1: LLM Parser)
 * ============================================================================ */

/* Tensor structure - multi-dimensional array for ML operations */
typedef struct {
    double *data;           /* Flat array of values (row-major order) */
    int *shape;             /* Dimensions: [dim0, dim1, ..., dim(rank-1)] */
    int rank;               /* Number of dimensions (0=scalar, 1=vector, 2=matrix, etc.) */
    int size;               /* Total number of elements (product of shape) */
    int ref_count;          /* Reference counting for memory management */
} Tensor;

/* Matrix operations */
typedef enum {
    OP_MATMUL,              /* Matrix multiplication */
    OP_DOT,                 /* Dot product (vectors) */
    OP_TRANSPOSE,           /* Matrix transpose */
    OP_RESHAPE,             /* Change tensor shape */
    OP_BROADCAST_ADD,       /* Broadcasting addition */
    OP_BROADCAST_MULTIPLY   /* Broadcasting multiplication */
} MatrixOp;

/* Forward declaration */
typedef struct ASTNode ASTNode;

/* AST Node structure */
struct ASTNode {
    ASTNodeType type;
    union {
        /* NUMBER */
        struct {
            double value;
        } number;

        /* VARIABLE */
        struct {
            char name[32];
        } variable;

        /* BINARY_OP */
        struct {
            BinaryOp op;
            ASTNode *left;
            ASTNode *right;
        } binary;

        /* UNARY_OP */
        struct {
            UnaryOp op;
            ASTNode *operand;
        } unary;

        /* FUNCTION_CALL */
        struct {
            char name[32];
            ASTNode **args;
            int arg_count;
        } function;

        /* TENSOR */
        struct {
            Tensor *tensor;     /* Pointer to tensor data */
        } tensor;
    } data;
};

/* AST Construction */
ASTNode* ast_create_number(double value);
ASTNode* ast_create_variable(const char *name);
ASTNode* ast_create_binary_op(BinaryOp op, ASTNode *left, ASTNode *right);
ASTNode* ast_create_unary_op(UnaryOp op, ASTNode *operand);
ASTNode* ast_create_function_call(const char *name, ASTNode **args, int arg_count);

/* AST Management */
void ast_free(ASTNode *node);
ASTNode* ast_clone(const ASTNode *node);

/* AST Evaluation */
double ast_evaluate(const ASTNode *node, VarContext *vars);

/* AST Printing */
void ast_print(const ASTNode *node);
char* ast_to_string(const ASTNode *node);

/* AST Analysis */
bool ast_contains_variable(const ASTNode *node, const char *var_name);
int ast_count_operations(const ASTNode *node);

/* Symbolic Differentiation */
ASTNode* ast_differentiate(const ASTNode *node, const char *var_name);

/* Symbolic Integration */
ASTNode* ast_integrate(const ASTNode *node, const char *var_name);

/* Numerical Integration */
typedef enum {
    INTEGRATE_TRAPEZOIDAL,
    INTEGRATE_SIMPSON
} IntegrationMethod;

/* Numerical integration using trapezoidal rule */
double ast_integrate_numerical_trapezoidal(
    const ASTNode *expr,
    const char *var_name,
    double a,
    double b,
    int steps
);

/* Numerical integration using Simpson's rule */
double ast_integrate_numerical_simpson(
    const ASTNode *expr,
    const char *var_name,
    double a,
    double b,
    int steps
);

/* General numerical integration with method selection */
double ast_integrate_numerical(
    const ASTNode *expr,
    const char *var_name,
    double a,
    double b,
    int steps,
    IntegrationMethod method
);

/* Partial Derivatives & Gradient */

/* Compute partial derivative ∂f/∂var (alias for ast_differentiate for clarity) */
ASTNode* ast_partial_derivative(const ASTNode *node, const char *var_name);

/* Gradient vector structure */
typedef struct {
    ASTNode **components;  /* Array of partial derivatives */
    int count;             /* Number of components */
    char **var_names;      /* Variable names for reference */
} Gradient;

/* Compute gradient vector ∇f = [∂f/∂x₁, ∂f/∂x₂, ...] */
Gradient ast_gradient(const ASTNode *node, const char **var_names, int var_count);

/* Free gradient structure */
void gradient_free(Gradient *grad);

/* Evaluate gradient at a point */
double* gradient_evaluate(const Gradient *grad, VarContext *vars);

/* Taylor Series Expansion */

/* Expand f(x) as Taylor series around x=center up to given order
 * Returns: c₀ + c₁(x-center) + c₂(x-center)²/2! + ... + cₙ(x-center)ⁿ/n!
 */
ASTNode* ast_taylor_series(
    const ASTNode *expr,
    const char *var_name,
    double center,
    int order
);

/* Expression Simplification */
ASTNode* ast_simplify(ASTNode *node);

/* Variable Substitution */
ASTNode* ast_substitute(const ASTNode *node, const char *var_name, const ASTNode *replacement);

/* Polynomial Factorization */
ASTNode* ast_factor(ASTNode *node, const char *var_name);

/* Equation Solving */
typedef struct {
    ASTNode **solutions;
    int solution_count;
    bool has_solution;
    char error_message[256];
} SolveResult;

/* Symbolic solving (linear & quadratic only) */
SolveResult ast_solve_equation(ASTNode *equation, const char *var_name);
void solve_result_free(SolveResult *result);

/* Numerical solving (Newton-Raphson for any equation) */
typedef struct {
    double solution;
    bool converged;
    int iterations;
    double final_error;
    char error_message[256];
} NumericalSolveResult;

NumericalSolveResult ast_solve_numerical(ASTNode *equation, const char *var_name,
                                          double initial_guess, double tolerance, int max_iterations);

/* ============================================================================
 * OPTIMIZATION ENGINE
 * ============================================================================ */

/* Optimization methods */
typedef enum {
    OPTIMIZER_GRADIENT_DESCENT,        /* Basic gradient descent */
    OPTIMIZER_GRADIENT_DESCENT_MOMENTUM, /* Gradient descent with momentum */
    OPTIMIZER_ADAM,                    /* Adaptive Moment Estimation (Adam) */
    OPTIMIZER_CONJUGATE_GRADIENT       /* Conjugate gradient method */
} OptimizerType;

/* Optimizer configuration */
typedef struct {
    double learning_rate;      /* Step size (typically 0.001 to 0.1) */
    double tolerance;          /* Convergence threshold */
    int max_iterations;        /* Maximum number of iterations */
    bool verbose;              /* Print progress during optimization */

    /* Momentum-specific */
    double momentum;           /* Momentum coefficient (typically 0.9) */

    /* Adam-specific */
    double beta1;              /* First moment decay (typically 0.9) */
    double beta2;              /* Second moment decay (typically 0.999) */
    double epsilon;            /* Small constant for numerical stability (1e-8) */

    /* Conjugate gradient specific */
    int restart_iterations;    /* Restart CG every N iterations (0 = no restart) */
} OptimizerConfig;

/* Optimization result */
typedef struct {
    double *solution;          /* Optimized variable values */
    double final_value;        /* Final objective function value */
    int iterations;            /* Number of iterations performed */
    bool converged;            /* Whether optimizer converged */
    double *history;           /* History of objective values (if verbose) */
    int history_count;         /* Number of history entries */
    char error_message[256];   /* Error message if failed */
} OptimizationResult;

/* Create default optimizer configuration */
OptimizerConfig optimizer_config_default(OptimizerType type);

/* Minimize objective function
 * expr: Objective function to minimize f(x1, x2, ..., xn)
 * var_names: Array of variable names ["x", "y", "z", ...]
 * var_count: Number of variables
 * initial_guess: Starting point for optimization
 * config: Optimizer configuration
 * type: Optimization method to use
 */
OptimizationResult ast_minimize(
    const ASTNode *expr,
    const char **var_names,
    int var_count,
    const double *initial_guess,
    const OptimizerConfig *config,
    OptimizerType type
);

/* Maximize objective function (minimizes -f) */
OptimizationResult ast_maximize(
    const ASTNode *expr,
    const char **var_names,
    int var_count,
    const double *initial_guess,
    const OptimizerConfig *config,
    OptimizerType type
);

/* Free optimization result */
void optimization_result_free(OptimizationResult *result);

/* Line search for optimal step size (used internally by optimizers) */
double line_search_backtracking(
    const ASTNode *expr,
    VarContext *ctx,
    const double *position,
    const double *direction,
    int var_count,
    double alpha_init,
    double rho,
    double c
);

/* ============================================================================
 * TENSOR/MATRIX API (Phase 1: LLM Parser)
 * ============================================================================ */

/* Tensor Creation & Management */
Tensor* tensor_create(const int *shape, int rank);
Tensor* tensor_create_from_data(const double *data, const int *shape, int rank);
Tensor* tensor_zeros(const int *shape, int rank);
Tensor* tensor_ones(const int *shape, int rank);
Tensor* tensor_random(const int *shape, int rank);  /* Uniform [0,1] */
Tensor* tensor_randn(const int *shape, int rank);   /* Normal N(0,1) */
void tensor_free(Tensor *tensor);
Tensor* tensor_clone(const Tensor *tensor);
void tensor_retain(Tensor *tensor);   /* Increment ref count */
void tensor_release(Tensor *tensor);  /* Decrement ref count, free if 0 */

/* Tensor Properties */
void tensor_print(const Tensor *tensor);
int tensor_get_size(const Tensor *tensor);
bool tensor_same_shape(const Tensor *a, const Tensor *b);

/* Tensor Operations (Element-wise) */
Tensor* tensor_add(const Tensor *a, const Tensor *b);
Tensor* tensor_subtract(const Tensor *a, const Tensor *b);
Tensor* tensor_multiply(const Tensor *a, const Tensor *b);  /* Element-wise (Hadamard) */
Tensor* tensor_divide(const Tensor *a, const Tensor *b);
Tensor* tensor_negate(const Tensor *a);

/* Scalar Operations */
Tensor* tensor_add_scalar(const Tensor *a, double scalar);
Tensor* tensor_multiply_scalar(const Tensor *a, double scalar);

/* Matrix Operations */
Tensor* tensor_matmul(const Tensor *a, const Tensor *b);    /* Matrix multiplication */
Tensor* tensor_transpose(const Tensor *a);                  /* Transpose (2D only) */
Tensor* tensor_dot(const Tensor *a, const Tensor *b);       /* Dot product (1D only) */

/* Activation Functions (Element-wise) */
Tensor* tensor_relu(const Tensor *x);
Tensor* tensor_sigmoid(const Tensor *x);
Tensor* tensor_tanh(const Tensor *x);
Tensor* tensor_softmax(const Tensor *x);  /* Along last dimension */

/* Reduction Operations */
double tensor_sum(const Tensor *x);
double tensor_mean(const Tensor *x);
double tensor_max(const Tensor *x);
double tensor_min(const Tensor *x);

/* AST Tensor Operations */
ASTNode* ast_create_tensor(Tensor *tensor);
ASTNode* ast_tensor_matmul(ASTNode *a, ASTNode *b);
ASTNode* ast_tensor_add(ASTNode *a, ASTNode *b);

/* Bytecode Compilation */
typedef enum {
    BC_PUSH_NUM,      /* Push number onto stack */
    BC_PUSH_VAR,      /* Push variable value onto stack */
    BC_ADD,
    BC_SUBTRACT,
    BC_MULTIPLY,
    BC_DIVIDE,
    BC_POWER,
    BC_NEGATE,
    BC_NOT,
    BC_AND,
    BC_OR,
    BC_GREATER,
    BC_LESS,
    BC_GREATER_EQ,
    BC_LESS_EQ,
    BC_EQUAL,
    BC_NOT_EQUAL,
    BC_CALL_FUNC,     /* Call function with N args */
    BC_HALT
} BytecodeOp;

typedef struct {
    BytecodeOp op;
    union {
        double num;
        int var_index;
        struct {
            char name[32];
            int arg_count;
        } func;
    } data;
} BytecodeInstruction;

typedef struct {
    BytecodeInstruction *instructions;
    int count;
    int capacity;
} Bytecode;

/* Bytecode Compilation */
Bytecode* ast_compile(const ASTNode *node);
void bytecode_free(Bytecode *bc);
void bytecode_print(const Bytecode *bc);

/* Bytecode VM Execution */
typedef struct {
    double *stack;
    int stack_pointer;
    int stack_capacity;
    VarContext *vars;
} VM;

VM* vm_create(VarContext *vars);
void vm_free(VM *vm);
double vm_execute(VM *vm, const Bytecode *bc);

/* High-level API */
typedef struct {
    ASTNode *ast;
    Bytecode *bytecode;
    char *original_expr;
} CompiledExpression;

CompiledExpression* compile_expression(const char *expr);
void compiled_expression_free(CompiledExpression *ce);
double compiled_expression_evaluate(CompiledExpression *ce, VarContext *vars);

/* Symbolic Operations */
char* differentiate_expression(const char *expr, const char *var_name);
char* simplify_expression(const char *expr);
char* integrate_expression(const char *expr, const char *var_name);
char** solve_expression(const char *equation, const char *var_name, int *solution_count);

#endif /* AST_H */
