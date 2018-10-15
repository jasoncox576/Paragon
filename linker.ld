ENTRY(_start)

SECTIONS {
	
	/* Begin putting sections at 1 MiB by convention */

	. = 1M;

	/* Put multiboot header */
	.text BLOCK(4K) : ALIGN(4K) {
		
		*(.multiboot)
		*(.text)
		
	}

	/* Read-only data. */
	.rodata BLOCK(4K) : ALIGN(4K) {
		*(.rodata)
	}

	/* Read-write-data (initialized)*/
	.data BLOCK(4K) : ALIGN(4K) {
		*(.data)
	}


	/* Read-write data (uninitialized) and stack */
	.bss BLOCK(4K) : ALIGN(4K) {

			*(COMMON)
			*(.bss)
	}

	








}
