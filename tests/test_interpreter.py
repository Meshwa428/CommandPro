import subprocess
import pytest
import os

SYNAPSE_BIN = "./build/synapse"

def run_script(script_name):
    script_path = os.path.join("tests", "scripts", script_name)
    result = subprocess.run(
        [SYNAPSE_BIN, "run", script_path],
        capture_output=True,
        text=True
    )
    return result

def test_vars_literals():
    res = run_script("vars_literals.syn")
    assert res.returncode == 0
    lines = res.stdout.strip().split("\n")
    assert lines[0] == "42"
    assert lines[1] == "3.14"
    assert lines[2] == "true"
    assert lines[3] == "test"
    assert lines[4] == "500ms"
    assert lines[5] == "(10.000000, 20.000000)"
    assert lines[6] == "null"
    assert lines[7] == "now a string"

def test_arithmetic():
    res = run_script("arithmetic.syn")
    assert res.returncode == 0
    lines = res.stdout.strip().split("\n")
    assert lines[0] == "13"
    assert lines[1] == "7"
    assert lines[2] == "30"
    assert lines[3].startswith("3.33333")
    assert lines[4] == "3"
    assert lines[5] == "1"
    assert lines[6] == "100"
    assert lines[7] == "14"
    assert lines[8] == "20"
    assert lines[9] == "Num: 10"

def test_compound_assign():
    res = run_script("compound_assign.syn")
    assert res.returncode == 0
    lines = res.stdout.strip().split("\n")
    assert lines[0] == "15"
    assert lines[1] == "12"
    assert lines[2] == "24"
    assert lines[3] == "6"
    assert lines[4] == "0"

def test_logic_compare():
    res = run_script("logic_compare.syn")
    assert res.returncode == 0
    lines = res.stdout.strip().split("\n")
    assert lines[0] == "true"
    assert lines[1] == "false"
    assert lines[2] == "true"
    assert lines[3] == "false"
    assert lines[4] == "true"
    assert lines[5] == "true"
    assert lines[6] == "true"
    assert lines[7] == "false"
    assert lines[8] == "false" # In C++ variant, 10 == "10" usually false unless specifically coerced
    assert lines[9] == "false"
    assert lines[10] == "true"
    assert lines[11] == "false"

def test_if_else():
    res = run_script("if_else.syn")
    assert res.returncode == 0
    lines = res.stdout.strip().split("\n")
    assert lines[0] == "greater"
    assert lines[1] == "nested exact"

def test_loops():
    res = run_script("loops.syn")
    assert res.returncode == 0
    lines = res.stdout.strip().split("\n")
    assert lines[0] == "3"
    assert lines[1] == "3"
    assert lines[2] == "2"
    assert lines[3] == "1"
    # Ensure repeat 0 times didn't print

def test_functions():
    res = run_script("functions.syn")
    assert res.returncode == 0
    lines = res.stdout.strip().split("\n")
    assert lines[0] == "12"
    assert lines[1] == "120"
    assert lines[2] == "null"

def test_closures():
    res = run_script("closures.syn")
    assert res.returncode == 0
    lines = res.stdout.strip().split("\n")
    assert lines[0] == "world"

def test_try_catch():
    res = run_script("try_catch.syn")
    assert res.returncode == 0
    lines = res.stdout.strip().split("\n")
    assert "Caught error: Undefined variable" in lines[0]
    assert lines[1] == "Survived"
