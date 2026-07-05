#!/usr/bin/env python3
"""Golden-file test harness (design doc 003).

Discovers tests/{conformance,errors,automation}/**/*.syn and checks each
against expectations embedded as trailing comments:

    say(10 // 3)
    # expect: 3

    x()
    # expect-error: E0100

    mouse 300, 400
    # expect-platform: MouseMove(300, 400)

Usage: python3 tests/run.py [--release|--debug]
"""
import os
import re
import subprocess
import sys

REPO_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

EXPECT_RE          = re.compile(r'^\s*#\s*expect:\s*(.*)$')
EXPECT_ERROR_RE    = re.compile(r'^\s*#\s*expect-error:\s*(\S+)')
EXPECT_PLATFORM_RE = re.compile(r'^\s*#\s*expect-platform:\s*(.*)$')


def find_syn_binary(preference=None):
    order = [preference] if preference else ["release", "debug"]
    for preset in order:
        path = os.path.join(REPO_DIR, "build", preset, "syn")
        if os.path.exists(path):
            return path
    print("no syn binary found under build/{release,debug} — build first", file=sys.stderr)
    sys.exit(1)


def parse_expectations(path):
    expect_lines, expect_errors, expect_platform = [], [], []
    with open(path) as f:
        for line in f:
            m = EXPECT_RE.match(line)
            if m: expect_lines.append(m.group(1)); continue
            m = EXPECT_ERROR_RE.match(line)
            if m: expect_errors.append(m.group(1)); continue
            m = EXPECT_PLATFORM_RE.match(line)
            if m: expect_platform.append(m.group(1))
    return expect_lines, expect_errors, expect_platform


def run_script(binary, path, category):
    env = os.environ.copy()
    # SYN_NO_JIT: the JIT's top-level-main path has known gaps (silently
    # swallows some errors) unrelated to what these goldens check — the
    # bytecode interpreter is the ground truth for correctness here.
    env["SYN_NO_JIT"] = "1"
    if category == "automation":
        env["SYN_MOCK_PLATFORM"] = "1"
        env["SYN_DUMP_PLATFORM_LOG"] = "1"
    return subprocess.run([binary, path], capture_output=True, text=True, env=env)


def check(binary, path, category):
    expect_lines, expect_errors, expect_platform = parse_expectations(path)
    r = run_script(binary, path, category)
    failures = []

    if expect_errors:
        for code in expect_errors:
            # Runtime errors carry a stable E#### code (bracketed); compile-time
            # diagnostics (lexer/parser) don't have codes yet, so for those
            # `expect-error:` names a substring to find in stderr instead.
            needle = f"error[{code}]" if re.match(r'^E\d+$', code) else code
            if needle not in r.stderr:
                failures.append(f"expected {needle!r} not found in stderr:\n{r.stderr}")
    elif r.returncode != 0:
        failures.append(f"unexpected nonzero exit ({r.returncode}); stderr:\n{r.stderr}")

    if expect_lines:
        actual = [l for l in r.stdout.splitlines() if not l.startswith("PLATFORM: ")]
        if actual != expect_lines:
            failures.append(f"stdout mismatch:\n  expected: {expect_lines}\n  actual:   {actual}")

    if expect_platform:
        actual_p = [l[len("PLATFORM: "):] for l in r.stdout.splitlines() if l.startswith("PLATFORM: ")]
        if actual_p != expect_platform:
            failures.append(f"platform log mismatch:\n  expected: {expect_platform}\n  actual:   {actual_p}")

    return failures


def discover(category):
    d = os.path.join(REPO_DIR, "tests", category)
    if not os.path.isdir(d):
        return []
    found = []
    for root, _, files in os.walk(d):
        for fn in sorted(files):
            if fn.endswith(".syn"):
                found.append(os.path.join(root, fn))
    return sorted(found)


def main():
    preference = "release" if "--release" in sys.argv else ("debug" if "--debug" in sys.argv else None)
    binary = find_syn_binary(preference)

    total = failed = 0
    for category in ("conformance", "errors", "automation"):
        for path in discover(category):
            total += 1
            rel = os.path.relpath(path, REPO_DIR)
            failures = check(binary, path, category)
            if failures:
                failed += 1
                print(f"FAIL {rel}")
                for f in failures:
                    print(f"  {f}")
            else:
                print(f"PASS {rel}")

    print(f"\n{total - failed}/{total} passed")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
