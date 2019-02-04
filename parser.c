#include "solidbits.h"

static pthread_t parser;
static int idx = 0;
static struct job tjob;
static pthread_cond_t parser_job_ready;
static pthread_mutex_t parser_mutex;

static int get_arg_count(char *str, size_t len)
{
    off_t i, l = 0, c = 0;
    len--; //without \n
    DLOG("into %s(%s, %lu)", __func__, str, len);
    for (i = 0; i < len; i++)
    {
        if (str[i] == 32) //space
        {
            l = i + 1; // the last space offset
            if (i > 0 && str[i - 1] != 32)
            {
                c++;
            }
        }
    }
    if (i > l) //for last arg
        c++;
    DRETURN(c, 1);
}

static off_t next_space_at(char *str, size_t len)
{
    off_t i;
    DLOG("into %s(%s, %lu)", __func__, str, len);
    for (i = 0; i < len; i++)
    {
        if (str[i] == 32) //space
        {
            DRETURN(i, 1);
        }
    }
    DRETURN(-1, 1);
}

static void free_args(char **args, int count)
{
    while (count--)
    {
        free(args[count]);
    }
    free(args);
}

static char **split_fill(char *str, size_t len, int count)
{
    int i, o;
    DLOG("into %s(%s, %lu, %d)", __func__, str, len, count);
    o = next_space_at(str, len); //skip cmd name
    str += o + 1;
    len -= o + 1;
    len--; //without \n
    count--;
    char **tmp = calloc(sizeof(char *), count);
    for (i = 0; i < count; i++)
    {
        o = next_space_at(str, len);
        if (o == -1)
            o = len;
        tmp[i] = calloc(sizeof(char), o + 1);
        memcpy(tmp[i], str, o);
        str += o + 1;
        len -= o + 1;
        DLOG("Get #%d = [%s]", i, tmp[i]);
    }
    DRETURN(tmp, 3);
}

