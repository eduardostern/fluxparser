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

#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
#include <stdarg.h>
#include <pthread.h>
#include <sys/time.h>

#define STRINGIFY_HELPER(x) #x
#define STRINGIFY(x) STRINGIFY_HELPER(x)

/* ========== DEBUG MODE & CALLBACKS ========== */

/* Global debug state */
static int debug_level = DEBUG_OFF;
static FILE *debug_output = NULL;  /* NULL means stderr */
static int debug_indent = 0;

/* Global callback state */
static ParserErrorCallback error_callback = NULL;
static void *error_callback_userdata = NULL;
static ParserDebugCallback debug_callback = NULL;
static void *debug_callback_userdata = NULL;

/* Debug helper: printf-style output with indentation */
static void debug_print(int level, const char *format, ...) {
    if (!(debug_level & level)) return;

    char buffer[1024];
    va_list args;

    /* Build indented message */
    int offset = 0;
    for (int i = 0; i < debug_indent && offset < (int)sizeof(buffer) - 1; i++) {
        buffer[offset++] = ' ';
        buffer[offset++] = ' ';
    }

    /* Format the message */
    va_start(args, format);
    vsnprintf(buffer + offset, sizeof(buffer) - offset, format, args);
    va_end(args);

    /* Call callback if set, otherwise print to file/stderr */
    if (debug_callback) {
        debug_callback(level, buffer, debug_callback_userdata);
    } else {
        FILE *out = debug_output ? debug_output : stderr;
        fprintf(out, "%s", buffer);
        fflush(out);
    }
}

/* Debug API implementation */
void parser_set_debug_level(int level) {
    debug_level = level;
    if (level != DEBUG_OFF) {
        debug_print(DEBUG_ALL, "═══ DEBUG MODE ENABLED (level=0x%02X) ═══\n", level);
    }
}

int parser_get_debug_level(void) {
    return debug_level;
}

void parser_set_debug_output(FILE *fp) {
    debug_output = fp;
}

void parser_reset_debug_output(void) {
    debug_output = NULL;
}

/* Callback API implementation */
void parser_set_error_callback(ParserErrorCallback callback, void *user_data) {
    error_callback = callback;
    error_callback_userdata = user_data;
}

void parser_set_debug_callback(ParserDebugCallback callback, void *user_data) {
    debug_callback = callback;
    debug_callback_userdata = user_data;
}

void parser_clear_error_callback(void) {
    error_callback = NULL;
    error_callback_userdata = NULL;
}

void parser_clear_debug_callback(void) {
    debug_callback = NULL;
    debug_callback_userdata = NULL;
}

/* ========== END DEBUG MODE & CALLBACKS ========== */

/* Thread-safe RNG with mutex protection (shared with ast.c) */
pthread_mutex_t rng_mutex = PTHREAD_MUTEX_INITIALIZER;
bool random_seeded = false;

/* Token types */
typedef enum {
    TOK_NUMBER,
    TOK_PLUS,
    TOK_MINUS,
    TOK_MULTIPLY,
    TOK_DIVIDE,
    TOK_POWER,
    TOK_NOT,
    TOK_AND,
    TOK_OR,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_COMMA,
    TOK_FUNCTION,
    TOK_GREATER,
    TOK_LESS,
    TOK_GREATER_EQ,
    TOK_LESS_EQ,
    TOK_EQUAL,
    TOK_NOT_EQUAL,
    TOK_END,
    TOK_ERROR
} TokenType;

/* Token structure */
typedef struct {
    TokenType type;
    double value;
    char func_name[32];
} Token;

/* Parser state */
typedef struct {
    const char *input;
    size_t pos;
    size_t input_length;
    Token current_token;
    int depth;
    int max_depth_reached;
    VarContext *vars;  /* Optional variable context */
    ParserErrorInfo *error;  /* Error tracking */
    bool has_error;
    struct timeval start_time;  /* For timeout */
    long timeout_us;  /* Timeout in microseconds (0 = no timeout) */
    int error_count;  /* For error recovery */
    bool continue_on_error;  /* Error recovery mode */
} Parser;

/* Forward declarations */
static void set_error(Parser *p, ParserError code, const char *message);
static double parse_or_expr(Parser *p);
static double parse_and_expr(Parser *p);
static double parse_comparison_expr(Parser *p);
static double parse_additive_expr(Parser *p);
static double parse_multiplicative_expr(Parser *p);
static double parse_power_expr(Parser *p);
static double parse_unary_expr(Parser *p);
static double parse_primary_expr(Parser *p);

