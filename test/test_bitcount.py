#!/usr/bin/env python3
"""BITCOUNT: full range, sub-range, negative indices (C2), sub-range count (C3)."""
import os, sys
from solidbits import Solidbits
from common import Checker

PORT = int(os.environ.get("SOLIDBITS_PORT", "16379"))
sb = Solidbits("127.0.0.1", PORT)
c = Checker()

# build bc = 2 bytes [0x80, 0x80], 2 bits set
sb.setbit("bc", 0, 1)
sb.setbit("bc", 8, 1)

c.check("BITCOUNT full range",      sb.bitcount("bc"), "2")
c.check("C3 BITCOUNT byte0 (0 0)",  sb.bitcount("bc", 0, 0), "1")
c.check("BITCOUNT byte1 (1 1)",     sb.bitcount("bc", 1, 1), "1")
c.check("C2 BITCOUNT 0 -1 (all)",   sb.bitcount("bc", 0, -1), "2")
c.check("C2 BITCOUNT -1 -1 (last)", sb.bitcount("bc", -1, -1), "1")
c.check("C2 BITCOUNT -2 -1 (both)", sb.bitcount("bc", -2, -1), "2")

sys.exit(c.done())
