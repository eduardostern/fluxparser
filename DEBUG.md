# FluxParser Debug Mode & Callbacks

## Overview

FluxParser includes a powerful debug system with customizable callbacks for production-ready error handling and monitoring.

## Debug Levels

Debug levels can be combined using bitwise OR (`|`):

```c
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
```

## Basic Usage

### Enable Debug Output

```c
#include "parser.h"

// Enable token debugging
parser_set_debug_level(DEBUG_TOKENS);

ParseResult result = parse_expression_safe("2 + 3 * 4");
// Output: [TOKEN] NUMBER = 2
//         [TOKEN] PLUS
//         [TOKEN] NUMBER = 3
//         ...

// Disable debugging
parser_set_debug_level(DEBUG_OFF);
```

### Combine Multiple Flags

```c
// Show both tokens and variable lookups
parser_set_debug_level(DEBUG_TOKENS | DEBUG_VARS);

VarContext ctx = ...;
ParseResult result = parse_expression_with_vars_safe("x + y", &ctx);
// Output: [TOKEN] FUNCTION 'X'
//         [VAR] X = 10 (index 0)
//         [TOKEN] PLUS
//         ...
```

### Debug Everything

```c
// Enable all debug output
parser_set_debug_level(DEBUG_ALL);

ParseResult result = parse_expression_safe("sqrt(sin(pi/4))");
// Shows: tokens, AST, evaluation, function calls, etc.
```

## Advanced: Custom Callbacks

### Error Callback

Perfect for production monitoring, logging, or GUI error display:

```c
bool my_error_handler(const ParserErrorInfo *error, const char *expr, void *user_data) {
    // Log to file, database, or monitoring system
    fprintf(stderr, "Parser Error: %s at position %d in '%s'\n",
            error->message, error->position, expr);

    // Return true to continue parsing (if possible), false to abort
    return false;
}

// Register callback
parser_set_error_callback(my_error_handler, NULL);

// All errors will now go through your callback
ParseResult result = parse_expression_safe("2 + + 3");
// Your callback gets called with error details

// Clear callback
parser_clear_error_callback();
```

### Debug Callback

Route debug output to files, structured logging, or custom formatters:

```c
void my_debug_handler(int level, const char *message, void *user_data) {
    FILE *log = (FILE*)user_data;
    const char *level_name = "UNKNOWN";

    if (level & DEBUG_TOKENS) level_name = "TOKENS";
    else if (level & DEBUG_VARS) level_name = "VARS";
    // ... handle other levels

    fprintf(log, "[%-8s] %s", level_name, message);
}

// Register callback with user data
FILE *logfile = fopen("parser.log", "w");
parser_set_debug_callback(my_debug_handler, logfile);
parser_set_debug_level(DEBUG_TOKENS | DEBUG_VARS);

// All debug output goes to your callback
ParseResult result = parse_expression_safe("2 + 3");

fclose(logfile);
parser_clear_debug_callback();
```

## Callback API Reference

### Error Callbacks

```c
// Error callback signature
typedef bool (*ParserErrorCallback)(
    const ParserErrorInfo *error,  // Error details
    const char *expr,               // Original expression
    void *user_data                 // Your custom data
);

// Register/clear
void parser_set_error_callback(ParserErrorCallback callback, void *user_data);
void parser_clear_error_callback(void);
```

**Error callback receives:**
- `error->code`: Error type (PARSER_ERROR_SYNTAX, etc.)
- `error->position`: Character position where error occurred
- `error->message`: Human-readable error message
- `expr`: The full expression being parsed
- `user_data`: Your custom context pointer

**Return value:**
- `true`: Try to continue parsing (error recovery mode)
- `false`: Abort parsing immediately

### Debug Callbacks

```c
// Debug callback signature
typedef void (*ParserDebugCallback)(
    int level,              // Debug level of this message
    const char *message,    // Debug message string
    void *user_data        // Your custom data
);

// Register/clear
void parser_set_debug_callback(ParserDebugCallback callback, void *user_data);
void parser_clear_debug_callback(void);
```

**Debug callback receives:**
- `level`: Which debug flag triggered this (DEBUG_TOKENS, DEBUG_VARS, etc.)
- `message`: Pre-formatted debug message
- `user_data`: Your custom context pointer

