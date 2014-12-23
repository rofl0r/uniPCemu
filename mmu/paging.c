#include "headers/mmu/mmu.h" //MMU reqs!
#include "headers/cpu/cpu.h" //CPU reqs!

extern byte EMU_RUNNING; //1 when paging can be applied!

byte is_paging()
{
	if (EMU_RUNNING!=1) //Emu isn't running: disable paging for direct memory access!
	{
		return 0; //Paging disabled on our real-mode system!
	}
	if (getcpumode()==CPU_MODE_REAL) //Real mode (no paging)?
	{
		return 0; //Not paging in REAL mode!
	}
	if (CPU.registers) //Gotten registers?
	{
		return CPU.registers->CR0.PG; //Are we paging!
	}
	return 0; //Not paging: we don't have registers!
}

union
{
	struct
	{
		byte P : 1; //Present: 1 when below is used, 0 is invalid: not present (below not used/to be trusted).
		byte RW : 1; //Write allowed? Else read-only.
		byte US : 1; //1=User, 0=Supervisor only
		byte W : 2; //Write through
		byte D : 1; //Cache disabled
		byte A : 1; //Accessed
		byte S : 1; //Page size (0 for 4KB, 1 for 4MB)
		byte G : 1;
		byte AVAIL : 3;
		uint_32 PageFrameAddress : 20;
	};
	uint_32 value;
} PDE;

union
{
	struct
	{
		byte P : 1;
		byte RW : 1;
		byte US : 1;
		byte W : 1;
		byte C : 1; //Cache disable
		byte A : 1;
		byte D : 1; //Dirty: we've been written to!
		byte is0 : 1;
		byte G : 1; //Global flag
		byte AVAIL : 3;
		uint_32 PhysicalPageAddress : 20; //Physical page address!
	};
	uint_32 value;
} PTE;

byte getUserLevel(byte CPL)
{
	return (CPL==3)?1:0; //1=User, 0=Supervisor
}

void PF(uint_32 address, word flags)
{
	if (!(flags&1) && CPU.registers) //Not present?
	{
		CPU.registers->CR2 = address; //Fill CR2 with the address cause!
	}
	CPU_PUSH16(&flags);
	//Call interrupt!
}

#define getCPL() CPU.SEG_DESCRIPTOR[CPU_SEGMENT_CS].DPL
byte verifyCPL(byte iswrite, byte userlevel, byte RW, byte US) //userlevel=CPL or 0 (with special instructions LDT, GDT, TSS, IDT, ring-crossing CALL/INT)
{
	if (!US && getUserLevel(userlevel)) //User when not an user page?
	{
		return 0; //Invalid CPL: we don't have rights!
	}
	if (!RW && iswrite) //Write on real-only page?
	{
		return 0; //Invalid: we don't allow writing!
	}
	return 1; //OK: verified!
}

int isvalidpage(uint_32 address, byte iswrite, byte CPL) //Do we have paging without error? userlevel=CPL usually.
{
	if (!CPU.registers) return 0; //No registers available!
	word DIR = (address>>22)&0x3FF; //The directory entry!
	word TABLE = (address>>12)&0x3FF; //The table entry!
	
	//Check PDE
	PDE.value = MMU_directrdw(CPU.registers->CR3.PageDirectoryBase+(DIR<<2)); //Read the page directory entry!
	if (!PDE.P) //Not present?
	{
		PF(address,PDE.P|(iswrite?1:0)|(getUserLevel(CPL)<<2)); //Run a not present page fault!
		return 0; //We have an error, abort!
	}
	if (!verifyCPL(iswrite,CPL,PDE.RW,PDE.US)) //Protection fault?
	{
		PF(address,PDE.P|(iswrite?1:0)|(getUserLevel(CPL)<<2)); //Run a not present page fault!
		return 0; //We have an error, abort!		
	}
	if (!PDE.A) //Not accessed yet?
	{
		PDE.A = 1; //Accessed!
		MMU_directwdw(CPU.registers->CR3.PageDirectoryBase+(DIR<<2),PDE.value); //Update in memory!
	}
	
	//Check PTE
	PTE.value = MMU_directrdw(PDE.PageFrameAddress+(TABLE<<2)); //Read the page table entry!
	if (!PTE.P) //Not present?
	{
		PF(address,PTE.P|(iswrite?1:0)|(getUserLevel(CPL)<<2)); //Run a not present page fault!
		return 0; //We have an error, abort!
	}
	if (!verifyCPL(iswrite,CPL,PTE.RW,PTE.US)) //Protection fault?
	{
		PF(address,PTE.P|(iswrite?1:0)|(getUserLevel(CPL)<<2)); //Run a not present page fault!
		return 0; //We have an error, abort!		
	}
	byte PTEUPDATED = 0; //Not update!
	if (!PTE.A)
	{
		PTEUPDATED = 1; //Updated!
		PTE.A = 1; //Accessed!
	}
	if (iswrite) //Writing?
	{
		if (!PTE.D)
		{
			PTEUPDATED = 1; //Updated!
		}
		PTE.D = 1; //Dirty!
	}
	if (PTEUPDATED) //Updated?
	{
		MMU_directwdw(PDE.PageFrameAddress+(TABLE<<2),PTE.value); //Update in memory!
	}
	return 1; //Valid!
}

uint_32 mappage(uint_32 address) //Maps a page to real memory when needed!
{
	if (!is_paging()) return address; //Direct address when not paging!
	word DIR = (address>>22)&0x3FF; //The directory entry!
	word TABLE = (address>>12)&0x3FF; //The table entry!
	word ADDR = (address&0xFFF);
	PDE.value = MMU_directrdw(CPU.registers->CR3.PageDirectoryBase+(DIR<<2)); //Read the page directory entry!
	PTE.value = MMU_directrdw(PDE.PageFrameAddress+(TABLE<<2)); //Read the page table entry!
	return PTE.PhysicalPageAddress+ADDR; //Give the actual address!
}