#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


#include <stdalign.h>
#include <stdarg.h>
#include <iso646.h>
#include <limits.h>


/* Check if the compiler thinks you are targeting the wrong operating system. */
#if defined(__linux__)
#error "You are not using a cross-compiler, you will most certainly run into trouble"
#endif

#if !defined(__i386__)
#error "This tutorial needs to be compiled with a ix86-elf compiler"
#endif

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

//Port reading function
//
//




inline uint8_t inportb(uint16_t portid) {

	uint8_t ret;
	asm volatile( "inb %1, %0"
				 : "=a"(ret)
				 : "Nd"(portid) );
	return ret;	


}

size_t size(uint16_t* arr) {
	
	size_t elem_count;
	size_t index = 0;

	while(arr[index]) {
		index++;
	}	
	
	elem_count = index+1;	
	return elem_count;
}










// Should (maybe) eventually create a TSS segment?
//

struct gdt_entry {

	unsigned short limit_low;
	unsigned short base_low;
	unsigned char base_middle;
	unsigned char access;
	unsigned char granularity;
	unsigned char base_high;

} __attribute__((packed));



struct gdt_info {
	
	unsigned short limit;
	unsigned int base;
}__attribute__((packed));


struct gdt_entry gdt[3];
struct gdt_info gdtr;



void create_gdt_entry(int index, unsigned long limit, unsigned long base, unsigned char access, unsigned char gran) {


	// WHY??
	// What is the purpose of this sort of formatting?
	// How does it work exactly?	
	
	gdt[index].limit_low = (limit & 0xFFFF);
	gdt[index].base_low = (base & 0xFFFF);
	gdt[index].base_middle = (base >> 16) & 0xFF;
	gdt[index].base_high = (base >> 24) & 0xFF;
   	gdt[index].granularity = (limit >> 16) & 0x0F;
	gdt[index].granularity |= (gran & 0xF0);	
	gdt[index].access = access;

}


void gdt_flush() {

	

	asm volatile("lgdt %0;"
				 //"ljmp $0x08, $1f;"


				 //Note: This '1' is defined as a local function.
				 //Trying to declare this as function with a regular
				 //Name results in re-declaration of the function
				 "1:"
				 "	mov $0x10, %%ax;"
				 /*
				 "	mov %%ax, %%ds;"
				 "	mov %%ax, %%es;"
				 "	mov %%ax, %%fs;"
				 "	mov %%ax, %%gs;"
				 "	mov %%ax, %%ss;"
				 */
				 : /* no outputs */
				 :"X" (gdtr));
					

}








void gdt_install() {

	
	gdtr.limit = (sizeof(struct gdt_entry));
	// The limit is 8 bytes long
	gdtr.base = &gdt;
	
	//WHY??
	// Why is the limit the size of 4 bytes, when it should be  8??
	// Answer: Not sure, but it is actually 8. it looks like its 4 bytes.
	// Why is the base set to zero?
	// Answer: I think that that is a logical address
	// And what exactly about that granularity?
	// Answer: That's just the offset, set at 4KiB


	// Null segment
	create_gdt_entry(0, 0, 0, 0, 0);
	
	// Code segment
	create_gdt_entry(1, 0xFFFFFFFF, 0, 0x9A, 0xCF);	

	// Data segment
	create_gdt_entry(2, 0xFFFFFFFF, 0, 0x92, 0xCF);
			
	gdt_flush();
	
}








// Why does this work?

static inline bool are_interrupts_enabled() {

	unsigned long flags;
	asm volatile ( "pushf\n\t"
				   "pop %0"
				   : "=g"(flags) );

	return flags & (1 << 9);

			
}










enum vga_color {
	
	VGA_COLOR_BLACK = 0,
	VGA_COLOR_BLUE = 1,
	VGA_COLOR_GREEN = 2,
	VGA_COLOR_CYAN = 3,
	VGA_COLOR_RED = 4,
	VGA_COLOR_MAGENTA= 5,
	VGA_COLOR_BROWN = 6,
	VGA_COLOR_LIGHT_GREY = 7,
	VGA_COLOR_DARK_GREY = 8,
	VGA_COLOR_LIGHT_BLUE = 9,
	VGA_COLOR_LIGHT_GREEN = 10,
	VGA_COLOR_LIGHT_CYAN = 11,
	VGA_COLOR_LIGHT_RED = 12,
	VGA_COLOR_LIGHT_MAGENTA = 13,
	VGA_COLOR_LIGHT_BROWN = 14,
	VGA_COLOR_WHITE = 15,


};

