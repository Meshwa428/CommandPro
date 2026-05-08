import subprocess
import os
import pytest

BINARY_PATH = "./build/synapse"

def run_synapse(script_path, use_mock=False):
    args = [BINARY_PATH, "run", script_path]
    if use_mock:
        args.append("--mock")
    result = subprocess.run(args, capture_output=True, text=True)
    return result

def test_mouse_automation():
    # Use mock to verify calls
    result = run_synapse("tests/scripts/mouse.syn", use_mock=True)
    assert result.returncode == 0
    assert "MOUSE_TEST_OK" in result.stdout
    assert "[MOCK] MOUSE_MOVE 100 100" in result.stdout
    assert "[MOCK] MOUSE_MOVE 300 250" in result.stdout

def test_keyboard_automation():
    # Use mock to verify keyboard calls
    result = run_synapse("tests/scripts/keyboard.syn", use_mock=True)
    assert result.returncode == 0
    assert "KEYBOARD_TEST_OK" in result.stdout
    assert "[MOCK] KEY_TYPE hello synapse" in result.stdout
    assert "[MOCK] KEY_PRESS Return" in result.stdout
