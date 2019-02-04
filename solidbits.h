#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <malloc.h>
#include <syslog.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/sysinfo.h>
#include <arpa/inet.h>
#include <libgen.h>
#include <syscall.h>
#include "config.h"
#ifdef HAVE_LIBJEMALLOC
#include <jemalloc/jemalloc.h>
#endif
#include "xxhash.h"

#define FILE_SIZE_LIMIT 2147483647 //2G - 1
#define FILE_BUFFER_SIZE 1146880
#define DESC_TABLE_SIZE 8192
#define DESC_HARD_LIMIT 0 //if set 1, when desc table full, no one will be close.
#define DESC_HASH_TABLE_SIZE 65537
#define FILE_NAME_LENGTH 65 //sha256 hash and \n
#define SYSLOG_NAME "Solidbits"
#define JOB_QUEUE_NUM 256
#define PARSER_QUEUE_NUM 16
#define WORKER_MINIMUM 1
#define ARG_SINGLE_PART 65
#define ARG_PART_MAXIMUM 17 //not include command,just args
#define WORKER_MAXIMUM 256
#define CMD_MININUM 10 //like GETBIT A 1
#define INIT_LOG() openlog(SYSLOG_NAME, LOG_PID|LOG_CONS, LOG_DEBUG);
#define LOG(msg,...) syslog(LOG_DEBUG,msg,##__VA_ARGS__);
#define UDP_DATA_SIZE 1464


enum OP_OPTIONS
{
    AND,
    OR,
    XOR,
    NOT
};
enum FIELD_OPTIONS
{
    GET,
    SET,
    INCRBY,
    OVERFLOW    
};
enum COMMANDS
{
    SETBIT,
    GETBIT,
    BITCOUNT,
    BITOP,
    BITFIELD,
    BITPOS,
    BITCOP,
    BITGOP
};

struct setbit_cmd
{
    char key[65];
    long offset;
    long value;
};

struct getbit_cmd
{
    char key[65];
    long offset;
};

struct bitcount_cmd
{
    char key[65];
    long start;
    long end;
};

struct bitop_cmd
{
    char key[65];
    long count;
    enum OP_OPTIONS option;
};

enum MODE
{
    GLIBC,
    DIRECT_IO,
    AUTO,  // Future feature
    RAW    // Future feature
};

struct desc_table
{
    XXH64_hash_t hash;
    union {
        FILE *glibc;
        int   direct_io;
    } fd;
    size_t length;
    uint64_t last_access; 
    uint64_t access_times;
    uint64_t created_at; //open or create in current life cycle.
    pthread_mutex_t lock;
    struct desc_table *next;
    struct desc_table *prev;
} *descs;

struct desc_table *XXTABLE[DESC_HASH_TABLE_SIZE];  //create in stack for speed

struct worker_table
{
    pthread_t id;
    int   tid;
    size_t j_count; //how many job done.
} *workers;

struct
 {
    enum MODE mode;
    struct sockaddr_in socket;
    int s_size;
    char *pid;
    char *dir;
    int worker;
    int debug:1;
    int fd;
    char buf[UDP_DATA_SIZE];

} server;


struct job
{
    enum COMMANDS name;
    union {
        struct setbit_cmd setbit;
        struct getbit_cmd getbit;
        struct bitcount_cmd bitcount;
        struct bitop_cmd    bitop;
    } cmd;
    struct sockaddr_in client;
    struct desc_table *desc[ARG_PART_MAXIMUM];
    uint64_t created_at;
};


struct
{
    struct job *jobs;
    int front;
    int rear;
    int size;
}  job_queue;

struct parser_queue
{
    char buf[UDP_DATA_SIZE];
    size_t size;
    struct sockaddr_in client;
    uint64_t created_at;
} *parsers;


char *strupr(char *str);
int set_addr_port(char *str);
void daemonize(void);
int check_dir(char *path);
int check_file(char *path);
void safe_exit(int signum);
void init_workers(void);
void *do_job(void * id);
uint64_t get_us();
void insert_parser_job(char *buf, size_t size, struct sockaddr_in * client);
void init_parser(void);
int prepare_file(struct desc_table **dt, char* key, int mode);
int prepare_files(struct desc_table ***dts, char **keys, int count);
int gen_path(char *path, XXH64_hash_t hash);
void init_job(void);
int push_job(struct job *j);
int setbitCommand(struct setbit_cmd *cmd, struct desc_table *dt);
int getbitCommand(struct getbit_cmd *cmd, struct desc_table *dt);
int bitcountCommand(struct bitcount_cmd *cmd, struct desc_table *dt);
int bitopCommand(struct bitop_cmd *cmd, struct desc_table **dts);
size_t (*write_to) (struct desc_table *dt, void *buf, size_t size, off_t offset);
size_t (*read_from) (struct desc_table *dt, void *buf, size_t size, off_t offset);
void init_file(void);
char *time2string(char* buf, uint64_t us);

#define DLOG(msg, ...)                                                            \
    if (server.debug)                                                             \
    {                                                                             \
        char *headbuf = malloc(2048);                                                \
        snprintf(headbuf, 2048, "[%s@%s:%d] %s", __func__, __FILE__, __LINE__, msg); \
        syslog(LOG_DEBUG, headbuf, ##__VA_ARGS__);                                   \
        free(headbuf);                                                               \
    }

#define DRETURN(x, y)         \
    if (y == 1)               \
    {                         \
        DLOG("return %d", x); \
    }                         \
    else if (y == 2)          \
    {                         \
        DLOG("return %s", x); \
    }                         \
    else if (y == 3)        \
    {                         \
        DLOG("return %p", x); \
    }                         \
    return x;

#define reply(x,y,z) sendto(server.fd, x, y, 0, (struct sockaddr *)z, sizeof(struct sockaddr_in));
