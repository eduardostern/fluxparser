# Variable Support in FluxParser

The parser now supports passing variables to expressions via an array of floats!

## Quick Start

### Method 1: Default Single-Letter Variables (a-z)

```c
#include "parser.h"

float values[] = {5.0, 10.0, 3.0};  // a=5, b=10, c=3

VarContext ctx = {
    .values = values,
    .count = 3,
    .mappings = NULL,     // NULL = use default a-z mapping
    .mapping_count = 0
};

float result = parse_expression_with_vars("a + b * c", &ctx);
// Result: 35.0
```

**Default mapping:**
- Index 0 → `A` or `a`
- Index 1 → `B` or `b`
- Index 2 → `C` or `c`
- ... up to index 25 → `Z` or `z`

### Method 2: Custom Variable Names

```c
#include "parser.h"

float values[] = {100.0, 50.0, 25.0};

VarMapping mappings[] = {
    {"WIDTH", 0},
    {"HEIGHT", 1},
    {"DEPTH", 2}
};

VarContext ctx = {
    .values = values,
    .count = 3,
    .mappings = mappings,
    .mapping_count = 3
};

float volume = parse_expression_with_vars("WIDTH * HEIGHT * DEPTH", &ctx);
// Result: 125000.0
```

## API Reference

### Data Structures

```c
typedef struct {
    const char *name;    // Variable name (uppercase)
    int index;           // Index in values array
} VarMapping;

typedef struct {
    float *values;       // Array of variable values
    int count;           // Number of values
    VarMapping *mappings; // Custom name mappings (or NULL for a-z)
    int mapping_count;   // Number of mappings
} VarContext;
```

### Functions

```c
// Parse expression with variables
float parse_expression_with_vars(const char *expr, VarContext *vars);

// Original function (no variables)
float parse_expression(const char *expr);
```

## Features

- **Case-insensitive:** Variable names are converted to uppercase
- **Flexible naming:** Use default a-z or define custom names
- **Type-safe:** All variables are floats
- **Zero overhead when not used:** Original `parse_expression()` unchanged

## Limitations

- Variable names must be alphanumeric (no underscores or special characters)
- All variables are floats
- Maximum 26 variables with default a-z mapping (unlimited with custom mappings)
- Variables cannot have the same name as functions or constants (PI, E, SIN, etc.)

## Examples

### Physics Simulation
```c
float values[] = {10.0, 9.81, 2.0};  // v0, g, t

VarMapping mappings[] = {
    {"V0", 0},  // initial velocity
    {"G", 1},   // gravity
    {"T", 2}    // time
};

VarContext ctx = {
    .values = values,
    .count = 3,
    .mappings = mappings,
    .mapping_count = 3
};

// Distance: d = v0*t - 0.5*g*t^2
float distance = parse_expression_with_vars("V0*T - 0.5*G*T^2", &ctx);
```

### Geometry Calculations
```c
float coords[] = {3.0, 4.0};  // x, y

VarContext ctx = {
    .values = coords,
    .count = 2,
    .mappings = NULL,  // Use default: a=3, b=4
    .mapping_count = 0
};

// Distance from origin
float dist = parse_expression_with_vars("sqrt(a^2 + b^2)", &ctx);
// Result: 5.0
```

### Batch Processing
```c
const char *formula = "a^2 + b^2";
VarContext ctx = {
    .count = 2,
    .mappings = NULL,
    .mapping_count = 0
};

float test_cases[][2] = {{3,4}, {5,12}, {8,15}};

for (int i = 0; i < 3; i++) {
    ctx.values = test_cases[i];
    float result = parse_expression_with_vars(formula, &ctx);
    printf("%.0f^2 + %.0f^2 = %.0f\n",
           test_cases[i][0], test_cases[i][1], result);
}
```

Output:
```
3^2 + 4^2 = 25
5^2 + 12^2 = 169
8^2 + 15^2 = 289
```

## Command-Line Usage

The `test_vars` program demonstrates command-line usage:

```bash
# Syntax: ./test_vars <expression> <value1> <value2> ...
# Values map to a, b, c, d, ...

./test_vars "a + b" 5 10
# Result: 15.00

./test_vars "sqrt(a^2 + b^2)" 3 4
# Result: 5.00

./test_vars "a * sin(b) + c" 10 1.57 5
# Result: 15.00
```

## Demo Programs

- **test_vars.c** - Comprehensive test suite with 5 examples
- **example_usage.c** - Practical usage examples
- **test.c** - Original interactive calculator (no variables)

Build with:
```bash
make all
```

Run tests:
```bash
./test_vars              # Run all tests
./example_usage          # Run usage examples
./parser_test            # Interactive mode
```
