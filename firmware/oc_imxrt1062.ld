/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Yakov Khalinsky */
/* The o_C port's memory map: the Teensy 4.0's own script, with the engine's
 * RAM sections put where the startup clears them.
 *
 * Why this file exists (the v0.1.0 bootloop, and the v0.1.2 fault):
 *
 * The engine is the vendored Tessera's, and it keeps its big buffers in two
 * sections of its own: .pool (the FM-1's zero-initialised working buffers) and
 * .noinit (the state that is meant to survive a reset). The Teensy 4.0's
 * imxrt1062.ld knows neither of them, so the linker orphans them: they land
 * between the copied .data and the cleared .bss, in a gap that no startup code
 * touches. Nothing ever writes them, so the engine starts on whatever the RAM
 * happened to hold - and the first timer tick reads it and faults. (v0.1.0.)
 *
 * The first fix put the two sections in .bss as sections of their own, and
 * pointed _ebss at the end of .noinit. That was not enough, and the v0.1.1
 * firmware faulted at the first tick, in the engine's pattern_init(), with a
 * memset at 0x200272A0 - the last bytes of .noinit.
 *
 * The reason is the MPU. The Teensy's startup arms a 32-byte NOACCESS region
 * at &_ebss, to catch a stack overflow (startup.c: "trap stack overflow"), and
 * an MPU region of 32 bytes must start on a 32-byte boundary. With _ebss at
 * the end of .noinit, that end was 0x200272BC - not a multiple of 32 - so the
 * hardware rounded the region down to 0x200272A0 and the trap covered the tail
 * of .noinit instead of the empty space behind it. The memset then wrote into
 * the trap and the module died.
 *
 * So the script is now the Teensy's own, with two deltas:
 *
 *   1. .bss is NOLOAD: it holds no initialised data, and saying so keeps it
 *      NOBITS (the map check wants that).
 *   2. the engine's .pool and .noinit are inputs of .bss, not sections of
 *      their own: the startup clears them with .bss, and _ebss - the end of
 *      .bss - is then the aligned end of everything, so the MPU's trap lands
 *      in the empty space behind the RAM, where it belongs.
 *
 * The rest of the file is the Teensy 4.0's imxrt1062.ld, unchanged (the
 * framework the build uses: teensy 6.0.0, framework-arduinoteensy 1.162.0).
 * tools/check_sections.py checks the result: _ebss 32-aligned, nothing in the
 * trap, .bss NOBITS, and the stack's headroom. */

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

	.bss ALIGN(4) (NOLOAD) : {
		*(SORT_BY_ALIGNMENT(SORT_BY_NAME(.bss*)))
		*(SORT_BY_ALIGNMENT(SORT_BY_NAME(COMMON)))
		/* the engine's own RAM, in the range the startup clears: the FM-1's
		 * .pool (its zero-initialised working buffers) and its .noinit. The
		 * Teensy's own script orphans them - they land between the copied
		 * .data and the cleared .bss, and nothing ever writes them, so the
		 * engine starts on garbage: the v0.1.0 bootloop. They are inputs of
		 * .bss here, so the startup clears them with it, and _ebss (the end
		 * of .bss) stays 32-aligned for the MPU's stack-overflow trap. */
		*(.pool*)
		*(.noinit*)
		. = ALIGN(32);
		. = . + 32; /* MPU to trap stack overflow */
	} > DTCM

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
	_ebss = ADDR(.bss) + SIZEOF(.bss);

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
