/*******************************************************************************/
/*  © Université Lille 1, The Pip Development Team (2015-2017)                 */
/*                                                                             */
/*  This software is a computer program whose purpose is to run a minimal,     */
/*  hypervisor relying on proven properties such as memory isolation.          */
/*                                                                             */
/*  This software is governed by the CeCILL license under French law and       */
/*  abiding by the rules of distribution of free software.  You can  use,      */
/*  modify and/ or redistribute the software under the terms of the CeCILL     */
/*  license as circulated by CEA, CNRS and INRIA at the following URL          */
/*  "http://www.cecill.info".                                                  */
/*                                                                             */
/*  As a counterpart to the access to the source code and  rights to copy,     */
/*  modify and redistribute granted by the license, users are provided only    */
/*  with a limited warranty  and the software's author,  the holder of the     */
/*  economic rights,  and the successive licensors  have only  limited         */
/*  liability.                                                                 */
/*                                                                             */
/*  In this respect, the user's attention is drawn to the risks associated     */
/*  with loading,  using,  modifying and/or developing or reproducing the      */
/*  software by the user in light of its specific status of free software,     */
/*  that may mean  that it is complicated to manipulate,  and  that  also      */
/*  therefore means  that it is reserved for developers  and  experienced      */
/*  professionals having in-depth computer knowledge. Users are therefore      */
/*  encouraged to load and test the software's suitability as regards their    */
/*  requirements in conditions enabling the security of their systems and/or   */
/*  data to be ensured and,  more generally, to use and operate it in the      */
/*  same conditions as regards security.                                       */
/*                                                                             */
/*  The fact that you are presently reading this means that you have had       */
/*  knowledge of the CeCILL license and that you accept its terms.             */
/*******************************************************************************/

/**
 * \file ial_init.c
 * \brief x86 interrupt abstraction layer initialization.
 */

#include "debug.h"
#include "port.h"
#include "pic8259.h"
#include "fpinfo.h"
#include "ial_defines.h"
#include "x86int.h"
#include "ial.h"
#include "libc.h"
#include "lapic.h"

uint32_t timer_ticks = 0;

static idt_entry_t idt_entries[256];	//!< Interrupt Descriptor Table
static idt_ptr_t idt_ptr;		//!< Pointer to the IDT

uint32_t pcid_enabled = 0;

/* ial.S */
/**
 * \fn idtFlush(uint32_t idt_ptr)
 * \brief Installs and flushes the IDT
 * \param idt_ptr Pointer to the IDT structure
 */
extern void idtFlush (void* idt_ptr);

/**
 * \fn idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags)
 * \brief Installs a handler into the IDT
 * \param num Interrupt number
 * \param base Base address for the handler
 * \param sel Selector
 * \param flags Flags
 */
    void
idtSetGate (uint8_t num, uint32_t base, uint16_t sel, uint8_t flags)
{
    idt_entries[num].base_lo = base & 0xFFFF;
    idt_entries[num].base_hi = (base >> 16) & 0xFFFF;

    idt_entries[num].sel = sel;
    idt_entries[num].always0 = 0;

    idt_entries[num].flags = flags;
}

/**
 * \fn remapIrq
 * \brief Remaps IRQ from int. 0-15 to int. 33-48
 */
    void
