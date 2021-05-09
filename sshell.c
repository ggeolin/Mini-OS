#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#define CMDLINE_MAX 512
#define ARG_MAX 16
#define TOKEN_LEN_MAX 32

struct cmd_and_argv {
        char raw_cmd[CMDLINE_MAX];
        char out_name[TOKEN_LEN_MAX];
        char* single_cmd_argvs[ARG_MAX+1];

        /* assume there are 3 pipe signs */
        char** array_of_pipe[4];
        char* argvs_pipe_1[ARG_MAX+1];
        char* argvs_pipe_2[ARG_MAX+1];
        char* argvs_pipe_3[ARG_MAX+1];
        char* argvs_pipe_4[ARG_MAX+1];

        int truncated_output_flag;
        int appended_output_flag;
        int pipe_flag; 
        int num_pipe;
        int fd;
};

/* helper functions: */
void initialize_struct(struct cmd_and_argv* argv);
void parse_cmd(char* cmd, struct cmd_and_argv* argv);
void run_builtin(struct cmd_and_argv* argv);
void run_regular_cmd(struct cmd_and_argv* argv);
void run_pipe_cmd(struct cmd_and_argv* argv);
void bye(void);                                                         /* functor */
void swap(int* a, int* b);
void pid_status_sort(pid_t* pid, int* status, int cmd_n);
void print_error(int error_flag);
char* rm_whit_space(char* input_str);
int cmd_checker(struct cmd_and_argv* argv, char** cmd_array, char* cmd);
int cmd_parser(struct cmd_and_argv* argv);                              /* return a parsing error flag */
int command_type(struct cmd_and_argv* argv);                            /* 1: builtin_cmd, 2: regular_cmd, 3: piped_cmd */
int argv_splitter(char** argv_array, char* cmd);                        /* return 1 if the argv exceed 16 */

/* buildin functions */
int pwd(void);                                                          /* return status */
int cd(char* dir);                                                      /* return status */
int sls();
void bye_bye(void);

int main(void) {
        while (1) {
                char* nl;
                int cmd_type;
                int error_flag;
                char cmd[CMDLINE_MAX];
                struct cmd_and_argv argv_data = {0};
                initialize_struct(&argv_data);

                /* Print prompt */
                printf("sshell@ucd$ ");
                fflush(stdout);

                /* Get command line */
                fgets(cmd, CMDLINE_MAX, stdin);

                /* Print command line if stdin is not provided by terminal */
                if (!isatty(STDIN_FILENO)) {
                        printf("%s", cmd);
                        fflush(stdout);
                }

                /* Remove trailing newline from command line */
                nl = strchr(cmd, '\n');
                if (nl)
                        *nl = '\0';

                strcpy(argv_data.raw_cmd, cmd);

                if(!strcmp("", rm_whit_space(cmd)))
                        continue;

                /* parse the cmd into tokens */
                error_flag = cmd_parser(&argv_data);
                print_error(error_flag);

                /* run the actual commands */
                if(!error_flag) {
                        cmd_type = command_type(&argv_data);
                        if (cmd_type == 1) {                    /* builtin_cmd */
                                run_builtin(&argv_data);
                        } else if (cmd_type == 2) {
                                run_regular_cmd(&argv_data);    /* regular_cmd */
                        } else {
                                run_pipe_cmd(&argv_data);       /* piped_cmd */
                        }
                }
                
        }

        return EXIT_SUCCESS;
}

void initialize_struct(struct cmd_and_argv* argv) {
        argv->fd = -1;
        strcpy(argv->raw_cmd, "");
        strcpy(argv->out_name, "");

        /* init all entries in the string array to nothing */
        for(int i = 0; i < ARG_MAX+1; i++) {
                argv->single_cmd_argvs[i] = strdup("");
        }

        for(int i = 0; i < ARG_MAX+2; i++) {
                argv->argvs_pipe_1[i] = strdup("");
        }

        for(int i = 0; i < ARG_MAX+2; i++) {
                argv->argvs_pipe_2[i] = strdup("");
        }
        for(int i = 0; i < ARG_MAX+2; i++) {
                argv->argvs_pipe_3[i] = strdup("");
        }

        for(int i = 0; i < ARG_MAX+2; i++) {
                argv->argvs_pipe_4[i] = strdup("");
        }
}

