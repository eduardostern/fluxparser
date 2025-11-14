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

#include "ast.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <pthread.h>
#include <time.h>

/* Thread-safe RNG - shared with parser.c */
extern pthread_mutex_t rng_mutex;
extern bool random_seeded;

/* ============================================================================
 * AST CONSTRUCTION
 * ============================================================================ */

ASTNode* ast_create_number(double value) {
    ASTNode *node = malloc(sizeof(ASTNode));
    node->type = AST_NUMBER;
    node->data.number.value = value;
    return node;
}

ASTNode* ast_create_variable(const char *name) {
    ASTNode *node = malloc(sizeof(ASTNode));
    node->type = AST_VARIABLE;
    strncpy(node->data.variable.name, name, sizeof(node->data.variable.name) - 1);
    node->data.variable.name[sizeof(node->data.variable.name) - 1] = '\0';
    return node;
}

ASTNode* ast_create_binary_op(BinaryOp op, ASTNode *left, ASTNode *right) {
    ASTNode *node = malloc(sizeof(ASTNode));
    node->type = AST_BINARY_OP;
    node->data.binary.op = op;
    node->data.binary.left = left;
    node->data.binary.right = right;
    return node;
}

ASTNode* ast_create_unary_op(UnaryOp op, ASTNode *operand) {
    ASTNode *node = malloc(sizeof(ASTNode));
    node->type = AST_UNARY_OP;
    node->data.unary.op = op;
    node->data.unary.operand = operand;
    return node;
}

ASTNode* ast_create_function_call(const char *name, ASTNode **args, int arg_count) {
    ASTNode *node = malloc(sizeof(ASTNode));
    node->type = AST_FUNCTION_CALL;
    strncpy(node->data.function.name, name, sizeof(node->data.function.name) - 1);
    node->data.function.name[sizeof(node->data.function.name) - 1] = '\0';
    node->data.function.args = malloc(sizeof(ASTNode*) * arg_count);
    for (int i = 0; i < arg_count; i++) {
        node->data.function.args[i] = args[i];
    }
    node->data.function.arg_count = arg_count;
    return node;
}

void ast_free(ASTNode *node) {
    if (!node) return;

    switch (node->type) {
        case AST_BINARY_OP:
            ast_free(node->data.binary.left);
            ast_free(node->data.binary.right);
            break;
        case AST_UNARY_OP:
            ast_free(node->data.unary.operand);
            break;
        case AST_FUNCTION_CALL:
            for (int i = 0; i < node->data.function.arg_count; i++) {
                ast_free(node->data.function.args[i]);
            }
            free(node->data.function.args);
            break;
        case AST_TENSOR:
            tensor_release(node->data.tensor.tensor);
            break;
        default:
            break;
    }

    free(node);
}

ASTNode* ast_clone(const ASTNode *node) {
    if (!node) return NULL;

    switch (node->type) {
        case AST_NUMBER:
            return ast_create_number(node->data.number.value);

        case AST_VARIABLE:
            return ast_create_variable(node->data.variable.name);

        case AST_BINARY_OP:
            return ast_create_binary_op(
                node->data.binary.op,
                ast_clone(node->data.binary.left),
                ast_clone(node->data.binary.right)
            );

        case AST_UNARY_OP:
            return ast_create_unary_op(
                node->data.unary.op,
                ast_clone(node->data.unary.operand)
            );

        case AST_FUNCTION_CALL: {
            ASTNode **args = malloc(sizeof(ASTNode*) * node->data.function.arg_count);
            for (int i = 0; i < node->data.function.arg_count; i++) {
                args[i] = ast_clone(node->data.function.args[i]);
            }
            return ast_create_function_call(
                node->data.function.name,
                args,
                node->data.function.arg_count
            );
        }

        case AST_TENSOR: {
            Tensor *cloned_tensor = tensor_clone(node->data.tensor.tensor);
            return ast_create_tensor(cloned_tensor);
        }
    }

    return NULL;
}

/* ============================================================================
 * AST EVALUATION
 * ============================================================================ */

static double lookup_var(const char *name, VarContext *vars) {
    if (!vars || !vars->values) return 0.0;

    if (vars->mappings && vars->mapping_count > 0) {
        for (int i = 0; i < vars->mapping_count; i++) {
            if (strcmp(name, vars->mappings[i].name) == 0) {
                int idx = vars->mappings[i].index;
                if (idx >= 0 && idx < vars->count) {
                    return vars->values[idx];
                }
            }
        }
    } else if (strlen(name) == 1) {
        char c = name[0];
        if (c >= 'A' && c <= 'Z') {
            int idx = c - 'A';
            if (idx < vars->count) {
                return vars->values[idx];
            }
        }
    }

    return 0.0;
}

static double eval_function(const char *name, double *args, int arg_count) {
    /* Zero-argument */
    if (strcmp(name, "RANDOM") == 0 || strcmp(name, "RND") == 0) {
        /* Thread-safe RNG with mutex protection */
        pthread_mutex_lock(&rng_mutex);
        if (!random_seeded) {
            srand(time(NULL));
            random_seeded = true;
        }
        double r = (double)rand() / (double)RAND_MAX;
        pthread_mutex_unlock(&rng_mutex);
        return r;
    }

    /* One-argument */
    if (arg_count == 1) {
        double x = args[0];
        if (strcmp(name, "ABS") == 0) return fabs(x);
        if (strcmp(name, "ROUND") == 0) return round(x);
        if (strcmp(name, "FLOOR") == 0) return floor(x);
        if (strcmp(name, "CEIL") == 0) return ceil(x);
        if (strcmp(name, "SQRT") == 0) return sqrt(x);
        if (strcmp(name, "SIN") == 0) return sin(x);
        if (strcmp(name, "COS") == 0) return cos(x);
        if (strcmp(name, "TAN") == 0) return tan(x);
        if (strcmp(name, "ASIN") == 0) return asin(x);
        if (strcmp(name, "ACOS") == 0) return acos(x);
        if (strcmp(name, "ATAN") == 0) return atan(x);
        if (strcmp(name, "LOG") == 0 || strcmp(name, "LN") == 0) return log(x);
        if (strcmp(name, "LOG10") == 0) return log10(x);
        if (strcmp(name, "EXP") == 0) return exp(x);
        if (strcmp(name, "INT") == 0) return floor(x);
        if (strcmp(name, "SGN") == 0) return (x > 0.0) ? 1.0 : (x < 0.0) ? -1.0 : 0.0;
    }

    /* Two-argument */
    if (arg_count == 2) {
        double x = args[0], y = args[1];
        if (strcmp(name, "MIN") == 0) return fmin(x, y);
        if (strcmp(name, "MAX") == 0) return fmax(x, y);
        if (strcmp(name, "POW") == 0) return pow(x, y);
        if (strcmp(name, "ATAN2") == 0) return atan2(y, x);
        if (strcmp(name, "MOD") == 0) return fmod(x, y);
    }

    return 0.0;
}

double ast_evaluate(const ASTNode *node, VarContext *vars) {
    if (!node) return 0.0;

    switch (node->type) {
        case AST_NUMBER:
            return node->data.number.value;

        case AST_VARIABLE:
            return lookup_var(node->data.variable.name, vars);

        case AST_BINARY_OP: {
            double left = ast_evaluate(node->data.binary.left, vars);
            double right = ast_evaluate(node->data.binary.right, vars);

            switch (node->data.binary.op) {
                case OP_ADD: return left + right;
                case OP_SUBTRACT: return left - right;
                case OP_MULTIPLY: return left * right;
                case OP_DIVIDE: return right != 0.0 ? left / right : 0.0;
                case OP_POWER: return pow(left, right);
                case OP_AND: return (left != 0.0 && right != 0.0) ? 1.0 : 0.0;
                case OP_OR: return (left != 0.0 || right != 0.0) ? 1.0 : 0.0;
                case OP_GREATER: return (left > right) ? 1.0 : 0.0;
                case OP_LESS: return (left < right) ? 1.0 : 0.0;
                case OP_GREATER_EQ: return (left >= right) ? 1.0 : 0.0;
                case OP_LESS_EQ: return (left <= right) ? 1.0 : 0.0;
                case OP_EQUAL: return (fabs(left - right) < 1e-12) ? 1.0 : 0.0;
                case OP_NOT_EQUAL: return (fabs(left - right) >= 1e-12) ? 1.0 : 0.0;
            }
            break;
        }

        case AST_UNARY_OP: {
            double operand = ast_evaluate(node->data.unary.operand, vars);

            switch (node->data.unary.op) {
                case OP_NEGATE: return -operand;
                case OP_NOT: return (operand == 0.0) ? 1.0 : 0.0;
            }
            break;
        }

        case AST_FUNCTION_CALL: {
            double args[10];
            for (int i = 0; i < node->data.function.arg_count && i < 10; i++) {
                args[i] = ast_evaluate(node->data.function.args[i], vars);
            }
            return eval_function(node->data.function.name, args, node->data.function.arg_count);
        }

        case AST_TENSOR:
            /* Tensors can't be evaluated to scalar - return mean as representative value */
            return tensor_mean(node->data.tensor.tensor);
    }

    return 0.0;
}

/* ============================================================================
 * AST PRINTING
 * ============================================================================ */

static void ast_print_helper(const ASTNode *node, int indent) {
    if (!node) return;

    for (int i = 0; i < indent; i++) printf("  ");

    switch (node->type) {
        case AST_NUMBER:
            printf("NUMBER: %.2f\n", node->data.number.value);
            break;

        case AST_VARIABLE:
            printf("VARIABLE: %s\n", node->data.variable.name);
            break;

        case AST_BINARY_OP: {
            const char *op_name = "?";
            switch (node->data.binary.op) {
                case OP_ADD: op_name = "+"; break;
                case OP_SUBTRACT: op_name = "-"; break;
                case OP_MULTIPLY: op_name = "*"; break;
                case OP_DIVIDE: op_name = "/"; break;
                case OP_POWER: op_name = "^"; break;
                case OP_AND: op_name = "&&"; break;
                case OP_OR: op_name = "||"; break;
                case OP_GREATER: op_name = ">"; break;
                case OP_LESS: op_name = "<"; break;
                case OP_GREATER_EQ: op_name = ">="; break;
                case OP_LESS_EQ: op_name = "<="; break;
                case OP_EQUAL: op_name = "=="; break;
                case OP_NOT_EQUAL: op_name = "!="; break;
            }
            printf("BINARY_OP: %s\n", op_name);
            ast_print_helper(node->data.binary.left, indent + 1);
            ast_print_helper(node->data.binary.right, indent + 1);
            break;
        }

        case AST_UNARY_OP: {
            const char *op_name = (node->data.unary.op == OP_NEGATE) ? "-" : "!";
            printf("UNARY_OP: %s\n", op_name);
            ast_print_helper(node->data.unary.operand, indent + 1);
            break;
        }

        case AST_FUNCTION_CALL:
            printf("FUNCTION: %s(%d args)\n", node->data.function.name, node->data.function.arg_count);
            for (int i = 0; i < node->data.function.arg_count; i++) {
                ast_print_helper(node->data.function.args[i], indent + 1);
            }
            break;

        case AST_TENSOR:
            printf("TENSOR: shape=[");
            for (int i = 0; i < node->data.tensor.tensor->rank; i++) {
                printf("%d", node->data.tensor.tensor->shape[i]);
                if (i < node->data.tensor.tensor->rank - 1) printf(",");
            }
            printf("], size=%d\n", node->data.tensor.tensor->size);
            break;
    }
}

void ast_print(const ASTNode *node) {
    printf("AST:\n");
    ast_print_helper(node, 0);
}

