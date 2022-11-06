#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#define SHELL_BEGIN 20
#define BUFSIZE 64
#define HISTSIZE 16

pid_t self;
char history[HISTSIZE][BUFSIZE];
int hp = 0;

int round_add(int *a, int b, int lim, int orig) {
    int tmp = *a;
    *a += b;
    while (*a >= lim) *a -= lim;
    while (*a < 0) *a += lim;
    return orig ? tmp : *a;
}

char getchar() {
    int ch;
    while ((ch=sys_getchar()) == -1);
    return ch;
}

void putchar(char ch) {
    char buf[2] = {ch, '\0'};
    sys_write(buf);
    sys_reflush();
}

void backspace() {
    sys_move_cursor_r(-1, 0);
    sys_write(" ");
    sys_move_cursor_r(-1, 0);
    sys_reflush();
}

void clearline(int len) {
    char blank[] = "                                ";
    int blank_size = 32;
    sys_move_cursor_r(-len, 0);
    sys_write(blank);
    sys_move_cursor_r(-blank_size, 0);
    sys_reflush();
}

int getline(char buf[], int bufsize) {
    int tmphp = hp;
    int len = 0;
    char ch;
    while ((ch=getchar()) != '\n' && ch != '\r' && len < bufsize-1) {
        switch (ch) {
        case '\b': case 127:
            if (!len) break;
            buf[--len] = '\0';
            backspace();
            break;
        case 3: // ctrl+c
            clearline(len);
            buf[len=0] = '\0';
            break;
        case 4: case 12: // ctrl+d / ctrl+l
            buf[0] = ch;
            buf[1] = '\0';
            return 0;
        case 27:
            /* handle control:
             * up:    \033[A
             * down:  \033[B
             * right: \033\000[C\000
             * left:  \033\000[D\000
             */
            if (getchar() == '\000')  // align to '['
                getchar();
            if ((ch=getchar()) == 'A' || ch == 'B') {
                strcpy(buf, history[round_add(&tmphp, ch=='A' ? -1 : 1, HISTSIZE, 0)]);
                clearline(len);
                sys_write(buf);
                sys_reflush();
                len = strlen(buf);
            } else { // 'C' || 'D'
                // FIXME: do nothing now
                getchar(); // remove '\000'
            }
            break;
        default:
            buf[len++] = ch;
            putchar(ch);
        }
    }
    putchar('\n');
    buf[len] = '\0';
    return len;
}

static void init_shell() {
    sys_clear();
    sys_move_cursor(0, SHELL_BEGIN);
    printf("------------------- COMMAND -------------------\n");
}

static int confirm(char msg[], int d) {
    char buf[BUFSIZE];
    printf("%sConfirm (%s)? ", msg, d?"Y/n":"y/N");
    getline(buf, BUFSIZE);
    strip(buf);
    lower(buf);
    if ((!d && !(strcmp("y", buf)==0 || strcmp("yes", buf)==0)) ||
        ( d && !(strcmp("n", buf)==0 || strcmp("no",  buf)==0))
    ) {
        printf("Abort\n");
        return 0;
    }
    return 1;
}

static void exit() {
    printf("Bye\n");
    sys_kill(self);
}

