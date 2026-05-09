import subprocess
import os
import sys

SYNAPSE_BIN = "./build/synapse"

def run_test(path, expected_code=0, expected_stdout=None, expected_stderr=None):
    cmd = [SYNAPSE_BIN, "run", path, "--mock", "--vm"]
    result = subprocess.run(cmd, capture_output=True, text=True)
    
    passed = True
    errors = []
    
    if result.returncode != expected_code:
        passed = False
        errors.append(f"Exit code mismatch: expected {expected_code}, got {result.returncode}")
        
    if expected_stdout:
        if isinstance(expected_stdout, list):
            for s in expected_stdout:
                if s not in result.stdout:
                    passed = False
                    errors.append(f"Stdout missing expected string: '{s}'")
        elif expected_stdout not in result.stdout:
            passed = False
            errors.append(f"Stdout missing expected string: '{expected_stdout}'")
        
    if expected_stderr:
        if isinstance(expected_stderr, list):
            for s in expected_stderr:
                if s not in result.stderr:
                    passed = False
                    errors.append(f"Stderr missing expected string: '{s}'")
        elif expected_stderr not in result.stderr:
            passed = False
            errors.append(f"Stderr missing expected string: '{expected_stderr}'")
        
    return passed, errors, result

def main():
    conformance_dir = "tests/conformance"
    
    tests = [
        # Lexer
        ("lexer/unterminated_string.syn", 1, None, "[LexerError]"),
        ("lexer/huge_identifier.syn", 0, "10", None),
        ("lexer/numeric_overflow.syn", 1, None, "[ParseError]"),
        ("lexer/weird_whitespace.syn", 0, "30", None),
        
        # Parser
        ("parser/precedence.syn", 0, ["Precedence OK", "Grouping OK", "Logic Precedence OK"], None),
        ("parser/associativity.syn", 0, "Associativity OK", None),
        
        # Semantics - Basic
        ("semantics/truthiness.syn", 0, ["0 is truthy: false", "null is truthy: false", "hello is truthy: true"], None),
        ("semantics/shadowing.syn", 0, ["Inner x is 20: OK", "Outer x is still 10: OK", "Deep inner OK"], None),
        ("semantics/closures.syn", 0, "f1 OK", None),

        # Semantics - Strings
        ("semantics/strings.syn", 0, ["Line 1\nLine 2", "Quote \" Inside", "Empty: []", "Empty is falsy: OK", "First: S", "Last: e", "Equality OK"], None),
        
        # Semantics - Types & Coercion
        ("semantics/types.syn", 0, ["Types OK", "Float to Int coercion OK", "Int to Float coercion OK"], None),
        ("semantics/type_error_str_to_int.syn", 1, None, "[RuntimeError]"),
        ("semantics/type_error_int_to_bool.syn", 1, None, "[RuntimeError]"),

        # Semantics - Control Flow
        ("semantics/control_flow.syn", 0, ["While loop skipped OK", "Short-circuit and: OK", "Short-circuit or: OK"], None),
        
        # Semantics - Functions
        ("semantics/functions_extra.syn", 0, ["Mult-arg OK", "high", "low", "none", "Fact 5: 120"], None),
        ("semantics/recursion.syn", 0, ["Recursion Sum 100 OK", "Mutual Recursion OK"], None),

        # Semantics - Advanced
        ("semantics/heavy_duty.syn", 0, ["Math OK", "Nested OK", "List access OK", "Map access OK", "Fib 10 OK"], None),
        ("semantics/collections.syn", 0, ["List mutation OK", "Map mutation OK", "Map insertion OK", "Item 2: cherry"], None),
        ("semantics/math_stress.syn", 0, ["Float math OK", "Negative modulo OK", "Exponent OK"], None),
        ("semantics/stress_500.syn", 0, "Stress OK", None),

        # Semantics - Error Cases
        ("semantics/err_undefined.syn", 1, None, "[RuntimeError]"),
        ("semantics/err_not_callable.syn", 1, None, "[RuntimeError]"),
        ("semantics/err_wrong_args.syn", 1, None, "[RuntimeError]"),
        ("semantics/err_div_zero.syn", 1, None, "[RuntimeError]"),

        # Parser
        ("parser/deep_math.syn", 0, ["Deep Math OK", "Associativity OK"], None),

        # VM Integrity
        ("vm/stack_exhaustion.syn", 1, "Starting recursion...", ["[VM RuntimeError]", "Stack overflow"]),
        
        # API Safety
        ("api/oal_errors.syn", 0, "OAL: Negative coords handled", None),
    ]
    
    print("🚀 Running Synapse Language Conformance Suite")
    print("-" * 75)
    print(f"{'Test Path':<45} | {'Result':<10}")
    print("-" * 75)
    
    passed_count = 0
    failed_count = 0
    
    for path, code, out, err in tests:
        full_path = os.path.join(conformance_dir, path)
        if not os.path.exists(full_path):
            print(f"⚠️  MISSING: {path:<36} | SKIP")
            continue
            
        success, errors, result = run_test(full_path, code, out, err)
        
        if success:
            print(f"✅ PASS: {path:<40} | OK")
            passed_count += 1
        else:
            print(f"❌ FAIL: {path:<40} | ERROR")
            for e in errors:
                print(f"   - {e}")
            failed_count += 1
            
    print("-" * 75)
    print(f"TOTAL: {passed_count + failed_count} | PASSED: {passed_count} | FAILED: {failed_count}")
    
    if failed_count > 0:
        sys.exit(1)

if __name__ == "__main__":
    main()
