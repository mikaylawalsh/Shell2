#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "./jobs.h"

/*
global variables:
job_list is a pointer to the job list which will be initialized in main
jid is a integer which represents the current job id and will be initialized to
1 in main
*/
job_list_t *job_list;
int jid;

/*
exit_and_clean: calls cleanup_job_list on the job list and then exits with the
exit status input
parameters: status, an integer which is the exit status
returns: nothing
*/
void exit_and_clean(int status) {
    cleanup_job_list(job_list);
    exit(status);
}

/*
parse: parses the buffer string and populates the tokens, argv, filepath, and
redirection arrays. first parses the buffer by white space and fills each token
into the tokens array. Then separates the tokens array into redireciton
components (redirection symbols and files) and argv components (everything
else). lastly the full filepath of the command (the first argument of argv) is
loaded into the filepath array.
parameters: buffer, a string from the user populated in main; tokens, an empty
array of strings; argv, an empty array of strings; filepath, an empty array of
one string; and redirection, an empty array of strings.
returns: a int, -1 if there is an error or the input is empty; 0 otherwise.
*/
int parse(char buffer[1024], x, char *argv[512],
          char *filepath[2], char *redirection[512]) {
    int i = 0;
    char *token;
    token = strtok(buffer, "\t\n ");  // returns pointer to next token, used to
                                      // handle tabs, new lins, and spaces
    if (token == NULL) {
        return -1;
    }
    while (token != NULL) {
        tokens[i] = token;
        i++;
        token = strtok(0, "\t\n ");
    }
    int k = 0;
    int a = 0;
    int r = 0;
    int output = 0;
    int input = 0;
    while (tokens[k] != NULL) {
        if (!strcmp(tokens[k], ">") || !strcmp(tokens[k], ">>") ||
            !strcmp(tokens[k], "<")) {
            if (tokens[k + 1] == NULL) {
                if (!strcmp(tokens[k], "<")) {
                    fprintf(stderr, "syntax error: no input file.\n");
                } else {
                    fprintf(stderr, "syntax error: no output file.\n");
                }
                return -1;
            } else if (!strcmp(tokens[k + 1], "<")) {
                fprintf(stderr,
                        "syntax error: input file is redirection symbol.\n");
                return -1;
            } else if (!strcmp(tokens[k + 1], ">") ||
                       !strcmp(tokens[k + 1], ">>")) {
                fprintf(stderr,
                        "syntax error: output file is redirection symbol.\n");
                return -1;
            } else if ((!strcmp(tokens[k], ">") || !strcmp(tokens[k], ">>")) &&
                       output == 0) {
                output = 1;
                redirection[r] = tokens[k];  // fills redirection array
                redirection[r + 1] = tokens[k + 1];
                r += 2;  // these increase by two because we fill two indexes in
                         // the array above and then need to skip that the next
                         // iteration
                k += 2;
            } else if (!strcmp(tokens[k], "<") && input == 0) {
                input = 1;
                redirection[r] = tokens[k];
                redirection[r + 1] = tokens[k + 1];
                r += 2;
                k += 2;
            } else {
                fprintf(stderr, "syntax error: multiple output files\n");
                return -1;
            }
        } else {
            argv[a] = tokens[k];
            k++;
            a++;
        }
    }
    filepath[0] = argv[0];
    if (!strcmp(argv[a - 1], "&")) {
        filepath[1] = "1"; /*sets second element in filepath array to be 1 if bg
                             process and 0 otherwise*/
        argv[a - 1] = NULL;
    } else {
        filepath[1] = "0";
    }
    return 0;
}

/*
cd: checks syntax of input and changes directory if possible. calls and error
checks chdir in order to change directory.
parameters: argv, an array of strings containing the parsed input from the user.
returns: nothing, but prints out an error if necessary.
*/
void cd(char *argv[512]) {
    if (argv[1] == NULL) {
        fprintf(stderr, "cd: syntax error\n");
        return;
    }
    int c = chdir(argv[1]); /* chdir() changes the current working directory of
                               the calling process to the secified directory */
    if (c == -1) {
        fprintf(stderr, "cd: No such file or directory.\n");
    }
}

/*
ln: checks syntax of input and links inputs if possible. calls and error checks
link in order to change directory.
parameters: argv, an array of strings containing the parsed input from the user.
returns: nothing, but prints out an error if necessary.
*/
void ln(char *argv[512]) {
    if (argv[1] == NULL || argv[2] == NULL) {
        fprintf(stderr, "ln: syntax error.\n");
        return;
    }
    int l =
        link(argv[1], argv[2]); /* link creates a new link to an existing file*/
    if (l == -1) {
        fprintf(stderr, "ln: No such file or directory.\n");
    }
}

