"""Minimal TCP client for the Solidbits bitmap server.

Solidbits speaks a simple line protocol over TCP: each request is a
space-separated command terminated by a newline; the server replies with
a line such as '0', '1', a number, or 'ERR:...'. This client mirrors the
old PHP client (clients/php7), adapted for TCP (Solidbits moved from UDP
to TCP + libuv).
"""

import socket


class Solidbits:
    def __init__(self, host="127.0.0.1", port=6379, timeout=3.0):
        self._host = host
        self._port = port
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._sock.settimeout(timeout)
        self._sock.connect((host, port))
        self._buf = b""

    def _recv_line(self):
        """Read one newline-terminated reply, buffering any remainder."""
        while b"\n" not in self._buf:
            chunk = self._sock.recv(256)
            if not chunk:
                return None          # connection closed by peer
            self._buf += chunk
        line, _, self._buf = self._buf.partition(b"\n")
        return line.decode().strip()

    def _roundtrip(self, line, timeout):
        self._sock.settimeout(timeout)
        self._sock.sendall((line + "\n").encode())
        return self._recv_line()

    def setbit(self, key, offset, value):
        """Set bit value (0/1) at offset of key; return the previous bit."""
        return self._roundtrip("SETBIT {} {} {}".format(key, offset, value), 3.0)

    def getbit(self, key, offset):
        """Return the bit at offset of key (0 or 1)."""
        return self._roundtrip("GETBIT {} {}".format(key, offset), 3.0)

    def bitcount(self, key, start=None, end=None):
        """Count set bits in key, optionally within [start, end] (bytes)."""
        parts = ["BITCOUNT", str(key)]
        if start is not None:
            parts.append(str(start))
        if end is not None:
            parts.append(str(end))
        return self._roundtrip(" ".join(parts), 60.0)

    def bitop(self, op, dest, *srcs):
        """Apply bitwise op (AND/OR/XOR/NOT) on srcs, store result in dest."""
        return self._roundtrip(
            "BITOP {} {} {}".format(op, dest, " ".join(srcs)), 60.0)

    def bitcop(self, op, *srcs):
        """BITCOP = BITCOUNT(BITOP): count set bits of the op result (no dest)."""
        return self._roundtrip(
            "BITOP {} \x05COUNTOP {}".format(op, " ".join(srcs)), 60.0)

    def close(self):
        try:
            self._sock.close()
        except OSError:
            pass

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()