static int parse_arg(int id)
{
    int c, r;
    long t;
    char **args;
    XXH64_hash_t hash;
    DLOG("into %s()", __func__);
    if (parsers[id].size < CMD_MININUM)
    {
        reply("ERR:CMD TOO SHORT\n", 18, &parsers[id].client);
        DLOG("Command too short: [%s]", parsers[id].buf);
        DRETURN(-1, 1);
    }
    c = get_arg_count(parsers[id].buf, parsers[id].size);
    hash = XXH64(parsers[id].buf, 5, 0);
    bzero(&tjob, sizeof(struct job)); //clear for cache hash
    switch (hash)
    {
    case 13964470052823109277ULL:
        if (c != 4)
        {
            reply("ERR:ARG TOO MANY OR FEW\n", 24, &parsers[id].client);
            DLOG("Argument error: [%s]", parsers[id].buf);
            DRETURN(-1, 1);
        }
        args = split_fill(parsers[id].buf, parsers[id].size, c);
        tjob.cmd.setbit.offset = atoll(args[1]);
        t = tjob.cmd.setbit.offset >> 3;
        if (t < 0 && t > FILE_SIZE_LIMIT)
        {
            reply("ERR:OFFSET OUT OF RANGE\n", 24, &parsers[id].client);
            free_args(args, c - 1);
            DLOG("Offset out of range: [%s]", args[1]);
            DRETURN(-1, 1);
        }
        tjob.cmd.setbit.value = atol(args[2]);
        if (tjob.cmd.setbit.value & ~1)
        {
            reply("ERR:SETBIT(3) MUST BE 1 OR 0\n", 29, &parsers[id].client);
            free_args(args, c - 1);
            DLOG("setbit(3) mast be 1 or 0", args[1]);
            DRETURN(-1, 1);
        }
        strcpy(tjob.cmd.setbit.key, args[0]);
        if (prepare_file(&tjob.desc[0], tjob.cmd.setbit.key, 0))
        {
            reply("ERR:FILESYSTEM FAILED\n", 22, &parsers[id].client);
            free_args(args, c - 1);
            DLOG("Filesystem failed: [%s]", args[0]);
            DRETURN(-1, 1);
        }
        tjob.name = SETBIT;
        free_args(args, c - 1);
        break;
    case 4534844053247176213ULL:
        if (c != 3)
        {
            reply("ERR:ARGS TOO MANY OR FEW\n", 25, &parsers[id].client);
            DLOG("Argument error: [%s]", parsers[id].buf);
            DRETURN(-1, 1);
        }
        args = split_fill(parsers[id].buf, parsers[id].size, c);
        tjob.cmd.getbit.offset = atol(args[1]);
        t = tjob.cmd.getbit.offset >> 3;
        if (t < 0 && t > FILE_SIZE_LIMIT)
        {
            reply("ERR:OFFSET OUT OF RANGE\n", 24, &parsers[id].client);
            free_args(args, c - 1);
            DLOG("Offset out of range: [%s]", args[1]);
            DRETURN(-1, 1);
        }
        strcpy(tjob.cmd.getbit.key, args[0]);
        r = prepare_file(&tjob.desc[0], tjob.cmd.getbit.key, 1);
        if (r == -2)
        {
            reply("0\n", 2, &parsers[id].client);
            DLOG("File not exists: [%s]", args[0]);
            free_args(args, c - 1);
            DRETURN(-2, 1);
        }
        else if (r == -1)
        {
            reply("ERR:FILESYSTEM FAILED\n", 22, &parsers[id].client);
            free_args(args, c - 1);
            DLOG("Filesystem failed: [%s]", args[0]);
            DRETURN(-1, 1);
        }
        tjob.name = GETBIT;
        free_args(args, c - 1);
        break;
    case 2226304069708374537ULL:
        if (c < 4 || c > 19)
        {
            reply("ERR:ARGS TOO MANY OR FEW\n", 25, &parsers[id].client);
            DLOG("Argument error: [%s]", parsers[id].buf);
            DRETURN(-1, 1);
        }
        args = split_fill(parsers[id].buf, parsers[id].size, c);
        if ((args[0][0] == 'a' || args[0][0] == 'A') && !strcasecmp(args[0], "and"))
            tjob.cmd.bitop.option = AND;
        else if ((args[0][0] == 'o' || args[0][0] == 'O') && !strcasecmp(args[0], "or"))
            tjob.cmd.bitop.option = OR;
        else if ((args[0][0] == 'x' || args[0][0] == 'X') && !strcasecmp(args[0], "xor"))
            tjob.cmd.bitop.option = XOR;
        else if ((args[0][0] == 'n' || args[0][0] == 'N') && !strcasecmp(args[0], "not"))
            tjob.cmd.bitop.option = NOT;
        else
        {
            reply("ERR:UKNOW OPTION\n", 17, &parsers[id].client);
            DLOG("Argument error: [%s]", parsers[id].buf);
            free_args(args, c - 1);
            DRETURN(-1, 1);
        }
        if (tjob.cmd.bitop.option == NOT && c != 4)
        {
            reply("ERR:BITOP NOT MUST NEEDS ONE SOURCE\n", 36, &parsers[id].client);
            DLOG("Argument error: [%s]", parsers[id].buf);
            free_args(args, c - 1);
            DRETURN(-1, 1);
        }
        strcpy(tjob.cmd.bitop.key, args[1]);
        if (prepare_file(&tjob.desc[0], tjob.cmd.bitop.key, 2))
        {
            reply("ERR:FILESYSTEM FAILED\n", 22, &parsers[id].client);
            free_args(args, c - 1);
            DLOG("Filesystem failed: [%s]", args[0]);
            DRETURN(-1, 1);
        }
        tjob.cmd.bitop.count = c - 3;
        if (prepare_files((struct desc_table ***)&tjob.desc[1], &args[2], tjob.cmd.bitop.count))
        {
            reply("ERR:FILESYSTEM FAILED\n", 22, &parsers[id].client);
            free_args(args, c - 1);
            DLOG("Filesystem failed: [%s]", args[0]);
            DRETURN(-1, 1);
        }
        tjob.name = BITOP;
        free_args(args, c - 1);
        break;
    case 9951046379166068647ULL:
        if (c < 2 || c > 4)
        {
            reply("ERR:ARGS TOO MANY OR FEW\n", 25, &parsers[id].client);
            DLOG("Argument error: [%s]", parsers[id].buf);
            DRETURN(-1, 1);
        }
        args = split_fill(parsers[id].buf, parsers[id].size, c);

        strcpy(tjob.cmd.bitcount.key, args[0]);
        r = prepare_file(&tjob.desc[0], tjob.cmd.getbit.key, 1);
        if (r == -2)
        {
            reply("0\n", 2, &parsers[id].client);
            DLOG("File not exists: [%s]", args[0]);
            free_args(args, c - 1);
            DRETURN(-2, 1);
        }
        else if (r == -1)
        {
            reply("ERR:FILESYSTEM FAILED\n", 22, &parsers[id].client);
            DLOG("Filesystem failed: [%s]", args[0]);
            free_args(args, c - 1);
            DRETURN(-1, 1);
        }
        if (c == 2)
        {
            tjob.cmd.bitcount.start = 0;
            tjob.cmd.bitcount.end = tjob.desc[0]->length - 1;
        }
        else if (c == 3)
        {
            tjob.cmd.bitcount.start = atol(args[1]);
            tjob.cmd.bitcount.end = tjob.desc[0]->length - 1;
        }
        else if (c == 4)
        {
            tjob.cmd.bitcount.start = atol(args[1]);
            tjob.cmd.bitcount.end = atol(args[2]);
        }
        tjob.name = BITCOUNT;
        free_args(args, c - 1);
        break;
    case 16475154490487774448ULL:
        if (c < 3 && c > 5)
        {
            reply("ERR:ARGS TOO MANY OR FEW\n", 25, &parsers[id].client);
            DLOG("Argument error: [%s]", parsers[id].buf);
            DRETURN(-1, 1);
        }
        tjob.name = BITPOS;
        args = split_fill(parsers[id].buf, parsers[id].size, c);
        free_args(args, c - 1);
        break;
    case 16283480983475320022ULL:
        if (c < 5)
        {
            reply("ERR:ARGS TOO FEW\n", 17, &parsers[id].client);
            DLOG("Argument error: [%s]", parsers[id].buf);
            DRETURN(-1, 1);
        }
        tjob.name = BITFIELD;
        break;
    default:
        reply("ERR:UNKNOW CMD\n", 15, &parsers[id].client);
        DLOG("Unknow command: [%s]", parsers[id].buf);
        DRETURN(-1, 1);
    }
    tjob.created_at = parsers[id].created_at;
    tjob.client = parsers[id].client;
    DRETURN(0, 1);
}