/*
rm: checks syntax of input and unlinks input if possible. calls and error checks
unlink in order to change directory.
parameters: argv, an array of strings containing the parsed input from the user.
returns: nothing, but prints out an error if necessary.
*/
void rm(char *argv[512]) {
    if (argv[1] == NULL) {
        fprintf(stderr, "rm: syntax error.\n");
        return;
    }
    int u = unlink(argv[1]); /* unlink deletes a name from a file system. */
    if (u == -1) {
        fprintf(stderr, "rm: No such file or directory.\n");
    }
}

/*
handle_redirection: creates a child process, handles redirection (if needed) by
closing and opening desired file descriptors in order to print output or read
input. also updates argv[0] to be only the command instead of the entire
filepath. calls execv on the command and the arguments in argv. and lastly waits
for the child process to finish before continuing. Also handles
parameters: argv, an array of strings containing the parsed input from the user;
filepath, an array of one string containing the full filepath of the command;
and redirection, an array of strings containing any redirection symbols and
files from the user.
returns: nothing, but prints out an error if necessary.
*/

void handle_redirection(char *argv[512], char *filepath[2],
                        char *redirection[512]) {
    pid_t child;
    child = fork(); /* creates child process */
    if (child == -1) {
        perror("pid");
        exit_and_clean(1);
    } else if (child == 0) {
        pid_t pid;
        pid_t pgrp;
        pid = getpid();
        setpgid(pid, pid);
        pgrp = getpgrp();
        if (pgrp == -1) {
            perror("pgrp");
        }
        if (!strcmp(filepath[1], "0")) {
            if (tcsetpgrp(0, pgrp) == -1) {
                perror("tcsetpgrp");
                exit_and_clean(1);
            }
        } else {
            if (fprintf(stdout, "%s%d%s%d%s\n", "[", jid, "] (", (int)pid,
                        ")") < 0) {
                fprintf(stderr, "fprintf failed");
            }
        }
        if ((signal(SIGINT, SIG_DFL) == SIG_ERR) ||
            (signal(SIGTSTP, SIG_DFL) == SIG_ERR) ||
            (signal(SIGTTOU, SIG_DFL) ==
             SIG_ERR)) {  // ignore signals 
            printf("sig_dfl error");
            exit_and_clean(1);
        }
        int i = 0;
        while (redirection[i] != NULL) {
            if (strcmp(redirection[i], ">") == 0) {
                if (close(1) == -1) {
                    perror("fd 1 failed closing");
                    exit_and_clean(1);
                }
                if (open(redirection[i + 1], O_WRONLY | O_CREAT | O_TRUNC,
                         0600) == -1) {
                    perror(redirection[i + 1]);
                    exit_and_clean(1);
                }
            } else if (strcmp(redirection[i], "<") == 0) {
                if (close(0) == -1) {
                    perror("fd 0 failed closing");
                    exit_and_clean(1);
                }
                if (open(redirection[i + 1], O_RDONLY) == -1) {
                    perror(redirection[i + 1]);
                    exit_and_clean(1);
                }
            } else if (strcmp(redirection[i], ">>") == 0) {
                if (close(1) == -1) {
                    perror("fd 1 failed closing");
                    exit_and_clean(1);
                }
                if (open(redirection[i + 1], O_CREAT | O_WRONLY | O_APPEND,
                         0600) == -1) {
                    perror(redirection[i + 1]);
                    exit_and_clean(1);
                }
            }
            i++;
        }
        char *arg_one = strrchr(
            argv[0],
            (int)'/'); /* dealing with '/' in command. strchr returns pointer to
                          first occurance of charachter in given string */
        if (arg_one != NULL) {
            arg_one++;
            argv[0] = arg_one;
        }
        if (execv(filepath[0], argv) == -1) {
            perror("failure for execv to execute");
            exit_and_clean(1);
        }
    }
    if (!strcmp(filepath[1], "1")) {  // adds a bg job using jid and pid
        if (add_job(job_list, jid, child, RUNNING, filepath[0]) == -1) {
            fprintf(stderr, "add_job failed 1");
        }
        jid++;  // incrementing job id
    }

    int status;  // initializes status; status gets overriden with proper status
                 // upon waitpid() call.

    if (!strcmp(filepath[1], "0")) {  // checking if a foreground process
        int pid_int = waitpid(child, &status, WUNTRACED);
        if (pid_int == -1) {
            perror("waitpid failed to execute");
            exit_and_clean(1);
        }

        if (WIFSTOPPED(status)) {  // checks for process suspension
            if (printf("%s%d%s%d%s%d\n", "[", jid, "] (", pid_int,
                       ") suspended by signal ", WSTOPSIG(status)) < 0) {
                fprintf(stderr, "printf failed");
            }
            if (!strcmp(filepath[1], "0")) { /* adding suspended foreground
                                                process to job list */
                if (add_job(job_list, jid, pid_int, STOPPED, filepath[0]) ==
                    -1) {
                    fprintf(stderr, "add_job failed 2");
                }
                jid++;
            }
        } else if (WIFSIGNALED(status)) {  // checks for process termination
            if (printf("%s%d%s%d%s%d\n", "[", jid, "] (", pid_int,
                       ") terminated by signal ", WTERMSIG(status)) < 0) {
                fprintf(stderr, "prinf failed");
            }
        }
        if (tcsetpgrp(0, getpid()) == -1) {  // returns terminal contol to shell
            perror("tcsetpgrp");
            exit_and_clean(1);
        }
    }
}

