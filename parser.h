#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>

/* Parser limits */
#define PARSER_MAX_EXPR_LENGTH 10000
#define PARSER_MAX_DEPTH 100
#define PARSER_MAX_FUNC_ARGS 10

/* Enable/disable debug output */
extern bool debug_mode;

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
    bool thread_safe;        /* Use thread-local debug mode */
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

/* UTILITY FUNCTIONS */

/* Set debug mode to show parser steps (thread-safe, global) */
void set_debug_mode(bool enable);

/* Set debug mode for current thread only (thread-local) */
void set_debug_mode_local(bool enable);

/* Get string description of error code */
const char* parser_error_string(ParserError error);

/* Print formatted error message with position indicator */
void parser_print_error(const char *expr, const ParseResult *result);

#endif /* PARSER_H */