int cmd_parser(struct cmd_and_argv* argv) {
        char* cmd_cp = strdup(argv->raw_cmd);
        int pipe_sign_num = 0;
        /* check for any pipe signs */
        for(unsigned int i = 0; i < strlen(cmd_cp); i++) {
                if(cmd_cp[i] == '|') pipe_sign_num++;
        }

        argv->num_pipe = pipe_sign_num;

        if (pipe_sign_num) {                            /* if pipe sign presents */
                argv->pipe_flag = 1;
                for(int i = 0; i < pipe_sign_num + 1; i++) {
                        char token[CMDLINE_MAX] = {'\0'};
                        int pipe_pos;
                        if(i != pipe_sign_num) {        /* not last pipe sign */
                                pipe_pos = strstr(cmd_cp, "|") - cmd_cp;
                                strncpy(token, cmd_cp, pipe_pos);
                                cmd_cp+=(pipe_pos+1);
                                /* output signs before pipe sign */
                                if (strstr(token, ">>") != NULL || strstr(token, ">") != NULL)
                                        return 5;
                                /* parsing error checking */
                                if (i == 0) {
                                        if (argv_splitter(argv->argvs_pipe_1, token))
                                                return 1;
                                        else if (!strcmp("", argv->argvs_pipe_1[0]))
                                                return 2;
                                } else if (i == 1) {
                                        if (argv_splitter(argv->argvs_pipe_2, token))
                                                return 1;
                                        else if (!strcmp("", argv->argvs_pipe_2[0]))
                                                return 2;
                                } else if (i == 2) {
                                        if (argv_splitter(argv->argvs_pipe_3, token))
                                                return 1;
                                        else if (!strcmp("", argv->argvs_pipe_3[0]))
                                                return 2;
                                }
                        } else {                         /* last pipe sign */
                                strcpy(token, cmd_cp);
                                int flager;

                                if(!strcmp("", token))
                                        return 2;

                                if(i == 1)
                                        flager = cmd_checker(argv, argv->argvs_pipe_2, token);
                                else if (i == 2)
                                        flager = cmd_checker(argv, argv->argvs_pipe_3, token);
                                else if (i == 3)
                                        flager = cmd_checker(argv, argv->argvs_pipe_4, token);

                                if (flager)
                                        return flager;
                        }
                }
        } else {                                        /* builtin and regular cmd */
                int flager = cmd_checker(argv, argv->single_cmd_argvs, argv->raw_cmd);

                if (flager)
                        return flager;
        }

        /* init all entries in the string array back to NULL for those empty string */
        for(int i = 0; i < ARG_MAX+1; i++) {
                if(!strcmp("", argv->single_cmd_argvs[i]))
                        argv->single_cmd_argvs[i] = NULL;
                if(!strcmp("", argv->argvs_pipe_1[i]))
                        argv->argvs_pipe_1[i] = NULL;
                if(!strcmp("", argv->argvs_pipe_2[i]))
                        argv->argvs_pipe_2[i] = NULL;
                if(!strcmp("", argv->argvs_pipe_3[i]))
                        argv->argvs_pipe_3[i] = NULL;
                if(!strcmp("", argv->argvs_pipe_4[i]))
                        argv->argvs_pipe_4[i] = NULL;
                
        }

        /* put arrays together */
        if(argv->pipe_flag) {
                for(int i = 0; i < (argv->num_pipe + 1); i++) {
                        if(i == 0)
                                argv->array_of_pipe[i] = argv->argvs_pipe_1;
                        else if(i == 1)
                                argv->array_of_pipe[i] = argv->argvs_pipe_2;
                        else if(i == 2)
                                argv->array_of_pipe[i] = argv->argvs_pipe_3;
                        else 
                                argv->array_of_pipe[i] = argv->argvs_pipe_4;
                }
        }

        return 0;
        
}

