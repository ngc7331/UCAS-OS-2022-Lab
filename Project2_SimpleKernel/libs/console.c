#include <os/console.h>

// get a valid char
char console_getchar() {
    int ch;
    while ((ch=bios_getchar()) == -1);  // ignore -1 (no input)
    return ch;
}

// parse special chars & echo on screen
char console_parsechar(int enable_backspace) {
    char ch = console_getchar();
    // echo
    switch (ch) {
    case '\b': case 127:   // backspace
        // NOTE: this is for qemu in linux, may should be changed on board
        // "\33[K": clear the content from the cursor to the end
        if (enable_backspace)
            bios_putstr("\b\033[K");
        ch = 127;
        break;
    case '\n': case '\r':  // new line
        bios_putstr("\n\r");
        ch = '\n';
        break;
    case '\033':           // control, dont echo
        break;
    default:               // echo
        bios_putchar(ch);
    }
    return ch;
}

int console_getline(char buf[], int bufsize) {
    int i = 0, len = 0;
    char ch;
    while ((ch=console_parsechar(len>0 && i==len)) != '\n' && len < bufsize-1) {
        if (ch == 127) {              // backspace
            // only support backspace at the end of line
            // no input before, abort
            if (len <= 0) continue;
            // set cursor to the end of line and backspace
            char off[3] = {(len-i)/10+'0',(len-i)%10+'0','\0'};
            console_print("\033[__C\b\033[K", off);
            // len--
            i = --len;
        } else if (ch == '\033') {    // control characters
            /* arrow keys:
               up:    \033[A
               down:  \033[B
               right: \033\000[C\000
               left:  \033\000[D\000
             */

            // clear all inputs
            ch = console_getchar();      // '[' or '\000'
            if (ch == '\000')
                console_getchar();       // '['
            char d = console_getchar();  // 'A' / 'B' / 'C' / 'D'
            if (ch == '\000')
                console_getchar();

            // determine what to do
            if (d == 'A' || d == 'B') {
                // FIXME: just ignore up and down for now
            } else if (d == 'C' && i < len){
                i++;
                bios_putstr("\033[C");
            } else if (d == 'D' && i > 0) {
                i--;
                bios_putstr("\033[D");
            }
        } else {
            buf[i++] = ch;
            if (i > len) len++;
        }
    }
    buf[len] = '\0';
    return len;
}

int str_format(char *fmt, char *buf, int len, char *val) {
    int skipped_cnt = 0, escaped = 0;
    for (int i=0; i<len; i++) {
        *buf = fmt[i];
        if (!escaped && fmt[i] == '\\') {
            escaped = 1;
        } else if (!escaped && fmt[i] == '_') {
            if (!skipped_cnt) {
                *buf++ = *val++;
                skipped_cnt += *val==0;
            } else {
                skipped_cnt ++;
            }
        } else {
            buf++;
            escaped = 0;
        }
    }
    return len - skipped_cnt;
}

void console_print(char *fmt, char *val) {
    int len = strlen(fmt);
    char buf[len+2];
    len = str_format(fmt, buf, len, val);
    buf[len+1] = '\0';
    bios_putstr(buf);
}
