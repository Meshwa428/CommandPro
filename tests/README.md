# Synapse Test System

A production-grade test system for the Synapse language implementation.

## Quick Start

```bash
# Run all tests
python3 scripts/test_runner.py --vm

# Run with benchmarks
python3 scripts/test_runner.py --benchmark -n 5 --vm

# Full verification (compile + test + benchmark + stability)
./scripts/verify.sh
```

## Directory Structure

```
tests/
├── unit/            # Unit tests
├── integration/     # Integration tests
├── conformance/     # Language conformance tests
│   ├── lexer/      # Lexer tests
│   ├── parser/     # Parser tests
│   ├── semantics/  # Semantic tests
│   ├── errors/     # Error handling tests
│   ├── vm/         # VM-specific tests
│   └── api/        # API tests
├── functional/      # Functional feature tests
│   ├── core/       # Core language features
│   ├── control/    # Control flow
│   ├── functions/  # Function tests
│   ├── automation/ # Automation API tests
│   ├── error/      # Error tests
│   └── vm/         # VM tests
├── benchmarks/      # Performance benchmarks
│   └── *.syn       # Synapse benchmarks
│   └── *.py        # Python baselines for comparison
├── property/       # Property-based tests
└── fixtures/       # Test fixtures
```

## Test Specification Format

### Method 1: Legacy @EXPECT Comments (Backward Compatible)

Place at the top of your `.syn` test file:

```syn
# @EXPECT EXIT 0
# @EXPECT STDOUT "Hello World"
# @EXPECT STDERR "Warning: something"
# @EXPECT SKIP_VM        # Skip when running with --vm flag
# @EXPECT SKIP_INTERP    # Skip when running with interpreter

println "Hello World";
```

**Supported Tags:**
| Tag | Description | Example |
|-----|-------------|---------|
| `@EXPECT EXIT <code>` | Expected exit code | `@EXPECT EXIT 0` |
| `@EXPECT STDOUT "<text>"` | Expected stdout substring | `@EXPECT STDOUT "result: 42"` |
| `@EXPECT STDERR "<text>"` | Expected stderr substring | `@EXPECT STDERR "[Error]"` |
| `@EXPECT SKIP_VM` | Skip VM execution | `@EXPECT SKIP_VM` |
| `@EXPECT SKIP_INTERP` | Skip interpreter execution | `@EXPECT SKIP_INTERP` |
| `@EXPECT INPUT "<value>"` | Stdin input (for automated tests) | `@EXPECT INPUT "42"` |

### Method 2: YAML Spec (Recommended for Complex Tests)

Create a `.yaml` file alongside your test:

```yaml
# tests/functional/my_test.yaml
exit: 0
stdout:
  - "result: 42"
  - "completed"
stderr: []
skip_vm: false
skip_interp: false
timeout: 30
iterations: 1
tags: [regression, critical]
description: "Tests basic arithmetic operations"
```

**YAML Schema:**
| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `exit` | int | 0 | Expected exit code |
| `stdout` | list | [] | Expected stdout substrings |
| `stderr` | list | [] | Expected stderr substrings |
| `skip_vm` | bool | false | Skip when using VM |
| `skip_interp` | bool | false | Skip when using interpreter |
| `timeout` | int | 30 | Timeout in seconds |
| `iterations` | int | 1 | Run count for stability |
| `baseline` | str | - | Python baseline filename (without .py) |
| `tags` | list | [] | Tags for filtering |
| `description` | str | "" | Human-readable description |

## Benchmark System

Benchmarks are `.syn` files in `tests/benchmarks/` that have a corresponding `.py` file for Python baseline comparison.

**Auto-detection:** If `stress_1_arithmetic.syn` exists, the runner automatically looks for `stress_1_arithmetic.py`.

**Benchmark Format:**
```syn
# Benchmark: Arithmetic stress test
let i = 0;
let sum = 0;
loop while (i < 100000) {
    sum = sum + i;
    i = i + 1;
}
```

**Python Baseline Format:**
```python
# Python baseline for stress_1_arithmetic
i = 0
sum = 0
while i < 100000:
    sum = sum + i
    i = i + 1
```

**Output:**
```
Name                                |    Synapse (ms) |     Python (ms) |    Speedup
------------------------------------------------------------------------------------------
stress_1_arithmetic                 |          8.2235 |         24.8743 |      3.02x
```

Speedup > 1.0 = Synapse is faster than Python.

## Test Runner Usage

