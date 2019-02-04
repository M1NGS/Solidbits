#include "solidbits.h"

static int idx = 0;
int (*open_file)(XXH64_hash_t hash, int mode);
void (*close_file)(struct desc_table *dt);

static int get_avalible_slot(void)
{
    int lowest = 0, id;
    uint64_t tmp = get_us();
    DLOG("into %s()", __func__);
    for (; idx < DESC_TABLE_SIZE; idx++)
    {
        if (!descs[idx].hash)
        {
            DRETURN(idx, 1);
        }
        if (tmp > descs[idx].last_access)
        {
            tmp = descs[idx].last_access;
            lowest = idx;
        }
    }

    idx = 0;
    if (DESC_HARD_LIMIT)
    {
        LOG("Description table is full, request will be reject, beacuse DESC_HARD_LIMIT=%d", DESC_HARD_LIMIT);
        DRETURN(-1, 1);
    }
    id = descs[lowest].hash % DESC_HASH_TABLE_SIZE;
    if (descs[lowest].prev == NULL)
    {
        if (descs[lowest].next == NULL)
        {
            XXTABLE[id] = NULL;
        }
        else
        {
            XXTABLE[id] = descs[lowest].next;
            descs[lowest].next->prev = NULL;
        }
    }
    else
    {
        if (descs[lowest].next == NULL)
        {
            descs[lowest].prev->next = NULL;
        }
        else
        {
            descs[lowest].prev->next = descs[lowest].next;
        }
    }
    char buf[20] = {0};
    time2string(buf, descs[lowest].last_access);
    DLOG("key %016llx will be close, last access at [%s]", descs[lowest].hash, buf);
    close_file(&descs[lowest]);
    pthread_mutex_destroy(&descs[lowest].lock);
    bzero(&descs[lowest], sizeof(struct desc_table));

    DRETURN(lowest, 1);
}

static void glibc_close(struct desc_table *dt)
{
    DLOG("Closing key %016llx", dt->hash);
    fflush(dt->fd.glibc);
    fsync(fileno(dt->fd.glibc));
    fclose(dt->fd.glibc);
}

static int glibc_open(XXH64_hash_t hash, int mode)
{
    char filename[256] = {0};
    int r;
    struct stat fi;
    DLOG("into %s()", __func__);
    if (get_avalible_slot() == -1)
    {
        DRETURN(-1, 1);
    }
    if ((r = gen_path(filename, hash)))
    {
        LOG("create dir failed - %d", r);
        DRETURN(-1, 1);
    }
    if (!stat(filename, &fi) && S_ISREG(fi.st_mode))
    {
        LOG("open a exists file [%s]", filename);
        if (mode < 2)
        {
            if ((descs[idx].fd.glibc = fopen(filename, "r+")) == NULL)
            {
                LOG("fopen(%s) failed[%d], because %s", filename, errno, strerror(errno));
                DRETURN(-1, 1);
            }
        }
        else if (mode == 2) // truncate file
        {
            if ((descs[idx].fd.glibc = fopen(filename, "w+")) == NULL)
            {
                LOG("fopen(%s) failed[%d], because %s", filename, errno, strerror(errno));
                DRETURN(-1, 1);
            }
        }
        descs[idx].length = fi.st_size;
    }
    else
    {
        if (mode == 1)
        {
            DRETURN(-2, 1); //file not exists, but do not need create it
        }
        LOG("create file [%s]", filename);
        if ((descs[idx].fd.glibc = fopen(filename, "w+")) == NULL)
        {
            LOG("fopen(%s) failed[%d], because %s", filename, errno, strerror(errno));
            DRETURN(-1, 1);
        }
        descs[idx].length = 0;
    }
    DRETURN(idx, 1);
}

int system_open(XXH64_hash_t hash, int mode)
{
    DRETURN(idx, 1);
}