remapIrq (void)
{
#define PIC1_OFFSET	0x20
#define PIC2_OFFSET	0x28

#ifdef KEEP_PIC_MASK
    uint8_t a1, a2;
    /* save masks */
    a1 = inb (PIC1_DATA);
    a2 = inb (PIC2_DATA);
#endif

    /* starts the initialization sequence (in cascade mode) */
    outb (PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    outb (PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    outb (PIC1_DATA, PIC1_OFFSET);
    outb (PIC2_DATA, PIC2_OFFSET);
    outb (PIC1_DATA, 0x04);	/* there is a slave PIC at IRQ2 */
    outb (PIC2_DATA, 0x02);	/* Slave PIC its cascade identity */
    outb (PIC1_DATA, ICW4_8086);
    outb (PIC2_DATA, ICW4_8086);

    /* masks */
#ifdef KEEP_PIC_MASK
    outb (PIC1_DATA, a1);
    outb (PIC2_DATA, a2);
#else
    outb (PIC1_DATA, 0);
    outb (PIC2_DATA, 0);
#endif
}

/**
 * \fn bindIrq()
 * \brief Binds IRQ handlers to corresponding IDT entries
 */
    void
bindIrq (void)
{
#define IRQ_IDT(X) { extern void irq ## X(); idtSetGate (32+X, (uint32_t) irq ## X, 0x08, 0x8E); }

    IRQ_IDT(0);
    IRQ_IDT(1);
    IRQ_IDT(2);
    IRQ_IDT(3);
    IRQ_IDT(4);
    IRQ_IDT(5);
    IRQ_IDT(6);
    IRQ_IDT(7);
    IRQ_IDT(8);
    IRQ_IDT(9);
    IRQ_IDT(10);
    IRQ_IDT(11);
    IRQ_IDT(12);
    IRQ_IDT(13);
    IRQ_IDT(14);
    IRQ_IDT(15);

    idtFlush (& idt_ptr);

    IAL_DEBUG (INFO, "Flushed IDT with hard. int entries\r\n");
}

/**
 * \fn bindIsr
 * \brief Binds ISR handlers to corresponding IDT entries
 */
    void
bindIsr (void)
{
    /*
     * Some segment selector stuff :
     * - Faults are in kernel level, flag is then 0x8E, because we won't explicitely trigger them from userland.
     * - But pipcalls may be triggered on purpose from userland (well, they sould always be, in fact), so our flags are 0xEE.
     */

#define KERN_IDT(X) { extern void isr ## X(); idtSetGate(X, (uint32_t) isr ## X, 0x08, 0x8E); }
#define USER_IDT(X) { extern void isr ## X(); idtSetGate (X, (uint32_t) isr ## X, 0x08, 0xEE); }

    /* Kernel-mode IDT entries */
    KERN_IDT(0);
    KERN_IDT(1);
    KERN_IDT(2);
    KERN_IDT(3);
    KERN_IDT(4);
    KERN_IDT(5);
    KERN_IDT(6);
    KERN_IDT(7);
    KERN_IDT(8);
    KERN_IDT(9);
    KERN_IDT(10);
    KERN_IDT(11);
    KERN_IDT(12);
    KERN_IDT(13);
    KERN_IDT(14);
    KERN_IDT(15);
    KERN_IDT(16);
    KERN_IDT(17);
    KERN_IDT(18);
    KERN_IDT(19);
    KERN_IDT(20);
    KERN_IDT(21);
    KERN_IDT(22);
    KERN_IDT(23);
    KERN_IDT(24);
    KERN_IDT(25);
    KERN_IDT(26);
    KERN_IDT(27);
    KERN_IDT(28);
    KERN_IDT(29);
    KERN_IDT(30);
    KERN_IDT(31);

    /* User-mode IDT entries */
    USER_IDT(48);
    USER_IDT(49);
    USER_IDT(50);
    USER_IDT(51);
    USER_IDT(52);
    USER_IDT(53);
    USER_IDT(54);
    USER_IDT(55);
    USER_IDT(56);
    USER_IDT(57);
    USER_IDT(58);
    USER_IDT(59);
    USER_IDT(60);
    USER_IDT(61);
    USER_IDT(62);
    USER_IDT(63);
    USER_IDT(64);
    USER_IDT(65);
    USER_IDT(66);
    USER_IDT(67);
    USER_IDT(68);
    USER_IDT(69);
    USER_IDT(70);
    USER_IDT(71);
    USER_IDT(72);
    USER_IDT(73);
    USER_IDT(74);
    USER_IDT(75);
    USER_IDT(76);
    USER_IDT(77);
    USER_IDT(78);
    USER_IDT(79);
    USER_IDT(80);
    USER_IDT(81);
    USER_IDT(82);
    USER_IDT(83);
    USER_IDT(84);
    USER_IDT(85);
    USER_IDT(86);
    USER_IDT(87);
    USER_IDT(88);
    USER_IDT(89);
    USER_IDT(90);
    USER_IDT(91);
    USER_IDT(92);
    USER_IDT(93);
    USER_IDT(94);
    USER_IDT(95);
    USER_IDT(96);
    USER_IDT(97);
    USER_IDT(98);
    USER_IDT(99);
    USER_IDT(100);
    USER_IDT(101);
    USER_IDT(102);
    USER_IDT(103);
    USER_IDT(104);
    USER_IDT(105);
    USER_IDT(106);
    USER_IDT(107);
    USER_IDT(108);
    USER_IDT(109);
    USER_IDT(110);
    USER_IDT(111);
    USER_IDT(112);
    USER_IDT(113);
    USER_IDT(114);
    USER_IDT(115);
    USER_IDT(116);
    USER_IDT(117);
    USER_IDT(118);
    USER_IDT(119);
    USER_IDT(120);
    USER_IDT(121);
    USER_IDT(122);
    USER_IDT(123);
    USER_IDT(124);
    USER_IDT(125);
    USER_IDT(126);
    USER_IDT(127);
    USER_IDT(128);
    USER_IDT(129);
    USER_IDT(130);
    USER_IDT(131);
    USER_IDT(132);
    USER_IDT(133);
    USER_IDT(134);
    USER_IDT(135);
    USER_IDT(136);
    USER_IDT(137);
    USER_IDT(138);
    USER_IDT(139);
    USER_IDT(140);
    USER_IDT(141);
    USER_IDT(142);
    USER_IDT(143);
    USER_IDT(144);
    USER_IDT(145);
    USER_IDT(146);
    USER_IDT(147);
    USER_IDT(148);
    USER_IDT(149);
    USER_IDT(150);
    USER_IDT(151);
    USER_IDT(152);
    USER_IDT(153);
    USER_IDT(154);
    USER_IDT(155);
    USER_IDT(156);
    USER_IDT(157);
    USER_IDT(158);
    USER_IDT(159);
    USER_IDT(160);
    USER_IDT(161);
    USER_IDT(162);
    USER_IDT(163);
    USER_IDT(164);
    USER_IDT(165);
    USER_IDT(166);
    USER_IDT(167);
    USER_IDT(168);
    USER_IDT(169);
    USER_IDT(170);
    USER_IDT(171);
    USER_IDT(172);
    USER_IDT(173);
    USER_IDT(174);
    USER_IDT(175);
    USER_IDT(176);
    USER_IDT(177);
    USER_IDT(178);
    USER_IDT(179);
    USER_IDT(180);
    USER_IDT(181);
    USER_IDT(182);
    USER_IDT(183);
    USER_IDT(184);
    USER_IDT(185);
    USER_IDT(186);
    USER_IDT(187);
    USER_IDT(188);
    USER_IDT(189);
    USER_IDT(190);
    USER_IDT(191);
    USER_IDT(192);
    USER_IDT(193);
    USER_IDT(194);
    USER_IDT(195);
    USER_IDT(196);
    USER_IDT(197);
    USER_IDT(198);
    USER_IDT(199);
    USER_IDT(200);
    USER_IDT(201);
    USER_IDT(202);
    USER_IDT(203);
    USER_IDT(204);
    USER_IDT(205);
    USER_IDT(206);
    USER_IDT(207);
    USER_IDT(208);
    USER_IDT(209);
    USER_IDT(210);
    USER_IDT(211);
    USER_IDT(212);
    USER_IDT(213);
    USER_IDT(214);
    USER_IDT(215);
    USER_IDT(216);
    USER_IDT(217);
    USER_IDT(218);
    USER_IDT(219);
    USER_IDT(220);
    USER_IDT(221);
    USER_IDT(222);
    USER_IDT(223);
    USER_IDT(224);
    USER_IDT(225);
    USER_IDT(226);
    USER_IDT(227);
    USER_IDT(228);
    USER_IDT(229);
    USER_IDT(230);
    USER_IDT(231);
    USER_IDT(232);
    USER_IDT(233);
    USER_IDT(234);
    USER_IDT(235);
    USER_IDT(236);
    USER_IDT(237);
    USER_IDT(238);
    USER_IDT(239);
    USER_IDT(240);
    USER_IDT(241);
    USER_IDT(242);
    USER_IDT(243);
    USER_IDT(244);
    USER_IDT(245);
    USER_IDT(246);
    USER_IDT(247);
    USER_IDT(248);
    USER_IDT(249);
    USER_IDT(250);
    USER_IDT(251);
    USER_IDT(252);
    USER_IDT(253);
    USER_IDT(254);
    USER_IDT(255);

    idtFlush (& idt_ptr);
    IAL_DEBUG (INFO, "Flushed IDT with fault and soft. int entries\r\n");
}

/**
 * \fn initIdt
 * \brief Initializes the IDT structure
 */
    void
initIdt (void)
{
    idt_ptr.limit = sizeof (idt_entry_t) * 256 - 1;
    idt_ptr.base = (uint32_t) & idt_entries;

    memset (&idt_entries, 0, sizeof (idt_entries));
    IAL_DEBUG (INFO, "Interrupt Descriptor Table setup complete\r\n");
}

/**
 * \fn timerPhase
 * \brief Set timer frequency
 * \param Frequency to set
 *
 */
    void
timerPhase (uint32_t hz)
{
    //  int divisor = 1193180 / hz;	/* Calculate our divisor */
    uint32_t divisor = 2600000 / hz;
    if (divisor > 0xffff) divisor = 0xffff;
    if (divisor < 1) divisor = 1;

    outb (0x43, 0x36);			/* Set our command byte 0x36 */
    outb (0x40, divisor & 0xFF);	/* Set low byte of divisor */
    outb (0x40, divisor >> 8);	/* Set high byte of divisor */

    IAL_DEBUG (INFO, "Timer phase changed to %d hz\r\n", hz);
}

/**
 * \fn void initCpu()
 * \brief Initializes CPU-specific features
 */
void initCpu()
{
    IAL_DEBUG(CRITICAL, "Identifying CPU model and features...\r\n");

    /* Display CPU vendor string */
    uint32_t cpu_string[4];
    cpuid_string(CPUID_GETVENDORSTRING, cpu_string); /* Vendor string will be 12 characters in EBX, EDX, ECX */
    char cpuident[17];
    char cpubrand[49];

    /* Build string */
    memcpy(cpuident, &(cpu_string[1]), 4 * sizeof(char));
    memcpy(&(cpuident[4]), &(cpu_string[3]), 4 * sizeof(char));
    memcpy(&(cpuident[8]), &(cpu_string[2]), 4 * sizeof(char));
    cpuident[12] = '\0';

    IAL_DEBUG(CRITICAL, "CPU identification: %s\r\n", cpuident);

    /* Processor brand */
    cpuid_string(CPUID_INTELBRANDSTRING, (uint32_t*)cpubrand);
    cpuid_string(CPUID_INTELBRANDSTRINGMORE, (uint32_t*)&cpubrand[16]);
    cpuid_string(CPUID_INTELBRANDSTRINGEND, (uint32_t*)&cpubrand[32]);
    cpubrand[48] = ' ';
    IAL_DEBUG(CRITICAL, "CPU brand: %s\r\n", cpubrand);

    /* Check whether PCID is supported as well as PGE */
    uint32_t ecx, edx;
    cpuid(CPUID_GETFEATURES, &ecx, &edx);
    uint32_t cr4;

    /* PGE check */
    if(edx & CPUID_FEAT_EDX_PGE)
    {
        IAL_DEBUG(CRITICAL, "PGE supported, enabling CR4.PGE\r\n");
        __asm volatile("MOV %%CR4, %0" : "=r"(cr4));
        cr4 |= (1 << 7); /* Enable Page Global as well */
        __asm volatile("MOV %0, %%CR4" :: "r"(cr4));
    } else {
        IAL_DEBUG(CRITICAL, "PGE unsupported, Global Page feature will be unavailable\r\n");
    }

    /* PCID check */
    if(ecx & CPUID_FEAT_ECX_PCID)
    {
        IAL_DEBUG(CRITICAL, "PCID supported, enabling CR4.PCIDE\r\n");
//        pcid_enabled = 1;

        /* Enable PCID */
  //      __asm volatile("MOV %%CR4, %0" : "=r"(cr4));
  //      cr4 |= (1 << 17);
  //      __asm volatile("MOV %0, %%CR4" :: "r"(cr4));
    } else {
        IAL_DEBUG(CRITICAL, "PCID unsupported, Process Context Identifiers feature will be unavailable\r\n");
    }
}

#define IA32_APIC_BASE_MSR          0x1B
#define IA32_APIC_BASE_MSR_BSP      0x100
#define IA32_APIC_BASE_MSR_ENABLE   0x800

/* Read a value in a MSR */
void cpu_get_msr(uint32_t msr, uint32_t *lo, uint32_t *hi)
{
    asm volatile("rdmsr" : "=a"(*lo), "=d"(*hi) : "c"(msr));
}

/* Set a value in a MSR */
void cpu_set_msr(uint32_t msr, uint32_t lo, uint32_t hi)
{
    asm volatile("wrmsr" : : "a"(lo), "d"(hi), "c"(msr));
}

/* Gets the base address of APIC in physical memory */
uintptr_t cpu_get_apic_base() {
    uint32_t eax, edx;
    cpu_get_msr(IA32_APIC_BASE_MSR, &eax, &edx);

    return (eax & 0xfffff000);
}

/* Set the physical address for local APIC registers */
void cpu_set_apic_base(uintptr_t apic) {
    uint32_t edx = 0;
    uint32_t eax = (apic & 0xfffff000) | IA32_APIC_BASE_MSR_ENABLE;

    cpu_set_msr(IA32_APIC_BASE_MSR, eax, edx);
}

/* Enables the APIC ! Kawabunga */
void enable_apic() {
    /* Hardware enable the Local APIC if it wasn't enabled */
    cpu_set_apic_base(cpu_get_apic_base());

    /* Set the Spourious Interrupt Vector Register bit 8 to start receiving interrupts */
    extern uint32_t* lapic_base;
    uint32_t cur = *(uint32_t*)((uint32_t)lapic_base + 0xF0);
    *(uint32_t*)((uint32_t)lapic_base + 0xF0) = cur | 0x100;
}

/* Sets up the APIC */
void apicInit()
{
    uint32_t eax, edx;
    extern void mask_pic();
    cpuid(1, &eax, &edx);
    if(edx & CPUID_FEAT_EDX_APIC)
    {
        IAL_DEBUG(CRITICAL, "APIC supported - configuring...\r\n");
        enable_apic();
        IAL_DEBUG(CRITICAL, "Enabled APIC!\r\n");
    } else {
        IAL_DEBUG(CRITICAL, "APIC unsupported - please use x86_multiboot instead.\r\n");
        for(;;);
    }
}

/* Sets up legacy PIC for APIC mode */
void pic_setup()
{
#define IRQ_BASE    0x20

    /* PIC init command (ICW1) */
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);

    /* ICW2 : Interrupt vector address */
    outb(PIC1_DATA, IRQ_BASE);
    outb(PIC2_DATA, IRQ_BASE + 8);

    /* ICW3 : Master/Slave wiring */
    outb(PIC1_DATA, 4);
    outb(PIC2_DATA, 2);

    /* ICW4 : Legacy 8086 mode */
    outb(PIC1_DATA, ICW4_8086);
    outb(PIC2_DATA, ICW4_8086);

    /* OCW1 : Mask all interrupts in order to setup APIC */
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

/* This sets up APIC timer in a 10 ms rate */
void setup_apic_timer()
{
    /* Tell APIC to use divider 16 */
    write_lapic(APIC_TMRDIV, 0x3);

    /* Prepare 10ms sleep */

    /* Reset APIC timer counter */
    write_lapic(APIC_TMRINITCNT, 0xFFFFFFFF);

    /* Perform pit sleep */
    /* TODO : je sais, je suis au courant */
    for(int i=0; i<100000000; i++)
        __asm volatile("NOP");

    /* Mask APIC timer interrupt */
    write_lapic(APIC_LVT_TMR, APIC_DISABLE);

    uint32_t ticks = 0xFFFFFFFF - read_lapic(APIC_TMRCURRCNT);

    /* Configure and start APIC timer */
    write_lapic(APIC_LVT_TMR, 32 | TMR_PERIODIC);
    write_lapic(APIC_TMRDIV, 0x3);
    write_lapic(APIC_TMRINITCNT, ticks);

    IAL_DEBUG (CRITICAL, "APIC timer set-up successfully.\r\n");
}

/**
 * \fn initInterrupts
 * \brief Initializes the IAL
 */
    void
initInterrupts (void)
{
    if(coreId() == 0)
    {
        DEBUG(CRITICAL, "Running IAL initialization for BSP.\r\n");
        extern uint32_t* ioapic_base, *lapic_base;
        extern void mask_pic();
        IAL_DEBUG (INFO, "Initializing interrupts, IAL %s \"On Steroids\" version %s\r\n", IAL_PREFIX, IAL_VERSION);
        IAL_DEBUG (CRITICAL, "\tIO-APIC at %x\r\n\tLAPIC at %x\r\n", ioapic_base, lapic_base);
        IAL_DEBUG (CRITICAL, "Masking PIC.\r\n");
        pic_setup();
        IAL_DEBUG(CRITICAL, "Masked PIC.\r\n");
        initIdt ();
        bindIsr ();
        // remapIrq ();
        bindIrq ();
        //apicInit();
        //setup_apic_timer();
        // timerPhase (100);
        timer_ticks = 0;
        initCpu();
    } else {
        DEBUG(CRITICAL, "Running IAL initialization for AP%d.\r\n", coreId());
        initIdt();
        bindIsr();
        bindIrq();
        apicInit();
        initCpu();
    }
}
