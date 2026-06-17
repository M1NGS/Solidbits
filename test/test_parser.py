#!/usr/bin/env python3
"""Parser edge cases: C12 multi-space must not silently mis-route."""
import os, sys
from solidbits import Solidbits
from common import Checker

PORT = int(os.environ.get("SOLIDBITS_PORT", "16379"))
sb = Solidbits("127.0.0.1", PORT)
c = Checker()

# send multi-space directly over the raw socket (the client lib only emits
# single spaces); before the fix this was silently routed to the empty key.
def raw(line):
    sb._sock.sendall((line + "\n").encode())
    return sb._recv_line()

sb.setbit("c12k", 0, 1)
c.check("single-space baseline", sb.getbit("c12k", 0), "1")
raw("SETBIT  c12k  7  1")
c.check("C12 double-space SETBIT hits key", sb.getbit("c12k", 7), "1")
c.check("C12 double-space GETBIT",          raw("GETBIT  c12k  0"), "1")

sys.exit(c.done())
