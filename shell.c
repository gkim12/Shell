#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>
//#include <sys/sendfile.h>

void myPrint(char *msg) {
    write(STDOUT_FILENO, msg, strlen(msg));
}

void print_error() {
    char error_message[30] = "An error has occurred\n";
    write(STDOUT_FILENO, error_message, strlen(error_message));
}

int main(int argc, char *argv[])  {
    char cmd_buff[514];
    char *pinput;
    FILE *batch_file;
    int batch_mode = 0;

    if(argc == 2) {
        batch_file = fopen(argv[1], "r");
        if(!batch_file) {
            print_error();
            exit(0);
        }
        batch_mode = 1;
    }
    if(argc > 2) {
        print_error();
        exit(0);
    }

    while (1) {
        if(!batch_mode) {
            myPrint("myshell> ");
            pinput = fgets(cmd_buff, 514, stdin);
        }
        else
            pinput = fgets(cmd_buff, 514, batch_file);
        if (!pinput) {
            exit(0);
        }

        // lines exceeding 512 error handling
        if (strlen(pinput) > 512 && cmd_buff[512] != '\n') {
            myPrint(cmd_buff);
            while(1) {
                if(!batch_mode)
                    pinput = fgets(cmd_buff, 514, stdin);
                else
                    pinput = fgets(cmd_buff, 514, batch_file);
                if ((strlen(pinput) < 514) | (cmd_buff[512] == '\n')) {
                    myPrint(cmd_buff);
                    break;
                }
                else {
                    myPrint(cmd_buff);
                }
            }
            cmd_buff[0] = '\0';
            print_error();
        }

        // ignoring lines with whitespace
        char blank_check[514];
        strcpy(blank_check, cmd_buff);
        if(!strtok(blank_check, " \t\n"))
            continue;
        
        if(batch_mode)
            myPrint(cmd_buff);

        // gather list of commands to execute
        char *cmds[180];
        int cmd_cnt = 0;
        char *curr_cmd = strtok(cmd_buff, ";");
        while (curr_cmd) {
            cmds[cmd_cnt++] = curr_cmd;
            curr_cmd = strtok(NULL, ";");
        }
       
        // iterate through commands to execute
        char *cmd_args[180];
        char *redir_args[2];
        int arg_cnt = 0;
        int redir_type = 0; // 0 no redir, 1 normal, 2 advanced
        for(int i = 0; i < cmd_cnt; i++) {
            curr_cmd = cmds[i];
            // blank command check
            strcpy(blank_check, curr_cmd);
            if(!strtok(blank_check, " \t\n"))
                continue;
            // redirection handling
            int num_redirs = 0;
            char *redir_check = strchr(curr_cmd, '>');
            while(redir_check) {
                num_redirs++;
                redir_check = strchr(redir_check + 1, '>');
            }
            if(num_redirs > 1) {
                print_error();
                continue;
            }
            if(num_redirs == 1 && strstr(curr_cmd, ">+")) {
                redir_args[0] = strtok(curr_cmd, ">+");
                redir_args[1] = strtok(NULL, ">+");
                redir_args[1] = strtok(redir_args[1], " \t\n");
                // check output is blank
                if(!redir_args[1]) {
                    print_error();
                    continue;
                }
                // check more than 1 output file
                if(strtok(NULL, " \t\n")) {
                    print_error();
                    continue;
                }
                // check command is blank
                char cmd_check[514];
                strcpy(cmd_check, redir_args[0]);
                if(!strtok(cmd_check, " \t\n")) {
                    print_error();
                    continue;
                }
                redir_type = 2;
                // additional check to see if output file exists
                if(access(redir_args[1], F_OK))
                    redir_type = 1;
            }
            else if (num_redirs == 1 && strstr(curr_cmd, ">")) {
                redir_args[0] = strtok(curr_cmd, ">");
                redir_args[1] = strtok(NULL, ">");
                redir_args[1] = strtok(redir_args[1], " \t\n");
                // check output is blank
                if(!redir_args[1]) {
                    print_error();
                    continue;
                }
                // check more than 1 output file
                if(strtok(NULL, " \t\n")) {
                    print_error();
                    continue;
                }
                // check command is blank
                char cmd_check[514];
                strcpy(cmd_check, redir_args[0]);
                if(!strtok(cmd_check, " \t\n")) {
                    print_error();
                    continue;
                }
                redir_type = 1;

                // additional check to see if output file exists
                if(!access(redir_args[1], F_OK)) {
                    print_error();
                    continue;
                }
            }
            else
                redir_type = 0;

            // tokenize command into its arguments
            // check for redir or not
            if(redir_type)
                strcpy(curr_cmd, redir_args[0]);
            curr_cmd = strtok(curr_cmd, " \t\n");
            arg_cnt = 0;
            while(curr_cmd) {
                cmd_args[arg_cnt++] = curr_cmd;
                curr_cmd = strtok(NULL, " \t\n");
            }
            cmd_args[arg_cnt] = (char*)'\0';

            if(!strcmp(cmd_args[0], "exit")) {
                if(redir_type) {
                    print_error();
                    continue;
                }
                if(arg_cnt == 1)
                    exit(0);
                else
                    print_error();
            }
            else if(!strcmp(cmd_args[0], "pwd")) {
                if(redir_type) {
                    print_error();
                    continue;
                }
                if(arg_cnt == 1) {
                    char cwd[500];
                    getcwd(cwd, 500);
                    myPrint(cwd);
                    myPrint("\n");
                }
                else
                    print_error();
            }
            else if(!strcmp(cmd_args[0], "cd")) {
                if(redir_type) {
                    print_error();
                    continue;
                }
                if(arg_cnt == 1) {
                    if(chdir(getenv("HOME")))
                        print_error();
                }
                else if(arg_cnt == 2) {
                    if(chdir(cmd_args[1]))
                        print_error();
                }
                else
                    print_error();
            }
            else {
                pid_t forkret;
                int output_fd;
                int temp_fd;
                mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
                if((forkret = fork()) == 0) {
                    if(redir_type == 1) {
                        output_fd = creat(redir_args[1], mode);
                        if(output_fd == -1) {
                            print_error();
                            exit(1);
                        }
                        dup2(output_fd, STDOUT_FILENO);
                    }
                    if(redir_type == 2) {
                        temp_fd = creat("temp", mode);
                        if(temp_fd == -1) {
                            print_error();
                            exit(1);
                        }
                        dup2(temp_fd, STDOUT_FILENO);
                    }
                    if(execvp(cmd_args[0], cmd_args) == -1)
                        print_error();
                    exit(0);
                }
                else {
                    wait(NULL);
                    if(redir_type == 2) {
                        FILE *temp_file = fopen("temp", "a");
                        FILE *desired_file = fopen(redir_args[1], "r");
                        char copy_buffer[514];
                        char *cinput = fgets(copy_buffer, 514, desired_file);
                        while(cinput) {
                            fwrite(cinput, 1, strlen(cinput), temp_file);
                            cinput = fgets(copy_buffer, 514, desired_file);
                        }
                        fclose(temp_file);
                        fclose(desired_file);
                        remove(redir_args[1]);
                        rename("temp", redir_args[1]);
                    }
                }
            }
        }
    }
    if(batch_mode)
        fclose(batch_file);
}