static void ast_to_string_helper(const ASTNode *node, char *buffer, size_t *offset, size_t buffer_size) {
    if (!node || *offset >= buffer_size - 1) return;

    switch (node->type) {
        case AST_NUMBER:
            *offset += snprintf(buffer + *offset, buffer_size - *offset, "%.2f", node->data.number.value);
            break;

        case AST_VARIABLE:
            *offset += snprintf(buffer + *offset, buffer_size - *offset, "%s", node->data.variable.name);
            break;

        case AST_BINARY_OP: {
            const char *op = "?";
            bool needs_parens = true;

            switch (node->data.binary.op) {
                case OP_ADD: op = " + "; break;
                case OP_SUBTRACT: op = " - "; break;
                case OP_MULTIPLY: op = " * "; break;
                case OP_DIVIDE: op = " / "; break;
                case OP_POWER: op = " ^ "; break;
                case OP_AND: op = " && "; break;
                case OP_OR: op = " || "; break;
                case OP_GREATER: op = " > "; break;
                case OP_LESS: op = " < "; break;
                case OP_GREATER_EQ: op = " >= "; break;
                case OP_LESS_EQ: op = " <= "; break;
                case OP_EQUAL: op = " == "; break;
                case OP_NOT_EQUAL: op = " != "; break;
            }

            if (needs_parens) {
                *offset += snprintf(buffer + *offset, buffer_size - *offset, "(");
            }
            ast_to_string_helper(node->data.binary.left, buffer, offset, buffer_size);
            *offset += snprintf(buffer + *offset, buffer_size - *offset, "%s", op);
            ast_to_string_helper(node->data.binary.right, buffer, offset, buffer_size);
            if (needs_parens) {
                *offset += snprintf(buffer + *offset, buffer_size - *offset, ")");
            }
            break;
        }

        case AST_UNARY_OP: {
            const char *op = (node->data.unary.op == OP_NEGATE) ? "-" : "!";
            *offset += snprintf(buffer + *offset, buffer_size - *offset, "%s", op);
            ast_to_string_helper(node->data.unary.operand, buffer, offset, buffer_size);
            break;
        }

        case AST_FUNCTION_CALL:
            *offset += snprintf(buffer + *offset, buffer_size - *offset, "%s(", node->data.function.name);
            for (int i = 0; i < node->data.function.arg_count; i++) {
                if (i > 0) {
                    *offset += snprintf(buffer + *offset, buffer_size - *offset, ", ");
                }
                ast_to_string_helper(node->data.function.args[i], buffer, offset, buffer_size);
            }
            *offset += snprintf(buffer + *offset, buffer_size - *offset, ")");
            break;

        case AST_TENSOR:
            *offset += snprintf(buffer + *offset, buffer_size - *offset, "Tensor[");
            for (int i = 0; i < node->data.tensor.tensor->rank; i++) {
                *offset += snprintf(buffer + *offset, buffer_size - *offset, "%d", node->data.tensor.tensor->shape[i]);
                if (i < node->data.tensor.tensor->rank - 1) {
                    *offset += snprintf(buffer + *offset, buffer_size - *offset, "x");
                }
            }
            *offset += snprintf(buffer + *offset, buffer_size - *offset, "]");
            break;
    }
}

char* ast_to_string(const ASTNode *node) {
    char *buffer = malloc(4096);
    size_t offset = 0;
    ast_to_string_helper(node, buffer, &offset, 4096);
    return buffer;
}

/* ============================================================================
 * AST ANALYSIS
 * ============================================================================ */

bool ast_contains_variable(const ASTNode *node, const char *var_name) {
    if (!node) return false;

    switch (node->type) {
        case AST_VARIABLE:
            return strcmp(node->data.variable.name, var_name) == 0;

        case AST_BINARY_OP:
            return ast_contains_variable(node->data.binary.left, var_name) ||
                   ast_contains_variable(node->data.binary.right, var_name);

        case AST_UNARY_OP:
            return ast_contains_variable(node->data.unary.operand, var_name);

        case AST_FUNCTION_CALL:
            for (int i = 0; i < node->data.function.arg_count; i++) {
                if (ast_contains_variable(node->data.function.args[i], var_name)) {
                    return true;
                }
            }
            return false;

        default:
            return false;
    }
}

int ast_count_operations(const ASTNode *node) {
    if (!node) return 0;

    switch (node->type) {
        case AST_NUMBER:
        case AST_VARIABLE:
            return 0;

        case AST_BINARY_OP:
            return 1 + ast_count_operations(node->data.binary.left) +
                   ast_count_operations(node->data.binary.right);

        case AST_UNARY_OP:
            return 1 + ast_count_operations(node->data.unary.operand);

        case AST_FUNCTION_CALL: {
            int count = 1;  /* Function call itself */
            for (int i = 0; i < node->data.function.arg_count; i++) {
                count += ast_count_operations(node->data.function.args[i]);
            }
            return count;
        }

        case AST_TENSOR:
            /* Tensors are data, not operations */
            return 0;
    }

    return 0;
}

/* ============================================================================
 * SYMBOLIC DIFFERENTIATION
 * ============================================================================ */

ASTNode* ast_differentiate(const ASTNode *node, const char *var_name) {
    if (!node) return NULL;

    switch (node->type) {
        case AST_NUMBER:
            /* d/dx(c) = 0 */
            return ast_create_number(0.0);

        case AST_VARIABLE:
            /* d/dx(x) = 1, d/dx(y) = 0 */
            if (strcmp(node->data.variable.name, var_name) == 0) {
                return ast_create_number(1.0);
            } else {
                return ast_create_number(0.0);
            }

        case AST_BINARY_OP: {
            ASTNode *left = node->data.binary.left;
            ASTNode *right = node->data.binary.right;
            ASTNode *dleft = ast_differentiate(left, var_name);
            ASTNode *dright = ast_differentiate(right, var_name);

            switch (node->data.binary.op) {
                case OP_ADD:
                    /* d/dx(f + g) = f' + g' */
                    return ast_create_binary_op(OP_ADD, dleft, dright);

                case OP_SUBTRACT:
                    /* d/dx(f - g) = f' - g' */
                    return ast_create_binary_op(OP_SUBTRACT, dleft, dright);

                case OP_MULTIPLY:
                    /* d/dx(f * g) = f' * g + f * g' */
                    return ast_create_binary_op(OP_ADD,
                        ast_create_binary_op(OP_MULTIPLY, dleft, ast_clone(right)),
                        ast_create_binary_op(OP_MULTIPLY, ast_clone(left), dright)
                    );

                case OP_DIVIDE:
                    /* d/dx(f / g) = (f' * g - f * g') / g^2 */
                    return ast_create_binary_op(OP_DIVIDE,
                        ast_create_binary_op(OP_SUBTRACT,
                            ast_create_binary_op(OP_MULTIPLY, dleft, ast_clone(right)),
                            ast_create_binary_op(OP_MULTIPLY, ast_clone(left), dright)
                        ),
                        ast_create_binary_op(OP_POWER, ast_clone(right), ast_create_number(2.0))
                    );

                case OP_POWER:
                    /* d/dx(f^g) = f^g * (g' * ln(f) + g * f'/f) */
                    /* Simplified: d/dx(f^n) = n * f^(n-1) * f' if g is constant */
                    if (!ast_contains_variable(right, var_name)) {
                        /* Power rule: d/dx(f^n) = n * f^(n-1) * f' */
                        return ast_create_binary_op(OP_MULTIPLY,
                            ast_create_binary_op(OP_MULTIPLY,
                                ast_clone(right),
                                ast_create_binary_op(OP_POWER,
                                    ast_clone(left),
                                    ast_create_binary_op(OP_SUBTRACT, ast_clone(right), ast_create_number(1.0))
                                )
                            ),
                            dleft
                        );
                    } else {
                        /* General case - not implemented, return 0 */
                        ast_free(dleft);
                        ast_free(dright);
                        return ast_create_number(0.0);
                    }

                default:
                    /* Comparison and logical operators have derivative 0 */
                    ast_free(dleft);
                    ast_free(dright);
                    return ast_create_number(0.0);
            }
        }

        case AST_UNARY_OP: {
            ASTNode *operand = node->data.unary.operand;
            ASTNode *doperand = ast_differentiate(operand, var_name);

            switch (node->data.unary.op) {
                case OP_NEGATE:
                    /* d/dx(-f) = -f' */
                    return ast_create_unary_op(OP_NEGATE, doperand);

                default:
                    /* d/dx(!f) = 0 (logical operators) */
                    ast_free(doperand);
                    return ast_create_number(0.0);
            }
        }

        case AST_FUNCTION_CALL: {
            const char *fname = node->data.function.name;

            if (node->data.function.arg_count == 1) {
                ASTNode *arg = node->data.function.args[0];
                ASTNode *darg = ast_differentiate(arg, var_name);

                /* Chain rule: d/dx(f(g(x))) = f'(g(x)) * g'(x) */

                if (strcmp(fname, "SIN") == 0) {
                    /* d/dx(sin(f)) = cos(f) * f' */
                    ASTNode *args[] = {ast_clone(arg)};
                    return ast_create_binary_op(OP_MULTIPLY,
                        ast_create_function_call("COS", args, 1),
                        darg
                    );
                } else if (strcmp(fname, "COS") == 0) {
                    /* d/dx(cos(f)) = -sin(f) * f' */
                    ASTNode *args[] = {ast_clone(arg)};
                    return ast_create_binary_op(OP_MULTIPLY,
                        ast_create_unary_op(OP_NEGATE, ast_create_function_call("SIN", args, 1)),
                        darg
                    );
                } else if (strcmp(fname, "TAN") == 0) {
                    /* d/dx(tan(f)) = sec^2(f) * f' = (1/cos^2(f)) * f' */
                    ASTNode *args[] = {ast_clone(arg)};
                    return ast_create_binary_op(OP_MULTIPLY,
                        ast_create_binary_op(OP_DIVIDE,
                            ast_create_number(1.0),
                            ast_create_binary_op(OP_POWER,
                                ast_create_function_call("COS", args, 1),
                                ast_create_number(2.0)
                            )
                        ),
                        darg
                    );
                } else if (strcmp(fname, "LOG") == 0 || strcmp(fname, "LN") == 0) {
                    /* d/dx(ln(f)) = f'/f */
                    return ast_create_binary_op(OP_DIVIDE, darg, ast_clone(arg));
                } else if (strcmp(fname, "EXP") == 0) {
                    /* d/dx(e^f) = e^f * f' */
                    ASTNode *args[] = {ast_clone(arg)};
                    return ast_create_binary_op(OP_MULTIPLY,
                        ast_create_function_call("EXP", args, 1),
                        darg
                    );
                } else if (strcmp(fname, "SQRT") == 0) {
                    /* d/dx(sqrt(f)) = f' / (2*sqrt(f)) */
                    ASTNode *args[] = {ast_clone(arg)};
                    return ast_create_binary_op(OP_DIVIDE,
                        darg,
                        ast_create_binary_op(OP_MULTIPLY,
                            ast_create_number(2.0),
                            ast_create_function_call("SQRT", args, 1)
                        )
                    );
                } else {
                    /* Unknown function - derivative is 0 */
                    ast_free(darg);
                    return ast_create_number(0.0);
                }
            } else {
                /* Multi-argument functions - not implemented */
                return ast_create_number(0.0);
            }
        }

        case AST_TENSOR:
            /* Tensor differentiation not supported - constant w.r.t scalar variables */
            return ast_create_number(0.0);
    }

    return ast_create_number(0.0);
}

/* ============================================================================
 * PARTIAL DERIVATIVES & GRADIENT
 * ============================================================================ */

/* Compute partial derivative ∂f/∂var
 * This is just an alias for ast_differentiate for clarity in multi-variable contexts
 */
ASTNode* ast_partial_derivative(const ASTNode *node, const char *var_name) {
    return ast_differentiate(node, var_name);
}

/* Compute gradient vector ∇f = [∂f/∂x₁, ∂f/∂x₂, ..., ∂f/∂xₙ] */
Gradient ast_gradient(const ASTNode *node, const char **var_names, int var_count) {
    Gradient grad;
    grad.components = NULL;
    grad.count = 0;
    grad.var_names = NULL;

    if (!node || !var_names || var_count <= 0) {
        return grad;
    }

    /* Allocate arrays */
    grad.components = malloc(sizeof(ASTNode*) * var_count);
    grad.var_names = malloc(sizeof(char*) * var_count);
    grad.count = var_count;

    /* Compute partial derivative for each variable */
    for (int i = 0; i < var_count; i++) {
        grad.components[i] = ast_partial_derivative(node, var_names[i]);

        /* Copy variable name */
        grad.var_names[i] = malloc(strlen(var_names[i]) + 1);
        strcpy(grad.var_names[i], var_names[i]);
    }

    return grad;
}

