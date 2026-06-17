#include "net.h"
#include <stdlib.h>

struct server_t server;

static uv_loop_t loop;
static uv_signal_t sigterm;

static void on_sigterm(uv_signal_t *h, int signum)
{
    (void)signum;
    LOG("[QUIT]recv SIGTERM, shutting down");
    close_files();
    uv_stop(h->loop);
}

int main(int argc, char const *argv[])
{
    int arg;
    char pool_sz[16];
    opterr = 0;
    bzero(&server.socket, sizeof(server.socket));
    server.socket.sin_family = PF_INET;
    server.socket.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.socket.sin_port = htons(6379); //listen on 127.0.0.1:6379 default
    server.worker = get_nprocs() * 2;
    INIT_LOG();
    LOG("System preparing for start");
    while ((arg = getopt(argc, (char **)argv, "l:d:Dp:w:")) != -1)
    {
        switch (arg)
        {
        case 'l':
            if (set_addr_port(optarg))
            {
                printf("The value %s of argument -%c invalid!\n\n", optarg, arg);
                return -1;
            }
            LOG("[SET]Server listen on %s:%d", inet_ntoa(server.socket.sin_addr), ntohs(server.socket.sin_port));
            break;
        case 'D':
            server.debug = 1;
            LOG("[SET]Enable debug mode");
            break;
        case 'p':
            server.pid = malloc(strlen(optarg) + 1);
            strcpy(server.pid, optarg);
            if (check_file(optarg) < 0)
            {
                printf("The file not writable or directory not writable\n\n");
                return -1;
            }
            LOG("[SET]PID path: %s", server.pid);
            break;
        case 'w':
            server.worker = atoi(optarg); //how many thread will be create.
            if (server.worker >= WORKER_MINIMUM && server.worker <= WORKER_MAXIMUM)
            {
                LOG("[SET]Worker numbers: %d", server.worker);
                break; //how many thread will be create.
            }
            else
            {
                printf("The number of worker is between %d and %d\n\n", WORKER_MINIMUM, WORKER_MAXIMUM);
                return -1;
            }
        case 'd':
            if (strlen(optarg) > 237)
            {
                printf("The path of directory is too long(greater than 236)");
                return -1;
            }
            if (check_dir(optarg))
            {
                printf("The directory not exist or not writable\n\n");
                return -1;
            }
            server.dir = malloc(strlen(optarg) + 1);
            strcpy(server.dir, optarg);
            LOG("[SET]Data directory: %s", server.dir);
            break;
        case '?':
            printf("invalid argument -%c\n\n", optopt);
        }
    }

    if (server.dir == NULL)
    {
        printf("You must set the data directory!\n\n");
        return -1;
    }
    //fork and write pid file
    daemonize();
    //signal setting
    signal(SIGHUP, SIG_IGN);  //kill -HUP
    signal(SIGPIPE, SIG_IGN);

    //libuv thread pool size must be set before uv_loop_init / first uv_queue_work
    snprintf(pool_sz, sizeof(pool_sz), "%d", server.worker);
    setenv("UV_THREADPOOL_SIZE", pool_sz, 1);

    uv_loop_init(&loop);
    //init file module
    init_file();
    //init hash table
    bzero(XXTABLE, DESC_HASH_TABLE_SIZE * sizeof(struct desc_table *));
    //SIGTERM handled in loop thread: safe to call close_files + uv_stop
    uv_signal_init(&loop, &sigterm);
    uv_signal_start(&sigterm, on_sigterm, SIGTERM);
    //listen (TCP via libuv)
    if (run_server(&loop, &server.socket))
    {
        LOG("Server terminal...");
        return -1;
    }
    LOG("Server started");
    uv_run(&loop, UV_RUN_DEFAULT);
    uv_loop_close(&loop);
    return 0;
}
