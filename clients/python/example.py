#!/usr/bin/env python3
"""Solidbits Python client example.

Start the server first:
    ./solidbits -d /tmp/sbdata -p /tmp/sb.pid

Then run:
    cd clients/python && python3 example.py
"""
from solidbits import Solidbits

with Solidbits("127.0.0.1", 6379) as sb:
    # SETBIT returns the previous bit value; GETBIT reads a bit.
    print("SETBIT foo 95 1 ->", sb.setbit("foo", 95, 1))   # 0 (old value)
    print("GETBIT foo 95   ->", sb.getbit("foo", 95))      # 1
    print("BITCOUNT foo    ->", sb.bitcount("foo"))         # 1

    # BITOP stores the op result into dest, returns bytes written.
    sb.setbit("a", 0, 1)
    sb.setbit("b", 1, 1)
    print("BITOP AND c a b ->", sb.bitop("AND", "c", "a", "b"))

    # BITCOP = BITCOUNT(BITOP): count set bits of the AND result, no dest.
    print("BITCOP AND a b  ->", sb.bitcop("AND", "a", "b"))