/* Free gradient structure */
void gradient_free(Gradient *grad) {
    if (!grad) return;

    if (grad->components) {
        for (int i = 0; i < grad->count; i++) {
            ast_free(grad->components[i]);
        }
        free(grad->components);
    }

    if (grad->var_names) {
        for (int i = 0; i < grad->count; i++) {
            free(grad->var_names[i]);
        }
        free(grad->var_names);
    }

    grad->components = NULL;
    grad->var_names = NULL;
    grad->count = 0;
}

/* Evaluate gradient at a point */
double* gradient_evaluate(const Gradient *grad, VarContext *vars) {
    if (!grad || grad->count <= 0) return NULL;

    double *result = malloc(sizeof(double) * grad->count);

    for (int i = 0; i < grad->count; i++) {
        result[i] = ast_evaluate(grad->components[i], vars);
    }

    return result;
}

/* ============================================================================
 * TAYLOR SERIES EXPANSION
 * ============================================================================ */

/* Helper: Compute factorial */
static double factorial(int n) {
    if (n <= 1) return 1.0;
    double result = 1.0;
    for (int i = 2; i <= n; i++) {
        result *= i;
    }
    return result;
}

/* Expand f(x) as Taylor series around x=center up to given order
 * Returns: f(c) + f'(c)(x-c) + f''(c)(x-c)²/2! + ... + f⁽ⁿ⁾(c)(x-c)ⁿ/n!
 */
ASTNode* ast_taylor_series(
    const ASTNode *expr,
    const char *var_name,
    double center,
    int order
) {
    if (!expr || order < 0) return NULL;

    /* Build the Taylor series term by term */
    ASTNode *series = NULL;
    ASTNode *current_derivative = ast_clone(expr);

    /* Variable mapping for evaluating derivatives at center */
    VarMapping mapping = {.name = var_name, .index = 0};
    double center_val = center;
    VarContext ctx = {
        .values = &center_val,
        .count = 1,
        .mappings = &mapping,
        .mapping_count = 1
    };

    /* Create (x - center) term */
    ASTNode *x_minus_c = NULL;
    if (fabs(center) < 1e-12) {
        /* If center = 0, just use x */
        x_minus_c = ast_create_variable(var_name);
    } else {
        /* x - c */
        x_minus_c = ast_create_binary_op(OP_SUBTRACT,
            ast_create_variable(var_name),
            ast_create_number(center)
        );
    }

    for (int n = 0; n <= order; n++) {
        /* Evaluate nth derivative at center point */
        double deriv_at_center = ast_evaluate(current_derivative, &ctx);

        /* Check for NaN or inf - can happen with unsimplified expressions containing x^(-1) at x=0 */
        if (isnan(deriv_at_center) || isinf(deriv_at_center)) {
            /* Stop expansion here - higher derivatives are problematic */
            break;
        }

        /* Create term: f⁽ⁿ⁾(c) * (x-c)ⁿ / n! */
        ASTNode *term = NULL;

        if (n == 0) {
            /* Constant term: f(c) */
            term = ast_create_number(deriv_at_center);
        } else {
            /* Coefficient: f⁽ⁿ⁾(c) / n! */
            double coeff = deriv_at_center / factorial(n);

            if (fabs(coeff) > 1e-12) {  /* Skip negligible terms */
                /* (x - c)ⁿ */
                ASTNode *power_term = NULL;
                if (n == 1) {
                    power_term = ast_clone(x_minus_c);
                } else {
                    power_term = ast_create_binary_op(OP_POWER,
                        ast_clone(x_minus_c),
                        ast_create_number((double)n)
                    );
                }

                /* coeff * (x - c)ⁿ */
                if (fabs(coeff - 1.0) < 1e-12) {
                    /* Coefficient is 1, just use power term */
                    term = power_term;
                } else {
                    term = ast_create_binary_op(OP_MULTIPLY,
                        ast_create_number(coeff),
                        power_term
                    );
                }
            }
        }

        /* Add term to series */
        if (term) {
            if (series == NULL) {
                series = term;
            } else {
                series = ast_create_binary_op(OP_ADD, series, term);
            }
        }

        /* Compute next derivative for next iteration */
        if (n < order) {
            ASTNode *next_derivative = ast_differentiate(current_derivative, var_name);
            ast_free(current_derivative);
            current_derivative = next_derivative;

            /* If differentiation failed, stop here */
            if (!current_derivative) {
                break;
            }
        }
    }

    /* Clean up */
    if (current_derivative) {
        ast_free(current_derivative);
    }
    if (x_minus_c) {
        ast_free(x_minus_c);
    }

    /* If series is NULL (all terms were zero), return 0 */
    if (series == NULL) {
        return ast_create_number(0.0);
    }

    /* Return the unsimplified series
     * Note: Simplification can be very slow on complex Taylor series
     * Users can call ast_simplify() separately if needed
     */
    return series;
}

/* ============================================================================
 * OPTIMIZATION ENGINE
 * ============================================================================ */

/* Create default optimizer configuration */
OptimizerConfig optimizer_config_default(OptimizerType type) {
    OptimizerConfig config;
    config.tolerance = 1e-6;
    config.max_iterations = 1000;
    config.verbose = false;

    switch (type) {
        case OPTIMIZER_GRADIENT_DESCENT:
            config.learning_rate = 0.01;
            config.momentum = 0.0;
            config.beta1 = 0.0;
            config.beta2 = 0.0;
            config.epsilon = 0.0;
            config.restart_iterations = 0;
            break;

        case OPTIMIZER_GRADIENT_DESCENT_MOMENTUM:
            config.learning_rate = 0.01;
            config.momentum = 0.9;
            config.beta1 = 0.0;
            config.beta2 = 0.0;
            config.epsilon = 0.0;
            config.restart_iterations = 0;
            break;

        case OPTIMIZER_ADAM:
            config.learning_rate = 0.001;
            config.momentum = 0.0;
            config.beta1 = 0.9;
            config.beta2 = 0.999;
            config.epsilon = 1e-8;
            config.restart_iterations = 0;
            break;

        case OPTIMIZER_CONJUGATE_GRADIENT:
            config.learning_rate = 1.0;  /* CG doesn't use fixed learning rate */
            config.momentum = 0.0;
            config.beta1 = 0.0;
            config.beta2 = 0.0;
            config.epsilon = 0.0;
            config.restart_iterations = 0;  /* 0 = auto (var_count) */
            break;
    }

    return config;
}

/* Free optimization result */
void optimization_result_free(OptimizationResult *result) {
    if (!result) return;

    if (result->solution) {
        free(result->solution);
        result->solution = NULL;
    }

    if (result->history) {
        free(result->history);
        result->history = NULL;
    }
}

/* Backtracking line search for step size */
double line_search_backtracking(
    const ASTNode *expr,
    VarContext *ctx,
    const double *position,
    const double *direction,
    int var_count,
    double alpha_init,
    double rho,
    double c
) {
    double alpha = alpha_init;
    double *new_position = malloc(sizeof(double) * var_count);

    /* Evaluate function at current position */
    for (int i = 0; i < var_count; i++) {
        ctx->values[i] = position[i];
    }
    double f_current = ast_evaluate(expr, ctx);

    /* Try smaller step sizes until Armijo condition is satisfied */
    for (int iter = 0; iter < 20; iter++) {
        /* new_position = position + alpha * direction */
        for (int i = 0; i < var_count; i++) {
            new_position[i] = position[i] + alpha * direction[i];
            ctx->values[i] = new_position[i];
        }

        double f_new = ast_evaluate(expr, ctx);

        /* Armijo condition: f(x + α*d) <= f(x) + c*α*∇f·d */
        /* Simplified: just check if we're decreasing */
        if (f_new < f_current) {
            free(new_position);
            return alpha;
        }

        alpha *= rho;  /* Reduce step size */
    }

    free(new_position);
    return alpha;  /* Return smallest tried value */
}

/* Gradient Descent optimizer */
static OptimizationResult optimize_gradient_descent(
    const ASTNode *expr,
    const char **var_names,
    int var_count,
    const double *initial_guess,
    const OptimizerConfig *config
) {
    OptimizationResult result;
    result.solution = malloc(sizeof(double) * var_count);
    result.converged = false;
    result.iterations = 0;
    result.history = config->verbose ? malloc(sizeof(double) * config->max_iterations) : NULL;
    result.history_count = 0;
    strcpy(result.error_message, "");

    /* Initialize position */
    for (int i = 0; i < var_count; i++) {
        result.solution[i] = initial_guess[i];
    }

    /* Compute gradient */
    Gradient grad = ast_gradient(expr, var_names, var_count);

    /* Setup variable context */
    VarMapping *mappings = malloc(sizeof(VarMapping) * var_count);
    for (int i = 0; i < var_count; i++) {
        mappings[i].name = var_names[i];
        mappings[i].index = i;
    }

    VarContext ctx = {
        .values = result.solution,
        .count = var_count,
        .mappings = mappings,
        .mapping_count = var_count
    };

    /* Optimization loop */
    for (int iter = 0; iter < config->max_iterations; iter++) {
        /* Evaluate gradient at current position */
        double *grad_values = gradient_evaluate(&grad, &ctx);

        /* Compute gradient norm */
        double grad_norm = 0.0;
        for (int i = 0; i < var_count; i++) {
            grad_norm += grad_values[i] * grad_values[i];
        }
        grad_norm = sqrt(grad_norm);

        /* Check convergence */
        if (grad_norm < config->tolerance) {
            result.converged = true;
            result.iterations = iter;
            free(grad_values);
            break;
        }

        /* Update position: x = x - learning_rate * gradient */
        for (int i = 0; i < var_count; i++) {
            result.solution[i] -= config->learning_rate * grad_values[i];
        }

        free(grad_values);

        /* Store history */
        if (config->verbose) {
            result.history[result.history_count++] = ast_evaluate(expr, &ctx);
        }

        result.iterations = iter + 1;
    }

    /* Evaluate final objective value */
    result.final_value = ast_evaluate(expr, &ctx);

    /* Cleanup */
    gradient_free(&grad);
    free(mappings);

    if (!result.converged && result.iterations >= config->max_iterations) {
        snprintf(result.error_message, sizeof(result.error_message),
                 "Max iterations reached without convergence");
    }

    return result;
}

/* Gradient Descent with Momentum optimizer */
static OptimizationResult optimize_gradient_descent_momentum(
    const ASTNode *expr,
    const char **var_names,
    int var_count,
    const double *initial_guess,
    const OptimizerConfig *config
) {
    OptimizationResult result;
    result.solution = malloc(sizeof(double) * var_count);
    result.converged = false;
    result.iterations = 0;
    result.history = config->verbose ? malloc(sizeof(double) * config->max_iterations) : NULL;
    result.history_count = 0;
    strcpy(result.error_message, "");

    /* Initialize position and velocity */
    for (int i = 0; i < var_count; i++) {
        result.solution[i] = initial_guess[i];
    }

    double *velocity = calloc(var_count, sizeof(double));  /* Initialize to zero */

    /* Compute gradient */
    Gradient grad = ast_gradient(expr, var_names, var_count);

    /* Setup variable context */
    VarMapping *mappings = malloc(sizeof(VarMapping) * var_count);
    for (int i = 0; i < var_count; i++) {
        mappings[i].name = var_names[i];
        mappings[i].index = i;
    }

    VarContext ctx = {
        .values = result.solution,
        .count = var_count,
        .mappings = mappings,
        .mapping_count = var_count
    };

    /* Optimization loop */
    for (int iter = 0; iter < config->max_iterations; iter++) {
        /* Evaluate gradient at current position */
        double *grad_values = gradient_evaluate(&grad, &ctx);

        /* Compute gradient norm */
        double grad_norm = 0.0;
        for (int i = 0; i < var_count; i++) {
            grad_norm += grad_values[i] * grad_values[i];
        }
        grad_norm = sqrt(grad_norm);

        /* Check convergence */
        if (grad_norm < config->tolerance) {
            result.converged = true;
            result.iterations = iter;
            free(grad_values);
            break;
        }

        /* Update velocity: v = momentum * v + learning_rate * gradient */
        /* Update position: x = x - v */
        for (int i = 0; i < var_count; i++) {
            velocity[i] = config->momentum * velocity[i] + config->learning_rate * grad_values[i];
            result.solution[i] -= velocity[i];
        }

        free(grad_values);

        /* Store history */
        if (config->verbose) {
            result.history[result.history_count++] = ast_evaluate(expr, &ctx);
        }

        result.iterations = iter + 1;
    }

    /* Evaluate final objective value */
    result.final_value = ast_evaluate(expr, &ctx);

    /* Cleanup */
    free(velocity);
    gradient_free(&grad);
    free(mappings);

    if (!result.converged && result.iterations >= config->max_iterations) {
        snprintf(result.error_message, sizeof(result.error_message),
                 "Max iterations reached without convergence");
    }

    return result;
}

