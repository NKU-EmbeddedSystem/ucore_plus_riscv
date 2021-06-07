#include <sbi.h>
#include <sync.h>
#include <types.h>
#include <console.h>
#include <common_console.h>

/* kbd_intr - try to feed input characters from keyboard */
void kbd_intr(void) {}

/* cons_init - initializes the console devices */
void cons_init(void) {}

/* serial_putc - print character to serial port */
void serial_putc(int c) {
    if (c != '\b') {
        sbi_console_putchar(c);
    } else {
        sbi_console_putchar('\b');
        sbi_console_putchar(' ');
        sbi_console_putchar('\b');
    }
}

/* serial_proc_data - get data from serial port */
int serial_proc_data(void) {
    int c = sbi_console_getchar();
    if (c < 0) {
        return -1;
    }
    if (c == 127) {
        c = '\b';
    }
    return c;
}