#
# Synapse Language - string_equality.py
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

def run():
    i = 0
    while i < 1000000:
        s1 = "hello_world"
        s2 = "hello_world"
        if s1 == s2: pass
        i += 1

if __name__ == '__main__':
    import time
    start = time.time()
    run()
    print("RESULT:" + str((time.time() - start) * 1000))
