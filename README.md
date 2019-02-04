# Solidbits
a redis style bitmap database

## Introduce

Solidbits works on the udp protocol now, just a experimental product, but it work good.

it supports BITGET/BITSET/BITCOUNT/BITOP, all command works like redis except BITCOUNT's parameter. 

I recommend you running it on btrfs and turning on compression.


## Parameter options

-d  working directory path, the data will be stored here.

-D  Enable debug mode, verbosely system log, default is disbale;

-p  PID file path

-w  Number of worker, default is number of cores * 2

-l  Listen, default is 127.0.0.1:6379
    ```
    -l 127.0.0.1:123
    
    -l 192.168.0.1:123
    
    -l :123
    ```

-m  Mode, default is GLIBC
    *GLIBC* use fopen/fread to access files
    *DIRECT_IO* use system call with O_DIRECT flag to access files
    
## Commands

BITCOUNT key [start] [end]
    ```
    SETBIT mykey 95 1 //size is 12 bytes
    BITCOUNT mykey //same redis
    BITCOUNT mykey 10 // from offset 10 to 12
    BITCOUNT mykey 10 -1 // from offset 10 to 11
    BITCOUNT mykey -3 -2 // from offset 9 to 10
    ```


## Roadmap

v1.01 Add Direct_IO and optimization some modules

v1.02 Add BITCOP, means BITCOUNT(BITOP)

v1.03 Add BITGOP, means BITGET(BITOP)

v1.04 Add Cache system for BITCOP/BITGOP/GETBIT, the cache data will auto update when SETBIT.

v2.0 Rebuild everything.

BTW: If you have some feature requirements, please write an Issue let me know.



***WARNING: The current version without Direct_IO mode and synchronize on exit when working at GLIBC mode.***