```bash
# Basic
python3 scripts/test_runner.py                    # Run all tests (default VM)
python3 scripts/test_runner.py --vm               # Use bytecode VM
python3 scripts/test_runner.py --interp           # Use tree-walking interpreter

# Filtering
python3 scripts/test_runner.py -c conformance     # Run specific category
python3 scripts/test_runner.py -c functional      # Run functional tests
python3 scripts/test_runner.py -c BENCHMARK       # Run benchmarks only

# Benchmarking
python3 scripts/test_runner.py --benchmark        # Run with benchmarks
python3 scripts/test_runner.py --benchmark -n 10  # 10 iterations per benchmark

# Advanced
python3 scripts/test_runner.py --stability -n 100 # Run stability test (100 iterations)
python3 scripts/test_runner.py --fuzz -n 1000    # Run fuzz tests (1000 iterations)
python3 scripts/test_runner.py --junit results.xml  # Generate JUnit XML report

# Verbose
python3 scripts/test_runner.py -v                 # Show all test details
python3 scripts/test_runner.py -v -c conformance # Verbose + specific category
```

## Configuration

Edit `scripts/test_config.json` to customize defaults:

```json
{
    "synapse_binary": "./build/synapse",
    "python_executable": "python3",
    "test_directories": ["tests/unit", "tests/integration", ...],
    "defaults": {
        "vm": true,
        "iterations": 5,
        "timeout": 30,
        "verbose": false
    },
    "benchmark": {
        "min_iterations": 3,
        "warmup_runs": 2
    }
}
```

## CI Integration

Generate JUnit XML for CI systems:

```bash
python3 scripts/test_runner.py --junit test-results.xml
```

This produces `test-results.xml` compatible with Jenkins, GitLab CI, GitHub Actions, etc.

## Test Categories

| Category | Purpose | Discovery Location |
|----------|---------|---------------------|
| `UNIT` | Individual component tests | `tests/unit/` |
| `INTEGRATION` | Component interaction tests | `tests/integration/` |
| `CONFORMANCE` | Language specification compliance | `tests/conformance/` |
| `FUNCTIONAL` | Feature-level tests | `tests/functional/` |
| `BENCHMARK` | Performance tests | `tests/benchmarks/` |
| `PROPERTY` | Property-based tests | `tests/property/` |

## Status Indicators

| Symbol | Status | Meaning |
|--------|--------|---------|
| ✓ | PASSED | Test executed and matched expectations |
| ✗ | FAILED | Test executed but expectations didn't match |
| ⊘ | SKIPPED | Test skipped (e.g., `skip_vm` flag) |
| ⚠ | ERROR | Test crashed or encountered exception |

## Troubleshooting

**Test fails with encoding error:**
- Ensure test files are UTF-8 encoded
- The runner handles invalid bytes gracefully with replacement characters

**Benchmark says "SKIPPED":**
- Check if the `.syn` file has `skip_vm: true` in YAML or `# @EXPECT SKIP_VM`
- Verify the corresponding `.py` baseline exists

**"Binary not found" error:**
- Run `./scripts/verify.sh` which compiles before testing
- Or manually: `cd build && cmake .. && make`

## Example: Adding a New Test

1. Create the test file:
   ```syn
   # tests/functional/core/my_new_feature.syn
   # @EXPECT EXIT 0
   # @EXPECT STDOUT "Success"

   let result = 42;
   println "Result: " + result;
   if (result == 42) {
       println "Success";
   }
   ```

2. Run it:
   ```bash
   python3 scripts/test_runner.py -c functional -v
   ```

3. (Optional) Create a YAML spec for complex cases:
   ```yaml
   # tests/functional/core/my_new_feature.yaml
   exit: 0
   stdout: ["Success"]
   tags: [new, regression]
   description: "Tests basic feature"
   ```

## Input Testing

Tests that require CLI input use stdin piping (same as Python). The test runner automatically provides inputs via stdin:

```syn
# tests/conformance/semantics/input_int.syn
# @EXPECT EXIT 0
# @EXPECT STDOUT "You entered: 42"
# @EXPECT INPUT "42"

ASK "Enter number" INTO num AS int;
println "You entered: " + num;
```

**Supported input types:**
- `ASK "prompt" INTO var AS int;` - Integer input
- `ASK "prompt" INTO var AS float;` - Float input
- `ASK "prompt" INTO var AS bool;` - Boolean input ("true", "false", "1", "0")
- `ASK "prompt" INTO var AS str;` - String input

**Multiple inputs:**
```syn
# @EXPECT INPUT "first"
# @EXPECT INPUT "second"

ASK "First" INTO a AS str;
ASK "Second" INTO b AS str;
```

**Manual testing:**
```bash
# Pipe input (like Python)
echo "42" | ./build/synapse run test.syn --vm

# Or use file redirection
./build/synapse run test.syn --vm < input.txt
```