static inline uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg) {

	return fg | bg << 4;
			
}

/* I'm guessing that this and the above function just format the color so that they may
 * be displayed properly on VGA? */

static inline uint16_t vga_entry(unsigned char uc, uint8_t color) {
	
		return (uint16_t) uc | (uint16_t) color << 8;
		
}

size_t strlen(const char* str) {

	size_t len = 0;
	while(str[len])
			len++;
	return len;
		
}


//static const size_t VGA_WIDTH = 80;
//static const size_t VGA_HEIGHT = 25;

size_t terminal_row;
size_t terminal_column;
uint8_t terminal_color;
uint16_t* terminal_buffer;

void terminal_initialize(void) {

	terminal_row = 0;
	terminal_column = 0;
	terminal_color = vga_entry_color(VGA_COLOR_GREEN, VGA_COLOR_BLACK);
	terminal_buffer = (uint16_t*) 0xB8000;
	for(size_t y = 0; y < VGA_HEIGHT; y++) {
		for(size_t x = 0; x < VGA_WIDTH; x++) {
			const size_t index = y * VGA_WIDTH + x;
			uint16_t entry = vga_entry(' ', terminal_color);
			terminal_buffer[index] = entry;

		}
	
	}



}



char keyboard_irq() {
	
	//Have yet to actually impement interrupt request...

	//asm volatile ( "hlt" );
	char character = inportb(0x60);	

	return character;
}


void terminal_setcolor(uint8_t color) {
	terminal_color = color;

}

void terminal_putentryat(char c, uint8_t color, size_t x, size_t y) {

	const size_t index = y * VGA_WIDTH + x;
	uint16_t entry = vga_entry(c, color);
	terminal_buffer[index] = entry;
}

void scroll_terminal(int16_t* buffer) {
	//THIS IS A TERRIBLE ALGORITHM!!!
	// This will run with close to cubic time complexity,
	// specifically O(mn^2)
	// This is terrible. Don't ever do this again.
	// You should make an example of how bad this is so that nobody
	// ever makes such a terrible mistake again
	// Also, this is not how scrolling works. It really really isn't.
	// The fact that you thought, for one second, that this is how
	// scrolling worked is really sad.
	//
	// Actualy wait. This is probably the sa
	/*for(uint16_t i = 0; i < VGA_WIDTH; ++i) {
		for(int index = 0; index < 100; ++index) {
			if(index + 1 < size(buffer))
				buffer[index] = buffer[index+1];	
			else {
				buffer[index] = 0;
			}
		}

	}*/

	for(int y = 0; y < VGA_HEIGHT; y++) {
		for(int x = 0; x < VGA_WIDTH; x++) {
			const size_t index = y * VGA_WIDTH + x;
			const size_t new_index = (y+1) * VGA_WIDTH + x;
			if((y+1) >= VGA_HEIGHT) {
					buffer[index] = 0;
			}
			buffer[index] = buffer[new_index];
		}
				
	}

	return;


}




void terminal_putchar(char c) {

		// SCROLLING:
		// when the ROW == VGA_HEIGHT
		// copy contents of all lines except first
		// Set this equal to the new screen buffer 
	
	if(c == '\n') {
		terminal_column = 0;
		++terminal_row;		
	}	

	else {
		terminal_putentryat(c, terminal_color, terminal_column, terminal_row);
		if (++terminal_column == VGA_WIDTH) {
			terminal_column = 0; 
			terminal_row += 1;
		}
	}


	// Test if necessary to scroll the screen down

	if(terminal_row == VGA_HEIGHT) {
		scroll_terminal(terminal_buffer);
		terminal_row -= 1; 
	}

}

void terminal_write(const char* data, size_t size) {

	for(size_t i = 0; i < size; i++) {
		terminal_putchar(data[i]);
	}
}

void terminal_writestring(const char* data) {
	
	terminal_write(data, strlen(data));

}



char * strcat(char * dest, const char *src) {

	size_t i, j;

	i = strlen(dest);

	for(j=0; dest[j] != '\0'; j++) {
			
		dest[i+j] = src[j];
	}	
	dest[i+j] = '\0';
	
	return dest;

}