/* Adam optimizer */
static OptimizationResult optimize_adam(
    const ASTNode *expr,
    const char **var_names,
    int var_count,
    const double *initial_guess,
    const OptimizerConfig *config
) {
    OptimizationResult result;
    result.solution = malloc(sizeof(double) * var_count);
    result.converged = false;
    result.iterations = 0;
    result.history = config->verbose ? malloc(sizeof(double) * config->max_iterations) : NULL;
    result.history_count = 0;
    strcpy(result.error_message, "");

    /* Initialize position */
    for (int i = 0; i < var_count; i++) {
        result.solution[i] = initial_guess[i];
    }

    /* Initialize first and second moment vectors */
    double *m = calloc(var_count, sizeof(double));  /* First moment */
    double *v = calloc(var_count, sizeof(double));  /* Second moment */

    /* Compute gradient */
    Gradient grad = ast_gradient(expr, var_names, var_count);

    /* Setup variable context */
    VarMapping *mappings = malloc(sizeof(VarMapping) * var_count);
    for (int i = 0; i < var_count; i++) {
        mappings[i].name = var_names[i];
        mappings[i].index = i;
    }

    VarContext ctx = {
        .values = result.solution,
        .count = var_count,
        .mappings = mappings,
        .mapping_count = var_count
    };

    /* Optimization loop */
    for (int iter = 0; iter < config->max_iterations; iter++) {
        int t = iter + 1;  /* Time step (starts at 1) */

        /* Evaluate gradient at current position */
        double *grad_values = gradient_evaluate(&grad, &ctx);

        /* Compute gradient norm */
        double grad_norm = 0.0;
        for (int i = 0; i < var_count; i++) {
            grad_norm += grad_values[i] * grad_values[i];
        }
        grad_norm = sqrt(grad_norm);

        /* Check convergence */
        if (grad_norm < config->tolerance) {
            result.converged = true;
            result.iterations = iter;
            free(grad_values);
            break;
        }

        /* Update biased first moment estimate: m = beta1 * m + (1 - beta1) * g */
        /* Update biased second moment estimate: v = beta2 * v + (1 - beta2) * g^2 */
        for (int i = 0; i < var_count; i++) {
            m[i] = config->beta1 * m[i] + (1.0 - config->beta1) * grad_values[i];
            v[i] = config->beta2 * v[i] + (1.0 - config->beta2) * grad_values[i] * grad_values[i];
        }

        /* Compute bias-corrected moment estimates */
        double beta1_t = pow(config->beta1, t);
        double beta2_t = pow(config->beta2, t);

        /* Update position */
        for (int i = 0; i < var_count; i++) {
            double m_hat = m[i] / (1.0 - beta1_t);
            double v_hat = v[i] / (1.0 - beta2_t);
            result.solution[i] -= config->learning_rate * m_hat / (sqrt(v_hat) + config->epsilon);
        }

        free(grad_values);

        /* Store history */
        if (config->verbose) {
            result.history[result.history_count++] = ast_evaluate(expr, &ctx);
        }

        result.iterations = iter + 1;
    }

    /* Evaluate final objective value */
    result.final_value = ast_evaluate(expr, &ctx);

    /* Cleanup */
    free(m);
    free(v);
    gradient_free(&grad);
    free(mappings);

    if (!result.converged && result.iterations >= config->max_iterations) {
        snprintf(result.error_message, sizeof(result.error_message),
                 "Max iterations reached without convergence");
    }

    return result;
}

/* Conjugate Gradient optimizer (Polak-Ribière variant) */
static OptimizationResult optimize_conjugate_gradient(
    const ASTNode *expr,
    const char **var_names,
    int var_count,
    const double *initial_guess,
    const OptimizerConfig *config
) {
    OptimizationResult result;
    result.solution = malloc(sizeof(double) * var_count);
    result.converged = false;
    result.iterations = 0;
    result.history = config->verbose ? malloc(sizeof(double) * config->max_iterations) : NULL;
    result.history_count = 0;
    strcpy(result.error_message, "");

    /* Initialize position */
    for (int i = 0; i < var_count; i++) {
        result.solution[i] = initial_guess[i];
    }

    double *direction = malloc(sizeof(double) * var_count);
    double *grad_old = calloc(var_count, sizeof(double));

    /* Compute gradient */
    Gradient grad = ast_gradient(expr, var_names, var_count);

    /* Setup variable context */
    VarMapping *mappings = malloc(sizeof(VarMapping) * var_count);
    for (int i = 0; i < var_count; i++) {
        mappings[i].name = var_names[i];
        mappings[i].index = i;
    }

    VarContext ctx = {
        .values = result.solution,
        .count = var_count,
        .mappings = mappings,
        .mapping_count = var_count
    };

    int restart_iter = config->restart_iterations > 0 ? config->restart_iterations : var_count;

    /* Optimization loop */
    for (int iter = 0; iter < config->max_iterations; iter++) {
        /* Evaluate gradient at current position */
        double *grad_values = gradient_evaluate(&grad, &ctx);

        /* Compute gradient norm */
        double grad_norm = 0.0;
        for (int i = 0; i < var_count; i++) {
            grad_norm += grad_values[i] * grad_values[i];
        }
        grad_norm = sqrt(grad_norm);

        /* Check convergence */
        if (grad_norm < config->tolerance) {
            result.converged = true;
            result.iterations = iter;
            free(grad_values);
            break;
        }

        /* Compute conjugate direction */
        if (iter % restart_iter == 0) {
            /* Restart: use steepest descent direction */
            for (int i = 0; i < var_count; i++) {
                direction[i] = -grad_values[i];
            }
        } else {
            /* Polak-Ribière formula: beta = (g_new · (g_new - g_old)) / ||g_old||^2 */
            double numerator = 0.0;
            double denominator = 0.0;
            for (int i = 0; i < var_count; i++) {
                numerator += grad_values[i] * (grad_values[i] - grad_old[i]);
                denominator += grad_old[i] * grad_old[i];
            }
            double beta = (denominator > 1e-12) ? (numerator / denominator) : 0.0;
            beta = fmax(0.0, beta);  /* Ensure non-negative */

            /* direction = -gradient + beta * direction_old */
            for (int i = 0; i < var_count; i++) {
                direction[i] = -grad_values[i] + beta * direction[i];
            }
        }

        /* Line search for step size */
        double alpha = line_search_backtracking(expr, &ctx, result.solution, direction, var_count, 1.0, 0.5, 1e-4);

        /* Update position */
        for (int i = 0; i < var_count; i++) {
            result.solution[i] += alpha * direction[i];
        }

        /* Store old gradient */
        for (int i = 0; i < var_count; i++) {
            grad_old[i] = grad_values[i];
        }

        free(grad_values);

        /* Store history */
        if (config->verbose) {
            result.history[result.history_count++] = ast_evaluate(expr, &ctx);
        }

        result.iterations = iter + 1;
    }

    /* Evaluate final objective value */
    result.final_value = ast_evaluate(expr, &ctx);

    /* Cleanup */
    free(direction);
    free(grad_old);
    gradient_free(&grad);
    free(mappings);

    if (!result.converged && result.iterations >= config->max_iterations) {
        snprintf(result.error_message, sizeof(result.error_message),
                 "Max iterations reached without convergence");
    }

    return result;
}

/* Main optimization interface - minimize */
OptimizationResult ast_minimize(
    const ASTNode *expr,
    const char **var_names,
    int var_count,
    const double *initial_guess,
    const OptimizerConfig *config,
    OptimizerType type
) {
    if (!expr || !var_names || var_count <= 0 || !initial_guess) {
        OptimizationResult result = {0};
        result.converged = false;
        strcpy(result.error_message, "Invalid input parameters");
        return result;
    }

    /* Use default config if none provided */
    OptimizerConfig default_config;
    if (!config) {
        default_config = optimizer_config_default(type);
        config = &default_config;
    }

    /* Call appropriate optimizer */
    switch (type) {
        case OPTIMIZER_GRADIENT_DESCENT:
            return optimize_gradient_descent(expr, var_names, var_count, initial_guess, config);

        case OPTIMIZER_GRADIENT_DESCENT_MOMENTUM:
            return optimize_gradient_descent_momentum(expr, var_names, var_count, initial_guess, config);

        case OPTIMIZER_ADAM:
            return optimize_adam(expr, var_names, var_count, initial_guess, config);

        case OPTIMIZER_CONJUGATE_GRADIENT:
            return optimize_conjugate_gradient(expr, var_names, var_count, initial_guess, config);

        default: {
            OptimizationResult result = {0};
            result.converged = false;
            strcpy(result.error_message, "Unknown optimizer type");
            return result;
        }
    }
}

/* Maximize objective function (minimizes -f) */
OptimizationResult ast_maximize(
    const ASTNode *expr,
    const char **var_names,
    int var_count,
    const double *initial_guess,
    const OptimizerConfig *config,
    OptimizerType type
) {
    /* Create negated expression */
    ASTNode *neg_expr = ast_create_unary_op(OP_NEGATE, ast_clone(expr));

    /* Minimize the negated function */
    OptimizationResult result = ast_minimize(neg_expr, var_names, var_count, initial_guess, config, type);

    /* Negate the final value back */
    result.final_value = -result.final_value;

    /* Negate history if present */
    if (result.history) {
        for (int i = 0; i < result.history_count; i++) {
            result.history[i] = -result.history[i];
        }
    }

    /* Cleanup */
    ast_free(neg_expr);

    return result;
}

/* ============================================================================
 * EXPRESSION SIMPLIFICATION
 * ============================================================================ */

/* Helper: Check if two nodes are structurally identical (for term combination) */
static bool ast_nodes_equal(const ASTNode *a, const ASTNode *b) {
    if (!a || !b) return false;
    if (a->type != b->type) return false;

    switch (a->type) {
        case AST_NUMBER:
            return fabs(a->data.number.value - b->data.number.value) < 1e-12;

        case AST_VARIABLE:
            return strcmp(a->data.variable.name, b->data.variable.name) == 0;

        case AST_BINARY_OP:
            return a->data.binary.op == b->data.binary.op &&
                   ast_nodes_equal(a->data.binary.left, b->data.binary.left) &&
                   ast_nodes_equal(a->data.binary.right, b->data.binary.right);

        case AST_UNARY_OP:
            return a->data.unary.op == b->data.unary.op &&
                   ast_nodes_equal(a->data.unary.operand, b->data.unary.operand);

        case AST_FUNCTION_CALL:
            if (strcmp(a->data.function.name, b->data.function.name) != 0 ||
                a->data.function.arg_count != b->data.function.arg_count) {
                return false;
            }
            for (int i = 0; i < a->data.function.arg_count; i++) {
                if (!ast_nodes_equal(a->data.function.args[i], b->data.function.args[i])) {
                    return false;
                }
            }
            return true;

        case AST_TENSOR:
            /* Compare tensor pointers (for now) */
            return a->data.tensor.tensor == b->data.tensor.tensor;
    }

    return false;
}

/* Helper: Extract coefficient and base term from a term like "3*x" or "x" */
static void extract_coef_and_term(const ASTNode *node, double *coef, ASTNode **term) {
    *coef = 1.0;
    *term = NULL;

    if (!node) return;

    /* If node is "coef * term", extract both */
    if (node->type == AST_BINARY_OP && node->data.binary.op == OP_MULTIPLY) {
        ASTNode *left = node->data.binary.left;
        ASTNode *right = node->data.binary.right;

        if (left->type == AST_NUMBER) {
            *coef = left->data.number.value;
            *term = right;
            return;
        } else if (right->type == AST_NUMBER) {
            *coef = right->data.number.value;
            *term = left;
            return;
        }
    }

    /* Otherwise, coefficient is 1 and term is the whole node */
    *coef = 1.0;
    *term = (ASTNode*)node;
}

