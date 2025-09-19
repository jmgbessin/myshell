#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

void myPrint(char *msg)
{
    write(STDOUT_FILENO, msg, strlen(msg));
}

void myPrintError() {
    char error_message[30] = "An error has occurred\n";
    write(STDOUT_FILENO, error_message, strlen(error_message));
}

int str_redir_symbol_cnt(char* str, int* advanced_redir_mode) {
    char* split;
    if ((split = strchr(str, '>')) == NULL) {
        return 0;
    }
    if (split[1] == '+') {
        *advanced_redir_mode = 1;
    }
    return 1 + str_redir_symbol_cnt(split + 1, advanced_redir_mode);
}

char** parse_command(char* cmd, int* numargs, char* delim) {
    /* The cmd string must be duplicated because strtok modifies strings */
    char* cmd_copy = strdup(cmd);

    /* set command arg and initialize parameter array*/
    char* token = strtok(cmd_copy, delim);
    *numargs = 0;
    while (token != NULL) {
        (*numargs)++;
        token = strtok(NULL, delim);
    }
    (*numargs)++;

    char** args = (char**)malloc(sizeof(char*) * *numargs);

    /* Fill parameter array with supplied parameters*/
    token = strtok(cmd, delim);
    unsigned int i = 0;
    while (token != NULL) {
        args[i] = token;
        token = strtok(NULL, delim);
        i++;
    }

    return args;
}

void shell_prompt(char* cmd_buff, int buff_len) {
    myPrint("myshell> ");
    fgets(cmd_buff, buff_len, stdin);
    if (!cmd_buff) {
        exit(0);
    }
}

void handle_execution(char* cmd_buff) {

    int advanced_redir_mode = 0;
    // Throw an error if there is more than one > delimiter
    int num_redir_symbols = str_redir_symbol_cnt(cmd_buff, 
        &advanced_redir_mode);
    if (num_redir_symbols > 1) {
        myPrintError();
        return;
    }
    
    /*Parses input into command and redirection file if > is present*/
    char *cmd, *file = NULL;
    int redir_mode = 0;
    if (num_redir_symbols == 1) {
        if (advanced_redir_mode) {
            cmd = strtok(cmd_buff, ">+");
            file = strtok(NULL, ">+");
        }
        else {
            cmd = strtok(cmd_buff, ">");
            file = strtok(NULL, ">");
        }

        if (file == NULL) {
            myPrintError();
            return;
        }

        file = strtok(strdup(file), " ");
        
        if (file == NULL) {
            myPrintError();
            return;
        }
        redir_mode = 1;
    }
    else {
        cmd = cmd_buff;
    }

    /* Checks for valid file format (no spaces between characters)*/
    if (redir_mode == 1 && strtok(NULL, " ") != NULL) {
        myPrintError();
        exit(1);
    } 

    /* Parse user input into multiple tokens*/
    int numargs;
    char** exec_argv = parse_command(cmd, &numargs, " ");

    if (numargs == 1) {
        if (redir_mode) {
            myPrintError();
        }
        return;
    }

    if (!strcmp(exec_argv[0], "exit")) {
        if (numargs > 2 || redir_mode) {
            myPrintError();
            return;
        }
        exit(0);
    }

    if (!strcmp(exec_argv[0], "pwd")) {
        if (numargs > 2 || redir_mode) {
            myPrintError();
            return;
        }
        char buff[100];
        char* path = getcwd(buff, 100);
        strcat(path, "\n");
        myPrint(path);
        return;
    }    

    if (!strcmp(exec_argv[0], "cd")) {
        if (redir_mode) {
            myPrintError();
            return;
        }
        if (numargs == 2) {
            char* path = getenv("HOME");
            chdir(path);
        }
        else if (numargs == 3) {
            if (chdir(exec_argv[1]) == -1) {
                myPrintError();
                return;
            }
        }
        else{
            myPrintError();
            return;
        }
        
        return;
    }
    
    int redirect_fd;
    int tmp_fd;
    if (redir_mode == 1) {
        redirect_fd = open(file, O_RDWR, 0644);
        if (redirect_fd != -1) {
            if (!advanced_redir_mode) {
                myPrintError();
                return;
            }
            tmp_fd = open("tmp", O_RDWR | O_CREAT, 0644);
            if (tmp_fd == -1) {
                myPrintError();
                exit(1);
            }
        }

        if (redirect_fd == -1) {
            redirect_fd = open(file, O_RDWR | O_CREAT, 0644);
            if (redirect_fd == -1) {
                myPrintError();
                return;
            }
            advanced_redir_mode = 0;
        }
    }

    int ret = fork();
    if (ret < 0) {
        myPrint("Fork failed");
        exit(1);
    }

    // Child executes command passed to shell
    if (ret == 0) {

        /* In the case of redirection, modifies file descriptor to redirect
        program output to specified file */
        if (redir_mode == 1) {
            
            /* Change child fild descriptors to redirect stdout flow to
            desired file */
            if (advanced_redir_mode) {
                if (dup2(tmp_fd, STDOUT_FILENO) == -1) {
                    myPrintError();
                    exit(1);
                }
            }
            else{
                if (dup2(redirect_fd, STDOUT_FILENO) == -1) {
                    myPrintError();
                    exit(1);
                }
            }
        }

        char cmd_path[600];
        strcpy(cmd_path, "/usr/bin/");
        strcat(cmd_path, exec_argv[0]);
        
        execvp(cmd_path, exec_argv);
        myPrintError();
        exit(1);
    }
    else {
        int status;
        waitpid(ret, &status, 0);
        //free(exec_argv);

        if (advanced_redir_mode) {
            
            char file_buff[100];
            lseek(redirect_fd, 0, SEEK_SET);
            while (1) {
                int read_status = read(redirect_fd, file_buff, 100);
                if (read_status == -1) {
                    myPrintError();
                    exit(1);
                }
                write(tmp_fd, file_buff, read_status);
                if (read_status <= 0) {
                    break;
                }
            }

            remove(file);
            rename ("tmp", file);
            close(tmp_fd);

        }
        else {
            close(tmp_fd);
            remove("tmp");
        }
        close(redirect_fd);
        //free(exec_argv);
    }

}