/*
reap: calls waitpid and checks the status of the process. if the process is
stopped by a signal, it prints a message. if a process is continued, it prints a
message. if the process exits normally, it prints a message and removes the job
from the job list. if the process is terminated by a signal, it prints a message
and removes the job from the job list. parameters: nothing returns: nothing
*/
void reap(void) {
    int status = 0;
    int pid;
    while ((pid = waitpid(-1, &status, WUNTRACED | WCONTINUED | WNOHANG)) > 0) {
        int current_jid = get_job_jid(job_list, pid);
        if (current_jid == -1) {
            fprintf(stderr, "get_job_jid failed");
        }
        if (WIFSTOPPED(status)) {  // checks for suspension and returns
                                   // associated status
            if (printf("%s%d%s%d%s%d\n", "[", current_jid, "] (", pid,
                       ") suspended by signal ", WSTOPSIG(status)) < 0) {
                fprintf(stderr, "printf failed");
            }
        } else if (WIFCONTINUED(status)) {  // checks for process being resumed
            if (printf("%s%d%s%d%s\n", "[", current_jid, "] (", pid,
                       ") resumed") < 0) {
                fprintf(stderr, "printf failed");
            }
        } else if (WIFEXITED(status)) {  // checks for process exit and returns
                                         // associated status
            if (printf("%s%d%s%d%s%d\n", "[", current_jid, "] (", pid,
                       ") terminated with exit status ",
                       WEXITSTATUS(status)) < 0) {
                fprintf(stderr, "printf failed");
            }
            if (remove_job_pid(job_list, pid) == -1) {
                fprintf(stderr, "remove_job_pid failed");
            }
        } else if (WIFSIGNALED(status)) {  // checks for termination but signal
                                           // and returns associated signal
            if (printf("%s%d%s%d%s%d\n", "[", current_jid, "] (", pid,
                       ") terminated by signal ", WTERMSIG(status)) < 0) {
                fprintf(stderr, "printf failed");
            }
            if (remove_job_pid(job_list, pid) == -1) {
                fprintf(stderr, "remove_job_pid failed");
            }
        }
    }
}

/*
fg: resumes a job in the foreground if it is suspended or brings a background
job into the foreground and resumes it if it is suspeneded. updates the state in
the job list if necessary and gives terminal control to the new foreground
process if there is one. also error checks any necessary functions. parameters:
argv, an array of strings containing the parsed input from the user returns:
nothing
*/
void fg(char *argv[512]) {
    int current_jid = atoi(argv[1] + 1);
    if (current_jid == 0) {
        fprintf(stderr, "atoi failed");
    }
    if (argv[1] == NULL || *argv[1] != '%') {
        fprintf(stderr, "fg: syntax error.\n");
        return;
    }
    pid_t current_pid = get_job_pid(job_list, current_jid);
    if (current_pid == -1) {
        fprintf(stderr, "job not found\n");
        return;
    }

    if (tcsetpgrp(0, current_pid) == -1) {
        perror("tcsetpgrp");
        exit_and_clean(1);
    }  // give terminal control to the process (aka move it into the foreground)

    int sig = kill(-current_pid, SIGCONT);  // sends signal to process group to resume
    if (update_job_pid(job_list, current_pid, RUNNING) == -1) {
        fprintf(stderr, "update_job_pid failed");
    }

    if (sig < 0) {
        perror("kill");
    }

    int status;

    int pid_int = waitpid(current_pid, &status, WUNTRACED);  // sets the status
    if (pid_int == -1) {
        perror("waitpid failed to execute");
        exit_and_clean(1);
    }

    // same signal/status checking as above
    if (WIFSTOPPED(status)) {
        if (printf("%s%d%s%d%s%d\n", "[", current_jid, "] (", current_pid,
                   ") suspended by signal ", WSTOPSIG(status)) < 0) {
            fprintf(stderr, "printf failed");
        }
        if (update_job_pid(job_list, current_pid, STOPPED) ==
            -1) {  // upadate job list given pid
            fprintf(stderr, "update_job_pid failed");
        }
    } else if (WIFSIGNALED(status)) {
        if (printf("%s%d%s%d%s%d\n", "[", current_jid, "] (", current_pid,
                   ") terminated by signal ", WTERMSIG(status)) < 0) {
            fprintf(stderr, "printf failed");
        }
        if (remove_job_pid(job_list, current_pid) == -1) {
            fprintf(stderr, "remove_job_pid failed");
        }
    } else if (WIFEXITED(status)) {
        if (remove_job_pid(job_list, current_pid) ==
            -1) {  // upadate job list given pid
            fprintf(stderr, "remove_job_pid failed");
        }
    } else if (WIFCONTINUED(status)) {
        if (printf("%s%d%s%d%s\n", "[", current_jid, "] (", current_pid,
                   ") resumed") < 0) {
            fprintf(stderr, "printf failed");
        }
        if (update_job_pid(job_list, current_pid, RUNNING) == -1) {
            fprintf(stderr, "update_job_pid failed");
        }
    }
    if (tcsetpgrp(0, getpgrp()) == -1) {
        perror("tcsetpgrp");
        exit_and_clean(1);  // returns terminal control to the shell
    }
}