int cmd_checker(struct cmd_and_argv* argv, char** cmd_array, char* cmd) {
                char* cmd_cp = strdup(cmd);
                char token[CMDLINE_MAX] = {'\0'};
                int output_sign_pos = -1;

                if (strstr(cmd_cp, ">>") != NULL) {
                        int argv_flag;
                        argv->appended_output_flag = 1;

                        /* taken apart the token A > B */
                        output_sign_pos = strstr(cmd_cp, ">>") - cmd_cp;
                        strncpy(token, cmd_cp, output_sign_pos);
                        cmd_cp+=(output_sign_pos+2);
                        strcpy(argv->out_name, rm_whit_space(cmd_cp));
                        /* insert all cmds into array */        
                        argv_flag = argv_splitter(cmd_array, token);
                        /* check for error */
                        if(!strcmp(cmd_array[0], "")) 
                                return 2;
                        else if(argv_flag)
                                return 1;
                        else if(!strcmp(argv->out_name, ""))
                                return 3;
                        else {
                                int fd = open(argv->out_name, O_WRONLY | O_CREAT | O_APPEND, 0644);
                                if(fd == -1)
                                        return 4;
                                else {
                                        argv->fd = fd;
                                }
                        }
                } else if(strstr(cmd_cp, ">") != NULL) {
                        int argv_flag;
                        argv->truncated_output_flag = 1;

                        /* taken apart the token A > B */
                        output_sign_pos = strstr(cmd_cp, ">") - cmd_cp;
                        strncpy(token, cmd_cp, output_sign_pos);
                        cmd_cp+=(output_sign_pos+1);
                        strcpy(argv->out_name, rm_whit_space(cmd_cp));
                        /* insert all cmds into array */
                        argv_flag = argv_splitter(cmd_array, token);
                        /* check for error */
                        if(!strcmp(cmd_array[0], ""))
                                return 2;
                        else if(argv_flag)
                                return 1;
                        else if(!strcmp(argv->out_name, ""))
                                return 3;
                        else {
                                int fd = open(argv->out_name, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                                if(fd == -1)
                                        return 4;
                                else {
                                        argv->fd = fd;
                                }
                        }
                }  else {
                        return argv_splitter(cmd_array, cmd);
                }

                return 0;
}

int argv_splitter(char** argv_array, char* cmd) {
        int index = 0;
        char* raw_cmd = strdup(cmd);
        char* token;

        token = strtok(raw_cmd, " ");

        while (token != NULL) {
                if(index >= ARG_MAX) {
                        return 1;
                }

                argv_array[index] = token;
                token = strtok(NULL, " ");
                index++;
        }

        return 0;
}

char* rm_whit_space(char* input_str) {
        unsigned int i = 0;
        unsigned int j = 0;
        char* output_str = input_str;

        while (i < strlen(input_str)) {
                if(input_str[i] == ' ') {
                        j--;
                } else {
                        output_str[j] = input_str[i];
                }
                i++;
                j++;
        }
        output_str[j]='\0';
        return output_str;
}

void print_error(int error_flag) {
        if(error_flag == 1) 
                fprintf(stderr, "Error: too many process arguments\n");
        else if(error_flag == 2) 
                fprintf(stderr, "Error: missing command\n");
        else if(error_flag == 3)
                fprintf(stderr, "Error: no output file\n");
        else if(error_flag == 4)
                fprintf(stderr, "Error: cannot open output file\n");
        else if(error_flag == 5)
                fprintf(stderr, "Error: mislocated output redirection\n");
}

int command_type(struct cmd_and_argv* argv) {   
        if(argv->pipe_flag == 0 &&
        (strcmp(argv->single_cmd_argvs[0], "exit") == 0 ||
        strcmp(argv->single_cmd_argvs[0], "cd") == 0 ||
        strcmp(argv->single_cmd_argvs[0], "pwd") == 0 ||
        strcmp(argv->single_cmd_argvs[0], "sls") == 0))         /* builtin_cmd */
                return 1;
        else if (argv->pipe_flag == 1)                          /* pipe_cmd */
                return 3;
        else                                                    /* regular_cmd */
                return 2;

}

void run_builtin(struct cmd_and_argv* argv) {
        char* cmd_name = argv->single_cmd_argvs[0];
        int status = 0;

        if(strcmp(cmd_name, "exit") == 0)
                bye_bye();
        else if(strcmp(cmd_name, "pwd") == 0)
                status = pwd();
        else if(strcmp(cmd_name, "cd") == 0)
                status = cd(argv->single_cmd_argvs[1]);
        else if(strcmp(cmd_name, "sls") == 0)
                status = sls();

        fprintf(stderr, "+ completed '%s' [%d]\n", argv->raw_cmd, status);
}

void run_regular_cmd(struct cmd_and_argv* argv) {
        int status;

        if(!fork()) {                   /* child process */
                if(argv->fd != -1) {
                        dup2(argv->fd, STDOUT_FILENO);
                        close(argv->fd);
                }
                execvp(argv->single_cmd_argvs[0], argv->single_cmd_argvs);
                fprintf(stderr, "Error: command not found\n");
                exit(1);
        } else {                        /* parent process */
                waitpid(-1, &status, 0);
        }

        fprintf(stderr, "+ completed '%s' [%d]\n", argv->raw_cmd, WEXITSTATUS(status));
}

void run_pipe_cmd(struct cmd_and_argv* argv) {
        pid_t pid[(argv->num_pipe + 1)];
        int status[(argv->num_pipe + 1)];
        int i;

        int pd[argv->num_pipe*2];
        for(int j = 0; j < argv->num_pipe; j++) {
                pipe(pd + j*2);
        }

        for( i=1; i<(argv->num_pipe + 2); i++) {      
                if (!fork()) {
                        if(i == 1) {                            /* initial child */
                                dup2(pd[1], STDOUT_FILENO);
                        } else if(i == argv->num_pipe + 1) {    /* last child */
                                dup2(pd[2*i-4], STDIN_FILENO);

                                if(argv->fd != -1) {
                                        dup2(argv->fd, STDOUT_FILENO);
                                        close(argv->fd);
                                }
                        } else {                                /*middle child */
                                dup2(pd[2*i-4], STDIN_FILENO);
                                dup2(pd[2*i-1], STDOUT_FILENO);
                        }

                        for(int t = 0; t < argv->num_pipe*2; t++) {
                                close(pd[t]);
                        }

                        execvp(argv->array_of_pipe[i-1][0], argv->array_of_pipe[i-1]);
                        fprintf(stderr, "Error: command not found\n");
                        exit(1);
                }
        }

        for(int k = 0; k < argv->num_pipe*2; k++) {
                close(pd[k]);
        }

        for(int q = 0; q < argv->num_pipe + 1; q++) {
                pid[q] = wait(&(status[q]));
        }

        pid_status_sort(pid, status, argv->num_pipe + 1);

        for(int l = 0; l < argv->num_pipe + 1; l++) {
                if(l == 0)
                        fprintf(stderr, "+ completed '%s' ", argv->raw_cmd);
                fprintf(stderr, "[%d]", WEXITSTATUS(status[l]));
                if(l == argv->num_pipe)
                        fprintf(stderr, "\n");
        }
}

void bye(void) {
        fprintf(stderr, "Bye...\n");
        fprintf(stderr, "+ completed '%s' [%d]\n", "exit", WEXITSTATUS(SIGCHLD));
}

void swap(int* a, int* b) { 
    int temp = *a; 
    *a = *b; 
    *b = temp; 
} 

void pid_status_sort(pid_t* pid, int* status, int cmd_n) { 
    int smallest_i; 

    for (int i = 0; i < cmd_n - 1; i++) { 
        smallest_i = i; 

        for (int j = i + 1; j < cmd_n; j++) {
            if (pid[j] < pid[smallest_i]) 
                smallest_i = j; 
        }

        swap(&pid[smallest_i], &pid[i]);
        swap(&status[smallest_i], &status[i]);
        
    }
}

/* buildin functions */
int pwd(void) {
        char current_dir[PATH_MAX];

        getcwd(current_dir, sizeof(current_dir));
        fprintf(stdout, "%s\n", current_dir);

        return 0;
}

int cd(char* dir) {
        if(chdir(dir) != 0) {
                fprintf(stderr, "Error: cannot cd into directory\n");
                return 1;
        }
        else                    
                return 0;
}

int sls() {
        struct stat sb;
        struct dirent *dp;
        DIR *dirp;

        dirp = opendir(".");

        if(dirp == NULL) {
                fprintf(stderr, "Error: cannot open directory\n");
                return 1;
        } else {
                while ((dp = readdir(dirp)) != NULL) {
                        stat(dp->d_name, &sb);
                        if(strcmp(".", dp->d_name) && strcmp("..", dp->d_name)) 
                                fprintf(stdout, "%s (%ld bytes)\n", dp->d_name, sb.st_size);
                }
        }

        return 0;
}

void bye_bye(void) {
        atexit(&bye);
        exit(0);
}
