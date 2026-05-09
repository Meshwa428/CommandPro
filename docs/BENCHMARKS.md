# Synapse Runtime Benchmarking Suite

Synapse utilizes a rigorous "Runtime Systems" benchmark suite to measure the microarchitectural efficiency of the Virtual Machine. Unlike feature tests, these benchmarks isolate specific system components to identify bottlenecks in opcode dispatch, memory management, and stack handling.

## Running the Benchmarks

To run the full suite with side-by-side comparison against Python:

```bash
python3 scripts/test_suite.py benchmark
```

## Benchmark Categories

### 1. Opcode Dispatch (`dispatch_stress.syn`)
- **Goal**: Measures the overhead of the core VM loop.
- **Method**: Performs a tight 1,000,000 iteration loop with minimal arithmetic.
- **Target**: Branch prediction efficiency and instruction fetch latency.

### 2. Stack & Expression (`stack_churn.syn`)
- **Goal**: Evaluates the cost of deep stack manipulation.
- **Method**: Calculates deeply nested arithmetic expressions (e.g., `((((i+1)*2)-3)/4)`) over 100,000 iterations.
- **Target**: Stack pointer movement and temporary value management.

### 3. Memory Allocation (`alloc_pressure.syn`)
- **Goal**: Stress tests the object allocator.
- **Method**: Rapidly creates and discards Lists, Tuples, and Maps in a 10,000 iteration loop.
- **Target**: Heap fragmentation, allocation speed, and garbage collection/ref-counting overhead.

### 4. Global variable Access (`global_bench.syn`)
- **Goal**: Measures variable lookup efficiency.
- **Method**: Accesses and modifies global variables 1,000,000 times.
- **Target**: The efficiency of indexed global access vs. hashed name-based lookup.

### 5. String Equality (`string_equality.syn`)
- **Goal**: Validates string interning performance.
- **Method**: Performs 1,000,000 string comparisons using variables to prevent constant folding.
- **Target**: Fast-path comparison for interned strings.

### 6. Native OAL Overhead (`native_call_bench.syn`)
- **Goal**: Measures the "bridge" cost between the VM and native C++ OS APIs.
- **Method**: Calls `get_mouse_pos()` 100,000 times.
- **Target**: The overhead of native function wrapping and coordinate unpacking.

### 7. Branch Efficiency (`branch_stress.syn`)
- **Goal**: Evaluates VM branch prediction behavior.
- **Method**: Executes unpredictable if-else branches based on loop index parity.
- **Target**: Instruction pipeline flushing and jump optimization.

## Performance Considerations

### Mock vs. Real Platforms
When running benchmarks with the `--mock` flag (default in the test suite), Synapse bypasses real OS interaction (X11/uinput). This measures the **pure overhead of the VM and Interpreter logic**.

In real-world usage (without `--mock`):
- **Latency**: Calls like `get_mouse_pos()` or `mouse_move()` involve communication with the X server, which can introduce **0.5ms - 2ms** of latency per call.
- **Comparison**: A script doing 1000 mouse movements might take ~1s on a real platform but only ~2ms on the mock platform.
- **Accuracy**: Always use the mock platform for language-level regression testing, but use the real platform for end-to-end automation timing.

### Current VM Limitations
- **Closures**: The Bytecode VM currently does not support upvalue capture. Tests using these features are skipped in VM mode to prevent incorrect results (they currently return `null` for captured variables). Implementing proper Upvalues is a high-priority roadmap item to achieve full feature parity with the Interpreter.

## Comparative Baselines (VM vs. Python)

We use CPython as a baseline because it is a highly optimized, industry-standard bytecode interpreter. Our goal is to achieve **< 1.5x overhead** compared to Python for standard workloads, and to **outperform** Python in automation-specific tasks (like global lookups and native calls).

| Metric | Goal | Status |
|---|---|---|
| **Raw Dispatch** | < Python | ✅ (Currently ~0.7x) |
| **Global Access** | < Python | ✅ (Currently ~0.4x) |
| **Allocation** | < 1.5x Python | ⚠️ (Currently ~2.8x) |
| **Native Bridge** | < 1.2x Python | ⚠️ (Currently ~1.5x) |

## High-Precision Timing
All benchmarks utilize internal timing via `now()` to exclude VM startup and teardown costs, providing a pure measurement of the execution runtime.
