

#define _BSD_SOURCE

#include <wait.h>
#include <unistd.h>
#include <memory.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdio.h>


#define MAX_PATH (128)
#define ARG_MAX (128)
#define ARGS_MAX (512)
#define MAX_JOBS (512)
#define BUFFER_SIZE (1024)

void regularShell(int argc, char **argv, int backFlag);

typedef struct Job {
    int argc;
    char **argv;
    int pid;
} Job;

// Globals
Job *jobs[MAX_JOBS];
int lastJ;
char curDir[MAX_PATH];
char *builtinsNames[] = {"exit", "cd"};


/**
 * CD command
 * @param argv
 */
void cdCommand(char **argv) {
    // -
    if (0==strcmp("-", argv[1])) {
        printf("%s", curDir);
        chdir(curDir);
        return;
    }

    // ~
    if (0==strcmp("~", argv[1]) || argv[1] == NULL ) {
        getcwd(curDir, sizeof(curDir));
        chdir(getenv("HOME"));
        return;
    }

    // regular form
    getcwd(curDir, sizeof(curDir));
    if (chdir(argv[1])) {
        usleep(2);
        fprintf(stderr, "CD Error\n");
        usleep(2);
    }
}


/**
 * Exit command
 * @param argv
 */
void exitCommand(char **argv) {
    int i, r;
    for ( i = lastJ; i > 0; i--) {
        if (jobs[i] != NULL) {
            for (r = 0; r < jobs[i]->argc; ++r) {
                free(jobs[i]->argv[r]);
            }
            free(jobs[i]->argv);
            free(jobs[i]);
        }
    }
    _exit(0);
}


/**
 * pointer to built in funcs
 * @return pointers
 */
void (*builtinsFuncs[])(char **) = {&exitCommand, &cdCommand};


/**
 * Garbage collector
 */
void compressJobs() {
    Job* copy[MAX_JOBS + 1];
    copy[MAX_JOBS + 1] = NULL;

    // save a copy
    int i, j = 0, k = 0;
    for (i = 0; i < MAX_JOBS; ++i) {
        if (jobs[i] == NULL) {
            copy[i] = NULL;
        } else {
            copy[j] = jobs[i];
        }
        ++j;
    }
    // restore
    while (copy[k] != NULL) {
        jobs[k] = copy[k];
        ++k;
    }
    lastJ = k - 1;
}


/**
 * Read Line
 * @return line
 */
char *readLine() {
    int bufSize = BUFFER_SIZE;
    int i = 0;
    char *l = (char *) malloc(sizeof(char) * BUFFER_SIZE);
    if (l == NULL) {
        fprintf(stderr, "Bad Malloc\n");
    }

    while (1) {
        // is need more memory
        if (i >= bufSize) {
            bufSize *= 2;
            l = (char *) realloc(l, sizeof(char) * bufSize);
            if (l == NULL) {
                fprintf(stderr, "Bad Malloc\n");
            }
        }
        int c = getchar();
        // mark end of line with #
        if (c != '\n' && c != EOF) {
            l[i] = (char)c;
        } else {
            l[i] = '#';
            return l;
        }
        i++;
    }
}


/**
 * Parse line and exeute it
 * @param line
 */
