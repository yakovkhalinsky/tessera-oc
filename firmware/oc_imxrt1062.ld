/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Yakov Khalinsky */
/* The o_C port's memory map: the Teensy 4.0's own, with the engine's RAM
 * sections put where the startup clears them.
 *
 * Why this file exists (the v0.1.0 bootloop):
 *
 * The engine is the vendored Tessera's, and it keeps its big buffers in two
 * sections of its own: .pool (the FM-1's zero-initialised working buffers) and
 * .noinit (the state that is meant to survive a reset). The Teensy 4.0's
 * imxrt1062.ld knows neither of them, so the linker orphans them: they land
 * between the copied .data and the cleared .bss, in a gap that no startup code
 * touches. Nothing ever writes them, so the engine starts on whatever the RAM
 * happened to hold - and the first timer tick reads it and faults.
 *
 * So this is the Teensy's script with the two sections inside .bss: the
 * startup clears _sbss.._ebss, and the engine's buffers are cleared with it,
 * before setup() runs. (On the FM-1 the .pool is a bank of its own and the
 * boot clears it by hand; here the startup does it.)
 *
 * The .noinit is cleared too: the only things in it in this build are the
 * pattern bank (which the engine's own pattern_init() clears) and the FM-1's
 * bootguard, which the port does not use - the port has no UBOOT fallback,
 * and the crash report over the USB serial is the diagnostic instead. */

MEMORY
{
	ITCM (rx):  ORIGIN = 0x00000000, LENGTH = 512K
	DTCM (rw):  ORIGIN = 0x20000000, LENGTH = 512K
	RAM (rw):   ORIGIN = 0x20200000, LENGTH = 512K
	FLASH (rx): ORIGIN = 0x60000000, LENGTH = 1984K
}

ENTRY(ImageVectorTable)

SECTIONS
{
	.text.headers : {
		KEEP(*(.flashconfig))
		FILL(0xFF)
		. = ORIGIN(FLASH) + 0x1000;
		KEEP(*(.ivt))
		KEEP(*(.bootdata))
		. = ALIGN(1024);
	} > FLASH

	.text.code : {
		KEEP(*(.startup))
		*(.flashmem*)
		. = ALIGN(4);
		KEEP(*(.init))
		__preinit_array_start = .;
		KEEP (*(.preinit_array))
		__preinit_array_end = .;
		__init_array_start = .;
		KEEP (*(.init_array))
		__init_array_end = .;
		. = ALIGN(4);
	} > FLASH

	.text.progmem : {
		*(.progmem*)
		. = ALIGN(4);
	} > FLASH

	.text.itcm : {
		. = . + 32; /* MPU to trap NULL pointer deref */
		*(.fastrun)
		*(.text*)
		. = ALIGN(16);
	} > ITCM  AT> FLASH

	.ARM.exidx : {
		__exidx_start = .;
		*(.ARM.exidx* .ARM.extab.text* .gnu.linkonce.armexidx.*)
		__exidx_end = .;
	} > ITCM  AT> FLASH

	.data : {
    		*(.endpoint_queue)
		*(SORT_BY_ALIGNMENT(SORT_BY_NAME(.rodata*)))
		*(SORT_BY_ALIGNMENT(SORT_BY_NAME(.data*)))
   		 KEEP(*(.vectorsram))
	} > DTCM  AT> FLASH

	.bss ALIGN(4) : {
		*(SORT_BY_ALIGNMENT(SORT_BY_NAME(.bss*)))
		*(SORT_BY_ALIGNMENT(SORT_BY_NAME(COMMON)))
		. = ALIGN(32);
		. = . + 32; /* MPU to trap stack overflow */
	} > DTCM

	/* The engine's own RAM, in the range the startup clears: the FM-1's
	 * .pool (its zero-initialised working buffers) and its .noinit. The
	 * Teensy's own script orphans them - they land between the copied .data
	 * and the cleared .bss, and nothing ever writes them, so the engine
	 * starts on garbage: the v0.1.0 bootloop. NOLOAD, so they cost no
	 * flash; the startup clears them with .bss. */
	.pool (NOLOAD) : { *(.pool*) . = ALIGN(4); } > DTCM
	.noinit (NOLOAD) : { *(.noinit*) . = ALIGN(4); } > DTCM

	.bss.dma (NOLOAD) : {
		*(.hab_log)
		*(.dmabuffers)
		. = ALIGN(32);
	} > RAM

	.text.csf : {
		FILL(0xFF)
		. = ALIGN(1024);
		KEEP(*(.csf))
		__text_csf_end = .;
	} > FLASH

	_stext = ADDR(.text.itcm);
	_etext = ADDR(.text.itcm) + SIZEOF(.text.itcm) + SIZEOF(.ARM.exidx);
	_stextload = LOADADDR(.text.itcm);

	_sdata = ADDR(.data);
	_edata = ADDR(.data) + SIZEOF(.data);
	_sdataload = LOADADDR(.data);

	_sbss = ADDR(.bss);
	_ebss = ADDR(.noinit) + SIZEOF(.noinit);

	_heap_start = ADDR(.bss.dma) + SIZEOF(.bss.dma);
	_heap_end = ORIGIN(RAM) + LENGTH(RAM);

	_itcm_block_count = (SIZEOF(.text.itcm) + SIZEOF(.ARM.exidx) + 0x7FFF) >> 15;
	_flexram_bank_config = 0xAAAAAAAA | ((1 << (_itcm_block_count * 2)) - 1);
	_estack = ORIGIN(DTCM) + ((16 - _itcm_block_count) << 15);

	_flashimagelen = __text_csf_end - ORIGIN(FLASH);
	_teensy_model_identifier = 0x24;

	.debug_info     0 : { *(.debug_info) }
	.debug_abbrev   0 : { *(.debug_abbrev) }
	.debug_line     0 : { *(.debug_line) }
	.debug_frame    0 : { *(.debug_frame) }
	.debug_str      0 : { *(.debug_str) }
	.debug_loc      0 : { *(.debug_loc) }

}