/*
bg: resumes a background process identified by the jid if it is suspended; does
nothing if it is running. also error checks any necessary functions.
parameters: argv, an array of strings containing the parsed input from the user
returns: nothing
*/
void bg(char *argv[512]) {
    int current_jid = atoi(argv[1] + 1);
    if (current_jid == 0) {
        fprintf(stderr, "atoi failed");
    }
    if (argv[1] == NULL || *argv[1] != '%') {
        fprintf(stderr, "bg: syntax error.\n");
        return;
    }
    pid_t current_pid = get_job_pid(job_list, current_jid);
    if (current_pid == -1) {
        fprintf(stderr, "job not found\n");
        return;
    }
    int sig = kill(-current_pid, SIGCONT);
    if (sig < 0) {
        perror("kill");
    }
}

/*
built_in: checks if argv[0] is one of the builtin commands. if so, it calls that
funtion; otherwise it calls handle_redirection.
parameters: argv, an array of strings containing the parsed input from the user;
filepath_and_fgbg_flag, an array of one string containing the full filepath of
the command; and redirection, an array of strings containing any redirection
symbols and files from the user.
returns: nothing.
*/
void built_in(char *argv[512], char *filepath_and_fgbg_flag[2],
              char *redirection[512]) {
    if (!strcmp(argv[0], "cd")) {
        cd(argv);
    } else if (!strcmp(argv[0], "ln")) {
        ln(argv);
    } else if (!strcmp(argv[0], "rm")) {
        rm(argv);
    } else if (!strcmp(argv[0], "exit")) {
        exit_and_clean(0);
    } else if (!strcmp(argv[0], "jobs")) {
        jobs(job_list);
    } else if (!strcmp(argv[0], "fg")) {
        fg(argv);
    } else if (!strcmp(argv[0], "bg")) {
        bg(argv);
    } else {
        handle_redirection(argv, filepath_and_fgbg_flag, redirection);
    }
}

/*
main: prompts the user if PROMPT flag is set. reads in the input from the stdin
and puts it in a string, buffer. if read is successful and does not return 0
(meaning user exits the shell), then main calls parse on the buffer and the empty
arrays tokens, argv, filepath, and redirection. memset is called on all of these
arrays in order to fill it will 0's or NULLS originally. if parse returns 0,
meaning it was successful, built_in is called on argv, filepath, and
redirection. when this is done, the user is reprompted (if the PROMPT flag is
set). this loop will continue until the user enters exits the shell.
parameters: nothing.
returns: an int, 0.
*/
int main(void) {
    job_list = init_job_list();
    jid = 1;

    while (1) {
#ifdef PROMPT
        if (printf("33sh> ") < 0) {
            fprintf(stderr, "printf prompt failed");
        }
        if (fflush(stdout) != 0) {
            perror("prompt printing failed");
        }
#endif

        size_t MAX = 1024;
        char buffer[MAX];
        memset(buffer, 0, MAX);
        char *tokens[512];
        memset(tokens, 0, 512 * sizeof(char *));
        char *argv[512];
        memset(argv, 0, 512 * sizeof(char *));
        char *filepath[2];
        memset(filepath, 0, 1 * sizeof(char *));
        char *redirection[512];
        memset(redirection, 0, 512 * sizeof(char *));

        if (signal(SIGINT, SIG_IGN) == SIG_ERR ||
            signal(SIGTSTP, SIG_IGN) == SIG_ERR ||
            signal(SIGTTOU, SIG_IGN) == SIG_ERR) {
            printf("sig_ign error");
        } /*handles signals with not fg process again*/

        if (read(0, buffer, MAX) != 0) {
            int p = parse(buffer, tokens, argv, filepath, redirection);
            if (p == 0) {
                built_in(argv, filepath, redirection);
            }
            reap();
        } else {
            exit_and_clean(1);
        }
    }
    return 0;
}