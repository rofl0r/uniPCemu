#include "headers/types.h" //Basic types!
#include "headers/hardware/vga/vga.h" //Basic VGA!

typedef struct {
	byte extensionsEnabled;

// Stored exact values of some registers. Documentation only specifies some bits but hardware checks may
// expect other bits to be preserved.
	byte store_3d4_31;
	byte store_3d4_32;
	byte store_3d4_33;
	byte store_3d4_34;
	byte store_3d4_35;
	byte store_3d4_36;
	byte store_3d4_37;
	byte store_3d4_3f;

	byte store_3c0_16;
	byte store_3c0_17;

	byte store_3c4_06;
	byte store_3c4_07;

	//Extra data added by superfury
	byte bank_read; //Read bank number!
	byte bank_write; //Write bank number!
	uint_32 display_start_high;
	uint_32 cursor_start_high;
	uint_32 line_compare_high;
	uint_32 memwrap; //The memory wrap to be AND-ed into the address given!
} SVGA_ET4K_DATA; //Dosbox ET4000 saved data!

//Retrieve a point to the et4k?
#define et4k(VGA) ((SVGA_ET4K_DATA *)VGA->SVGAExtension)
//Retrieve the active et4k!
#define et4k_data et4k(getActiveVGA())

#define et4k_reg(data,port,index) data->store_##port##_##index

#define STORE_ET4K(port, index, category) \
	case 0x##index: \
	et4k_data->store_##port##_##index = val; \
	VGA_calcprecalcs(getActiveVGA(),category|0x##port); \
	return 1;

#define RESTORE_ET4K(port, index) \
	case 0x##index: \
		*result = et4k_data->store_##port##_##index; \
		return 1;

void SVGA_Setup_TsengET4K(uint_32 VRAMSize);