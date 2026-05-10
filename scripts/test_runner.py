#
# Synapse Language - test_runner.py
# Copyright (C) 2024-2026 Meshwa428
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
#
# ADDITIONAL TERMS: Under Section 7 of the AGPLv3, you must preserve 
# the following attribution: Any interactive user interfaces of the 
# Covered Work or its derivatives must visibly display the text 
# "Powered by Synapse".
#

#!/usr/bin/env python3
"""
Synapse Production Test Runner
A comprehensive, structured test system for the Synapse language implementation.

Features:
- Automatic test discovery with configurable categories
- YAML-based test specifications
- Legacy @EXPECT comment support
- Benchmark framework with statistical analysis
- JUnit XML report generation for CI
- Parallel test execution
- Rich terminal output with progress tracking
"""

import argparse
import os
import sys
import subprocess
import time
import statistics
import hashlib
import json
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from typing import Optional, List, Dict, Any
from pathlib import Path
from enum import Enum, auto
import re
import random
import shutil

ANSI_ESCAPE = re.compile(r'\x1b\[[0-9;]*m')

def strip_ansi(text: str) -> str:
    return ANSI_ESCAPE.sub('', text)

SCRIPT_DIR = Path(__file__).parent
PROJECT_ROOT = SCRIPT_DIR.parent
BUILD_DIR = PROJECT_ROOT / "build"
SYNAPSE_BIN = BUILD_DIR / "synapse"
PYTHON_EXE = sys.executable

class TestCategory(Enum):
    UNIT = auto()
    INTEGRATION = auto()
    CONFORMANCE = auto()
    FUNCTIONAL = auto()
    BENCHMARK = auto()
    PROPERTY = auto()

class TestStatus(Enum):
    PASSED = "passed"
    FAILED = "failed"
    SKIPPED = "skipped"
    ERROR = "error"

@dataclass
class TestSpec:
    name: str
    path: Path
    category: TestCategory
    expected_exit: int = 0
    expected_stdout: List[str] = field(default_factory=list)
    expected_stderr: List[str] = field(default_factory=list)
    skip_vm: bool = False
    skip_interp: bool = False
    timeout: int = 30
    iterations: int = 1
    baseline_py: Optional[Path] = None
    tags: List[str] = field(default_factory=list)
    description: str = ""
    inputs: List[str] = field(default_factory=list)

@dataclass
class TestResult:
    spec: TestSpec
    status: TestStatus
    exit_code: int = 0
    stdout: str = ""
    stderr: str = ""
    duration_ms: float = 0
    error_message: str = ""

@dataclass
class BenchmarkResult:
    name: str
    syn_mean_ms: float
    syn_stddev_ms: float
    py_mean_ms: float
    speedup: float
    iterations: int

class Colors:
    RESET = "\033[0m"
    BOLD = "\033[1m"
    RED = "\033[0;31m"
    GREEN = "\033[0;32m"
    YELLOW = "\033[0;33m"
    BLUE = "\033[0;34m"
    MAGENTA = "\033[0;35m"
    CYAN = "\033[0;36m"
    GRAY = "\033[0;90m"

    @staticmethod
    def color(status: TestStatus) -> str:
        return {
            TestStatus.PASSED: Colors.GREEN,
            TestStatus.FAILED: Colors.RED,
            TestStatus.SKIPPED: Colors.YELLOW,
            TestStatus.ERROR: Colors.MAGENTA,
        }.get(status, Colors.RESET)

