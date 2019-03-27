#include "solidbits.h"

static pthread_cond_t job_ready;
static pthread_mutex_t job_mutex;

void init_job(void)
{
    DLOG("into %s", __func__);
    job_queue.front = 0;
    job_queue.rear = 0;
    job_queue.size = 0;
    job_queue.jobs = calloc(sizeof(struct job), JOB_QUEUE_NUM);
    pthread_mutex_init(&job_mutex, NULL);
    pthread_cond_init(&job_ready, NULL);
}

int push_job(struct job *j)
{
    DLOG("into %s", __func__);
    pthread_mutex_lock(&job_mutex);
    if (job_queue.size == JOB_QUEUE_NUM)
    {
        reply("ERR:JOB QUEUE FULL\n", 19, &j->client);
        pthread_mutex_unlock(&job_mutex);
        DRETURN(-1, 1);
    }
    memcpy(job_queue.jobs + job_queue.rear, j, sizeof(struct job));
    job_queue.size++;
    job_queue.rear++;
    job_queue.rear %= JOB_QUEUE_NUM;

    pthread_mutex_unlock(&job_mutex);
    pthread_cond_broadcast(&job_ready);

    DRETURN(0, 1);
}

static int pop_job(struct job *j)
{
    DLOG("into %s", __func__);
    if (job_queue.size == 0)
    {
        pthread_mutex_unlock(&job_mutex);
        DRETURN(-1, 1);
    }
    memcpy(j, job_queue.jobs + job_queue.front, sizeof(struct job));
    job_queue.size--;
    job_queue.front++;
    job_queue.front %= JOB_QUEUE_NUM;
    pthread_mutex_unlock(&job_mutex);
    DRETURN(0, 1);
}

void *do_job(void *id)
{
    struct job tmp;
    int w_id = (long)id;
    long r;
    char buf[16];
    workers[w_id].tid = syscall(SYS_gettid);
    pthread_mutex_lock(&job_mutex); // for first pop
    while (1)
    {
        if (pop_job(&tmp))
        {
            //DLOG("Thread #%d: No job, waitting...", workers[w_id].tid);
            pthread_mutex_lock(&job_mutex);
            pthread_cond_wait(&job_ready, &job_mutex);
            //DLOG("Thread #%d revive", w_id);
        }
        else
        {
            DLOG("Thread #%d got job", w_id);
            switch(tmp.name)
            {
                case SETBIT:
                    r = setbitCommand(&tmp.cmd.setbit, tmp.desc[0]);
                    if (r < 0)
                    {
                        reply("ERR:FILE ACCESS FAILED\n", 23, &tmp.client);
                    }
                    else
                    {
                        reply(buf, snprintf(buf, 16, "%ld\n", r), &tmp.client);
                    }
                    break;
                case GETBIT:
                    r = getbitCommand(&tmp.cmd.getbit, tmp.desc[0]);
                    if (r < 0)
                    {
                        reply("ERR:FILE ACCESS FAILED\n", 23, &tmp.client);
                    }
                    else
                    {
                        reply(buf, snprintf(buf, 16, "%ld\n", r), &tmp.client);
                    }
                    break;
                case BITCOUNT:
                    r = bitcountCommand(&tmp.cmd.bitcount, tmp.desc[0]);
                    if (r < 0)
                    {
                        reply("ERR:FILE ACCESS FAILED\n", 23, &tmp.client);
                    }
                    else
                    {
                        reply(buf, snprintf(buf, 16, "%ld\n", r), &tmp.client);
                    }
                    break;

                case BITOP:
                    r = bitopCommand(&tmp.cmd.bitop, &tmp.desc[1]);
                    if (r < 0)
                    {
                        reply("ERR:FILE ACCESS FAILED\n", 23, &tmp.client);
                    }
                    else
                    {
                        reply(buf, snprintf(buf, 16, "%ld\n", r), &tmp.client);
                    }
                    break;
                default:
                    DLOG("Something was wrong %d", tmp.name);
            }
            DLOG("Job done, it took %ldus", get_us() - tmp.created_at);
        }
    }
}
//int get