/* Token type to string (for debug output) */
static const char* token_type_string(TokenType type) {
    switch (type) {
        case TOK_NUMBER: return "NUMBER";
        case TOK_PLUS: return "PLUS";
        case TOK_MINUS: return "MINUS";
        case TOK_MULTIPLY: return "MULTIPLY";
        case TOK_DIVIDE: return "DIVIDE";
        case TOK_POWER: return "POWER";
        case TOK_NOT: return "NOT";
        case TOK_AND: return "AND";
        case TOK_OR: return "OR";
        case TOK_LPAREN: return "LPAREN";
        case TOK_RPAREN: return "RPAREN";
        case TOK_COMMA: return "COMMA";
        case TOK_FUNCTION: return "FUNCTION";
        case TOK_GREATER: return "GREATER";
        case TOK_LESS: return "LESS";
        case TOK_GREATER_EQ: return "GREATER_EQ";
        case TOK_LESS_EQ: return "LESS_EQ";
        case TOK_EQUAL: return "EQUAL";
        case TOK_NOT_EQUAL: return "NOT_EQUAL";
        case TOK_END: return "END";
        case TOK_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

/* Timeout checking */
static bool check_timeout(Parser *p) {
    if (p->timeout_us == 0) return true;  /* No timeout */

    struct timeval now;
    gettimeofday(&now, NULL);

    long elapsed_us = (now.tv_sec - p->start_time.tv_sec) * 1000000L +
                     (now.tv_usec - p->start_time.tv_usec);

    if (elapsed_us > p->timeout_us) {
        set_error(p, PARSER_ERROR_SYNTAX, "Parsing timeout exceeded");
        return false;
    }
    return true;
}

/* Error reporting helpers */
static void set_error(Parser *p, ParserError code, const char *message) {
    if (!p->error) return;

    /* For error recovery: count errors, continue if enabled */
    p->error_count++;

    if (!p->continue_on_error && p->has_error) {
        return;  /* Don't overwrite first error */
    }

    p->has_error = true;
    p->error->code = code;
    p->error->position = (int)p->pos;
    snprintf(p->error->message, sizeof(p->error->message), "%s", message);

    /* Call error callback if registered */
    if (error_callback) {
        error_callback(p->error, p->input, error_callback_userdata);
    }
}

static void set_error_fmt(Parser *p, ParserError code, const char *fmt, ...) {
    if (!p->error) return;

    p->error_count++;

    if (!p->continue_on_error && p->has_error) {
        return;
    }

    p->has_error = true;
    p->error->code = code;
    p->error->position = (int)p->pos;

    va_list args;
    va_start(args, fmt);
    vsnprintf(p->error->message, sizeof(p->error->message), fmt, args);
    va_end(args);

    /* Call error callback if registered */
    if (error_callback) {
        error_callback(p->error, p->input, error_callback_userdata);
    }
}

static bool check_depth(Parser *p) {
    if (p->depth > PARSER_MAX_DEPTH) {
        set_error(p, PARSER_ERROR_TOO_DEEP, "Expression too deeply nested (max depth: "
                  STRINGIFY(PARSER_MAX_DEPTH) ")");
        return false;
    }
    if (p->depth > p->max_depth_reached) {
        p->max_depth_reached = p->depth;
    }
    return true;
}

/* Look up a variable by name */
static bool lookup_variable(Parser *p, const char *name, double *value) {
    if (!p->vars || !p->vars->values) {
        return false;
    }

    /* If custom mappings provided, use them */
    if (p->vars->mappings && p->vars->mapping_count > 0) {
        for (int i = 0; i < p->vars->mapping_count; i++) {
            if (strcmp(name, p->vars->mappings[i].name) == 0) {
                int idx = p->vars->mappings[i].index;
                if (idx >= 0 && idx < p->vars->count) {
                    *value = p->vars->values[idx];
                    debug_print(DEBUG_VARS, "[VAR] %s = %.6g (index %d)\n", name, *value, idx);
                    return true;
                }
                set_error_fmt(p, PARSER_ERROR_UNKNOWN_VAR,
                             "Variable '%s' index out of range", name);
                return false;
            }
        }
        /* Not found in mappings - might be a constant or function, return false without error */
        return false;
    }

    /* Default: single-letter variables a-z */
    if (strlen(name) == 1) {
        char c = name[0];
        if (c >= 'A' && c <= 'Z') {
            int idx = c - 'A';
            if (idx < p->vars->count) {
                *value = p->vars->values[idx];
                debug_print(DEBUG_VARS, "[VAR] %s = %.6g (index %d)\n", name, *value, idx);
                return true;
            }
        }
    }

    return false;
}

/* Skip whitespace */
static void skip_whitespace(Parser *p) {
    while (p->input[p->pos] && isspace(p->input[p->pos])) {
        p->pos++;
    }
}

/* Get next token */
static void next_token(Parser *p) {
    skip_whitespace(p);

    if (!p->input[p->pos]) {
        p->current_token.type = TOK_END;
        return;
    }

    char c = p->input[p->pos];

    /* Check for numbers */
    if (isdigit(c) || c == '.') {
        char *endptr;
        p->current_token.type = TOK_NUMBER;
        p->current_token.value = strtod(&p->input[p->pos], &endptr);
        p->pos = endptr - p->input;
        return;
    }

    /* Check for function names and constants (identifiers) */
    if (isalpha(c)) {
        size_t start = p->pos;
        while (isalnum(p->input[p->pos])) {
            p->pos++;
        }
        size_t len = p->pos - start;
        if (len >= sizeof(p->current_token.func_name)) {
            len = sizeof(p->current_token.func_name) - 1;
        }
        strncpy(p->current_token.func_name, &p->input[start], len);
        p->current_token.func_name[len] = '\0';

        /* Convert to uppercase for case-insensitive matching */
        for (size_t i = 0; i < len; i++) {
            p->current_token.func_name[i] = toupper(p->current_token.func_name[i]);
        }

        /* Check for variables first */
        double var_value;
        if (lookup_variable(p, p->current_token.func_name, &var_value)) {
            p->current_token.type = TOK_NUMBER;
            p->current_token.value = var_value;
            return;
        }

        /* Check for mathematical constants */
        if (strcmp(p->current_token.func_name, "PI") == 0) {
            p->current_token.type = TOK_NUMBER;
            p->current_token.value = 3.14159265358979323846;
            return;
        }
        if (strcmp(p->current_token.func_name, "E") == 0) {
            p->current_token.type = TOK_NUMBER;
            p->current_token.value = 2.71828182845904523536;
            return;
        }

        p->current_token.type = TOK_FUNCTION;
        return;
    }

    /* Check for operators */
    p->pos++;
    switch (c) {
        case '+':
            p->current_token.type = TOK_PLUS;            break;
        case '-':
            p->current_token.type = TOK_MINUS;            break;
        case '*':
            p->current_token.type = TOK_MULTIPLY;            break;
        case '/':
            p->current_token.type = TOK_DIVIDE;            break;
        case '^':
            p->current_token.type = TOK_POWER;            break;
        case '!':
            /* Check for != */
            if (p->input[p->pos] == '=') {
                p->pos++;
                p->current_token.type = TOK_NOT_EQUAL;            } else {
                p->current_token.type = TOK_NOT;            }
            break;
        case '(':
            p->current_token.type = TOK_LPAREN;            break;
        case ')':
            p->current_token.type = TOK_RPAREN;            break;
        case ',':
            p->current_token.type = TOK_COMMA;            break;
        case '&':
            if (p->input[p->pos] == '&') {
                p->pos++;
                p->current_token.type = TOK_AND;            } else {
                p->current_token.type = TOK_ERROR;
                fprintf(stderr, "Error: Expected '&&' but got '&'\n");
            }
            break;
        case '|':
            if (p->input[p->pos] == '|') {
                p->pos++;
                p->current_token.type = TOK_OR;            } else {
                p->current_token.type = TOK_ERROR;
                fprintf(stderr, "Error: Expected '||' but got '|'\n");
            }
            break;
        case '>':
            /* Check for >= */
            if (p->input[p->pos] == '=') {
                p->pos++;
                p->current_token.type = TOK_GREATER_EQ;            } else {
                p->current_token.type = TOK_GREATER;            }
            break;
        case '<':
            /* Check for <= */
            if (p->input[p->pos] == '=') {
                p->pos++;
                p->current_token.type = TOK_LESS_EQ;            } else {
                p->current_token.type = TOK_LESS;            }
            break;
        case '=':
            /* Check for == */
            if (p->input[p->pos] == '=') {
                p->pos++;
                p->current_token.type = TOK_EQUAL;            } else {
                p->current_token.type = TOK_ERROR;
                fprintf(stderr, "Error: Expected '==' but got '='\n");
            }
            break;
        default:
            p->current_token.type = TOK_ERROR;
            fprintf(stderr, "Error: Unexpected character '%c'\n", c);
            break;
    }

    /* Debug: show tokenization */
    if (debug_level & DEBUG_TOKENS) {
        if (p->current_token.type == TOK_NUMBER) {
            debug_print(DEBUG_TOKENS, "[TOKEN] %s = %.6g\n",
                       token_type_string(p->current_token.type),
                       p->current_token.value);
        } else if (p->current_token.type == TOK_FUNCTION) {
            debug_print(DEBUG_TOKENS, "[TOKEN] %s '%s'\n",
                       token_type_string(p->current_token.type),
                       p->current_token.func_name);
        } else {
            debug_print(DEBUG_TOKENS, "[TOKEN] %s\n",
                       token_type_string(p->current_token.type));
        }
    }
}

/* Evaluate a math function call */
static double eval_function(const char *name, double *args, int arg_count, Parser *p) {
    (void)p;  /* Unused parameter - kept for API consistency */

    /* Zero-argument functions */
    if (strcmp(name, "RANDOM") == 0 || strcmp(name, "RND") == 0) {
        if (arg_count != 0) {
            fprintf(stderr, "Error: %s expects 0 arguments, got %d\n", name, arg_count);
            return 0.0;
        }
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

    /* One-argument functions */
    if (arg_count == 1) {
        double x = args[0];

        if (strcmp(name, "ABS") == 0) return fabs(x);
        if (strcmp(name, "ROUND") == 0) return round(x);
        if (strcmp(name, "FLOOR") == 0) return floor(x);
        if (strcmp(name, "CEIL") == 0) return ceil(x);
        if (strcmp(name, "SQRT") == 0) {
            if (x < 0.0) {
                fprintf(stderr, "Error: SQRT of negative number\n");
                return 0.0;
            }
            return sqrt(x);
        }
        if (strcmp(name, "SIN") == 0) return sin(x);
        if (strcmp(name, "COS") == 0) return cos(x);
        if (strcmp(name, "TAN") == 0) return tan(x);
        if (strcmp(name, "ASIN") == 0) return asin(x);
        if (strcmp(name, "ACOS") == 0) return acos(x);
        if (strcmp(name, "ATAN") == 0) return atan(x);
        if (strcmp(name, "LOG") == 0 || strcmp(name, "LN") == 0) {
            if (x <= 0.0) {
                fprintf(stderr, "Error: LOG of non-positive number\n");
                return 0.0;
            }
            return log(x);
        }
        if (strcmp(name, "LOG10") == 0) {
            if (x <= 0.0) {
                fprintf(stderr, "Error: LOG10 of non-positive number\n");
                return 0.0;
            }
            return log10(x);
        }
        if (strcmp(name, "EXP") == 0) return exp(x);
        if (strcmp(name, "INT") == 0) return floor(x);  /* BASIC-style INT */
        if (strcmp(name, "SGN") == 0) return (x > 0.0) ? 1.0 : (x < 0.0) ? -1.0 : 0.0;
    }

    /* Two-argument functions */
    if (arg_count == 2) {
        double x = args[0];
        double y = args[1];

        if (strcmp(name, "MIN") == 0) return fmin(x, y);
        if (strcmp(name, "MAX") == 0) return fmax(x, y);
        if (strcmp(name, "POW") == 0) return pow(x, y);
        if (strcmp(name, "ATAN2") == 0) return atan2(y, x);
        if (strcmp(name, "MOD") == 0) return fmod(x, y);
    }

    fprintf(stderr, "Error: Unknown function '%s' or wrong number of arguments\n", name);
    return 0.0;
}

/* Parse primary expression: number, function call, or (expression) */
static double parse_primary_expr(Parser *p) {
    p->depth++;
    if (!check_depth(p)) {
        p->depth--;
        return 0.0;
    }

    double result = 0.0;

    if (p->has_error) {
        p->depth--;
        return 0.0;
    }

    if (p->current_token.type == TOK_NUMBER) {
        result = p->current_token.value;
        next_token(p);
    } else if (p->current_token.type == TOK_FUNCTION) {
        /* Function call */
        char func_name[32];
        strncpy(func_name, p->current_token.func_name, sizeof(func_name) - 1);
        func_name[sizeof(func_name) - 1] = '\0';

        next_token(p);

        /* Expect opening parenthesis */
        if (p->current_token.type != TOK_LPAREN) {
            fprintf(stderr, "Error: Expected '(' after function name\n");
            p->depth--;
            return 0.0;
        }
        next_token(p);

        /* Parse arguments */
        double args[10];  /* Max 10 arguments */
        int arg_count = 0;

        /* Handle zero-argument functions */
        if (p->current_token.type == TOK_RPAREN) {
            next_token(p);
            result = eval_function(func_name, args, 0, p);
        } else {
            /* Parse comma-separated argument list */
            while (1) {
                if (arg_count >= 10) {
                    fprintf(stderr, "Error: Too many function arguments (max 10)\n");
                    break;
                }

                args[arg_count++] = parse_or_expr(p);

                if (p->current_token.type == TOK_COMMA) {
                    next_token(p);
                } else if (p->current_token.type == TOK_RPAREN) {
                    next_token(p);
                    break;
                } else {
                    fprintf(stderr, "Error: Expected ',' or ')' in function call\n");
                    break;
                }
            }

            result = eval_function(func_name, args, arg_count, p);
        }

    } else if (p->current_token.type == TOK_LPAREN) {
        next_token(p);
        result = parse_or_expr(p);
        if (p->current_token.type == TOK_RPAREN) {
            next_token(p);
        } else {
            fprintf(stderr, "Error: Expected closing parenthesis\n");
        }
    } else {
        fprintf(stderr, "Error: Expected number, function, or '('\n");
    }

    p->depth--;
    return result;
}

/* Parse unary expression: !expr or primary */
static double parse_unary_expr(Parser *p) {
    p->depth++;

    double result;

    if (p->current_token.type == TOK_NOT) {
        next_token(p);
        result = !parse_unary_expr(p);
    } else if (p->current_token.type == TOK_MINUS) {
        next_token(p);
        result = -parse_unary_expr(p);
    } else {
        result = parse_primary_expr(p);
    }

    p->depth--;
    return result;
}

/* Parse power expression: base ^ exponent (right associative) */
static double parse_power_expr(Parser *p) {
    p->depth++;

    double result = parse_unary_expr(p);

    if (p->current_token.type == TOK_POWER) {
        next_token(p);
        double exponent = parse_power_expr(p);  /* Right associative */
        result = pow(result, exponent);
    }

    p->depth--;
    return result;
}

/* Parse multiplicative expression: * and / */
static double parse_multiplicative_expr(Parser *p) {
    p->depth++;

    double result = parse_power_expr(p);

    while (p->current_token.type == TOK_MULTIPLY ||
           p->current_token.type == TOK_DIVIDE) {
        TokenType op = p->current_token.type;
        if (op == TOK_MULTIPLY) {
        } else {
        }
        next_token(p);
        double right = parse_power_expr(p);

        if (op == TOK_MULTIPLY) {
            result *= right;
        } else {
            if (right == 0.0) {
                fprintf(stderr, "Error: Division by zero\n");
            } else {
                result /= right;
            }
        }

    }

    p->depth--;
    return result;
}

/* Parse additive expression: + and - */
static double parse_additive_expr(Parser *p) {
    p->depth++;

    double result = parse_multiplicative_expr(p);

    while (p->current_token.type == TOK_PLUS ||
           p->current_token.type == TOK_MINUS) {
        TokenType op = p->current_token.type;
        if (op == TOK_PLUS) {
        } else {
        }
        next_token(p);
        double right = parse_multiplicative_expr(p);

        if (op == TOK_PLUS) {
            result += right;
        } else {
            result -= right;
        }

    }

    p->depth--;
    return result;
}

/* Parse comparison expression: <, >, <=, >=, ==, != */
static double parse_comparison_expr(Parser *p) {
    p->depth++;
    if (!check_depth(p) || !check_timeout(p)) {
        p->depth--;
        return 0.0;
    }

    double result = parse_additive_expr(p);

    /* Comparisons are non-associative - only one per expression */
    if (p->current_token.type == TOK_GREATER ||
        p->current_token.type == TOK_LESS ||
        p->current_token.type == TOK_GREATER_EQ ||
        p->current_token.type == TOK_LESS_EQ ||
        p->current_token.type == TOK_EQUAL ||
        p->current_token.type == TOK_NOT_EQUAL) {

        TokenType op = p->current_token.type;
        next_token(p);
        double right = parse_additive_expr(p);

        switch (op) {
            case TOK_GREATER:
                result = (result > right) ? 1.0 : 0.0;
                break;
            case TOK_LESS:
                result = (result < right) ? 1.0 : 0.0;
                break;
            case TOK_GREATER_EQ:
                result = (result >= right) ? 1.0 : 0.0;
                break;
            case TOK_LESS_EQ:
                result = (result <= right) ? 1.0 : 0.0;
                break;
            case TOK_EQUAL:
                /* Floating point equality with small epsilon */
                result = (fabs(result - right) < 1e-12) ? 1.0 : 0.0;
                break;
            case TOK_NOT_EQUAL:
                result = (fabs(result - right) >= 1e-12) ? 1.0 : 0.0;
                break;
            default:
                break;
        }

    }

    p->depth--;
    return result;
}

/* Parse AND expression: && */
static double parse_and_expr(Parser *p) {
    p->depth++;
    if (!check_timeout(p)) {
        p->depth--;
        return 0.0;
    }

    double result = parse_comparison_expr(p);

    while (p->current_token.type == TOK_AND) {
        next_token(p);
        double right = parse_comparison_expr(p);
        result = (result != 0.0) && (right != 0.0) ? 1.0 : 0.0;
    }

    p->depth--;
    return result;
}

/* Parse OR expression: || */
static double parse_or_expr(Parser *p) {
    p->depth++;

    double result = parse_and_expr(p);

    while (p->current_token.type == TOK_OR) {
        next_token(p);
        double right = parse_and_expr(p);
        result = (result != 0.0) || (right != 0.0) ? 1.0 : 0.0;
    }

    p->depth--;
    return result;
}

/* Public API */
double parse_expression(const char *expr) {
    if (!expr || !*expr) {
        fprintf(stderr, "Error: Empty expression\n");
        return 0.0;
    }

    Parser parser = {
        .input = expr,
        .pos = 0,
        .input_length = strlen(expr),
        .depth = 0,
        .max_depth_reached = 0,
        .vars = NULL,
        .error = NULL,
        .has_error = false
    };


    next_token(&parser);
    double result = parse_or_expr(&parser);

    if (parser.current_token.type != TOK_END) {
        fprintf(stderr, "Error: Unexpected tokens at end of expression\n");
    }


    return result;
}

double parse_expression_with_vars(const char *expr, VarContext *vars) {
    if (!expr || !*expr) {
        fprintf(stderr, "Error: Empty expression\n");
        return 0.0;
    }

    Parser parser = {
        .input = expr,
        .pos = 0,
        .input_length = strlen(expr),
        .depth = 0,
        .max_depth_reached = 0,
        .vars = vars,
        .error = NULL,
        .has_error = false
    };


    next_token(&parser);
    double result = parse_or_expr(&parser);

    if (parser.current_token.type != TOK_END) {
        fprintf(stderr, "Error: Unexpected tokens at end of expression\n");
    }


    return result;
}

/* NEW SAFE API IMPLEMENTATION */

ParseResult parse_expression_safe(const char *expr) {
    return parse_expression_with_vars_safe(expr, NULL);
}

ParseResult parse_expression_with_vars_safe(const char *expr, VarContext *vars) {
    ParseResult result = {
        .value = 0.0,
        .error = {
            .code = PARSER_OK,
            .position = 0,
            .message = {0}
        },
        .has_error = false
    };

    /* Input validation */
    if (!expr) {
        result.has_error = true;
        result.error.code = PARSER_ERROR_EMPTY_EXPR;
        snprintf(result.error.message, sizeof(result.error.message),
                 "Expression is NULL");
        return result;
    }

    size_t len = strlen(expr);
    if (len == 0) {
        result.has_error = true;
        result.error.code = PARSER_ERROR_EMPTY_EXPR;
        snprintf(result.error.message, sizeof(result.error.message),
                 "Expression is empty");
        return result;
    }

    if (len > PARSER_MAX_EXPR_LENGTH) {
        result.has_error = true;
        result.error.code = PARSER_ERROR_TOO_LONG;
        snprintf(result.error.message, sizeof(result.error.message),
                 "Expression too long (%zu chars, max %d)",
                 len, PARSER_MAX_EXPR_LENGTH);
        return result;
    }

    /* Initialize parser with error tracking */
    Parser parser = {
        .input = expr,
        .pos = 0,
        .input_length = len,
        .depth = 0,
        .max_depth_reached = 0,
        .vars = vars,
        .error = &result.error,
        .has_error = false
    };


    /* Parse expression */
    next_token(&parser);
    result.value = parse_or_expr(&parser);

    /* Check for trailing tokens */
    if (!parser.has_error && parser.current_token.type != TOK_END) {
        set_error(&parser, PARSER_ERROR_UNEXPECTED_TOKEN,
                  "Unexpected tokens at end of expression");
    }

    result.has_error = parser.has_error;


    return result;
}

ParseResult parse_expression_ex(const char *expr, VarContext *vars, ParserConfig *config) {
    ParseResult result = {
        .value = 0.0,
        .error = {
            .code = PARSER_OK,
            .position = 0,
            .message = {0}
        },
        .has_error = false
    };

    /* Input validation */
    if (!expr) {
        result.has_error = true;
        result.error.code = PARSER_ERROR_EMPTY_EXPR;
        snprintf(result.error.message, sizeof(result.error.message),
                 "Expression is NULL");
        return result;
    }

    size_t len = strlen(expr);
    if (len == 0) {
        result.has_error = true;
        result.error.code = PARSER_ERROR_EMPTY_EXPR;
        snprintf(result.error.message, sizeof(result.error.message),
                 "Expression is empty");
        return result;
    }

    if (len > PARSER_MAX_EXPR_LENGTH) {
        result.has_error = true;
        result.error.code = PARSER_ERROR_TOO_LONG;
        snprintf(result.error.message, sizeof(result.error.message),
                 "Expression too long (%zu chars, max %d)",
                 len, PARSER_MAX_EXPR_LENGTH);
        return result;
    }

    /* Initialize parser with full configuration */
    Parser parser = {
        .input = expr,
        .pos = 0,
        .input_length = len,
        .depth = 0,
        .max_depth_reached = 0,
        .vars = vars,
        .error = &result.error,
        .has_error = false,
        .timeout_us = config ? config->timeout_ms * 1000 : 0,
        .error_count = 0,
        .continue_on_error = config ? config->continue_on_error : false
    };

    /* Start timeout timer */
    if (parser.timeout_us > 0) {
        gettimeofday(&parser.start_time, NULL);
    }

    /* Parse expression */
    next_token(&parser);
    result.value = parse_or_expr(&parser);

    /* Check for trailing tokens */
    if (!parser.has_error && parser.current_token.type != TOK_END) {
        set_error(&parser, PARSER_ERROR_UNEXPECTED_TOKEN,
                  "Unexpected tokens at end of expression");
    }

    result.has_error = parser.has_error;

    return result;
}

/* UTILITY FUNCTIONS */

const char* parser_error_string(ParserError error) {
    switch (error) {
        case PARSER_OK: return "No error";
        case PARSER_ERROR_EMPTY_EXPR: return "Empty expression";
        case PARSER_ERROR_TOO_LONG: return "Expression too long";
        case PARSER_ERROR_TOO_DEEP: return "Expression too deeply nested";
        case PARSER_ERROR_SYNTAX: return "Syntax error";
        case PARSER_ERROR_UNKNOWN_FUNC: return "Unknown function";
        case PARSER_ERROR_WRONG_ARGS: return "Wrong number of arguments";
        case PARSER_ERROR_DIVISION_BY_ZERO: return "Division by zero";
        case PARSER_ERROR_DOMAIN: return "Math domain error";
        case PARSER_ERROR_UNEXPECTED_TOKEN: return "Unexpected token";
        case PARSER_ERROR_UNMATCHED_PAREN: return "Unmatched parenthesis";
        case PARSER_ERROR_UNKNOWN_VAR: return "Unknown variable";
        default: return "Unknown error";
    }
}

void parser_print_error(const char *expr, const ParseResult *result) {
    if (!result->has_error) {
        return;
    }

    fprintf(stderr, "\nParse error: %s\n", result->error.message);
    fprintf(stderr, "Position: %d\n", result->error.position);

    if (expr && result->error.position >= 0) {
        fprintf(stderr, "\n%s\n", expr);
        for (int i = 0; i < result->error.position && expr[i]; i++) {
            fprintf(stderr, " ");
        }
        fprintf(stderr, "^\n");
    }
}