int main(void) {
    self = sys_getpid();
    sys_set_scroll_base(SHELL_BEGIN + 1);
    init_shell();

    char buf[BUFSIZE];

    printf("Shell inited: pid=%d\n", self);

    while (1)
    {
        printf("> root@UCAS_OS: ");
        // call syscall to read UART port & parse input
        int len = getline(buf, BUFSIZE);
        if (!len) {
            if (buf[0] == 4) exit();              // ctrl+d
            else if (buf[0] == 12) init_shell();  // ctrl+l
            continue;
        } else if (!(len = strip(buf))) {
            continue;
        }
        // record history for up/down
        strcpy(history[round_add(&hp, 1, HISTSIZE, 1)], buf);

        // argparser
        int argc = 0;
        char *argv[BUFSIZE / 2];
        char *pbuf = buf;
        while (*pbuf) {
            while (isspace(*pbuf)) pbuf++;
            // pbuf -> start of an arg
            argv[argc++] = pbuf;
            while (!isspace(*pbuf) && *pbuf) pbuf++;
            // pbuf -> next ch of the end of an arg, should be space or '\0'
            if (*pbuf) *pbuf++ = '\0';
        }

        // commands
        if (strcmp("ps", argv[0]) == 0) {
            sys_ps();
        } else if (strcmp("clear", argv[0]) == 0) {
            init_shell();
        } else if (strcmp("exec", argv[0]) == 0) {
            int bg = argv[argc-1][0] == '&' && argv[argc-1][1] == '\0';
            // remove cmd and last '&' from args
            argc -= bg + 1;
#ifdef S_CORE
            // no arg
            if (!argc) {
                printf("Error: task id can't be empty\nUsage: exec id [arg0] ... [arg3]\n");
                continue;
            }
            // exec
            pid_t pid = sys_exec(atoi(argv[1]), argc, argc>0 ? atoi(argv[2]) : 0,
                                                      argc>1 ? atoi(argv[3]) : 0,
                                                      argc>2 ? atoi(argv[4]) : 0);
#else
            // no arg
            if (!argc) {
                printf("Error: task name can't be empty\nUsage: exec name [arg0] ...\n");
                continue;
            }
            // exec
            pid_t pid = sys_exec(argv[1], argc, argv+1);
#endif
            if (!pid) {
                printf("Execute failed\n");
            } else if (bg) {
                printf("Process created with pid=%d\n", pid);
            } else {
                printf("Process created with pid=%d, waiting... ", pid);
                sys_waitpid(pid);
                printf("Done\n");
            }
        } else if (strcmp("exit", argv[0]) == 0) {
            exit();
        } else if (strcmp("kill", argv[0]) == 0) {
            if (argc == 1) {
                printf("Error: pid can't be empty\nUsage: kill pid\n");
                continue;
            }
            pid_t pid = atoi(argv[1]);
            if (pid == 0) {
                printf("Error: pid can't be 0\n");
                continue;
            } else if (pid == self && !confirm("Warning: you're trying to kill the shell\n", 0)) {
                continue;
            }
            int retval = sys_kill(pid);
            switch (retval) {
            case -1:
                printf("Critical error, abort\n");
                break;
            case 0:
                printf("No process found with pid=%d\n", pid);
                break;
            case 1:
                printf("Success\n");
                break;
            default:
                printf("Internal Error\n");
            }
        } else if (strcmp("help", argv[0]) == 0) {
            printf("--- HELP START ---\n");
            printf("  ps: show processes\n");
            printf("  clear: clear screen\n");
            printf("  exec name [arg0] ...: start a new process\n");
            printf("  exit: exit shell\n");
            printf("  kill pid: kill a existing process\n");
            printf("  help: print this help message\n");
            printf("  ts: show tasks\n");
            printf("  taskset -p mask pid / taskset mask name [arg0] ...: set pid's mask\n");
            printf("  shortcut keys:\n");
            printf("     Ctrl+C: clear line\n");
            printf("     Ctrl+D: exit shell\n");
            printf("     Ctrl+L: clear screen\n");
            printf("---- HELP END ----\n");
        } else if (strcmp("ts", argv[0]) == 0) {
            sys_show_task();
#ifndef S_CORE
        } else if (strcmp("taskset", argv[0]) == 0) {
            unsigned mask;
            pid_t pid;
            if (strcmp("-p", argv[1]) == 0) {
                mask = atoi(argv[2]);
                pid = atoi(argv[3]);
            } else {
                mask = atoi(argv[1]);
                pid = sys_exec(argv[2], argc-2, argv+2);
                printf("Process created with pid=%d\n", pid);
            }
            sys_taskset(pid, mask);
            printf("Set pid=%d's mask=0x%04x\n", pid, mask);
#endif
        } else {
            printf("Command %s not found\n", buf);
        }
    }
    return 0;
}