/* Helper: Try to combine two addition operands if they're like terms */
static ASTNode* try_combine_like_terms(ASTNode *left, ASTNode *right) {
    double coef_left, coef_right;
    ASTNode *term_left, *term_right;

    extract_coef_and_term(left, &coef_left, &term_left);
    extract_coef_and_term(right, &coef_right, &term_right);

    /* Check if the terms are equal */
    if (term_left && term_right && ast_nodes_equal(term_left, term_right)) {
        /* Combine: (a*term) + (b*term) = (a+b)*term */
        double new_coef = coef_left + coef_right;

        /* Clone the term BEFORE freeing (term_left points into left node) */
        ASTNode *cloned_term = ast_clone(term_left);

        /* Free the original nodes */
        ast_free(left);
        ast_free(right);

        if (fabs(new_coef) < 1e-12) {
            /* Coefficient is zero */
            ast_free(cloned_term);
            return ast_create_number(0.0);
        } else if (fabs(new_coef - 1.0) < 1e-12) {
            /* Coefficient is 1, just return the term */
            return cloned_term;
        } else {
            /* Return coef * term */
            return ast_create_binary_op(OP_MULTIPLY,
                ast_create_number(new_coef),
                cloned_term
            );
        }
    }

    /* Not like terms, can't combine */
    return NULL;
}

ASTNode* ast_simplify(ASTNode *node) {
    if (!node) return NULL;

    /* First, recursively simplify children */
    switch (node->type) {
        case AST_BINARY_OP:
            node->data.binary.left = ast_simplify(node->data.binary.left);
            node->data.binary.right = ast_simplify(node->data.binary.right);
            break;
        case AST_UNARY_OP:
            node->data.unary.operand = ast_simplify(node->data.unary.operand);
            break;
        case AST_FUNCTION_CALL:
            for (int i = 0; i < node->data.function.arg_count; i++) {
                node->data.function.args[i] = ast_simplify(node->data.function.args[i]);
            }
            break;
        default:
            break;
    }

    /* Now apply simplification rules */
    if (node->type == AST_BINARY_OP) {
        ASTNode *left = node->data.binary.left;
        ASTNode *right = node->data.binary.right;

        /* Constant folding */
        if (left->type == AST_NUMBER && right->type == AST_NUMBER) {
            float result = 0.0;
            switch (node->data.binary.op) {
                case OP_ADD: result = left->data.number.value + right->data.number.value; break;
                case OP_SUBTRACT: result = left->data.number.value - right->data.number.value; break;
                case OP_MULTIPLY: result = left->data.number.value * right->data.number.value; break;
                case OP_DIVIDE: result = right->data.number.value != 0.0 ?
                                        left->data.number.value / right->data.number.value : 0.0; break;
                case OP_POWER: result = powf(left->data.number.value, right->data.number.value); break;
                default: return node;
            }
            ast_free(node);
            return ast_create_number(result);
        }

        /* Algebraic identities */
        switch (node->data.binary.op) {
            case OP_ADD:
                /* x + 0 = x */
                if (right->type == AST_NUMBER && right->data.number.value == 0.0) {
                    ASTNode *result = left;
                    free(right);
                    free(node);
                    return result;
                }
                /* 0 + x = x */
                if (left->type == AST_NUMBER && left->data.number.value == 0.0) {
                    ASTNode *result = right;
                    free(left);
                    free(node);
                    return result;
                }
                /* Try to combine like terms: x + x = 2*x, 3*x + 2*x = 5*x */
                {
                    ASTNode *combined = try_combine_like_terms(left, right);
                    if (combined) {
                        free(node);
                        return combined;
                    }
                }
                break;

            case OP_SUBTRACT:
                /* x - 0 = x */
                if (right->type == AST_NUMBER && right->data.number.value == 0.0) {
                    ASTNode *result = left;
                    free(right);
                    free(node);
                    return result;
                }
                break;

            case OP_MULTIPLY:
                /* x * 0 = 0 */
                if ((right->type == AST_NUMBER && right->data.number.value == 0.0) ||
                    (left->type == AST_NUMBER && left->data.number.value == 0.0)) {
                    ast_free(node);
                    return ast_create_number(0.0);
                }
                /* x * 1 = x */
                if (right->type == AST_NUMBER && right->data.number.value == 1.0) {
                    ASTNode *result = left;
                    free(right);
                    free(node);
                    return result;
                }
                /* 1 * x = x */
                if (left->type == AST_NUMBER && left->data.number.value == 1.0) {
                    ASTNode *result = right;
                    free(left);
                    free(node);
                    return result;
                }
                break;

            case OP_DIVIDE:
                /* 0 / x = 0 */
                if (left->type == AST_NUMBER && left->data.number.value == 0.0) {
                    ast_free(node);
                    return ast_create_number(0.0);
                }
                /* x / 1 = x */
                if (right->type == AST_NUMBER && right->data.number.value == 1.0) {
                    ASTNode *result = left;
                    free(right);
                    free(node);
                    return result;
                }
                break;

            case OP_POWER:
                /* x^0 = 1 */
                if (right->type == AST_NUMBER && right->data.number.value == 0.0) {
                    ast_free(node);
                    return ast_create_number(1.0);
                }
                /* x^1 = x */
                if (right->type == AST_NUMBER && right->data.number.value == 1.0) {
                    ASTNode *result = left;
                    free(right);
                    free(node);
                    return result;
                }
                /* 0^x = 0 (for x != 0) */
                if (left->type == AST_NUMBER && left->data.number.value == 0.0) {
                    ast_free(node);
                    return ast_create_number(0.0);
                }
                /* 1^x = 1 */
                if (left->type == AST_NUMBER && left->data.number.value == 1.0) {
                    ast_free(node);
                    return ast_create_number(1.0);
                }
                break;

            default:
                break;
        }
    } else if (node->type == AST_UNARY_OP) {
        ASTNode *operand = node->data.unary.operand;

        /* Constant folding for unary */
        if (operand->type == AST_NUMBER) {
            float result = 0.0;
            switch (node->data.unary.op) {
                case OP_NEGATE: result = -operand->data.number.value; break;
                case OP_NOT: result = (operand->data.number.value == 0.0) ? 1.0 : 0.0; break;
            }
            ast_free(node);
            return ast_create_number(result);
        }

        /* Double negation: -(-x) = x */
        if (node->data.unary.op == OP_NEGATE &&
            operand->type == AST_UNARY_OP &&
            operand->data.unary.op == OP_NEGATE) {
            ASTNode *result = operand->data.unary.operand;
            free(operand);
            free(node);
            return result;
        }
    }

    return node;
}

/* ============================================================================
 * VARIABLE SUBSTITUTION
 * ============================================================================ */

ASTNode* ast_substitute(const ASTNode *node, const char *var_name, const ASTNode *replacement) {
    if (!node) return NULL;

    switch (node->type) {
        case AST_NUMBER:
            /* Numbers remain unchanged */
            return ast_create_number(node->data.number.value);

        case AST_VARIABLE:
            /* If this is the variable to substitute, return a clone of replacement */
            if (strcmp(node->data.variable.name, var_name) == 0) {
                return ast_clone(replacement);
            } else {
                /* Otherwise, keep the variable */
                return ast_create_variable(node->data.variable.name);
            }

        case AST_BINARY_OP:
            /* Recursively substitute in both operands */
            return ast_create_binary_op(
                node->data.binary.op,
                ast_substitute(node->data.binary.left, var_name, replacement),
                ast_substitute(node->data.binary.right, var_name, replacement)
            );

        case AST_UNARY_OP:
            /* Recursively substitute in operand */
            return ast_create_unary_op(
                node->data.unary.op,
                ast_substitute(node->data.unary.operand, var_name, replacement)
            );

        case AST_FUNCTION_CALL: {
            /* Recursively substitute in all arguments */
            ASTNode **new_args = malloc(sizeof(ASTNode*) * node->data.function.arg_count);
            for (int i = 0; i < node->data.function.arg_count; i++) {
                new_args[i] = ast_substitute(node->data.function.args[i], var_name, replacement);
            }
            ASTNode *result = ast_create_function_call(
                node->data.function.name,
                new_args,
                node->data.function.arg_count
            );
            free(new_args);
            return result;
        }

        case AST_TENSOR:
            /* Tensors don't contain variables, return clone */
            return ast_clone(node);
    }

    return NULL;
}

/* ============================================================================
 * POLYNOMIAL FACTORIZATION
 * ============================================================================ */

/* Helper: Check if node is a simple polynomial in var_name */
static bool is_polynomial_form(const ASTNode *node, const char *var_name,
                               double *a, double *b, double *c) {
    /* Try to match: a*x^2 + b*x + c */
    *a = 0.0;
    *b = 0.0;
    *c = 0.0;

    if (!node) return false;

    /* Evaluate at x=0 to get c */
    VarContext ctx_zero = {.values = NULL, .count = 0};
    *c = ast_evaluate(node, &ctx_zero);

    /* Evaluate at x=1 to get a+b+c */
    double one = 1.0;
    VarMapping mapping = {.name = var_name, .index = 0};
    VarContext ctx_one = {.values = &one, .count = 1, .mappings = &mapping, .mapping_count = 1};
    double val_at_one = ast_evaluate(node, &ctx_one);

    /* Evaluate at x=-1 to get a-b+c */
    double minus_one = -1.0;
    VarContext ctx_minus_one = {.values = &minus_one, .count = 1, .mappings = &mapping, .mapping_count = 1};
    double val_at_minus_one = ast_evaluate(node, &ctx_minus_one);

    /* Solve system:
     *   val_at_one = a + b + c
     *   val_at_minus_one = a - b + c
     *   *c already known
     */
    double sum = val_at_one - (*c);      /* a + b */
    double diff = val_at_minus_one - (*c); /* a - b */
    *a = (sum + diff) / 2.0;
    *b = (sum - diff) / 2.0;

    /* Verify it's actually polynomial by checking x=2 */
    double two = 2.0;
    VarContext ctx_two = {.values = &two, .count = 1, .mappings = &mapping, .mapping_count = 1};
    double val_at_two = ast_evaluate(node, &ctx_two);
    double expected = (*a) * 4.0 + (*b) * 2.0 + (*c);

    return fabs(val_at_two - expected) < 1e-9;
}

