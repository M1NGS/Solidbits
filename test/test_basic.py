#!/usr/bin/env python3
"""Basic SETBIT/GETBIT semantics + C1 (offset bounds) + C6 (key length)."""
import os, sys
from solidbits import Solidbits
from common import Checker

PORT = int(os.environ.get("SOLIDBITS_PORT", "16379"))
sb = Solidbits("127.0.0.1", PORT)
c = Checker()

c.check("SETBIT on new key returns old 0",  sb.setbit("b_k1", 0, 1), "0")
c.check("GETBIT set bit",                   sb.getbit("b_k1", 0), "1")
c.check("GETBIT unset bit (sparse hole)",   sb.getbit("b_k1", 50), "0")
c.check("GETBIT missing key",               sb.getbit("b_nope", 0), "0")
c.check("SETBIT rejects value 2",           sb.setbit("b_k2", 0, 2), "ERR:SETBIT(3) MUST BE 1 OR 0")

c.check("C1 SETBIT negative offset",        sb.setbit("b_c1a", -1, 1), "ERR:OFFSET OUT OF RANGE")
c.check("C1 SETBIT huge offset",            sb.setbit("b_c1b", 18000000000, 1), "ERR:OFFSET OUT OF RANGE")
c.check("C1 GETBIT negative offset",        sb.getbit("b_c1c", -1), "ERR:OFFSET OUT OF RANGE")

c.check("C6 SETBIT 64-char key (ok)",       sb.setbit("k" * 64, 0, 1), "0")
c.check("C6 SETBIT 65-char key (reject)",   sb.setbit("k" * 65, 0, 1), "ERR:KEY TOO LONG")
c.check("C6 GETBIT 65-char key (reject)",   sb.getbit("k" * 65, 0), "ERR:KEY TOO LONG")

sys.exit(c.done())
