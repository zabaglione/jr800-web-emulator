# SPDX-License-Identifier: MIT
"""Build and check exactly one game's native rule fixtures."""
import re
import subprocess
import sys

root, assembler, linker, runner, game = sys.argv[1:]
if not re.fullmatch(r"[a-z][a-z0-9-]{0,39}", game):
    raise SystemExit("Invalid game ID")
subprocess.run(["make", "-s", "-C", root + "/games/" + game,
                "JR8AS=" + assembler, "JR8LD=" + linker], check=True)
subprocess.run([runner, root, game], check=True)