ASTNode* ast_factor(ASTNode *node, const char *var_name) {
    if (!node) return NULL;

    /* Try to match different factorization patterns */

    /* Pattern 1: Difference of squares a^2 - b^2 = (a-b)(a+b) */
    if (node->type == AST_BINARY_OP && node->data.binary.op == OP_SUBTRACT) {
        ASTNode *left = node->data.binary.left;
        ASTNode *right = node->data.binary.right;

        /* Check if left is x^2 and right is a number */
        if (left->type == AST_BINARY_OP && left->data.binary.op == OP_POWER &&
            left->data.binary.left->type == AST_VARIABLE &&
            strcmp(left->data.binary.left->data.variable.name, var_name) == 0 &&
            left->data.binary.right->type == AST_NUMBER &&
            fabs(left->data.binary.right->data.number.value - 2.0) < 1e-12 &&
            right->type == AST_NUMBER && right->data.number.value > 0) {

            /* x^2 - n^2 = (x - sqrt(n))(x + sqrt(n)) */
            double n = right->data.number.value;
            double sqrt_n = sqrt(n);

            /* Check if n is a perfect square */
            if (fabs(sqrt_n - round(sqrt_n)) < 1e-9) {
                sqrt_n = round(sqrt_n);

                return ast_create_binary_op(OP_MULTIPLY,
                    ast_create_binary_op(OP_SUBTRACT,
                        ast_create_variable(var_name),
                        ast_create_number(sqrt_n)
                    ),
                    ast_create_binary_op(OP_ADD,
                        ast_create_variable(var_name),
                        ast_create_number(sqrt_n)
                    )
                );
            }
        }
    }

    /* Pattern 2: Quadratic trinomial ax^2 + bx + c */
    double a, b, c;
    if (is_polynomial_form(node, var_name, &a, &b, &c)) {
        if (fabs(a) > 1e-9) { /* It's quadratic */
            /* Use quadratic formula to find roots */
            double discriminant = b*b - 4*a*c;

            if (discriminant >= 0) {
                double sqrt_disc = sqrt(discriminant);
                double r1 = (-b + sqrt_disc) / (2*a);
                double r2 = (-b - sqrt_disc) / (2*a);

                /* Check if roots are rational (close to integers) */
                if (fabs(r1 - round(r1)) < 1e-9 && fabs(r2 - round(r2)) < 1e-9) {
                    r1 = round(r1);
                    r2 = round(r2);

                    /* Return a*(x - r1)*(x - r2) */
                    ASTNode *factor1 = ast_create_binary_op(OP_SUBTRACT,
                        ast_create_variable(var_name),
                        ast_create_number(r1)
                    );
                    ASTNode *factor2 = ast_create_binary_op(OP_SUBTRACT,
                        ast_create_variable(var_name),
                        ast_create_number(r2)
                    );
                    ASTNode *product = ast_create_binary_op(OP_MULTIPLY, factor1, factor2);

                    if (fabs(a - 1.0) < 1e-9) {
                        /* a = 1, just return (x-r1)*(x-r2) */
                        return product;
                    } else {
                        /* Return a*(x-r1)*(x-r2) */
                        return ast_create_binary_op(OP_MULTIPLY,
                            ast_create_number(a),
                            product
                        );
                    }
                }
            }
        }
    }

    /* Pattern 3: Common factor - try GCD extraction */
    if (node->type == AST_BINARY_OP && node->data.binary.op == OP_ADD) {
        ASTNode *left = node->data.binary.left;
        ASTNode *right = node->data.binary.right;

        /* Check if both are multiples like 2*x + 4 = 2*(x + 2) */
        if (left->type == AST_BINARY_OP && left->data.binary.op == OP_MULTIPLY &&
            left->data.binary.left->type == AST_NUMBER &&
            right->type == AST_NUMBER) {

            double coef = left->data.binary.left->data.number.value;
            double constant = right->data.number.value;

            /* Check if they share a common factor */
            double gcd_val = fabs(coef);
            if (fabs(constant) > 1e-9) {
                /* Find GCD (simplified - works for small integers) */
                int a_int = (int)round(fabs(coef));
                int b_int = (int)round(fabs(constant));
                while (b_int != 0) {
                    int temp = b_int;
                    b_int = a_int % b_int;
                    a_int = temp;
                }
                gcd_val = (double)a_int;
            }

            if (gcd_val > 1.0 + 1e-9) {
                /* Factor out the GCD */
                return ast_create_binary_op(OP_MULTIPLY,
                    ast_create_number(gcd_val),
                    ast_create_binary_op(OP_ADD,
                        ast_create_binary_op(OP_MULTIPLY,
                            ast_create_number(coef / gcd_val),
                            ast_clone(left->data.binary.right)
                        ),
                        ast_create_number(constant / gcd_val)
                    )
                );
            }
        }
    }

    /* No factorization found, return clone of original */
    return ast_clone(node);
}

/* ============================================================================
 * SYMBOLIC INTEGRATION
 * ============================================================================ */

ASTNode* ast_integrate(const ASTNode *node, const char *var_name) {
    if (!node) return NULL;

    switch (node->type) {
        case AST_NUMBER:
            /* ∫c dx = c*x */
            return ast_create_binary_op(OP_MULTIPLY,
                ast_create_number(node->data.number.value),
                ast_create_variable(var_name)
            );

        case AST_VARIABLE:
            /* ∫x dx = x^2/2, ∫y dx = y*x (y is constant w.r.t. x) */
            if (strcmp(node->data.variable.name, var_name) == 0) {
                /* ∫x dx = x^2/2 */
                return ast_create_binary_op(OP_DIVIDE,
                    ast_create_binary_op(OP_POWER,
                        ast_create_variable(var_name),
                        ast_create_number(2.0)
                    ),
                    ast_create_number(2.0)
                );
            } else {
                /* ∫y dx = y*x (y is constant) */
                return ast_create_binary_op(OP_MULTIPLY,
                    ast_create_variable(node->data.variable.name),
                    ast_create_variable(var_name)
                );
            }

        case AST_BINARY_OP: {
            ASTNode *left = node->data.binary.left;
            ASTNode *right = node->data.binary.right;

            switch (node->data.binary.op) {
                case OP_ADD:
                    /* ∫(f + g) dx = ∫f dx + ∫g dx */
                    return ast_create_binary_op(OP_ADD,
                        ast_integrate(left, var_name),
                        ast_integrate(right, var_name)
                    );

                case OP_SUBTRACT:
                    /* ∫(f - g) dx = ∫f dx - ∫g dx */
                    return ast_create_binary_op(OP_SUBTRACT,
                        ast_integrate(left, var_name),
                        ast_integrate(right, var_name)
                    );

                case OP_MULTIPLY:
                    /* ∫(c*f) dx = c*∫f dx (if c is constant) */
                    if (!ast_contains_variable(left, var_name)) {
                        return ast_create_binary_op(OP_MULTIPLY,
                            ast_clone(left),
                            ast_integrate(right, var_name)
                        );
                    } else if (!ast_contains_variable(right, var_name)) {
                        return ast_create_binary_op(OP_MULTIPLY,
                            ast_clone(right),
                            ast_integrate(left, var_name)
                        );
                    } else {
                        /* Integration by parts not implemented */
                        return ast_create_number(0.0);
                    }

                case OP_POWER:
                    /* ∫x^n dx = x^(n+1)/(n+1) */
                    if (left->type == AST_VARIABLE &&
                        strcmp(left->data.variable.name, var_name) == 0 &&
                        right->type == AST_NUMBER) {

                        double n = right->data.number.value;
                        if (fabs(n + 1.0) < 1e-12) {
                            /* ∫x^(-1) dx = ln(x) */
                            ASTNode *args[] = {ast_create_variable(var_name)};
                            return ast_create_function_call("LN", args, 1);
                        } else {
                            /* ∫x^n dx = x^(n+1)/(n+1) */
                            return ast_create_binary_op(OP_DIVIDE,
                                ast_create_binary_op(OP_POWER,
                                    ast_create_variable(var_name),
                                    ast_create_number(n + 1.0)
                                ),
                                ast_create_number(n + 1.0)
                            );
                        }
                    }
                    /* General case not supported */
                    return ast_create_number(0.0);

                default:
                    /* Other operators not supported for integration */
                    return ast_create_number(0.0);
            }
        }

        case AST_UNARY_OP: {
            if (node->data.unary.op == OP_NEGATE) {
                /* ∫(-f) dx = -∫f dx */
                return ast_create_unary_op(OP_NEGATE,
                    ast_integrate(node->data.unary.operand, var_name)
                );
            }
            return ast_create_number(0.0);
        }

        case AST_FUNCTION_CALL: {
            const char *fname = node->data.function.name;

            if (node->data.function.arg_count == 1) {
                ASTNode *arg = node->data.function.args[0];

                /* Only handle simple cases where arg == var_name */
                if (arg->type == AST_VARIABLE &&
                    strcmp(arg->data.variable.name, var_name) == 0) {

                    if (strcmp(fname, "SIN") == 0) {
                        /* ∫sin(x) dx = -cos(x) */
                        ASTNode *args[] = {ast_create_variable(var_name)};
                        return ast_create_unary_op(OP_NEGATE,
                            ast_create_function_call("COS", args, 1)
                        );
                    } else if (strcmp(fname, "COS") == 0) {
                        /* ∫cos(x) dx = sin(x) */
                        ASTNode *args[] = {ast_create_variable(var_name)};
                        return ast_create_function_call("SIN", args, 1);
                    } else if (strcmp(fname, "EXP") == 0) {
                        /* ∫e^x dx = e^x */
                        ASTNode *args[] = {ast_create_variable(var_name)};
                        return ast_create_function_call("EXP", args, 1);
                    } else if (strcmp(fname, "LN") == 0 || strcmp(fname, "LOG") == 0) {
                        /* ∫ln(x) dx = x*ln(x) - x */
                        ASTNode *x = ast_create_variable(var_name);
                        ASTNode *args[] = {ast_create_variable(var_name)};
                        ASTNode *ln_x = ast_create_function_call("LN", args, 1);
                        ASTNode *x_ln_x = ast_create_binary_op(OP_MULTIPLY, x, ln_x);
                        ASTNode *x2 = ast_create_variable(var_name);
                        return ast_create_binary_op(OP_SUBTRACT, x_ln_x, x2);
                    }
                }
            }

            /* General case not supported */
            return ast_create_number(0.0);
        }

        case AST_TENSOR:
            /* Tensor integration not supported */
            return ast_create_number(0.0);
    }

    return ast_create_number(0.0);
}

/* ============================================================================
 * NUMERICAL INTEGRATION
 * ============================================================================ */

/* Trapezoidal rule for numerical integration
 * Approximates ∫[a,b] f(x)dx using trapezoids
 * More accurate with more steps
 */
double ast_integrate_numerical_trapezoidal(
    const ASTNode *expr,
    const char *var_name,
    double a,
    double b,
    int steps
) {
    if (!expr || steps <= 0) return 0.0;

    double h = (b - a) / steps;  /* Step size */
    double sum = 0.0;

    /* Variable mapping for evaluation */
    VarMapping mapping = {.name = var_name, .index = 0};
    double x_val;
    VarContext ctx = {
        .values = &x_val,
        .count = 1,
        .mappings = &mapping,
        .mapping_count = 1
    };

    /* Trapezoidal rule: (h/2) * [f(a) + 2*f(x₁) + 2*f(x₂) + ... + 2*f(xₙ₋₁) + f(b)] */

    /* First point f(a) */
    x_val = a;
    sum += ast_evaluate(expr, &ctx);

    /* Middle points: 2*f(xᵢ) for i=1 to n-1 */
    for (int i = 1; i < steps; i++) {
        x_val = a + i * h;
        sum += 2.0 * ast_evaluate(expr, &ctx);
    }

    /* Last point f(b) */
    x_val = b;
    sum += ast_evaluate(expr, &ctx);

    return (h / 2.0) * sum;
}

/* Simpson's rule for numerical integration
 * Approximates ∫[a,b] f(x)dx using parabolic segments
 * Generally more accurate than trapezoidal for same number of steps
 * Requires even number of steps
 */
double ast_integrate_numerical_simpson(
    const ASTNode *expr,
    const char *var_name,
    double a,
    double b,
    int steps
) {
    if (!expr || steps <= 0) return 0.0;

    /* Simpson's rule requires even number of intervals */
    if (steps % 2 != 0) {
        steps++;  /* Make it even */
    }

    double h = (b - a) / steps;  /* Step size */
    double sum = 0.0;

    /* Variable mapping for evaluation */
    VarMapping mapping = {.name = var_name, .index = 0};
    double x_val;
    VarContext ctx = {
        .values = &x_val,
        .count = 1,
        .mappings = &mapping,
        .mapping_count = 1
    };

    /* Simpson's rule: (h/3) * [f(a) + 4*f(x₁) + 2*f(x₂) + 4*f(x₃) + ... + f(b)] */

    /* First point f(a) */
    x_val = a;
    sum += ast_evaluate(expr, &ctx);

    /* Middle points with alternating coefficients */
    for (int i = 1; i < steps; i++) {
        x_val = a + i * h;
        double coeff = (i % 2 == 0) ? 2.0 : 4.0;  /* Even indices: 2, odd indices: 4 */
        sum += coeff * ast_evaluate(expr, &ctx);
    }

    /* Last point f(b) */
    x_val = b;
    sum += ast_evaluate(expr, &ctx);

    return (h / 3.0) * sum;
}

/* General numerical integration with method selection */
double ast_integrate_numerical(
    const ASTNode *expr,
    const char *var_name,
    double a,
    double b,
    int steps,
    IntegrationMethod method
) {
    switch (method) {
        case INTEGRATE_TRAPEZOIDAL:
            return ast_integrate_numerical_trapezoidal(expr, var_name, a, b, steps);
        case INTEGRATE_SIMPSON:
            return ast_integrate_numerical_simpson(expr, var_name, a, b, steps);
        default:
            return 0.0;
    }
}