void parseLine(const char *line) {
    int k, q, a, h, w, r, j;
    int argc = 0, path = 0, backFlag = 0, i = 0;
    char **argv = (char **) malloc(ARGS_MAX * sizeof(char *));
    if (argv == NULL) {
        fprintf(stderr, "Bad Malloc\n");
    }

    // init array to null
    for (k = ARGS_MAX; k > 0; --k) {
        argv[k] = NULL;
    }

    // allocate memory for args
    while (!(line[i] == '#')) {
        char *arg = (char *) malloc(sizeof(char) * ARG_MAX);
        if (arg == NULL) {
            fprintf(stderr, "Bad Malloc\n");
        }

        // ignore whileSpaces
        while (!(line[i] == '#' || line[i] != ' ')) {
            i++;
        }

        int j = 0;
        // parse
        while (line[i] != '#' && (line[i] != ' ' || path)) {
            // "something something" format
            if (line[i] != '"') {
                arg[j] = line[i];
                j++;
            } else {
                path = (path + 1) % 2;
            }
            ++i;
        }

        // args end
        arg[j] = '\0';
        // put in array
        argv[argc] = arg;

        // ignore whitespaces
        while (!(line[i] == '#' || line[i] != ' ')) {
            ++i;
        }
        ++argc;
    }

    // background process
    if (0==strcmp("&", argv[argc - 1])) {
        free(argv[argc - 1]);
        argv[argc - 1] = NULL;
        argc--;
        backFlag = 1;
    }

    // exe built in funcs
    for (q = 0; q < 2; q++) {
        if (0==strcmp(builtinsNames[q], argv[0])) {
            // print pid
            printf("%d\n", getpid());
            // call func
            (*builtinsFuncs[q])(argv);
            for (r = 0; r < argc; ++r) {
                free(argv[r]);
            }
            free(argv);
            return;
        }
    }
    // try run jobs
    if (0==strcmp(argv[0], "jobs")) {
        for (a = 0; a < lastJ; ++a) {
            if (jobs[a] != NULL) {
                // for finished job
                if (waitpid(jobs[a]->pid, NULL, WNOHANG != 0)) {

                    if (jobs[a] != NULL) {
                        for (w = 0; w < jobs[a]->argc; ++w) {
                            free((jobs[a]->argv)[w]);
                        }
                        free(jobs[a]->argv);
                        free(jobs[a]);
                    }

                    jobs[a] = NULL;

                    // unfinished process
                } else {
                    char* data;
                    printf("%d ", jobs[a]->pid);
                    // print job's data
                    for (j = 0; j < jobs[a]->argc - 1; ++j) {
                        data = jobs[a]->argv[j];
                        printf("%s ", data);
                    }
                    data = jobs[a]->argv[jobs[a]->argc - 1];
                    printf("%s\n", data);
                }
            }
        }
        for (h = 0; h < argc; ++h) {
            free(argv[h]);
        }
        free(argv);
        return;
    }
    // run regular
    regularShell(argc, argv, backFlag);
}

/**
 * run regular shell
 * @param argc num of args
 * @param argv params
 * @param backFlag bool flag
 */
void regularShell(int argc, char **argv, int backFlag) {
    pid_t childPid = fork();

    //  dad
    if (childPid > 0) {
        printf("%d\n", childPid);
    }

    // error
    if (childPid < 0) {
        fprintf(stderr, "Fork Error\n");
    }

        // child
    else if (childPid == 0) {
        if (execvp(argv[0], argv) == -1) {
            usleep(2);
            fprintf(stderr, "System call Error\n");
            usleep(2);
        }
        _exit(EXIT_FAILURE);

        // parent
    } else {
        // background process
        if (backFlag) {
            Job *newJob = (Job *) malloc(sizeof(Job));
            newJob->argv = argv;
            newJob->argc = argc;
            newJob->pid = childPid;

            jobs[lastJ] = newJob;
            ++lastJ;

            // array is full
            if (lastJ == MAX_JOBS) {
                compressJobs();
            }

            // regular process
        } else {
            int r;
            for (r = 0; r < argc; ++r) {
                free(argv[r]);
            }
            free(argv);
            waitpid(childPid, NULL, WUNTRACED);
        }
    }
}


/**
 * Main
 * @return
 */
int main() {
    // initial globals
    lastJ = 0;
    int i;
    for (i = MAX_JOBS; i > 0; i--) {
        jobs[i] = NULL;
    }
    while (1) {
        printf("> ");
        char *l = readLine();
        parseLine(l);
        free(l);
    }
}
