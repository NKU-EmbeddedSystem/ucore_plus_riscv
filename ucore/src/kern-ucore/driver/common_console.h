#ifndef __KERN_DRIVER_COMMON_CONSOLE__
#define __KERN_DRIVER_COMMON_CONSOLE__

#include <types.h>

/* *
 * Here we manage the console input buffer, where we stash characters
 * received from the keyboard or serial port whenever the corresponding
 * interrupt occurs.
 * */

#define CONSBUFSIZE 512

static struct {
	uint8_t buf[CONSBUFSIZE];
	uint32_t rpos;
	uint32_t wpos;
} cons;

/* cons_putc - print a single character @c to console devices */
void cons_putc(int c);

/* serial_putc - print character to serial port */
void serial_putc(int c);

/* *
 * cons_intr - called by device interrupt routines to feed input
 * characters into the circular console input buffer.
 * */
static void cons_intr(int (*proc) (void));

/* *
 * cons_getc - return the next input character from console,
 * or 0 if none waiting.
 * */
int cons_getc(void);

/* serial_intr - try to feed input characters from serial port */
void serial_intr(void);

int serial_proc_data(void);

#endif