"""Shared helpers for the Solidbits test suite."""
import os, sys

# Color only when stdout is a TTY (and NO_COLOR unset), or when FORCE_COLOR is
# set (handy for piping into a viewer / CI). Plain text otherwise so redirected
# output stays clean.
if "FORCE_COLOR" in os.environ:
    _USE_COLOR = True
elif "NO_COLOR" in os.environ:
    _USE_COLOR = False
else:
    _USE_COLOR = sys.stdout.isatty()

_GREEN = "\033[32m" if _USE_COLOR else ""
_RED   = "\033[31m" if _USE_COLOR else ""
_RESET = "\033[0m"  if _USE_COLOR else ""


class Checker:
    """Counting assert helper: PASS prints green, FAIL prints red."""
    def __init__(self):
        self.passed = 0
        self.failed = 0

    def check(self, name, got, want):
        if str(got) == str(want):
            self.passed += 1
            print("  {}PASS{} {}".format(_GREEN, _RESET, name))
        else:
            self.failed += 1
            print("  {}FAIL{} {}: got {!r}, want {!r}".format(_RED, _RESET, name, got, want))

    def done(self):
        """Print the per-suite tally and return a process exit code (0/1)."""
        print("  -> {} passed, {} failed".format(self.passed, self.failed))
        return 1 if self.failed else 0
