#!/usr/bin/env python3
"""BITOP AND/OR/XOR/NOT, BITCOP, multi-source, C9 dest writeback."""
import os, sys
from solidbits import Solidbits
from common import Checker

PORT = int(os.environ.get("SOLIDBITS_PORT", "16379"))
sb = Solidbits("127.0.0.1", PORT)
c = Checker()

sb.setbit("x", 0, 1); sb.setbit("x", 1, 1)   # x = 0xC0
sb.setbit("y", 1, 1)                          # y = 0x40
sb.setbit("z", 0, 1)                          # z = 0x80

c.check("BITOP AND d=x&y",    sb.bitop("AND", "d", "x", "y"), "1")
c.check("  AND bit6 set",     sb.getbit("d", 1), "1")
c.check("  AND bit7 clear",   sb.getbit("d", 0), "0")
c.check("BITOP OR e=x|y",     sb.bitop("OR", "e", "x", "y"), "1")
c.check("  OR bit7 set",      sb.getbit("e", 0), "1")
c.check("BITOP XOR f=x^y",    sb.bitop("XOR", "f", "x", "y"), "1")
c.check("  XOR bit7 set",     sb.getbit("f", 0), "1")
c.check("BITOP NOT g=~x",     sb.bitop("NOT", "g", "x"), "1")
c.check("  NOT popcount 6",   sb.bitcount("g"), "6")
c.check("BITCOP AND x y",     sb.bitcop("AND", "x", "y"), "1")
c.check("BITCOP OR x y",      sb.bitcop("OR", "x", "y"), "2")
c.check("BITOP AND 3 src",    sb.bitop("AND", "m", "x", "y", "z"), "1")
c.check("  3-src bit7 clear", sb.getbit("m", 0), "0")
c.check("  3-src bit6 clear", sb.getbit("m", 1), "0")

sys.exit(c.done())
