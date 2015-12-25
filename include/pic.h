#ifndef _PIC_H_
#define _PIC_H_

#include <stdint.h>

void pic_remap(uint8_t, uint8_t);
void pic_send_eoi(uint8_t);
void pic_enable(void);
void pic_disable(void);

#endif // _PIC_H_
