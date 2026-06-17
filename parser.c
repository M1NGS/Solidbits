#include "net.h"
#include <stdlib.h>

static int get_arg_count(char *str, size_t len)
{
    off_t i, l = 0, c = 0;
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
    {
        c++;
    }
    DRETURN(c, 1);
}

static void free_args(char **args, int count)
{
    while (count--)
    {
        free(args[count]);
    }
    free(args);
}

/* split_fill: skip the command name, then split the rest into (count-1)
 * space-separated args. Consecutive spaces are treated as ONE separator
 * (aligned with get_arg_count). Input line has no trailing '\n' (framed
 * by the net layer). Cursor-based walk, no size_t underflow. */
static char **split_fill(char *str, size_t len, int count)
{
    int i;
    size_t pos = 0, start;
    char **tmp;
    DLOG("into %s(%s, %lu, %d)", __func__, str, len, count);
    while (pos < len && str[pos] == 32) pos++;   /* leading spaces (defensive) */
    while (pos < len && str[pos] != 32) pos++;   /* command name */
    count--;
    tmp = calloc(sizeof(char *), count);
    for (i = 0; i < count; i++)
    {
        while (pos < len && str[pos] == 32) pos++;   /* skip ALL spaces between args */
        start = pos;
        while (pos < len && str[pos] != 32) pos++;   /* one arg */
        tmp[i] = calloc(sizeof(char), pos - start + 1);
        memcpy(tmp[i], str + start, pos - start);
        DLOG("Get #%d = [%s]", i, tmp[i]);
    }
    DRETURN(tmp, 3);
}

static void release_all_descs(struct job *job)
{
    int i;
    for (i = 0; i < ARG_PART_MAXIMUM; i++)
    {
        if (job->desc[i])
        {
            release_desc(job->desc[i]);
            job->desc[i] = NULL;
        }
    }
}

static int copy_key(char *dst, const char *src)
{
    if (strlen(src) > 64)
    {
        return -1;
    }
    strcpy(dst, src);
    return 0;
}

