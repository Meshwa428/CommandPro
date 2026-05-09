# Synapse Performance Analysis

| Feature | Condition | Baseline | Optimized | Improvement |
| :--- | :--- | :--- | :--- | :--- |
| **Arithmetic & Loop** | 100k iterations (sum += i) | 785ms | 7.4ms | **99.1%** |
| **Function Recursion** | Fibonacci 20 | 328ms | 5.8ms | **98.2%** |
| **String Concatenation** | 5,000 operations | 54ms | 3.0ms | **94.4%** |
| **Typed Var Coercion** | 50k assignments | 324ms | 7.0ms | **97.8%** |
| **Tuple Creation** | 5,000 creations | 67ms | 1.6ms | **97.6%** |

## Optimized Architecture (Latest)

1.  **Zero-Exception Returns**: Replaced C++ exception-based `ReturnSignal` with a lightweight `isReturning` flag. This eliminated stack unwinding overhead, providing a ~10x boost to recursive function calls.
2.  **Environment Pooling**: Implemented a reusable pool for `Environment` objects to avoid high-frequency heap allocations during function calls and block entry.
3.  **AST Node Tagging**: Added `NodeType` enum to all AST nodes, replacing slow `dynamic_cast` with fast integer checks in hot paths like expression evaluation.
4.  **Size Hinting**: Added capacity hints to `Environment` map resets to minimize rehashes for function parameters.

## Python Comparison (Post-Optimization)

| Feature | Synapse | Python | Factor |
| :--- | :--- | :--- | :--- |
| **Arithmetic Loop (100k)** | 7.60ms | 5.27ms | **1.4x slower** |
| **Fibonacci 20** | 5.80ms | 0.79ms | **7.3x slower** |
| **Fibonacci 25** | 64.20ms | 9.79ms | **6.6x slower** |
| **Fn Calls (100k)** | 20.60ms | 6.62ms | **3.1x slower** |
| **Nested Scopes (1k)** | 1.00ms | 0.14ms | **7.1x slower** |
| **List Iter (10k)** | 1.80ms | 0.67ms | **2.7x slower** |
| **Tuple Creation (5k)** | 1.40ms | 0.40ms | **3.5x slower** |
| **Complex Expr (100k)** | 59.40ms | 10.41ms | **5.7x slower** |
| **Typed Var Coercion (50k)**| 7.00ms | 1.60ms | **4.4x slower** |

> [!NOTE]
> Synapse is currently a **Tree-Walking Interpreter**. Each operation involves traversing polymorphic AST nodes and looking up variables in a chained hash map. In contrast, Python (CPython) uses a Bytecode VM with heavily optimized internal C loops. While Synapse is slower for raw computation, its primary design goal is high-level OS automation and readability, where performance is typically gated by UI response times rather than interpreter overhead.
