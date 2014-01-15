#ifndef DUTILS_H_
#define DUTILS_H_

#include "LPC8xx.h"
#include "config.h"

#ifdef DEBUG
#define ASSERT(x) if (!(x)) __ASM volatile("BKPT #01");
//#define NV_STORE_START 0x3400				// Storage range for logging in debug mode
#else
#define ASSERT(x)
#endif

// Some magic incantations to create a ringbuffer - this is done with an external size to avoid needing to
// keep all of the struct data in the initalised data section
#define dringbuffer_create(x,sizex)  static struct { char s[sizex]; volatile uint32_t rp; uint32_t wp; } rb_##x; const uint32_t rb_ ## x ## _size=sizex
#define dringbuffer_reset(x) rb_##x.rp=rp_##x.wp=0
#define dringbuffer_elements(x) ((rb_##x.wp+rb_##x ## _size-rb_##x.rp)%rb_##x ## _size)
#define dringbuffer_full(x) (dringbuffer_elements(x)==rb_##x ##_size-1)
#define dringbuffer_empty(x) (!dringbuffer_elements(x))
#define dringbuffer_putByte(x,y) if (!dringbuffer_full(x)) { rb_##x.s[(rb_##x.wp=(rb_##x.wp+1)%rb_##x ##_size)]=y; }
#define dringbuffer_getByte(x) ((!dringbuffer_empty(x))?rb_##x.s[(rb_##x.rp=(rb_##x.rp+1)%rb_##x ##_size)]:0)

#define _dabs(x) ((x<0)?-x:x)								/* Get absolute value of operand */
#define _dsign(x) ((x<0)?-1:1)								/* Get sign of operand */
#define _dsignnumdiv(a,b) (((a)>=0)?(a)/(b):-((-(a))/(b)))	/* Performed signed division correctly */
void denter_critical(void);			  						// Enter a critical section
void dleave_critical(void);									// Exit a critical section
// ============================================================================================

#endif /* DUTILS_H_ */
