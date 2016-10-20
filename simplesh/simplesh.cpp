#include <iostream> // for debug only
#include <unistd.h>
#include <string.h>
#include <signal.h>
 
#define BUFF_FULLSIZE 2049 // lets have \0 on the end of buffer
#define BUFF_SIZE 2048
 
char * bash_read(char * buff, size_t * buffer_size)
{
 
    // let fill buff with \0, just to be in the save side
    if (*buffer_size == 0)
        memset(buff, 0, BUFF_FULLSIZE);
 
    char * clcr = strchr(buff, '\n');
found:
    if (clcr && (size_t) (clcr - buff) < *buffer_size) // if we found '\n' in buff[0]..buff[buffer_size] bound
    {
       
        // make a copy of command(c-string from buff[0] to clcr)
        size_t command_size = clcr - buff;
        char * command = new char[command_size + 1];
        memcpy(command, buff, command_size);
        command[command_size + 1] = '\0';
       
        //shift buff (eg: buff="aba\ncaba" => command="aba\n", buff="caba")
        memmove(buff, clcr + 1, *buffer_size - command_size - 1);
        *buffer_size -= command_size + 1;  
 
        return command;
    }
   
    ssize_t sread = 0;
    while (1)
    {
        sread = read(0, buff + *buffer_size, BUFF_SIZE - *buffer_size);
        // if error or 0
        if (sread == -1)
            return NULL;
        if (sread == 0)
        {
            *buffer_size = 0;
            return NULL;
        }
       
        *buffer_size += sread;
        clcr = strchr(buff, '\n');
        if (!clcr)
            continue;
        goto found;
    }
}
 
size_t count_args(char * command)
{
    size_t args_cnt = 0;
    bool open = false;
    for(ssize_t pos = -1;;)
    {
        pos++;
        if (command[pos] == '\"')
        {
            open = !open;
            if (!open)
            {
                pos++; // skip "
                args_cnt++;
            }
        }
 
        if (open)
            continue;
 
        if ((pos && command[pos - 1] != ' ' && command[pos - 1] != '\"')
             && (command[pos] == ' ' || command[pos] == '\0' || command[pos] == '|'))
                 args_cnt++;
 
        if (command[pos] == '\0' || command[pos] == '|')
            break;
    }
    return args_cnt;
}
 
char * get_next_arg(char ** substr)
{
    char * line = *substr;
    while (*line == ' ')
        line++;
    char * arg_start = line;
    bool open = *arg_start == '\"';
    if (open)
        line++;
    while ((open || *line != ' ') && (!open || *line != '\"') && *line != '\0' && *line != '|') {
        line++;
    }
    if (open)
        line++;
    char * arg_end = line;
    char * arg = new char[arg_end - arg_start + 1];
    memcpy(arg, arg_start, arg_end - arg_start);
    arg[arg_end - arg_start] = '\0';
    *substr = line;
    return arg;
}
 
char *** bash_parse(char * command, size_t * args_size)
{
    if (*command == '\0') {
        *args_size = 0;
        return NULL;
    }
   
    size_t subcommands_counter = 0;
    char * substr = command;
    while (substr)
    {
        substr = strchr(substr, '|');
        if (substr)
            substr++;
        subcommands_counter++;
    }
    char *** args = new char ** [subcommands_counter + 1];
    args[subcommands_counter] = NULL;
    *args_size = subcommands_counter;
 
    substr = command;
    size_t subcommand_num = 0;
    while (substr)
    {
        size_t args_count = count_args(substr);
        args[subcommand_num] = new char * [args_count + 1];
        args[subcommand_num][args_count] = NULL;
        char * delim = strchr(substr, '|');
        for (size_t arg_num = 0; arg_num < args_count; ++arg_num) {
            args[subcommand_num][arg_num] = get_next_arg(&substr);
        }
        for (size_t i = 0; i <= args_count; ++i)
        if (delim)
            substr = delim + 1; // skip '|'
        else
            substr = delim;
        subcommand_num++;
    }
 
    return args;
}
 
pid_t * childs = NULL;
bool sig_int = false;
bool first_alive = false;
 
