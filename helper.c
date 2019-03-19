#include "solidbits.h"
extern bool terminate;
char *strupr(char *str)
{
    char *orig = str;
    DLOG("into %s(%s)",__func__, str);
    for (; *str != '\0'; str++)
        *str = toupper(*str);
    str = orig;
    DRETURN(str,2);
}

int set_addr_port(char *str)
{
    char *orig = str;
    int len = strlen(str);
    DLOG("into %s(%s)",__func__, str);
    if (*str == ':')
    {
        str++;
        if (atoi(str) > 0 && atoi(str) < 65535)
        {
            server.socket.sin_port = htons(atoi(str));
        }
        else
        {
            DRETURN(-1,1);
        }
        DRETURN(0,1);
    }

    if (len < 9)
    {
        LOG("%s not a invaid address and port", str);
        DRETURN(-1,1);
    }
    str = str + 7;
    while (*str != '\0')
    {
        if (*str == ':')
        {
            *str = '\0';
            str++;
            if (atoi(str) > 0 && atoi(str) < 65535)
            {
                server.socket.sin_port = htons(atoi(str));
            }
            else
            {
                DRETURN(-1,1);
            }
            break;
        }
        str++;
    }
    server.socket.sin_addr.s_addr = inet_addr(orig);
    DRETURN(0,1);
}
void write_pid(void)
{
    FILE *fd = fopen(server.pid,"w");
    
    if (fd == NULL)
    {
        LOG("Write pid to file failed");
        return;
    }
    fprintf(fd,"%d\n",(int)getpid());
    fclose(fd);
}
void daemonize(void) {
    int fd;
    DLOG("into %s",__func__);
    if (fork() != 0) exit(0);
    umask(0);
    if ((fd = open("/dev/null", O_RDWR, 0)) != -1) {
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        if (fd > STDERR_FILENO) close(fd);
    }
    write_pid();
}

int check_dir(char *path)
{
    struct stat sb;
    DLOG("into %s(%s)", __func__, path);
    if (!stat(path, &sb) && S_ISDIR(sb.st_mode))
    {

        if (!access(path, W_OK | R_OK))
        {
            DRETURN(0,1);
        }
        else
        {
            DRETURN(-1,1);
        }
    }
    DRETURN(-2,1); //dir not exists
}

int check_file(char *path)
{
    struct stat sb;
    int a;
    char *orig = path;
    DLOG("into %s(%s)", __func__, path);
    if (!stat(orig, &sb) && S_ISREG(sb.st_mode))
    {
        if (!access(orig, W_OK | R_OK))
        {
            DRETURN(0,1); //file exists and writable.
        }
        else { 
            DRETURN(-1,1); //file exists and not writable.
        }
    }
    a = check_dir(dirname(path));
    if (a == -2) { //dir not exist
        DRETURN(-2,1);
    } else if (!a)
    {
        DRETURN(1,1);  //file not exists and dir writable.
    }

    DRETURN(-1,1); //file not exists and dir not writable.
}

int gen_path(char *path, XXH64_hash_t hash)
{
    int r, offset = strlen(server.dir);
    char hex[17];
    DLOG("into %s(point, %016llx)", __func__, hash);
    snprintf(hex, 17, "%016llx", hash);
    memcpy(path, server.dir,offset);
    path[offset++] = '/';
    memcpy(path+offset, hex, 2);
    if ((r = check_dir(path)))
    {
        if (r == -2)
        {
            if (mkdir(path, 0755))
            {
                DRETURN(-2, 1) //dir is a file or can't access parent dir.
            }
        }
        else if(r == -1)
        {
            DRETURN(-1, 1)  //can't access dir
        }
    }
    offset +=2;
    path[offset++] = '/';
    memcpy(path+offset, hex+2, 2);
    if ((r = check_dir(path)))
    {
        if (r == -2)
        {
            if (mkdir(path, 0755))
            {
                DRETURN(-2, 1) 
            }
        }
        else if(r == -1)
        {
            DRETURN(-1, 1)
        }
    }
    offset +=2;
    path[offset++] = '/';
    memcpy(path+offset, hex+4, 12);
    DRETURN(0,1);

}

void safe_exit(int signum)
{
    syslog(LOG_USER|LOG_INFO,"[QUIT]Recv Signal Number %d.\n", signum);
    terminate = true;
    while (job_queue.size)
    {
        sleep(1);
    }
    close_files();
    exit(errno);
}

uint64_t get_us(void) {
    struct timeval tv;
    gettimeofday(&tv,NULL);
    return tv.tv_sec*(uint64_t)1000000+tv.tv_usec;
}


char *time2string(char* buf, uint64_t us)
{
    time_t sec= us/1000000;
    strftime(buf, 20, "%Y-%m-%d %H:%M:%S", localtime(&sec)); //format date and time.
    return buf;
}