static size_t glibc_write(struct desc_table *dt, void *buf, size_t size, off_t offset)
{
    size_t r;
    pthread_mutex_lock(&dt->lock);
    if (fseek(dt->fd.glibc, offset, SEEK_SET))
    {
        pthread_mutex_unlock(&dt->lock);
        LOG("Seek to %ld failed, beacuse %s", offset, strerror(errno));
        DRETURN(0, 1);
    }
    r = fwrite(buf, 1, size, dt->fd.glibc);
    if (ferror(dt->fd.glibc))
    {
        pthread_mutex_unlock(&dt->lock);
        LOG("Write maybe failed, beacuse %s", strerror(errno));
        DRETURN(0, 1);
    }
    if (ftell(dt->fd.glibc) > dt->length)
    {
        dt->length = ftell(dt->fd.glibc);
        DLOG("File expand to %lu", dt->length);
    }
    pthread_mutex_unlock(&dt->lock);
    DRETURN(r, 1);
}

static size_t glibc_read(struct desc_table *dt, void *buf, size_t size, off_t offset)
{
    size_t r;
    pthread_mutex_lock(&dt->lock);
    fseek(dt->fd.glibc, offset, SEEK_SET);
    r = fread(buf, 1, size, dt->fd.glibc);
    if (ferror(dt->fd.glibc))
    {
        pthread_mutex_unlock(&dt->lock);
        LOG("Read maybe failed, beacuse %s", strerror(errno));
        DRETURN(0, 1);
    }
    pthread_mutex_unlock(&dt->lock);
    DRETURN(r, 1);
}

int prepare_file(struct desc_table **dt, char *key, int mode)
{
    struct desc_table *tmp, *new = NULL;
    XXH64_hash_t hash;
    off_t id, depth = 0;
    int r;
    hash = XXH64(key, strlen(key), 0);
    id = hash % DESC_HASH_TABLE_SIZE;
    DLOG("into %s(point, %016llx)", __func__, hash);
    tmp = XXTABLE[id];
    while (tmp != NULL)
    {
        if (tmp->hash == hash)
        {
            DLOG("Found key %016llx at depth %d, point is %p", hash, depth, tmp);
            memcpy(dt, &tmp, sizeof(struct desc_table *));
            tmp->access_times++;
            tmp->last_access = get_us();
            DRETURN(0, 1);
        }
        else if (tmp->next != NULL)
        {
            depth++;
            tmp = tmp->next;
        }
        else
        {
            break;
        }
    }
    DLOG("Can't find key %016llx util depth %d, will create or open file.", hash, depth);
    new = &descs[(r = open_file(hash, mode))]; //terrible code, sorry
    if (r < 0)
    {
        DRETURN(r, 1);
    }
    //init some fields
    new->hash = hash;
    new->created_at = get_us();
    new->last_access = new->created_at;
    new->access_times = 1;
    pthread_mutex_init(&new->lock, NULL);
    if (depth)
    { //set link for hash
        tmp->next = new;
        new->prev = tmp;
    }
    else
        XXTABLE[id] = new;
    memcpy(dt, &new, sizeof(struct desc_table *)); //file desc of job
    DRETURN(0, 1);
}

int prepare_files(struct desc_table ***dts, char **keys, int count)
{
    int r;
    struct desc_table *tmp[count];
    while (count--)
    {
        r = prepare_file(&tmp[count], keys[count], 1);
        if (r == -2)
        {
            tmp[count] = NULL;
        }
        else if (r == -1)
        {
            DRETURN(-1, 1);
        }
    }
    memcpy(dts, &tmp, sizeof(tmp)); //file desc of job
    DRETURN(0, 1);
}

void init_file(void)
{
    descs = calloc(sizeof(struct desc_table), DESC_TABLE_SIZE);
    if (server.mode == GLIBC)
    {
        write_to = &glibc_write;
        read_from = &glibc_read;
        open_file = &glibc_open;
        close_file = &glibc_close;
    }
    else if (server.mode == DIRECT_IO)
    {
        open_file = &system_open;
    }
}