#include "solidbits.h"

int main(int argc, char const *argv[])
{
    //init and get param
    int arg;
    size_t count;
    struct sockaddr_in client;
    opterr = 0;
    bzero(&server.socket, sizeof(server.socket));
    server.socket.sin_family = PF_INET;
    server.mode = GLIBC; //work with glibc default
    server.socket.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.socket.sin_port = htons(6379); //listen on 127.0.0.1:6379 default
    server.worker = get_nprocs() * 2;
    INIT_LOG();
    LOG("System preparing for start");
    while ((arg = getopt(argc, (char **)argv, "m:l:d:Dp:w:")) != -1)
    {
        switch (arg)
        {
        case 'm':
            if (!strcasecmp(optarg, "glibc"))
                server.mode = GLIBC;
            else if (!strcasecmp(optarg, "direct_io"))
                server.mode = DIRECT_IO;
            else
            {
                printf("The value %s of argument -%c invalid!\n\n", optarg, arg);
                return -1;
            }
            LOG("[SET]Set mode to %s", strupr(optarg));
            break;
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
    signal(SIGTERM, safe_exit); //kill
    signal(SIGHUP, SIG_IGN);  //kill -HUP
    signal(SIGPIPE, SIG_IGN);

    //init file module
    init_file();

    //init hash table
    bzero(XXTABLE, DESC_HASH_TABLE_SIZE * sizeof(struct desc_table *));

    //start threads
    init_parser();
    init_workers();
    //listen
    server.s_size = sizeof(server.socket);
    if ((server.fd = socket(PF_INET, SOCK_DGRAM, 0)) < 0) //old is AF_UNIX
    {
        LOG("Socket error: %s", strerror(errno));
        LOG("Server terminal...");
        return errno;
    }
    if (bind(server.fd, (struct sockaddr *)&server.socket, server.s_size) < 0)
    {
        LOG("Bind error: %s", strerror(errno));
        LOG("Server terminal...");
        return errno;
    }

    for (;;)
    {
        count = recvfrom(server.fd, server.buf, UDP_DATA_SIZE, 0, (struct sockaddr *)&client, (socklen_t *)&server.s_size);
        insert_parser_job(server.buf, count, &client);
    }
}