void signal_handler(int signal, siginfo_t * siginfo, void * ptr) {
    if (signal == SIGCHLD && childs && siginfo->si_pid == childs[0])
    {
        first_alive = false;
    }
    if (signal == SIGINT) 
    {
        sig_int = true;
    }
}
 
void write_all(int fd, const char *buf, size_t len) {
    while (len > 0)
    {
        ssize_t writen = write(fd, buf, len);
        if (writen == -1) {
            continue;
        }
        buf += writen;
        len -= writen;
    }  
}
 
void sig_intr() {
    if (sig_int) {
        for (pid_t * child = childs; childs && *child != 0; ++child)
            kill(*child ,SIGKILL);
    }
    sig_int = false;
}
 
int bash_execute(char * buffer, size_t * buffer_size, char *** args, size_t args_size)
{
    if (!args_size)
        return 0;
    int pipefd[args_size][2];
    childs = new pid_t[args_size + 1];
    childs[args_size] = 0;
 
    pid_t pid;    
    pipe(pipefd[0]); // pipe from parent to first child
   
    for (size_t i = 0; i < args_size; ++i)
    {
        if (i != args_size - 1) {
            pipe(pipefd[i + 1]); // pipe from i command to i + 1 command
        }
 
        pid = fork();
        if (pid == 0) { //child
            if (*buffer_size != 0 && i == 0) {
                close(pipefd[0][1]);
                dup2(pipefd[0][0], 0);
                close(pipefd[0][0]);
            }
            if (i != 0)
            {
                close(pipefd[0][1]);
                close(pipefd[0][0]);
            }
            if (i != 0)
            {
                close(pipefd[i][1]);
                dup2(pipefd[i][0], 0);
                close(pipefd[i][0]);
            }
            if (i != args_size - 1)
            {
                close(pipefd[i + 1][0]);
                dup2(pipefd[i + 1][1], 1);
                close(pipefd[i + 1][1]);
            }
            if (i == 0)
                first_alive = true;
            _exit(execvp(args[i][0], args[i]));
        }
        else
        {
            childs[i] = pid;
            if (i < args_size - 1)
                close(pipefd[i + 1][1]);
            if (i >= 1)
                close(pipefd[i][0]);
        }
    }
 
 
    if (*buffer_size != 0)
    {
        write_all(pipefd[0][1], buffer, *buffer_size);
        *buffer_size = 0;
        while (!sig_int && first_alive)
        {
            ssize_t rsize = read(0, buffer, *buffer_size);
            if (rsize == 0)
                break;
            if (rsize == -1)
                continue;
            write_all(pipefd[0][1], buffer, rsize);
        }
    }
 
    if (sig_int)
        sig_intr();
 
    close(pipefd[0][1]);
 
    int status;
   
    for (size_t i = 0; i < args_size; i++)
        while (true)
        {
            if (waitpid(childs[i], NULL, 0) == -1) {
                sig_intr();
                continue;
            }
            break;
        }
 
 
    ssize_t sread;
    while ((sread = read(pipefd[0][0], buffer + *buffer_size, BUFF_SIZE - *buffer_size)) != 0) {
        if (sread == -1) {
            continue;
        }
        *buffer_size += sread;
    }
 
 
    close(pipefd[0][0]);
 
    childs = NULL;
 
    return 0;
}
 
int bash_loop()
{
    struct sigaction act;
    act.sa_sigaction = &signal_handler;
    act.sa_flags = SA_SIGINFO;
   
    sigemptyset(&act.sa_mask);
    sigaddset(&act.sa_mask, SIGINT);
    sigaddset(&act.sa_mask, SIGCHLD);
    sigaction(SIGINT, &act, 0);
    sigaction(SIGCHLD, &act, 0);
 
    char buff[BUFF_FULLSIZE];
    size_t buffer_size;
   
    char *** args = NULL;
    size_t args_size = 0;
 
    while(1) {
        write(0, "$ ", 2);
       
        char * command = NULL;
        if ((command = bash_read(buff, &buffer_size)) == NULL && buffer_size == 0)
            break;
 
       
        if (command == NULL)
            continue;
 
        args = bash_parse(command, &args_size);
 
        free(command);
 
        bash_execute(buff, &buffer_size, args, args_size);
    }
 
    return 0;
}
 
int main()
{
    return bash_loop();
}
