# Solidbits
A redis-style bitmap database that works on the filesystem.

## Introduction

Solidbits is an experimental, file-backed bitmap database. It speaks a simple
line protocol over **TCP** and supports `SETBIT` / `GETBIT` / `BITCOUNT` /
`BITOP` / `BITCOP`, behaving like the Redis bit commands (`BITCOUNT`'s range
arguments are byte offsets, same as Redis).

File storage and the hash table rely on xxhash64; 20 million random key names
were tested with zero collisions, so xxhash64 is considered safe here.

Running it on btrfs with compression is recommended.

## Build

```
./configure
make
```

Requires **libuv >= 1.0**, looked up via pkg-config. Install `libuv1-dev`
(Debian/Ubuntu) or `libuv-devel` (RHEL/Fedora). For a custom install prefix:

```
./configure --with-libuv=/path/to/prefix
```

## Options

```
-d  working directory (data is stored here; required)
-D  enable debug mode (verbose syslog); default off
-p  PID file path
-w  worker thread-pool size; default = cores * 2
-l  listen address; default 127.0.0.1:6379
      -l 127.0.0.1:6380
      -l 192.168.0.1:6380
      -l :6380
```

Run:

```
./solidbits -d /var/lib/solidbits -p /var/run/solidbits.pid
```

## Commands

Requests are text lines terminated by `\n`, arguments separated by spaces.
Replies are `0`, `1`, a number, or `ERR:...`.

### SETBIT / GETBIT

```
SETBIT key offset value     # value is 0 or 1; returns the old bit
GETBIT key offset           # returns 0 or 1
```

### BITCOUNT

```
BITCOUNT key [start end]    # count set bits; start/end are byte offsets,
                            # negative offsets count from the end (-1 = last byte)
```

```
SETBIT mykey 95 1       # creates the key, file grows to 12 bytes
BITCOUNT mykey          # whole key
BITCOUNT mykey 10       # bytes 10..end
BITCOUNT mykey 10 -1    # bytes 10..last
BITCOUNT mykey -3 -2    # bytes 9..10
```

### BITOP

```
BITOP AND|OR|XOR dest src [src ...]    # store op result into dest
BITOP NOT dest src                     # dest = ~src
```

### BITCOP  (BITCOUNT of BITOP)

Count set bits of a BITOP result without storing it. The dest slot carries a
`\x05COUNTOP` marker (Ctrl-E + `COUNTOP`):

```
BITOP AND|OR|XOR \x05COUNTOP src [src ...]
```

## Python client

A minimal client lives in `clients/python/`:

```python
from solidbits import Solidbits

with Solidbits("127.0.0.1", 6379) as sb:
    sb.setbit("foo", 95, 1)
    print(sb.getbit("foo", 95))        # 1
    print(sb.bitcount("foo"))           # 1
    sb.bitop("AND", "c", "a", "b")
    print(sb.bitcop("AND", "a", "b"))   # count bits of (a AND b)
```

Run the bundled example:

```
cd clients/python
python3 example.py
```

## Notes

- Single backend: standard buffered file I/O (the former Direct I/O mode was removed).
- Requests on one connection are processed in order (ordered pipelining);
  different connections run in parallel via the libuv thread pool.