class TestRunner:
    def __init__(self, args):
        self.args = args
        self.results: List[TestResult] = []
        self.benchmarks: List[BenchmarkResult] = []
        self._ensure_build()

    def _ensure_build(self):
        if not SYNAPSE_BIN.exists():
            print(f"{Colors.YELLOW}Synapse binary not found. Compiling...{Colors.RESET}")
            self._compile()

    def _compile(self):
        BUILD_DIR.mkdir(parents=True, exist_ok=True)
        subprocess.run(
            ["cmake", ".."],
            cwd=BUILD_DIR,
            capture_output=True,
            check=True
        )
        subprocess.run(
            ["make", "-j", str(os.cpu_count() or 4)],
            cwd=BUILD_DIR,
            capture_output=True,
            check=True
        )

    def discover_tests(self) -> List[TestSpec]:
        tests = []
        test_dirs = {
            TestCategory.UNIT: PROJECT_ROOT / "tests" / "unit",
            TestCategory.INTEGRATION: PROJECT_ROOT / "tests" / "integration",
            TestCategory.CONFORMANCE: PROJECT_ROOT / "tests" / "conformance",
            TestCategory.FUNCTIONAL: PROJECT_ROOT / "tests" / "functional",
            TestCategory.BENCHMARK: PROJECT_ROOT / "tests" / "benchmarks",
            TestCategory.PROPERTY: PROJECT_ROOT / "tests" / "property",
        }

        for category, dir_path in test_dirs.items():
            if not dir_path.exists():
                continue
            tests.extend(self._scan_directory(dir_path, category))

        return sorted(tests, key=lambda t: (t.category.name, t.name))

    def _scan_directory(self, directory: Path, category: TestCategory) -> List[TestSpec]:
        tests = []
        for file_path in directory.rglob("*.syn"):
            spec = self._parse_test_file(file_path, category)
            if spec:
                tests.append(spec)
        return tests

    def _parse_test_file(self, path: Path, category: TestCategory) -> Optional[TestSpec]:
        spec = TestSpec(
            name=path.stem,
            path=path,
            category=category,
        )

        yaml_path = path.with_suffix(".yaml")
        if yaml_path.exists():
            self._parse_yaml_spec(yaml_path, spec)
        else:
            self._parse_legacy_expect(path, spec)

        return spec

    def _parse_yaml_spec(self, yaml_path: Path, spec: TestSpec):
        try:
            import yaml
            with open(yaml_path, encoding='utf-8') as f:
                data = yaml.safe_load(f) or {}

            spec.expected_exit = data.get("exit", 0)
            spec.expected_stdout = data.get("stdout", [])
            spec.expected_stderr = data.get("stderr", [])
            spec.skip_vm = data.get("skip_vm", False)
            spec.skip_interp = data.get("skip_interp", False)
            spec.timeout = data.get("timeout", 30)
            spec.iterations = data.get("iterations", 1)
            spec.tags = data.get("tags", [])
            spec.description = data.get("description", "")
            spec.inputs = data.get("inputs", [])

            baseline = data.get("baseline")
            if baseline:
                spec.baseline_py = (PROJECT_ROOT / "tests" / "benchmarks" / baseline).with_suffix(".py")
        except ImportError:
            self._auto_detect_baseline(spec)
            self._parse_legacy_expect(spec.path, spec)
        except Exception as e:
            print(f"{Colors.YELLOW}Warning: Failed to parse {yaml_path}: {e}{Colors.RESET}")

    def _auto_detect_baseline(self, spec: TestSpec):
        if spec.category == TestCategory.BENCHMARK:
            py_path = spec.path.with_suffix(".py")
            if py_path.exists():
                spec.baseline_py = py_path

    def _parse_legacy_expect(self, path: Path, spec: TestSpec):
        self._auto_detect_baseline(spec)
        try:
            with open(path, encoding='utf-8', errors='replace') as f:
                for line in f:
                    if not line.strip().startswith("# @EXPECT"):
                        continue
                    parts = line.strip().split()
                    if len(parts) < 3:
                        continue
                    tag = parts[2].upper()
                    value = " ".join(parts[3:]).strip()
                    if value.startswith('"') and value.endswith('"'):
                        value = value[1:-1]

                    if tag == "EXIT":
                        spec.expected_exit = int(value)
                    elif tag == "STDOUT":
                        spec.expected_stdout.append(value)
                    elif tag == "STDERR":
                        spec.expected_stderr.append(value)
                    elif tag == "SKIP_VM":
                        spec.skip_vm = True
                    elif tag == "SKIP_INTERP":
                        spec.skip_interp = True
                    elif tag == "INPUT":
                        spec.inputs.append(value)
        except Exception as e:
            print(f"{Colors.YELLOW}Warning: Failed to parse {path}: {e}{Colors.RESET}")

    def run_test(self, spec: TestSpec, use_vm: bool) -> TestResult:
        if use_vm and spec.skip_vm:
            return TestResult(spec, TestStatus.SKIPPED)
        if not use_vm and spec.skip_interp:
            return TestResult(spec, TestStatus.SKIPPED)

        cmd = [str(SYNAPSE_BIN), "run", str(spec.path), "--mock"]
        if use_vm:
            cmd.append("--vm")

        start = time.perf_counter()
        try:
            proc = subprocess.run(
                cmd,
                capture_output=True,
                input="\n".join(spec.inputs).encode() if spec.inputs else None,
                timeout=spec.timeout
            )
            duration = (time.perf_counter() - start) * 1000
            exit_code = proc.returncode
            stdout = strip_ansi(proc.stdout.decode('utf-8', errors='replace'))
            stderr = strip_ansi(proc.stderr.decode('utf-8', errors='replace'))
        except subprocess.TimeoutExpired:
            duration = spec.timeout * 1000
            return TestResult(
                spec, TestStatus.ERROR, 0, "", f"Timeout after {spec.timeout}s",
                duration, "Test timed out"
            )
        except Exception as e:
            import traceback
            duration = 0
            return TestResult(spec, TestStatus.ERROR, 0, "", str(e), 0, f"{e}\n{traceback.format_exc()}")

        status = TestStatus.PASSED
        errors = []

        if exit_code != spec.expected_exit:
            status = TestStatus.FAILED
            errors.append(f"Exit code: expected {spec.expected_exit}, got {exit_code}")

        for expected in spec.expected_stdout:
            if expected not in stdout:
                status = TestStatus.FAILED
                errors.append(f"Stdout missing: '{expected}'")

        for expected in spec.expected_stderr:
            if expected not in stderr:
                status = TestStatus.FAILED
                errors.append(f"Stderr missing: '{expected}'")

        return TestResult(
            spec, status, exit_code, stdout, stderr, duration,
            "; ".join(errors) if errors else ""
        )

    def run_benchmark(self, spec: TestSpec, iterations: int) -> Optional[BenchmarkResult]:
        if not spec.baseline_py or not spec.baseline_py.exists():
            return None

        syn_times = []
        py_times = []

        for _ in range(iterations):
            res = self.run_test(spec, self.args.vm)
            if res.status == TestStatus.PASSED:
                syn_times.append(res.duration_ms)
            else:
                return None

            start = time.perf_counter()
            subprocess.run([PYTHON_EXE, str(spec.baseline_py)],
                         capture_output=True, check=True)
            py_times.append((time.perf_counter() - start) * 1000)

        return BenchmarkResult(
            spec.name,
            statistics.mean(syn_times),
            statistics.stdev(syn_times) if len(syn_times) > 1 else 0,
            statistics.mean(py_times),
            statistics.mean(py_times) / statistics.mean(syn_times),
            iterations
        )

    def run_stability_test(self, spec: TestSpec, iterations: int) -> bool:
        hashes = set()
        for _ in range(iterations):
            res = self.run_test(spec, self.args.vm)
            if res.status != TestStatus.PASSED:
                return False
            filtered = "\n".join(l for l in res.stdout.splitlines()
                                 if "Time:" not in l and "ms" not in l and not l.startswith("RESULT:"))
            hashes.add(hashlib.sha256(filtered.encode()).hexdigest())
        return len(hashes) == 1

    def run_fuzz_test(self, iterations: int) -> Dict[str, int]:
        chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 \n\t+-*/=(){}[],;\"'#!"
        fuzz_file = PROJECT_ROOT / "fuzz_temp.syn"
        crashes = {"segfault": 0, "timeout": 0, "other": 0}

        for _ in range(iterations):
            content = "".join(random.choice(chars) for _ in range(random.randint(10, 200)))
            fuzz_file.write_text(content)
            try:
                proc = subprocess.run(
                    [str(SYNAPSE_BIN), "run", str(fuzz_file), "--mock", "--vm"],
                    capture_output=True,
                    timeout=1
                )
                if proc.returncode in (-11, 139):
                    crashes["segfault"] += 1
            except subprocess.TimeoutExpired:
                crashes["timeout"] += 1
            except Exception:
                crashes["other"] += 1

        fuzz_file.unlink(missing_ok=True)
        return crashes

    def print_results(self):
        total = len(self.results)
        passed = sum(1 for r in self.results if r.status == TestStatus.PASSED)
        failed = sum(1 for r in self.results if r.status == TestStatus.FAILED)
        skipped = sum(1 for r in self.results if r.status == TestStatus.SKIPPED)
        errors = sum(1 for r in self.results if r.status == TestStatus.ERROR)

        print(f"\n{Colors.BOLD}Test Results{Colors.RESET}")
        print("=" * 70)
        print(f"Total: {total} | {Colors.GREEN}Passed: {passed}{Colors.RESET} | "
              f"{Colors.RED}Failed: {failed}{Colors.RESET} | "
              f"{Colors.YELLOW}Skipped: {skipped}{Colors.RESET} | "
              f"{Colors.MAGENTA}Errors: {errors}{Colors.RESET}")

        if self.args.verbose:
            print("\nDetails:")
            for r in self.results:
                color = Colors.color(r.status)
                symbol = {"passed": "✓", "failed": "✗", "skipped": "⊘", "error": "⚠"}[r.status.value]
                print(f"  {color}{symbol} {r.spec.name} ({r.spec.category.name}){Colors.RESET}")
                if r.status != TestStatus.PASSED:
                    print(f"      {r.error_message[:100]}")

        return failed == 0 and errors == 0

    def print_benchmarks(self):
        if not self.benchmarks:
            return
        print(f"\n{Colors.BOLD}Benchmark Results{Colors.RESET}")
        print("=" * 90)
        print(f"{'Name':<35} | {'Synapse (ms)':>15} | {'Python (ms)':>15} | {'Speedup':>10}")
        print("-" * 90)
        for b in self.benchmarks:
            color = Colors.GREEN if b.speedup >= 1.0 else Colors.YELLOW if b.speedup > 0.2 else Colors.RED
            print(f"{b.name:<35} | {b.syn_mean_ms:>15.4f} | {b.py_mean_ms:>15.4f} | "
                  f"{color}{b.speedup:>9.2f}x{Colors.RESET}")

    def generate_junit_xml(self, path: Path):
        testsuite = ET.Element("testsuite")
        testsuite.set("name", "Synapse")
        testsuite.set("tests", str(len(self.results)))

        for r in self.results:
            testcase = ET.SubElement(testsuite, "testcase")
            testcase.set("name", r.spec.name)
            testcase.set("classname", r.spec.category.name)
            testcase.set("time", f"{r.duration_ms/1000:.3f}")

            if r.status == TestStatus.FAILED:
                failure = ET.SubElement(testcase, "failure")
                failure.set("message", r.error_message)
            elif r.status == TestStatus.ERROR:
                error = ET.SubElement(testcase, "error")
                error.set("message", r.error_message)
            elif r.status == TestStatus.SKIPPED:
                skipped = ET.SubElement(testcase, "skipped")

        tree = ET.ElementTree(testsuite)
        ET.indent(tree)
        tree.write(path, encoding="utf-8", xml_declaration=True)

    def run(self):
        tests = self.discover_tests()
        print(f"{Colors.CYAN}Discovered {len(tests)} tests{Colors.RESET}")

        use_vm = self.args.vm
        for spec in tests:
            if self.args.category and spec.category.name.lower() != self.args.category.lower():
                continue
            result = self.run_test(spec, use_vm)
            self.results.append(result)

            if self.args.verbose or result.status != TestStatus.PASSED:
                color = Colors.color(result.status)
                symbol = {"passed": "✓", "failed": "✗", "skipped": "⊘", "error": "⚠"}[result.status.value]
                print(f"{color}{symbol} {spec.name} ({spec.category.name}){Colors.RESET}")

        if self.args.benchmark:
            print(f"\n{Colors.CYAN}Running benchmarks...{Colors.RESET}")
            for spec in [t for t in tests if t.category == TestCategory.BENCHMARK]:
                if result := self.run_benchmark(spec, self.args.iterations):
                    self.benchmarks.append(result)

        if self.args.stability:
            print(f"\n{Colors.CYAN}Running stability test...{Colors.RESET}")
            stability_spec = TestSpec(
                name="stability",
                path=PROJECT_ROOT / "tests/functional/stability.syn",
                category=TestCategory.FUNCTIONAL
            )
            if self.run_stability_test(stability_spec, self.args.iterations):
                print(f"{Colors.GREEN}✓ Stability test passed{Colors.RESET}")
            else:
                print(f"{Colors.RED}✗ Stability test failed{Colors.RESET}")

        if self.args.fuzz:
            print(f"\n{Colors.CYAN}Running fuzz tests...{Colors.RESET}")
            crashes = self.run_fuzz_test(self.args.iterations)
            if sum(crashes.values()) == 0:
                print(f"{Colors.GREEN}✓ Fuzz testing passed{Colors.RESET}")
            else:
                print(f"{Colors.RED}✗ Fuzz testing found issues: {crashes}{Colors.RESET}")

        success = self.print_results()
        self.print_benchmarks()

        if self.args.junit:
            self.generate_junit_xml(Path(self.args.junit))
            print(f"\n{Colors.CYAN}JUnit XML report: {self.args.junit}{Colors.RESET}")

        sys.exit(0 if success else 1)

def main():
    parser = argparse.ArgumentParser(
        description="Synapse Production Test Runner",
        formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("--vm", action="store_true", default=True, help="Use VM (default: True)")
    parser.add_argument("--interp", action="store_false", dest="vm", help="Use interpreter")
    parser.add_argument("-v", "--verbose", action="store_true", help="Verbose output")
    parser.add_argument("-n", "--iterations", type=int, default=5, help="Benchmark iterations")
    parser.add_argument("-c", "--category", type=str, help="Run only specific category")
    parser.add_argument("--benchmark", action="store_true", help="Run benchmarks")
    parser.add_argument("--stability", action="store_true", help="Run stability tests")
    parser.add_argument("--fuzz", action="store_true", help="Run fuzz tests")
    parser.add_argument("--junit", type=str, help="Generate JUnit XML report")
    parser.add_argument("tests", nargs="*", help="Specific test files")

    args = parser.parse_args()
    TestRunner(args).run()

if __name__ == "__main__":
    main()