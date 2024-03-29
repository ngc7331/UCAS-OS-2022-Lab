#include <screen.h>
#include <printk.h>
#include <os/string.h>
#include <os/sched.h>
#include <os/smp.h>
#include <os/irq.h>
#include <os/kernel.h>

#define SCREEN_WIDTH    80
#define SCREEN_HEIGHT   40
#define SCREEN_LOC(x, y) ((y) * SCREEN_WIDTH + (x))

/* screen buffer */
char new_screen[SCREEN_HEIGHT * SCREEN_WIDTH] = {0};
char old_screen[SCREEN_HEIGHT * SCREEN_WIDTH] = {0};

int scroll_base = 0;

/* cursor position */
static void vt100_move_cursor(int x, int y) {
    // \033[y;xH
    printv("%c[%d;%dH", 27, y, x);
}

/* clear screen */
static void vt100_clear() {
    // \033[2J
    printv("%c[2J", 27);
}

/* hidden cursor */
static void vt100_hidden_cursor() {
    // \033[?25l
    printv("%c[?25l", 27);
}

/* display cursor */
// static void vt100_display_cursor() {
//     // \033[?25h
//     printv("%c[?25h", 27);
// }

/* write a char */
static void screen_write_ch(char ch) {
    int cid = get_current_cpu_id();
    if (ch == '\n')
    {
        current_running[cid]->cursor_x = 0;
        current_running[cid]->cursor_y++;
        while (current_running[cid]->cursor_y >= SCREEN_HEIGHT) {
            current_running[cid]->cursor_y --;
            strncpy(new_screen + SCREEN_LOC(0, scroll_base),
                    new_screen + SCREEN_LOC(0, scroll_base + 1),
                    SCREEN_WIDTH * (SCREEN_HEIGHT - 1 - scroll_base)
            );
            for (int i=0; i<SCREEN_WIDTH; i++)
                new_screen[SCREEN_LOC(i, SCREEN_HEIGHT - 1)] = ' ';
        }
    }
    else
    {
        new_screen[SCREEN_LOC(current_running[cid]->cursor_x, current_running[cid]->cursor_y)] = ch;
        current_running[cid]->cursor_x++;  // FIXME: this may overflow to next line?
    }
}

void init_screen(void) {
    vt100_hidden_cursor();
    vt100_clear();
    screen_clear();
}

void screen_set_scroll_base(int base) {
    scroll_base = base;
}

void screen_clear(void) {
    int cid = get_current_cpu_id();
    int i, j;
    for (i = 0; i < SCREEN_HEIGHT; i++)
    {
        for (j = 0; j < SCREEN_WIDTH; j++)
        {
            new_screen[SCREEN_LOC(j, i)] = ' ';
        }
    }
    current_running[cid]->cursor_x = 0;
    current_running[cid]->cursor_y = 0;
    screen_reflush();
}

void screen_move_cursor(int x, int y) {
    int cid = get_current_cpu_id();
    current_running[cid]->cursor_x = x;
    current_running[cid]->cursor_y = y;
    vt100_move_cursor(x, y);
}

void screen_move_cursor_r(int x, int y) {
    int cid = get_current_cpu_id();
    int nx = current_running[cid]->cursor_x + x;
    int ny = current_running[cid]->cursor_y + y;
    if (nx < 0) nx = 0;
    if (nx > SCREEN_WIDTH) nx = SCREEN_WIDTH;
    if (ny < 0) ny = 0;
    if (ny > SCREEN_HEIGHT) ny = SCREEN_HEIGHT;
    screen_move_cursor(nx, ny);
}

void screen_write(char *buff) {
    int i = 0;
    int l = strlen(buff);

    for (i = 0; i < l; i++)
    {
        screen_write_ch(buff[i]);
    }
}

/*
 * This function is used to print the serial port when the clock
 * interrupt is triggered. However, we need to pay attention to
 * the fact that in order to speed up printing, we only refresh
 * the characters that have been modified since this time.
 */
void screen_reflush(void) {
    int cid = get_current_cpu_id();
    int i, j;

    /* here to reflush screen buffer to serial port */
    for (i = 0; i < SCREEN_HEIGHT; i++)
    {
        for (j = 0; j < SCREEN_WIDTH; j++)
        {
            /* We only print the data of the modified location. */
            if (new_screen[SCREEN_LOC(j, i)] != old_screen[SCREEN_LOC(j, i)])
            {
                vt100_move_cursor(j + 1, i + 1);
                bios_putchar(new_screen[SCREEN_LOC(j, i)]);
                old_screen[SCREEN_LOC(j, i)] = new_screen[SCREEN_LOC(j, i)];
            }
        }
    }

    /* recover cursor position */
    vt100_move_cursor(current_running[cid]->cursor_x, current_running[cid]->cursor_y);
}