void insert_parser_job(char *buf, size_t size, struct sockaddr_in *client)
{
    int i;
    DLOG("into %s()", __func__);
    for (i = 0; i < PARSER_QUEUE_NUM; i++)
    {
        if (parsers[i].size == 0)
        {
            memcpy(parsers[i].buf, buf, size);
            parsers[i].buf[size] = '\0';
            parsers[i].size = size;
            parsers[i].created_at = get_us();
            memcpy(&parsers[i].client, client, sizeof(struct sockaddr_in));
            DLOG("Accept job #%d request from %s:%d: [%d]%s", i, inet_ntoa(parsers[i].client.sin_addr),
                 ntohs(parsers[i].client.sin_port), parsers[i].size, parsers[i].buf);
            pthread_cond_signal(&parser_job_ready); //send signal
            return;
        }
    }
    reply("ERR:QUEUE FULL\n", 15, &parsers[i].client);
    DLOG("Parser queue is full, the request from %s:%d failed", inet_ntoa(parsers[i].client.sin_addr),
         ntohs(parsers[i].client.sin_port));
}

static int get_parser_job(void)
{
    DLOG("into %s()", __func__);
    for (; idx < PARSER_QUEUE_NUM; idx++)
    {
        if (parsers[idx].size)
        {
            DLOG("Get job #%d", idx);
            DRETURN(idx, 1);
        }
    }
    idx = 0;
    DRETURN(-1, 1);
}

static void del_parser_job(int id)
{
    DLOG("into %s(%d)", __func__, id);
    parsers[id].size = 0;
    DLOG("Job #%d deleted, it took %ldus", id, get_us() - parsers[id].created_at);
}

static void *process_request(void *arg)
{
    int id;
    while (1)
    {
        id = get_parser_job();
        if (id >= 0)
        {
            if (!parse_arg(id))
            {
                push_job(&tjob);
            }
            del_parser_job(id);
        }
        else
        {
            DLOG("No job, waitting...", id);
            pthread_cond_wait(&parser_job_ready, &parser_mutex);
        }
    }

    pthread_exit(NULL);
}

void init_parser(void)
{
    DLOG("into %s", __func__);
    pthread_mutex_init(&parser_mutex, NULL);
    pthread_cond_init(&parser_job_ready, NULL);
    parsers = calloc(sizeof(struct parser_queue), PARSER_QUEUE_NUM);
    if (pthread_create(&parser, NULL, &process_request, NULL))
    {
        LOG("Create work failed: %s", strerror(errno));
        LOG("Server terminal...");
        exit(errno);
    }
    DLOG("Parser thread[%ld] created", parser);
}