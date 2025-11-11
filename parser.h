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

#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>
#include <stdio.h>

/* Parser limits */
#define PARSER_MAX_EXPR_LENGTH 10000
#define PARSER_MAX_DEPTH 100
#define PARSER_MAX_FUNC_ARGS 10

/* Error codes */
typedef enum {
    PARSER_OK = 0,
    PARSER_ERROR_EMPTY_EXPR,
    PARSER_ERROR_TOO_LONG,
    PARSER_ERROR_TOO_DEEP,
    PARSER_ERROR_SYNTAX,
    PARSER_ERROR_UNKNOWN_FUNC,
    PARSER_ERROR_WRONG_ARGS,
    PARSER_ERROR_DIVISION_BY_ZERO,
    PARSER_ERROR_DOMAIN,  /* Math domain error (sqrt(-1), log(0), etc.) */
    PARSER_ERROR_UNEXPECTED_TOKEN,
    PARSER_ERROR_UNMATCHED_PAREN,
    PARSER_ERROR_UNKNOWN_VAR
} ParserError;

/* Error information */
typedef struct {
    ParserError code;
    int position;           /* Character position where error occurred */
    char message[256];      /* Human-readable error message */
} ParserErrorInfo;

/* Parse result */
typedef struct {
    double value;
    ParserErrorInfo error;
    bool has_error;
} ParseResult;

/* Variable name mapping */
typedef struct {
    const char *name;
    int index;
} VarMapping;

/* Variable context for expression evaluation */
typedef struct {
    double *values;          /* Array of variable values */
    int count;               /* Number of variables */
    VarMapping *mappings;    /* Optional name-to-index mappings (NULL for default a-z) */
    int mapping_count;       /* Number of custom mappings */
} VarContext;

/* Parser configuration */
typedef struct {
    long timeout_ms;         /* Timeout in milliseconds (0 = no timeout) */
    bool continue_on_error;  /* Continue parsing after errors to collect all errors */
    bool thread_safe;        /* Reserved for future use */
} ParserConfig;

/* NEW SAFE API - Returns detailed error information */

/* Parse and evaluate an expression string (safe version)
 * Returns ParseResult with value and error information
 */
ParseResult parse_expression_safe(const char *expr);

/* Parse and evaluate with variables (safe version)
 * Returns ParseResult with value and error information
 */
ParseResult parse_expression_with_vars_safe(const char *expr, VarContext *vars);

/* Parse with full configuration (timeout, error recovery, etc.)
 * Returns ParseResult with value and error information
 */
ParseResult parse_expression_ex(const char *expr, VarContext *vars, ParserConfig *config);

/* LEGACY API - For backward compatibility (less safe) */

/* Parse and evaluate an expression string
 * Returns the result as a double
 * Returns 0.0 on error (check stderr for error messages)
 * DEPRECATED: Use parse_expression_safe() for better error handling
 */
double parse_expression(const char *expr);

/* Parse and evaluate an expression with variables
 * vars: pointer to VarContext with variable values and mappings
 * If mappings is NULL, uses default single-letter variables (a-z)
 * Returns the result as a double
 * Returns 0.0 on error (check stderr for error messages)
 * DEPRECATED: Use parse_expression_with_vars_safe() for better error handling
 */
double parse_expression_with_vars(const char *expr, VarContext *vars);

/* DEBUG MODE & CALLBACKS */

/* Debug levels (can be OR'ed together) */
typedef enum {
    DEBUG_OFF       = 0,      /* No debug output */
    DEBUG_TOKENS    = 1 << 0, /* Show tokenization (1) */
    DEBUG_AST       = 1 << 1, /* Show AST structure (2) */
    DEBUG_EVAL      = 1 << 2, /* Show evaluation steps (4) */
    DEBUG_VARS      = 1 << 3, /* Show variable lookups (8) */
    DEBUG_FUNCS     = 1 << 4, /* Show function calls (16) */
    DEBUG_OPTIMIZE  = 1 << 5, /* Show optimization steps (32) */
    DEBUG_TIMING    = 1 << 6, /* Show timing information (64) */
    DEBUG_ALL       = 0xFF    /* Enable all debug output (255) */
} DebugLevel;

/* Callback types for error and debug handling */

/* Error callback: called when an error occurs
 * Parameters:
 *   - error: Error information structure
 *   - expr: The expression being parsed
 *   - user_data: User-provided context pointer
 * Return: true to continue parsing (if possible), false to abort
 */
typedef bool (*ParserErrorCallback)(const ParserErrorInfo *error, const char *expr, void *user_data);

/* Debug callback: called for debug events
 * Parameters:
 *   - level: Debug level of this message (DEBUG_TOKENS, DEBUG_EVAL, etc.)
 *   - message: Debug message string
 *   - user_data: User-provided context pointer
 */
typedef void (*ParserDebugCallback)(int level, const char *message, void *user_data);

/* Set debug level (can OR multiple flags: DEBUG_TOKENS | DEBUG_AST) */
void parser_set_debug_level(int level);

/* Get current debug level */
int parser_get_debug_level(void);

/* Set debug output file (default: stderr) */
void parser_set_debug_output(FILE *fp);

/* Reset debug output to stderr */
void parser_reset_debug_output(void);

/* Set error callback (called when errors occur) */
void parser_set_error_callback(ParserErrorCallback callback, void *user_data);

/* Set debug callback (called for debug messages) */
void parser_set_debug_callback(ParserDebugCallback callback, void *user_data);

/* Clear error callback (revert to default stderr output) */
void parser_clear_error_callback(void);

/* Clear debug callback (revert to default stderr/file output) */
void parser_clear_debug_callback(void);

/* UTILITY FUNCTIONS */

/* Get string description of error code */
const char* parser_error_string(ParserError error);

/* Print formatted error message with position indicator */
void parser_print_error(const char *expr, const ParseResult *result);

#endif /* PARSER_H */