/* ============================================================================
 * EQUATION SOLVING
 * ============================================================================ */

/* Helper: Check if expression is linear in variable */
static bool is_linear(const ASTNode *node, const char *var_name, double *a, double *b) {
    /* Try to extract expression in form: a*var + b = 0 */
    *a = 0.0;
    *b = 0.0;

    if (!node) return false;

    /* Evaluate with var=0 to get b */
    VarContext ctx_zero = {.values = NULL, .count = 0};
    double val_at_zero = ast_evaluate(node, &ctx_zero);

    /* Evaluate with var=1 to get slope */
    double one = 1.0;
    VarMapping mapping = {.name = var_name, .index = 0};
    VarContext ctx_one = {
        .values = &one,
        .count = 1,
        .mappings = &mapping,
        .mapping_count = 1
    };
    double val_at_one = ast_evaluate(node, &ctx_one);

    *b = val_at_zero;
    *a = val_at_one - val_at_zero;

    /* Check if it's actually linear by testing another point */
    double two = 2.0;
    VarContext ctx_two = {
        .values = &two,
        .count = 1,
        .mappings = &mapping,
        .mapping_count = 1
    };
    double val_at_two = ast_evaluate(node, &ctx_two);
    double expected = *a * 2.0 + *b;

    return fabs(val_at_two - expected) < 1e-10;
}

/* Helper: Check if expression is quadratic in variable */
static bool is_quadratic(const ASTNode *node, const char *var_name, double *a, double *b, double *c) {
    /* Try to extract expression in form: a*x^2 + b*x + c = 0 */
    *a = 0.0;
    *b = 0.0;
    *c = 0.0;

    if (!node) return false;

    VarMapping mapping = {.name = var_name, .index = 0};

    /* Evaluate at three points to determine coefficients */
    double vals[] = {0.0, 1.0, 2.0};
    double results[3];

    for (int i = 0; i < 3; i++) {
        VarContext ctx = {
            .values = &vals[i],
            .count = 1,
            .mappings = &mapping,
            .mapping_count = 1
        };
        results[i] = ast_evaluate(node, &ctx);
    }

    /* Solve system of equations:
     * f(0) = c
     * f(1) = a + b + c
     * f(2) = 4a + 2b + c
     */
    *c = results[0];
    *b = results[1] - results[0] - (results[2] - 2*results[1] + results[0]) / 2.0;
    *a = (results[2] - 2*results[1] + results[0]) / 2.0;

    /* Verify it's actually quadratic by testing another point */
    double test_x = 3.0;
    VarContext test_ctx = {
        .values = &test_x,
        .count = 1,
        .mappings = &mapping,
        .mapping_count = 1
    };
    double test_val = ast_evaluate(node, &test_ctx);
    double expected = (*a) * test_x * test_x + (*b) * test_x + (*c);

    return fabs(test_val - expected) < 1e-9;
}

SolveResult ast_solve_equation(ASTNode *equation, const char *var_name) {
    SolveResult result = {
        .solutions = NULL,
        .solution_count = 0,
        .has_solution = false,
        .error_message = {0}
    };

    if (!equation || !var_name) {
        snprintf(result.error_message, sizeof(result.error_message),
                 "Invalid input");
        return result;
    }

    /* Try linear equation first: ax + b = 0 */
    double a, b;
    if (is_linear(equation, var_name, &a, &b)) {
        if (fabs(a) < 1e-12) {
            if (fabs(b) < 1e-12) {
                /* 0 = 0, infinite solutions */
                snprintf(result.error_message, sizeof(result.error_message),
                         "Infinite solutions");
            } else {
                /* 0 = b (b != 0), no solution */
                snprintf(result.error_message, sizeof(result.error_message),
                         "No solution");
            }
            return result;
        }

        /* Solution: x = -b/a */
        result.solutions = malloc(sizeof(ASTNode*));
        result.solutions[0] = ast_create_number(-b / a);
        result.solution_count = 1;
        result.has_solution = true;
        return result;
    }

    /* Try quadratic equation: ax^2 + bx + c = 0 */
    double qa, qb, qc;
    if (is_quadratic(equation, var_name, &qa, &qb, &qc)) {
        if (fabs(qa) < 1e-12) {
            /* Actually linear, fall back */
            snprintf(result.error_message, sizeof(result.error_message),
                     "Equation is linear, not quadratic");
            return result;
        }

        /* Discriminant: b^2 - 4ac */
        double discriminant = qb * qb - 4.0 * qa * qc;

        if (discriminant < -1e-12) {
            /* No real solutions */
            snprintf(result.error_message, sizeof(result.error_message),
                     "No real solutions (discriminant < 0)");
            return result;
        } else if (fabs(discriminant) < 1e-12) {
            /* One solution: x = -b/(2a) */
            result.solutions = malloc(sizeof(ASTNode*));
            result.solutions[0] = ast_create_number(-qb / (2.0 * qa));
            result.solution_count = 1;
            result.has_solution = true;
            return result;
        } else {
            /* Two solutions: x = (-b ± sqrt(discriminant))/(2a) */
            double sqrt_disc = sqrt(discriminant);
            result.solutions = malloc(2 * sizeof(ASTNode*));
            result.solutions[0] = ast_create_number((-qb + sqrt_disc) / (2.0 * qa));
            result.solutions[1] = ast_create_number((-qb - sqrt_disc) / (2.0 * qa));
            result.solution_count = 2;
            result.has_solution = true;
            return result;
        }
    }

    /* Higher order or transcendental equations not supported */
    snprintf(result.error_message, sizeof(result.error_message),
             "Equation type not supported (only linear and quadratic)");
    return result;
}

void solve_result_free(SolveResult *result) {
    if (!result) return;
    if (result->solutions) {
        for (int i = 0; i < result->solution_count; i++) {
            ast_free(result->solutions[i]);
        }
        free(result->solutions);
    }
}

/* ============================================================================
 * NUMERICAL EQUATION SOLVING (Newton-Raphson)
 * ============================================================================ */

NumericalSolveResult ast_solve_numerical(ASTNode *equation, const char *var_name,
                                          double initial_guess, double tolerance, int max_iterations) {
    NumericalSolveResult result = {
        .solution = 0.0,
        .converged = false,
        .iterations = 0,
        .final_error = INFINITY,
        .error_message = {0}
    };

    if (!equation || !var_name) {
        snprintf(result.error_message, sizeof(result.error_message),
                 "Invalid input: equation or variable name is NULL");
        return result;
    }

    /* Compute derivative symbolically using our differentiation engine */
    ASTNode *derivative = ast_differentiate(equation, var_name);
    if (!derivative) {
        snprintf(result.error_message, sizeof(result.error_message),
                 "Failed to compute derivative");
        return result;
    }

    /* Simplify derivative for better performance */
    ASTNode *derivative_simplified = ast_simplify(derivative);

    /* Set up variable context for evaluation */
    VarMapping mapping = {.name = var_name, .index = 0};
    double x = initial_guess;

    /* Newton-Raphson iteration: x_{n+1} = x_n - f(x_n)/f'(x_n) */
    for (int iter = 0; iter < max_iterations; iter++) {
        result.iterations = iter + 1;

        /* Evaluate f(x) and f'(x) at current x */
        VarContext ctx = {
            .values = &x,
            .count = 1,
            .mappings = &mapping,
            .mapping_count = 1
        };

        double f_x = ast_evaluate(equation, &ctx);
        double fp_x = ast_evaluate(derivative_simplified, &ctx);

        /* Check for convergence */
        result.final_error = fabs(f_x);
        if (result.final_error < tolerance) {
            result.solution = x;
            result.converged = true;
            ast_free(derivative_simplified);
            return result;
        }

        /* Check for zero derivative (would cause division by zero) */
        if (fabs(fp_x) < 1e-15) {
            snprintf(result.error_message, sizeof(result.error_message),
                     "Derivative is zero at x=%.6f, cannot continue", x);
            result.solution = x;
            ast_free(derivative_simplified);
            return result;
        }

        /* Newton-Raphson update */
        double x_new = x - f_x / fp_x;

        /* Check for divergence (going to infinity) */
        if (!isfinite(x_new) || fabs(x_new) > 1e10) {
            snprintf(result.error_message, sizeof(result.error_message),
                     "Solution diverged (x -> infinity)");
            result.solution = x;
            ast_free(derivative_simplified);
            return result;
        }

        /* Check for oscillation or very slow convergence */
        if (iter > 10 && fabs(x_new - x) < 1e-15) {
            /* Converged to machine precision */
            result.solution = x_new;
            result.converged = true;
            result.final_error = fabs(f_x);
            ast_free(derivative_simplified);
            return result;
        }

        x = x_new;
    }

    /* Max iterations reached without convergence */
    snprintf(result.error_message, sizeof(result.error_message),
             "Max iterations (%d) reached, error=%.6e", max_iterations, result.final_error);
    result.solution = x;
    ast_free(derivative_simplified);
    return result;
}

/* ============================================================================
 * BYTECODE COMPILATION
 * ============================================================================ */

static void bytecode_add_instruction(Bytecode *bc, BytecodeInstruction inst) {
    if (bc->count >= bc->capacity) {
        bc->capacity *= 2;
        bc->instructions = realloc(bc->instructions, sizeof(BytecodeInstruction) * bc->capacity);
    }
    bc->instructions[bc->count++] = inst;
}

static void compile_node(const ASTNode *node, Bytecode *bc) {
    if (!node) return;

    switch (node->type) {
        case AST_NUMBER: {
            BytecodeInstruction inst = {.op = BC_PUSH_NUM};
            inst.data.num = node->data.number.value;
            bytecode_add_instruction(bc, inst);
            break;
        }

        case AST_VARIABLE: {
            BytecodeInstruction inst = {.op = BC_PUSH_VAR};
            /* Map variable name to index (A=0, B=1, etc.) */
            if (strlen(node->data.variable.name) == 1) {
                char c = node->data.variable.name[0];
                if (c >= 'A' && c <= 'Z') {
                    inst.data.var_index = c - 'A';
                } else {
                    inst.data.var_index = 0;
                }
            } else {
                inst.data.var_index = 0;
            }
            bytecode_add_instruction(bc, inst);
            break;
        }

        case AST_BINARY_OP: {
            /* Compile operands first (postfix order) */
            compile_node(node->data.binary.left, bc);
            compile_node(node->data.binary.right, bc);

            /* Then the operator */
            BytecodeInstruction inst;
            switch (node->data.binary.op) {
                case OP_ADD: inst.op = BC_ADD; break;
                case OP_SUBTRACT: inst.op = BC_SUBTRACT; break;
                case OP_MULTIPLY: inst.op = BC_MULTIPLY; break;
                case OP_DIVIDE: inst.op = BC_DIVIDE; break;
                case OP_POWER: inst.op = BC_POWER; break;
                case OP_AND: inst.op = BC_AND; break;
                case OP_OR: inst.op = BC_OR; break;
                case OP_GREATER: inst.op = BC_GREATER; break;
                case OP_LESS: inst.op = BC_LESS; break;
                case OP_GREATER_EQ: inst.op = BC_GREATER_EQ; break;
                case OP_LESS_EQ: inst.op = BC_LESS_EQ; break;
                case OP_EQUAL: inst.op = BC_EQUAL; break;
                case OP_NOT_EQUAL: inst.op = BC_NOT_EQUAL; break;
            }
            bytecode_add_instruction(bc, inst);
            break;
        }

        case AST_UNARY_OP: {
            compile_node(node->data.unary.operand, bc);

            BytecodeInstruction inst;
            switch (node->data.unary.op) {
                case OP_NEGATE: inst.op = BC_NEGATE; break;
                case OP_NOT: inst.op = BC_NOT; break;
            }
            bytecode_add_instruction(bc, inst);
            break;
        }

        case AST_FUNCTION_CALL: {
            /* Compile arguments */
            for (int i = 0; i < node->data.function.arg_count; i++) {
                compile_node(node->data.function.args[i], bc);
            }

            /* Function call instruction */
            BytecodeInstruction inst = {.op = BC_CALL_FUNC};
            strncpy(inst.data.func.name, node->data.function.name, 31);
            inst.data.func.name[31] = '\0';
            inst.data.func.arg_count = node->data.function.arg_count;
            bytecode_add_instruction(bc, inst);
            break;
        }

        case AST_TENSOR: {
            /* Push tensor mean as scalar value (tensors can't be bytecode compiled) */
            BytecodeInstruction inst = {.op = BC_PUSH_NUM};
            inst.data.num = tensor_mean(node->data.tensor.tensor);
            bytecode_add_instruction(bc, inst);
            break;
        }
    }
}

