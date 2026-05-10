#
# Synapse Language - run_tests.py
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
Synapse Test Runner - CLI Wrapper
Usage:
    python3 scripts/run_tests.py              # Run all tests
    python3 scripts/run_tests.py -c conformance  # Run specific category
    python3 scripts/run_tests.py --benchmark     # Run with benchmarks
"""

import sys
import subprocess
from pathlib import Path

SCRIPT_DIR = Path(__file__).parent
RUNNER = SCRIPT_DIR / "test_runner.py"

def main():
    args = sys.argv[1:]
    cmd = [sys.executable, str(RUNNER)] + args
    sys.exit(subprocess.call(cmd))

if __name__ == "__main__":
    main()