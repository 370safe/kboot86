#include <console.h>
#include <pic.h>
#include <idt.h>
#include <intr.h>
#include <pit.h>
#include <shell.h>
#include <kstdio.h>
#include <hdd.h>
#include <fat16.h>
#include <elf32.h>
#include <jmp.h>
#include <paging.h>
#include <vga.h>
#include <shell.h>
#include <string.h>
#include <timer.h>
#include <halt.h>

// TODO replace with a more elegant solution
//#define intr(n) asm volatile ("int $" #n : : : "cc", "memory")

#define FILENAME "kernel.elf"
#define BUF_SIZE (1 << 20)
static void *_mem_load_addr = (void *) (2 << 20);

uint32_t cr0_get(void)
{
    uint32_t cr0;

    asm volatile (
                    "movl %%cr0, %0\n\t"
                    :   "=r" (cr0)
                 );
    return cr0;
}

void main(void)
{
    intr_disable();

    pic_init();
    idt_init();

    if (pit_init())
        goto error;

    console_init();

    kprintf("kboot86\n");
    
    kprintf("initializing paging...");
    if (paging_init())
        goto error;
    // first page of the address space will not be mapped
    if (paging_unmap(0))
        goto error;
    paging_enable();
    kprintf("ok\n");

    // XXX
    {
        uint32_t cr0;

        cr0 = cr0_get();
        kprintf("CR0: %08x\n", cr0);
    }

#if 0
    // XXX
    {
        timer_t point, a, b, c, d;
        
        intr_enable();
        kprintf("starting timers...\n");

#define CNTPOINT    2
#define CNTA        10
#define CNTB        20
#define CNTC        50
#define CNTD        100

        timer_start(&point, CNTPOINT);
        timer_start(&a, CNTA);
        timer_start(&b, CNTB);
        timer_start(&c, CNTC);
        timer_start(&d, CNTD);
        
        do {
            if (timer_is_triggered(&point)) {
                kprintf(".");
                timer_restart(&point);
            }

            if (timer_is_triggered(&a)) {
                kprintf("a");
                timer_restart(&a);
            }

            if (timer_is_triggered(&b)) {
                kprintf("b");
                timer_restart(&b);
            }
    
            if (timer_is_triggered(&c)) {
                kprintf("C");
                timer_restart(&c);
            }

            timer_restart(&d);
            if (timer_is_triggered(&d)) {
                kprintf("D");
            }

            halt();
        } while (1);
    }
#endif
    
    // XXX 
    //intr_enable();
    //shell_do();
   
    kprintf("initializing hdd...");
    if (hdd_init() || fat16_init(0))
        goto error;
    kprintf("ok\n");
    

    {
        int count;
        uintptr_t jmp_addr;

        kprintf("file size: %d\n", fat16_get_file_size(FILENAME));
        if (-1 == (count = fat16_load(FILENAME, _mem_load_addr, BUF_SIZE))) {
            kprintf("error while loading file %s\n", FILENAME);
            goto error;
        } else {
            kprintf("read: %d bytes\n", count);
        }

        if (!(jmp_addr = elf32_map(_mem_load_addr))) {
            static char str_error[128];
            ksprintf(str_error, "error while mapping ELF: %s\n",
                        elf32_strerror());
            vga_set_bsod();
            console_puts_err(str_error);
            goto error;
        }
        kprintf("jmp addr: %x\n", jmp_addr);
        kprintf("vaddr: %x => paddr: %x\n",
                jmp_addr, paging_vaddr2paddr(jmp_addr));

        intr_disable();
        jmp(jmp_addr);
    }

error:
    kprintf("\n*** ERROR! ***\n");

    shell_do();
}
