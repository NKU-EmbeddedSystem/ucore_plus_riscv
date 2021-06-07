#include <common_console.h>
#include <types.h>
#include <sync.h>

/* cons_putc - print a single character @c to console devices */
void cons_putc(int c)
{
	bool intr_flag;
	local_intr_save(intr_flag);
	{
		serial_putc(c);
	}
	local_intr_restore(intr_flag);
}

/* *
 * cons_intr - called by device interrupt routines to feed input
 * characters into the circular console input buffer.
 * */
static void cons_intr(int (*proc) (void))
{
	int c;
	while ((c = (*proc) ()) != -1) {
		if (c != 0) {
			cons.buf[cons.wpos++] = c;
			if (cons.wpos == CONSBUFSIZE) {
				cons.wpos = 0;
			}
		}
	}
}

/* *
 * cons_getc - return the next input character from console,
 * or 0 if none waiting.
 * */
int cons_getc(void)
{
	int c = 0;
	bool intr_flag;
	local_intr_save(intr_flag);
	{
		// poll for any pending input characters,
		// so that this function works even when interrupts are disabled
		// (e.g., when called from the kernel monitor).
		serial_intr();

		// grab the next character from the input buffer.
		if (cons.rpos != cons.wpos) {
			c = cons.buf[cons.rpos++];
			if (cons.rpos == CONSBUFSIZE) {
				cons.rpos = 0;
			}
		}
	}
	local_intr_restore(intr_flag);
	return c;
}

/* serial_intr - try to feed input characters from serial port */
void serial_intr(void) {
    cons_intr(serial_proc_data);
}