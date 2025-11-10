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
    AST_FUNCTION_CALL
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