void parse_and_execute(struct work_req *w)
{
    int c, r;
    long t, result;
    char **args;
    XXH64_hash_t hash;
    char buf[16];
    struct job *job = &w->job;

    if (w->line_len < CMD_MININUM)
    {
        reply("ERR:CMD TOO SHORT\n", 18);
        return;
    }
    c = get_arg_count(w->line, w->line_len);
    hash = XXH64(w->line, 5, 0);
    bzero(job, sizeof(struct job));
    switch (hash)
    {
    case 13964470052823109277ULL: /* SETBIT */
        if (c != 4)
        {
            reply("ERR:ARG TOO MANY OR FEW\n", 24);
            return;
        }
        args = split_fill(w->line, w->line_len, c);
        job->cmd.setbit.offset = atoll(args[1]);
        t = job->cmd.setbit.offset >> 3;
        if (t < 0 || t > FILE_SIZE_LIMIT)
        {
            reply("ERR:OFFSET OUT OF RANGE\n", 24);
            free_args(args, c - 1);
            return;
        }
        job->cmd.setbit.value = atol(args[2]);
        if (job->cmd.setbit.value & ~1)
        {
            reply("ERR:SETBIT(3) MUST BE 1 OR 0\n", 29);
            free_args(args, c - 1);
            return;
        }
        if (copy_key(job->cmd.setbit.key, args[0]))
        {
            reply("ERR:KEY TOO LONG\n", 17);
            free_args(args, c - 1);
            return;
        }
        free_args(args, c - 1);
        if (prepare_file(&job->desc[0], job->cmd.setbit.key, 0))
        {
            reply("ERR:FILESYSTEM FAILED\n", 22);
            release_all_descs(job);
            return;
        }
        result = setbitCommand(&job->cmd.setbit, job->desc[0]);
        if (result < 0)
        {
            reply("ERR:FILE ACCESS FAILED\n", 23);
        }
        else
        {
            reply(buf, snprintf(buf, 16, "%ld\n", result));
        }
        release_all_descs(job);
        return;

    case 4534844053247176213ULL: /* GETBIT */
        if (c != 3)
        {
            reply("ERR:ARGS TOO MANY OR FEW\n", 25);
            return;
        }
        args = split_fill(w->line, w->line_len, c);
        job->cmd.getbit.offset = atol(args[1]);
        t = job->cmd.getbit.offset >> 3;
        if (t < 0 || t > FILE_SIZE_LIMIT)
        {
            reply("ERR:OFFSET OUT OF RANGE\n", 24);
            free_args(args, c - 1);
            return;
        }
        if (copy_key(job->cmd.getbit.key, args[0]))
        {
            reply("ERR:KEY TOO LONG\n", 17);
            free_args(args, c - 1);
            return;
        }
        free_args(args, c - 1);
        r = prepare_file(&job->desc[0], job->cmd.getbit.key, 1);
        if (r == -2)
        {
            reply("0\n", 2);
            return;
        }
        if (r == -1)
        {
            reply("ERR:FILESYSTEM FAILED\n", 22);
            release_all_descs(job);
            return;
        }
        result = getbitCommand(&job->cmd.getbit, job->desc[0]);
        if (result < 0)
        {
            reply("ERR:FILE ACCESS FAILED\n", 23);
        }
        else
        {
            reply(buf, snprintf(buf, 16, "%ld\n", result));
        }
        release_all_descs(job);
        return;

    case 2226304069708374537ULL: /* BITOP */
        if (c < 4 || c > 19)
        {
            reply("ERR:ARGS TOO MANY OR FEW\n", 25);
            return;
        }
        args = split_fill(w->line, w->line_len, c);
        if ((args[0][0] == 'a' || args[0][0] == 'A') && !strcasecmp(args[0], "and"))
        {
            job->cmd.bitop.option = AND;
        }
        else if ((args[0][0] == 'o' || args[0][0] == 'O') && !strcasecmp(args[0], "or"))
        {
            job->cmd.bitop.option = OR;
        }
        else if ((args[0][0] == 'x' || args[0][0] == 'X') && !strcasecmp(args[0], "xor"))
        {
            job->cmd.bitop.option = XOR;
        }
        else if ((args[0][0] == 'n' || args[0][0] == 'N') && !strcasecmp(args[0], "not"))
        {
            job->cmd.bitop.option = NOT;
        }
        else
        {
            reply("ERR:UKNOW OPTION\n", 17);
            free_args(args, c - 1);
            return;
        }
        if (job->cmd.bitop.option == NOT && c != 4)
        {
            reply("ERR:BITOP NOT JUST NEEDS ONE SOURCE\n", 36);
            free_args(args, c - 1);
            return;
        }
        if (*args[1] == 5) /* Ctrl+E means countop/getop hook */
        {
            if (!strcasecmp(args[1] + 1, "COUNTOP"))
            {
                job->cmd.bitop.hook = COUNTOP;
            }
            else if (!strcasecmp(args[1] + 1, "GETOP"))
            {
                job->cmd.bitop.hook = GETOP;
            }
            else
            {
                reply("ERR:UKNOW OPTION\n", 17);
                free_args(args, c - 1);
                return;
            }
        }
        else
        {
            job->cmd.bitop.hook = NONE;
            if (copy_key(job->cmd.bitop.key, args[1]))
            {
                reply("ERR:KEY TOO LONG\n", 17);
                free_args(args, c - 1);
                return;
            }
            if (prepare_file(&job->desc[0], job->cmd.bitop.key, 2))
            {
                reply("ERR:FILESYSTEM FAILED\n", 22);
                free_args(args, c - 1);
                release_all_descs(job);
                return;
            }
        }
        job->cmd.bitop.count = c - 3;
        if (prepare_files((struct desc_table ***)&job->desc[1], &args[2], job->cmd.bitop.count))
        {
            reply("ERR:FILESYSTEM FAILED\n", 22);
            free_args(args, c - 1);
            release_all_descs(job);
            return;
        }
        free_args(args, c - 1);
        result = bitopCommand(&job->cmd.bitop, job->desc[0], &job->desc[1]);
        if (result < 0)
        {
            reply("ERR:FILE ACCESS FAILED\n", 23);
        }
        else
        {
            reply(buf, snprintf(buf, 16, "%ld\n", result));
        }
        release_all_descs(job);
        return;

    case 9951046379166068647ULL: /* BITCOUNT */
        if (c < 2 || c > 4)
        {
            reply("ERR:ARGS TOO MANY OR FEW\n", 25);
            return;
        }
        args = split_fill(w->line, w->line_len, c);
        if (copy_key(job->cmd.bitcount.key, args[0]))
        {
            reply("ERR:KEY TOO LONG\n", 17);
            free_args(args, c - 1);
            return;
        }
        r = prepare_file(&job->desc[0], job->cmd.bitcount.key, 1);
        if (r == -2)
        {
            reply("0\n", 2);
            free_args(args, c - 1);
            return;
        }
        if (r == -1)
        {
            reply("ERR:FILESYSTEM FAILED\n", 22);
            free_args(args, c - 1);
            release_all_descs(job);
            return;
        }
        if (job->desc[0]->length == 0)
        {
            reply("0\n", 2);
            free_args(args, c - 1);
            release_all_descs(job);
            return;
        }
        if (c == 2)
        {
            job->cmd.bitcount.start = 0;
            job->cmd.bitcount.end = job->desc[0]->length - 1;
        }
        else if (c == 3)
        {
            job->cmd.bitcount.start = atol(args[1]);
            job->cmd.bitcount.end = job->desc[0]->length - 1;
        }
        else if (c == 4)
        {
            job->cmd.bitcount.start = atol(args[1]);
            job->cmd.bitcount.end = atol(args[2]);
        }
        free_args(args, c - 1);
        result = bitcountCommand(&job->cmd.bitcount, job->desc[0]);
        if (result < 0)
        {
            reply("ERR:FILE ACCESS FAILED\n", 23);
        }
        else
        {
            reply(buf, snprintf(buf, 16, "%ld\n", result));
        }
        release_all_descs(job);
        return;

    case 16475154490487774448ULL: /* BITPOS - not implemented, preserves existing behavior */
        if (c < 3 && c > 5)
        {
            reply("ERR:ARGS TOO MANY OR FEW\n", 25);
            return;
        }
        args = split_fill(w->line, w->line_len, c);
        free_args(args, c - 1);
        return;

    case 16283480983475320022ULL: /* BITFIELD - not implemented, preserves existing behavior */
        if (c < 5)
        {
            reply("ERR:ARGS TOO FEW\n", 17);
            return;
        }
        return;

    default:
        reply("ERR:UNKNOW CMD\n", 15);
        return;
    }
}
