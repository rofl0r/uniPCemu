#include "headers/cpu/mmu.h" //MMU reqs!
#include "headers/cpu/cpu.h" //CPU reqs!
#include "headers/mmu/mmu_internals.h" //Internal transfer support!
#include "headers/mmu/mmuhandler.h" //MMU direct access support!

extern byte EMU_RUNNING; //1 when paging can be applied!

//20-bit PDBR. Could also be CR3 in total?
#define PDBR CPU[activeCPU].registers->CR3
//#define PDBR ((CPU[activeCPU].registers->CR3>>12)&0xFFFFF)

byte is_paging()
{
	if (getcpumode()==CPU_MODE_REAL) //Real mode (no paging)?
	{
		return 0; //Not paging in REAL mode!
	}
	if (CPU[activeCPU].registers) //Gotten registers?
	{
		return (CPU[activeCPU].registers->CR0&CR0_PG); //Are we paging!
	}
	return 0; //Not paging: we don't have registers!
}

//Present: 1 when below is used, 0 is invalid: not present (below not used/to be trusted).
#define PXE_P 0x00000001
//Write allowed? Else read-only.
#define PXE_RW 0x00000002
//1=User, 0=Supervisor only
#define PXE_US 0x00000004
//Write through
#define PXE_W 0x00000008
//Cache disabled
#define PDE_D 0x00000010
//Cache disable
#define PTE_C 0x00000010
//Accessed
#define PXE_A 0x00000020
//Page size (0 for 4KB, 1 for 4MB)
#define PDE_S 0x00000040
//Dirty: we've been written to!
#define PTE_D 0x00000040
//Global flag! Must be on PTE only (PDE is cleared always)
#define PTE_G 0x00000100
//Address mask
#define PXE_ADDRESSMASK 0xFFFFF000
//Address shift
#define PXE_ADDRESSSHIFT 12

byte getUserLevel(byte CPL)
{
	return (CPL==3)?1:0; //1=User, 0=Supervisor
}

void FLAG_PF(uint_32 address, word flags)
{
	if (!(flags&1) && CPU[activeCPU].registers) //Not present?
	{
		CPU[activeCPU].registers->CR2 = address; //Fill CR2 with the address cause!
	}
	//Call interrupt!
	CPU_resetOP(); //Go back to the start of the instruction!
	call_soft_inthandler(EXCEPTION_PAGEFAULT,(int_64)flags); //Call IVT entry #13 decimal!
	//Execute the interrupt!
	CPU[activeCPU].faultraised = 1; //We have a fault raised, so don't raise any more!
}

OPTINLINE byte verifyCPL(byte iswrite, byte userlevel, byte RW, byte US) //userlevel=CPL or 0 (with special instructions LDT, GDT, TSS, IDT, ring-crossing CALL/INT)
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
	word DIR, TABLE;
	byte PTEUPDATED = 0; //Not update!
	uint_32 PDE, PTE; //PDE/PTE entries currently used!
	if (!CPU[activeCPU].registers) return 0; //No registers available!
	DIR = (address>>22)&0x3FF; //The directory entry!
	TABLE = (address>>12)&0x3FF; //The table entry!
	
	//Check PDE
	PDE = memory_directrdw(PDBR+(DIR<<2)); //Read the page directory entry!
	if (!(PDE&PXE_P)) //Not present?
	{
		FLAG_PF(address,(PDE&PXE_P)|(iswrite?1:0)|(getUserLevel(CPL)<<2)); //Run a not present page fault!
		return 0; //We have an error, abort!
	}
	if (!verifyCPL(iswrite,CPL,(PDE&PXE_RW)>>1,(PDE&PXE_US)>>2)) //Protection fault?
	{
		FLAG_PF(address,(PDE&PXE_P)|(iswrite?1:0)|(getUserLevel(CPL)<<2)); //Run a not present page fault!
		return 0; //We have an error, abort!		
	}
	if (!(PDE&PXE_A)) //Not accessed yet?
	{
		PDE |= PXE_A; //Accessed!
		memory_directwdw(PDBR+(DIR<<2),PDE); //Update in memory!
	}
	
	//Check PTE
	PTE = memory_directrdw(((PDE&PXE_ADDRESSMASK)>>PXE_ADDRESSSHIFT)+(TABLE<<2)); //Read the page table entry!
	if (!(PTE&PXE_P)) //Not present?
	{
		FLAG_PF(address,(PTE&PXE_P)|(iswrite?1:0)|(getUserLevel(CPL)<<2)); //Run a not present page fault!
		return 0; //We have an error, abort!
	}
	if (!verifyCPL(iswrite,CPL,(PTE&PXE_RW),(PTE&PXE_US))) //Protection fault?
	{
		FLAG_PF(address,(PTE&PXE_P)|(iswrite?1:0)|(getUserLevel(CPL)<<2)); //Run a not present page fault!
		return 0; //We have an error, abort!		
	}
	if (!(PTE&PXE_A))
	{
		PTEUPDATED = 1; //Updated!
		PTE |= PXE_A; //Accessed!
	}
	if (iswrite) //Writing?
	{
		if (!(PTE&PTE_D))
		{
			PTEUPDATED = 1; //Updated!
		}
		PTE |= PTE_D; //Dirty!
	}
	if (PTEUPDATED) //Updated?
	{
		memory_directwdw(((PDE&PXE_ADDRESSMASK)>>PXE_ADDRESSSHIFT)+(TABLE<<2),PTE); //Update in memory!
	}
	return 1; //Valid!
}

byte CPU_Paging_checkPage(uint_32 address, byte readflags, byte CPL)
{
	return (isvalidpage(address,(readflags==0),CPL)==0); //Are we an invalid page? We've raised an error!
}

uint_32 mappage(uint_32 address) //Maps a page to real memory when needed!
{
	word DIR,TABLE,ADDR;
	uint_32 PDE, PTE; //PDE/PTE entries currently used!
	if (!is_paging()) return address; //Direct address when not paging!
	DIR = (address>>22)&0x3FF; //The directory entry!
	TABLE = (address>>12)&0x3FF; //The table entry!
	ADDR = (address&0xFFF);
	PDE = memory_directrdw(PDBR+(DIR<<2)); //Read the page directory entry!
	PTE = memory_directrdw(((PDE&PXE_ADDRESSMASK)>>PXE_ADDRESSSHIFT)+(TABLE<<2)); //Read the page table entry!
	return ((PTE&PXE_ADDRESSMASK)>>PXE_ADDRESSSHIFT)+ADDR; //Give the actual address!
}