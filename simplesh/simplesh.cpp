#include <iostream> // for debug only
#include <unistd.h>
#include <string.h>
#include <signal.h>
 
#define BUFF_SIZE 2048
 
int bash_read(char * buff)
{
    size_t sread = 0;
    while (1)
    {
        sread += read(0, buff + sread, BUFF_SIZE - sread);
        if (!sread)
            return 0;
        char * clcr = strchr(buff, '\n');
        if (!clcr)
            continue;
        return sread;
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
 
char *** bash_parse(char * buff, size_t * args_size, ssize_t * end)
{
    if (*buff == '\0') {
        *args_size = 0;
        return NULL;
    }
    char * del  = strchr(buff, '\n');
    char * line = new char[del - buff + 1];
    memcpy(line, buff, del - buff);
    line[del - buff] = '\0';
    *end = del - buff + 1;
    size_t commands = 0;
    char * substr = line;
    while (substr)
    {
        substr = strchr(substr, '|');
        if (substr)
            substr++;
        commands++;
    }
    char *** args = new char ** [commands + 1];
    args[commands] = NULL;
    *args_size = commands;
 
    substr = line;
    size_t command_num = 0;
    while (substr)
    {
        size_t args_count = count_args(substr);
        args[command_num] = new char * [args_count + 1];
        args[command_num][args_count] = NULL;
        char * delim = strchr(substr, '|');
        for (size_t arg_num = 0; arg_num < args_count; ++arg_num) {
            args[command_num][arg_num] = get_next_arg(&substr);
        }
        for (size_t i = 0; i <= args_count; ++i)
        if (delim)
            substr = delim + 1; // skip '|'
        else
            substr = delim;
        command_num++;
    }
 
    return args;
}
 
pid_t * childs = NULL;
 
void killChildren(int ignored) {
    if (childs)
        for (pid_t * child = childs; *child != 0; ++child)
            kill(*child ,SIGKILL);
}
 
int bash_execute(char * buff, size_t size, ssize_t end, char *** args, size_t args_size)
{
    if (!args_size)
        return 0;
    int pipefd[args_size][2];
    childs = new pid_t[args_size + 1];
    childs[args_size] = 0;
 
    pid_t pid;    
    if (size != (size_t)end)
        pipe(pipefd[0]); // pipe from parent to first child
   
    for (size_t i = 0; i < args_size; ++i)
    {
        if (i != args_size - 1) {
            pipe(pipefd[i + 1]); // pipe from i command to i + 1 command
        }
 
        pid = fork();
        if (!pid) { //child
            if (size != (size_t) end && i == 0) {
                close(pipefd[0][1]);
                dup2(pipefd[0][0], 0);
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
            _exit(execvp(args[i][0], args[i]));
        }
        else
        {
            childs[i] = pid;
            if (i == 0 && size != (size_t)end)
            {
                close(pipefd[0][0]);
                write(pipefd[0][1], buff + end, size - end);
                close(pipefd[0][1]);
            }
            if (i < args_size - 1)
                close(pipefd[i + 1][1]);
            if (i >= 1)
                close(pipefd[i][0]);
        }
    }
 
    struct sigaction act;
    memset(&act, '\0', sizeof act);
    act.sa_handler = &killChildren;
 
    if (sigaction(SIGINT, &act, 0) < 0)
        return -1;
                   
    int status;
    for (size_t i = 0; i < args_size; i++)
        waitpid(childs[i], &status, 0);
 
    childs = NULL;
 
    return 0;
}
 
int bash_loop()
{
    char buff[BUFF_SIZE];
    char *** args = NULL;
    size_t args_size;
    ssize_t end;
 
    while(1) {
        write(0, "$ ", 2);
       
        size_t sread = 0;
        if (!(sread = bash_read(buff)))
            break;
 
        if (sread == -1)
            continue;
 
        args = bash_parse(buff, &args_size, &end);
 
        bash_execute(buff, sread, end, args, args_size);
    }
 
    return 0;
}
 
int main()
{
    return bash_loop();
}