int main(int argc, char *argv[]) 
{
    int cmd_buff_len = 514;
    char cmd_buff[cmd_buff_len];
    FILE* fd;
    int batch_mode = 0;

    if (argc == 2) {
        batch_mode = 1;
        fd = fopen(argv[1], "r");
        if (fd == NULL) {
            myPrintError();
            exit(1);
        }
    }
    if (argc > 2) {
        myPrintError();
        exit(1);
    }
    
    while (1) {

        if (batch_mode) {
            void* res = fgets(cmd_buff, cmd_buff_len, fd);
            if (res == NULL) {
                exit(0);
            }
        }
        else {
            shell_prompt(cmd_buff, cmd_buff_len);
        }

        int blank_line = 1;
        unsigned int j = 0;
        // Check for blank line
        while (cmd_buff[j] != '\n') {
            if (cmd_buff[j] != ' ' && cmd_buff[j] != '\t') {
                blank_line = 0;
                break;
            }
            j++;
        }

        if (blank_line) {
            continue;
        }
        if (batch_mode) {
            myPrint(cmd_buff);
        }
        

        j = 0;
        while (cmd_buff[j] != '\n') {
            if (cmd_buff[j] == '\t') {
                cmd_buff[j] = ' ';
            }
            j++;
        }

        FILE* reading_file;
        if (batch_mode) {
            reading_file = fd;
        }
        else {
            reading_file = stdin;
        }

        // Checks if line is too long
        int cmd_len = strlen(cmd_buff);
        char long_command_read;
        if (cmd_len > 0 && cmd_buff[cmd_len - 1] != '\n') {
            while ((long_command_read = fgetc(reading_file)) != '\n' && 
            long_command_read != EOF) {
                write(STDOUT_FILENO, &long_command_read, 1);
            }
            if (long_command_read == '\n') {
                write(STDOUT_FILENO, &long_command_read, 1);
            }
            myPrintError();
            continue;
        }

        cmd_buff[cmd_len - 1] = '\0';

        int num_commands;
        char** commands = parse_command(cmd_buff, &num_commands, ";");

        // removed unsigned int later?
        for (unsigned int i = 0; i < num_commands - 1; i++) {
            handle_execution(commands[i]);
        }

        free(commands);
    }

    if (fclose(fd) == EOF) {
        myPrint("could not close batch file");
        exit(1);
    }
}