Bytecode* ast_compile(const ASTNode *node) {
    Bytecode *bc = malloc(sizeof(Bytecode));
    bc->capacity = 64;
    bc->count = 0;
    bc->instructions = malloc(sizeof(BytecodeInstruction) * bc->capacity);

    compile_node(node, bc);

    /* Add HALT instruction */
    BytecodeInstruction halt = {.op = BC_HALT};
    bytecode_add_instruction(bc, halt);

    return bc;
}

void bytecode_free(Bytecode *bc) {
    if (!bc) return;
    free(bc->instructions);
    free(bc);
}

void bytecode_print(const Bytecode *bc) {
    if (!bc) return;

    printf("Bytecode (%d instructions):\n", bc->count);
    for (int i = 0; i < bc->count; i++) {
        printf("  %3d: ", i);
        BytecodeInstruction inst = bc->instructions[i];

        switch (inst.op) {
            case BC_PUSH_NUM: printf("PUSH_NUM %.2f\n", inst.data.num); break;
            case BC_PUSH_VAR: printf("PUSH_VAR %d\n", inst.data.var_index); break;
            case BC_ADD: printf("ADD\n"); break;
            case BC_SUBTRACT: printf("SUBTRACT\n"); break;
            case BC_MULTIPLY: printf("MULTIPLY\n"); break;
            case BC_DIVIDE: printf("DIVIDE\n"); break;
            case BC_POWER: printf("POWER\n"); break;
            case BC_NEGATE: printf("NEGATE\n"); break;
            case BC_NOT: printf("NOT\n"); break;
            case BC_AND: printf("AND\n"); break;
            case BC_OR: printf("OR\n"); break;
            case BC_GREATER: printf("GREATER\n"); break;
            case BC_LESS: printf("LESS\n"); break;
            case BC_GREATER_EQ: printf("GREATER_EQ\n"); break;
            case BC_LESS_EQ: printf("LESS_EQ\n"); break;
            case BC_EQUAL: printf("EQUAL\n"); break;
            case BC_NOT_EQUAL: printf("NOT_EQUAL\n"); break;
            case BC_CALL_FUNC: printf("CALL_FUNC %s(%d)\n", inst.data.func.name, inst.data.func.arg_count); break;
            case BC_HALT: printf("HALT\n"); break;
        }
    }
}

/* ============================================================================
 * BYTECODE VM EXECUTION
 * ============================================================================ */

VM* vm_create(VarContext *vars) {
    VM *vm = malloc(sizeof(VM));
    vm->stack_capacity = 256;
    vm->stack_pointer = 0;
    vm->stack = malloc(sizeof(double) * vm->stack_capacity);
    vm->vars = vars;
    return vm;
}

void vm_free(VM *vm) {
    if (!vm) return;
    free(vm->stack);
    free(vm);
}

static void vm_push(VM *vm, double value) {
    if (vm->stack_pointer >= vm->stack_capacity) {
        vm->stack_capacity *= 2;
        vm->stack = realloc(vm->stack, sizeof(double) * vm->stack_capacity);
    }
    vm->stack[vm->stack_pointer++] = value;
}

static double vm_pop(VM *vm) {
    if (vm->stack_pointer <= 0) return 0.0;
    return vm->stack[--vm->stack_pointer];
}

static double vm_eval_function(const char *name, double *args, int arg_count) {
    /* Reuse the same function evaluation as AST */
    /* Zero-argument */
    if (strcmp(name, "RANDOM") == 0 || strcmp(name, "RND") == 0) {
        /* Thread-safe RNG with mutex protection */
        pthread_mutex_lock(&rng_mutex);
        if (!random_seeded) {
            srand(time(NULL));
            random_seeded = true;
        }
        double r = (double)rand() / (double)RAND_MAX;
        pthread_mutex_unlock(&rng_mutex);
        return r;
    }

    /* One-argument */
    if (arg_count == 1) {
        double x = args[0];
        if (strcmp(name, "ABS") == 0) return fabs(x);
        if (strcmp(name, "ROUND") == 0) return round(x);
        if (strcmp(name, "FLOOR") == 0) return floor(x);
        if (strcmp(name, "CEIL") == 0) return ceil(x);
        if (strcmp(name, "SQRT") == 0) return sqrt(x);
        if (strcmp(name, "SIN") == 0) return sin(x);
        if (strcmp(name, "COS") == 0) return cos(x);
        if (strcmp(name, "TAN") == 0) return tan(x);
        if (strcmp(name, "ASIN") == 0) return asin(x);
        if (strcmp(name, "ACOS") == 0) return acos(x);
        if (strcmp(name, "ATAN") == 0) return atan(x);
        if (strcmp(name, "LOG") == 0 || strcmp(name, "LN") == 0) return log(x);
        if (strcmp(name, "LOG10") == 0) return log10(x);
        if (strcmp(name, "EXP") == 0) return exp(x);
        if (strcmp(name, "INT") == 0) return floor(x);
        if (strcmp(name, "SGN") == 0) return (x > 0.0) ? 1.0 : (x < 0.0) ? -1.0 : 0.0;
    }

    /* Two-argument */
    if (arg_count == 2) {
        double x = args[0], y = args[1];
        if (strcmp(name, "MIN") == 0) return fmin(x, y);
        if (strcmp(name, "MAX") == 0) return fmax(x, y);
        if (strcmp(name, "POW") == 0) return pow(x, y);
        if (strcmp(name, "ATAN2") == 0) return atan2(y, x);
        if (strcmp(name, "MOD") == 0) return fmod(x, y);
    }

    return 0.0;
}

double vm_execute(VM *vm, const Bytecode *bc) {
    if (!vm || !bc) return 0.0;

    vm->stack_pointer = 0;  /* Reset stack */

    for (int pc = 0; pc < bc->count; pc++) {
        BytecodeInstruction inst = bc->instructions[pc];

        switch (inst.op) {
            case BC_PUSH_NUM:
                vm_push(vm, inst.data.num);
                break;

            case BC_PUSH_VAR: {
                double value = 0.0;
                if (vm->vars && inst.data.var_index < vm->vars->count) {
                    value = vm->vars->values[inst.data.var_index];
                }
                vm_push(vm, value);
                break;
            }

            case BC_ADD: {
                double b = vm_pop(vm);
                double a = vm_pop(vm);
                vm_push(vm, a + b);
                break;
            }

            case BC_SUBTRACT: {
                double b = vm_pop(vm);
                double a = vm_pop(vm);
                vm_push(vm, a - b);
                break;
            }

            case BC_MULTIPLY: {
                double b = vm_pop(vm);
                double a = vm_pop(vm);
                vm_push(vm, a * b);
                break;
            }

            case BC_DIVIDE: {
                double b = vm_pop(vm);
                double a = vm_pop(vm);
                vm_push(vm, b != 0.0 ? a / b : 0.0);
                break;
            }

            case BC_POWER: {
                double b = vm_pop(vm);
                double a = vm_pop(vm);
                vm_push(vm, pow(a, b));
                break;
            }

            case BC_NEGATE: {
                double a = vm_pop(vm);
                vm_push(vm, -a);
                break;
            }

            case BC_NOT: {
                double a = vm_pop(vm);
                vm_push(vm, (a == 0.0) ? 1.0 : 0.0);
                break;
            }

            case BC_AND: {
                double b = vm_pop(vm);
                double a = vm_pop(vm);
                vm_push(vm, (a != 0.0 && b != 0.0) ? 1.0 : 0.0);
                break;
            }

            case BC_OR: {
                double b = vm_pop(vm);
                double a = vm_pop(vm);
                vm_push(vm, (a != 0.0 || b != 0.0) ? 1.0 : 0.0);
                break;
            }

            case BC_GREATER: {
                double b = vm_pop(vm);
                double a = vm_pop(vm);
                vm_push(vm, (a > b) ? 1.0 : 0.0);
                break;
            }

            case BC_LESS: {
                double b = vm_pop(vm);
                double a = vm_pop(vm);
                vm_push(vm, (a < b) ? 1.0 : 0.0);
                break;
            }

            case BC_GREATER_EQ: {
                double b = vm_pop(vm);
                double a = vm_pop(vm);
                vm_push(vm, (a >= b) ? 1.0 : 0.0);
                break;
            }

            case BC_LESS_EQ: {
                double b = vm_pop(vm);
                double a = vm_pop(vm);
                vm_push(vm, (a <= b) ? 1.0 : 0.0);
                break;
            }

            case BC_EQUAL: {
                double b = vm_pop(vm);
                double a = vm_pop(vm);
                vm_push(vm, (fabs(a - b) < 1e-12) ? 1.0 : 0.0);
                break;
            }

            case BC_NOT_EQUAL: {
                double b = vm_pop(vm);
                double a = vm_pop(vm);
                vm_push(vm, (fabs(a - b) >= 1e-12) ? 1.0 : 0.0);
                break;
            }

            case BC_CALL_FUNC: {
                double args[10];
                int arg_count = inst.data.func.arg_count;
                /* Pop arguments in reverse order */
                for (int i = arg_count - 1; i >= 0; i--) {
                    args[i] = vm_pop(vm);
                }
                double result = vm_eval_function(inst.data.func.name, args, arg_count);
                vm_push(vm, result);
                break;
            }

            case BC_HALT:
                /* Return top of stack */
                return vm->stack_pointer > 0 ? vm->stack[vm->stack_pointer - 1] : 0.0;
        }
    }

    return vm->stack_pointer > 0 ? vm->stack[vm->stack_pointer - 1] : 0.0;
}

/* ============================================================================
 * HIGH-LEVEL API
 * ============================================================================ */

CompiledExpression* compile_expression(const char *expr) {
    (void)expr;  /* Suppress unused parameter warning */
    /* This would require integrating with the parser - placeholder for now */
    /* In a full implementation, we'd parse expr to AST, then compile to bytecode */
    return NULL;
}

void compiled_expression_free(CompiledExpression *ce) {
    if (!ce) return;
    ast_free(ce->ast);
    bytecode_free(ce->bytecode);
    free(ce->original_expr);
    free(ce);
}

double compiled_expression_evaluate(CompiledExpression *ce, VarContext *vars) {
    if (!ce || !ce->bytecode) return 0.0;

    VM *vm = vm_create(vars);
    double result = vm_execute(vm, ce->bytecode);
    vm_free(vm);

    return result;
}

/* ============================================================================
 * SYMBOLIC OPERATIONS API
 * ============================================================================ */

char* differentiate_expression(const char *expr, const char *var_name) {
    (void)expr;  /* Suppress unused parameter warning */
    (void)var_name;  /* Suppress unused parameter warning */
    /* This would require integrating with the parser - placeholder for now */
    /* In a full implementation:
     * 1. Parse expr to AST
     * 2. Differentiate AST
     * 3. Simplify result
     * 4. Convert back to string
     */
    return NULL;
}

char* simplify_expression(const char *expr) {
    (void)expr;  /* Suppress unused parameter warning */
    /* This would require integrating with the parser - placeholder for now */
    /* In a full implementation:
     * 1. Parse expr to AST
     * 2. Simplify AST
     * 3. Convert back to string
     */
    return NULL;
}

char* integrate_expression(const char *expr, const char *var_name) {
    /* This would require integrating with the parser - placeholder for now */
    /* In a full implementation:
     * 1. Parse expr to AST
     * 2. Integrate AST
     * 3. Simplify result
     * 4. Convert back to string
     */
    (void)expr;
    (void)var_name;
    return NULL;
}

char** solve_expression(const char *equation, const char *var_name, int *solution_count) {
    /* This would require integrating with the parser - placeholder for now */
    /* In a full implementation:
     * 1. Parse equation to AST
     * 2. Solve for variable
     * 3. Convert solutions to strings
     */
    (void)equation;
    (void)var_name;
    if (solution_count) *solution_count = 0;
    return NULL;
}
