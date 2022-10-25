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

// #define S_CORE

static char getchar();
static int getline(char buf[], int bufsize);
int round_add(int *a, int b, int lim, int orig) {
    int tmp = *a;
    *a += b;
    while (*a >= lim) *a -= lim;
    while (*a < 0) *a += lim;
    return orig ? tmp : *a;
}

char history[HISTSIZE][BUFSIZE];
int hp = 0;

static void init_shell() {
    sys_move_cursor(0, SHELL_BEGIN);
    printf("------------------- COMMAND -------------------\n");
}

static int iscmd(const char *cmd, char *buf) {
    int len = strlen(cmd);
    return strncmp(buf, cmd, len-1) == 0 && (isspace(buf[len]) || buf[len] == '\0');
}

int main(void) {
    init_shell();

    char buf[BUFSIZE];

    pid_t self = sys_getpid();
    printf("Shell inited: pid=%d\n", self);

    while (1)
    {
        printf("> root@UCAS_OS: ");
        // call syscall to read UART port & parse input
        int len = getline(buf, BUFSIZE);
        len = strip(buf);
        // record history for later use
        strncpy(history[round_add(&hp, 1, HISTSIZE, 1)], buf, len);

        // ps, exec, kill, clear
        if (iscmd("ps", buf)) {
            sys_ps();
        } else if (iscmd("clear", buf)) {
            sys_clear();
            init_shell();
        } else if (iscmd("exec", buf)) {
            int argc = 0, bg = buf[len-1] == '&' && isspace(buf[len-2]);
            char *pbuf = buf + 5;
#ifdef S_CORE
            char arg[BUFSIZE];
            uint64_t args[4];
            for (; argc<4; argc++) {
                while (isspace(*pbuf)) pbuf++;
                int i;
                for (i=0; pbuf[i] != '\0' && !isspace(pbuf[i]); i++)
                    arg[i] = pbuf[i];
                if (!i)  // no arg
                    break;
                arg[i] = '\0';
                args[argc] = atoi(arg);
                pbuf += i;
            }
            pid_t pid = sys_exec(args[0], argc-1, args[1], args[2], args[3]);
#else
            char *argv[BUFSIZE / 2];
            while (*pbuf) {
                while (isspace(*pbuf)) pbuf++;
                // pbuf -> start of an arg
                argv[argc++] = pbuf;
                while (!isspace(*pbuf) && *pbuf) pbuf++;
                // pbuf -> next ch of the end of an arg, should be space or '\0'
                if (*pbuf) *pbuf++ = '\0';
            }
            pid_t pid = sys_exec(argv[0], argc, argv);
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
        } else if (iscmd("kill", buf)) {
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

static void clearline(int len) {
    char blank[] = "                                ";
    int blank_size = 32;
    sys_move_cursor_r(-len, 0);
    sys_write(blank);
    sys_move_cursor_r(-blank_size, 0);
    sys_reflush();
}

static int getline(char buf[], int bufsize) {
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
        case '\033':
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