char* reverse(char* string) {
	
	size_t len = strlen(string);
	size_t i = 0;
	size_t k = len/2;


	for(; i < k; i++) {

		char ch = string[i];
		string[i] = string[len-1-i];
		string[len-1-i] = ch;
	}
	
	string[len] = '\0';
	return string;

}























char* itoa(int value, char* buffer, int base) {
	//Assumed, for now, that base = 10.
		
	// Upon passing 'buffer' as an arg, make sure that it is big enough
	// to allow strcat

	//TODO figure out a way to actually find the size of an int
	// in digits given the int.	

	int str_index = 0;
	bool isNegative = false;

	if(value == 0) {
		buffer[str_index++] = '0';
		buffer[str_index] = '\0';
		return buffer;
	
	}




	if(base == 10 && value < 0) {
		isNegative = true;
		value = (value * -1);
	}


	while(value != 0) {
		int rem = value % base;
		//This next line is only needed if base is something other than 10.
		//buffer[str_index++] = (rem > 9)? (rem-10) + 'a' : rem + '0';
		buffer[str_index++] = rem + '0';
		value = value/base;
	}
	
	if(isNegative) { 
		//strcat(buffer, '-');
		buffer[str_index++] = '-';
	}


	buffer[str_index] = '\0';
	
	reverse(buffer);	


	return buffer;

}	


































void kernel_main(void) {

	/* Initializing terminal interface*/
	terminal_initialize();

	
	if(are_interrupts_enabled()) terminal_writestring("SYSTATUS: Interrupts enabled! \n");
	else terminal_writestring("SYSTATUS: Interrupts are not yet enabled\n");




	gdt_install();
	

	//terminal_writestring("SYSTATUS: Global Descriptor Table loaded\n");	

}








enum KEYCODE {
	NULL_KEY = 0,
	Q_PRESSED = 0x10,
	Q_RELEASED = 0x90,
	W_PRESSED = 0x11,
	W_RELEASED = 0x91,
	E_PRESSED = 0x12,
	E_RELEASED = 0x92,
	R_PRESSED = 0x13,
	R_RELEASED = 0x93,
	T_PRESSED = 0x14,
	T_RELEASED = 0x94,
	Z_PRESSED = 0x15,
	Z_RELEASED = 0x95,
	U_PRESSED = 0x16,
	U_RELEASED = 0x96,
	I_PRESSED = 0x17,
	I_RELEASED = 0x97,
	O_PRESSED = 0x18,
	O_RELEASED = 0x98,
	P_PRESSED = 0x19,
	P_RELEASED = 0x99,
	A_PRESSED = 0x1E,
	A_RELEASED = 0x9E,
	S_PRESSED = 0x1F,
	S_RELEASED = 0x9F,
	D_PRESSED = 0x20,
	D_RELEASED = 0xA0,
	F_PRESSED = 0x21,
	F_RELEASED = 0xA1,
	G_PRESSED = 0x22,
	G_RELEASED = 0xA2,
	H_PRESSED = 0x23,
	H_RELEASED = 0xA3,
	J_PRESSED = 0x24,
	J_RELEASED = 0xA4,
	K_PRESSED = 0x25,
	K_RELEASED = 0xA5,
	L_PRESSED = 0x26,
	L_RELEASED = 0xA6,
	Y_PRESSED = 0x2C,
	Y_RELEASED = 0xAC,
	X_PRESSED = 0x2D,
	X_RELEASED = 0xAD,
	C_PRESSED = 0x2E,
	C_RELEASED = 0xAE,
	V_PRESSED = 0x2F,
	V_RELEASED = 0xAF,
	B_PRESSED = 0x30,
	B_RELEASED = 0xB0,
	N_PRESSED = 0x31,
	N_RELEASED = 0xB1,
	M_PRESSED = 0x32,
	M_RELEASED = 0xB2,

	ZERO_PRESSED = 0x29,
	ONE_PRESSED = 0x2,
	NINE_PRESSED = 0xA,

	POINT_PRESSED = 0x34,
	POINT_RELEASED = 0xB4,

	SLASH_RELEASED = 0xB5,

	BACKSPACE_PRESSED = 0xE,
	BACKSPACE_RELEASED = 0x8E,
	SPACE_PRESSED = 0x39,
	SPACE_RELEASED = 0xB9,
	ENTER_PRESSED = 0x1C,
	ENTER_RELEASED = 0x9C,

};





















