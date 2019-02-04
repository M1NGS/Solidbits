#include "solidbits.h"

void init_workers(void)
{
    long i;
    DLOG("into %s",__func__);
    workers = calloc(sizeof(struct worker_table), server.worker);
    init_job();
    for (i=0; i  < server.worker; i++)
    {
        
        if (pthread_create(&workers[i].id, NULL, &do_job, (void*)i))
        {
            LOG("Create work failed: %s", strerror(errno));
            LOG("Server terminal...");
            exit(errno);
        }
        
        DLOG("Worker thread #%d created", i);
    }

}