## Use Cases

### 1. Development & Debugging

```c
// See everything during development
parser_set_debug_level(DEBUG_ALL);
```

### 2. Production Error Monitoring

```c
// Custom error handler for logging/monitoring
bool production_error_handler(const ParserErrorInfo *error, const char *expr, void *user_data) {
    // Log to monitoring system
    log_to_datadog("parser_error", error->message);

    // Send alert if critical
    if (error->code == PARSER_ERROR_SYNTAX) {
        send_alert("Invalid expression from user", expr);
    }

    return false;
}

parser_set_error_callback(production_error_handler, NULL);
parser_set_debug_level(DEBUG_OFF);  // No debug overhead
```

### 3. GUI Application

```c
// Show friendly errors in GUI
bool gui_error_handler(const ParserErrorInfo *error, const char *expr, void *user_data) {
    GtkWidget *dialog = (GtkWidget*)user_data;

    char *friendly_msg;
    switch (error->code) {
        case PARSER_ERROR_SYNTAX:
            friendly_msg = "Invalid syntax - please check your expression";
            break;
        case PARSER_ERROR_UNKNOWN_FUNC:
            friendly_msg = "Unknown function - use sin, cos, sqrt, etc.";
            break;
        default:
            friendly_msg = error->message;
    }

    gtk_label_set_text(GTK_LABEL(dialog), friendly_msg);
    return false;
}

parser_set_error_callback(gui_error_handler, error_label);
```

### 4. Unit Testing

```c
// Collect all errors during tests
typedef struct {
    int count;
    ParserErrorInfo errors[100];
} ErrorCollector;

bool test_error_collector(const ParserErrorInfo *error, const char *expr, void *user_data) {
    ErrorCollector *collector = (ErrorCollector*)user_data;
    collector->errors[collector->count++] = *error;
    return true;  // Continue to collect all errors
}

// Test
ErrorCollector collector = {0};
parser_set_error_callback(test_error_collector, &collector);

parse_expression_safe("invalid1");
parse_expression_safe("invalid2");

assert(collector.count == 2);
```

### 5. Performance Analysis

```c
// Log timing information
parser_set_debug_level(DEBUG_TIMING);
parser_set_debug_output(fopen("timing.log", "w"));

for (int i = 0; i < 1000; i++) {
    parse_expression_safe("complex_expression");
}

// Analyze timing.log for bottlenecks
```

## Debug Output Examples

### DEBUG_TOKENS
```
[TOKEN] NUMBER = 2
[TOKEN] PLUS
[TOKEN] NUMBER = 3
[TOKEN] MULTIPLY
[TOKEN] NUMBER = 4
[TOKEN] END
```

### DEBUG_VARS
```
[VAR] X = 10 (index 0)
[VAR] Y = 20 (index 1)
```

### DEBUG_TOKENS | DEBUG_VARS
```
[TOKEN] FUNCTION 'X'
[VAR] X = 10 (index 0)
[TOKEN] PLUS
[TOKEN] FUNCTION 'Y'
[VAR] Y = 20 (index 1)
```

## Performance Considerations

- **Debug Output:** Minimal overhead when `DEBUG_OFF`
- **Callbacks:** Zero overhead when not registered
- **Production:** Use error callbacks with `DEBUG_OFF` for monitoring without performance impact

## Complete Example

See `demo_debug.c` for a comprehensive example demonstrating:
- All debug levels
- Error callbacks for custom handling
- Debug callbacks for logging
- Production monitoring setup
- GUI integration patterns

## API Summary

```c
// Debug control
void parser_set_debug_level(int level);
int parser_get_debug_level(void);
void parser_set_debug_output(FILE *fp);
void parser_reset_debug_output(void);

// Callbacks
void parser_set_error_callback(ParserErrorCallback callback, void *user_data);
void parser_set_debug_callback(ParserDebugCallback callback, void *user_data);
void parser_clear_error_callback(void);
void parser_clear_debug_callback(void);
```

## Thread Safety

⚠️ **Note:** Debug level and callbacks are global. In multithreaded applications, set them once during initialization before spawning threads.

## See Also

- `parser.h` - Full API documentation
- `demo_debug.c` - Working examples
- `README.md` - General parser documentation
