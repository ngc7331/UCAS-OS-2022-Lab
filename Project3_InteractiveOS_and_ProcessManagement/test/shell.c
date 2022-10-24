/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 *               Author : Han Shukai (email : hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *                  The shell acts as a task running in user mode.
 *       The main function is to make system calls through the user's output.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#define SHELL_BEGIN 20
#define BUFSIZE 64
#define HISTSIZE 16

static char getchar();
static int getline(char buf[], int bufsize);

static void init_shell() {
    sys_move_cursor(0, SHELL_BEGIN);
    printf("------------------- COMMAND -------------------\n");
}

int main(void) {
    init_shell();

    char history[HISTSIZE][BUFSIZE];
    char buf[BUFSIZE];
    int hp = 0;

    pid_t self = sys_getpid();
    printf("Shell inited: pid=%d\n", self);

    while (1)
    {
        printf("> root@UCAS_OS: ");
        // call syscall to read UART port & parse input
        int len = getline(buf, BUFSIZE);
        len = strip(buf);
        // record history for later use
        strncpy(history[hp++], buf, len);
        if (hp == HISTSIZE) hp = 0;

        // ps, exec, kill, clear
        if (strncmp("ps", buf, 1) == 0) {
            sys_ps();
        } else if (strncmp("clear", buf, 4) == 0) {
            sys_clear();
            init_shell();
        } else if (strncmp("exec", buf, 3) == 0) {
            char arg[BUFSIZE];
            int i = 4, argc;
            int bg = buf[len-1] == '&';
            uint64_t args[4];
            for (argc=0; argc<4; argc++) {
                int j;
                lstrip(buf+i);
                for (j=0; buf[i+j] != '\0' && !isspace(buf[i+j]); j++)
                    arg[j] = buf[i+j];
                if (!j)  // no arg
                    break;
                args[argc] = atoi(arg);
                i += j;
            }
            pid_t pid = sys_exec(args[0], argc, args[1], args[2], args[3]);
            if (bg) {
                printf("Process created with pid=%d\n", pid);
            } else {
                sys_waitpid(pid);
            }
        } else if (strncmp("kill", buf, 3) == 0) {
            lstrip(buf+4);
            pid_t pid = atoi(buf+4);
            if (pid == self) {
                printf("Warning: you're trying to kill the shell\nConfirm (y/N)? ");
                getline(buf, BUFSIZE);
                lower(buf);
                if (!(strcmp("y", buf)==0 || strcmp("yes", buf)==0)) {
                    printf("Abort\n");
                    continue;
                }
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
        } else {
            printf("Command %s not found\n", buf);
        }
    }
    return 0;
}

static char getchar() {
    int ch;
    while ((ch=sys_getchar()) == -1);
    return ch;
}

static void putchar(char ch) {
    char buf[2] = {ch, '\0'};
    sys_write(buf);
    sys_reflush();
}

static void backspace() {
    sys_move_cursor_r(-1, 0);
    sys_write(" ");
    sys_move_cursor_r(-1, 0);
    sys_reflush();
}

static int getline(char buf[], int bufsize) {
    int len = 0;
    char ch;
    while ((ch=getchar()) != '\n' && ch != '\r' && len < bufsize-1) {
        switch (ch) {
        case '\b': case 127:
            if (!len) break;
            buf[--len] = '\0';
            backspace();
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
