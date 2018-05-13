#include "headers/cpu/mmu.h" //MMU reqs!
#include "headers/cpu/cpu.h" //CPU reqs!
#include "headers/mmu/mmu_internals.h" //Internal transfer support!
#include "headers/mmu/mmuhandler.h" //MMU direct access support!
#include "headers/cpu/easyregs.h" //Easy register support!
#include "headers/support/log.h" //Logging support!
#include "headers/emu/debugger/debugger.h" //Debugger support!
#include "headers/cpu/protection.h" //Fault raising support!
#include "headers/cpu/cpu_execution.h" //Execution phase support!
#include "headers/cpu/biu.h" //Memory support!
#include "headers/support/signedness.h" //Sign support!

extern byte EMU_RUNNING; //1 when paging can be applied!

//20-bit PDBR. Could also be CR3 in total?
#define PDBR CPU[activeCPU].registers->CR3
//#define PDBR ((CPU[activeCPU].registers->CR3>>12)&0xFFFFF)

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
//Address mask/active mask(in the lookup table)
#define PXE_ADDRESSMASK 0xFFFFF000
#define PXE_ACTIVEMASK 0xFFF
//Address shift to get the physical address
#define PXE_ADDRESSSHIFT 0
//What to ignore when reading the TLB for read accesses during normal execution? We ignore Dirty and Read/Write access bits!
#define TLB_IGNOREREADMASK 0xC

byte getUserLevel(byte CPL)
{
	return (CPL==3)?1:0; //1=User, 0=Supervisor
}

void raisePF(uint_32 address, word flags)
{
	if (debugger_logging()) //Are we logging?
	{
		dolog("debugger","#PF fault(%08X,%08X)!",address,flags);
	}
	CPU[activeCPU].registers->CR2 = address; //Fill CR2 with the address cause!
	//Call interrupt!
	CPU_resetOP(); //Go back to the start of the instruction!
	/*
	if (CPU[activeCPU].have_oldESP && CPU[activeCPU].registers) //Returning the (E)SP to it's old value?
	{
		REG_ESP = CPU[activeCPU].oldESP; //Restore ESP to it's original value!
		CPU[activeCPU].have_oldESP = 0; //Don't have anything to restore anymore!
	}
	*/
	CPU_onResettingFault(); //Set the fault data!
	if (CPU_faultraised(EXCEPTION_PAGEFAULT)) //Fault raising exception!
	{
		CPU_executionphase_startinterrupt(EXCEPTION_PAGEFAULT,0,(int_64)flags); //Call IVT entry #13 decimal!
		//Execute the interrupt!
		CPU[activeCPU].faultraised = 1; //We have a fault raised, so don't raise any more!
	}
}

OPTINLINE byte verifyCPL(byte iswrite, byte userlevel, byte PDERW, byte PDEUS, byte PTERW, byte PTEUS) //userlevel=CPL or 0 (with special instructions LDT, GDT, TSS, IDT, ring-crossing CALL/INT)
{
	byte uslevel; //Combined US level! 0=Supervisor, 1=User
	byte rwlevel; //Combined RW level! 1=Writable, 0=Not writable
	if (PDEUS&&PTEUS) //User level?
	{
		uslevel = 1; //We're user!
		rwlevel = ((PDERW&&PTERW)?1:0); //Are we writable?
	}
	else //System? Allow read/write if supervisor only! Otherwise, fault!
	{
		uslevel = 0; //We're system!
		rwlevel = 1; //Ignore read/write!
	}
	if ((uslevel==0) && userlevel) //System access by user isn't allowed!
	{
		return 0; //Fault: system access by user!
	}
	if (userlevel && (rwlevel==0) && iswrite) //Write to read-only page for user level?
	{
		return 0; //Fault: read-only write by user!
	}
	return 1; //OK: verified!
}

int isvalidpage(uint_32 address, byte iswrite, byte CPL, byte isPrefetch) //Do we have paging without error? userlevel=CPL usually.
{
	word DIR, TABLE;
	byte PTEUPDATED = 0; //Not update!
	uint_32 PDE, PTE; //PDE/PTE entries currently used!
	if (!CPU[activeCPU].registers) return 0; //No registers available!
	DIR = (address>>22)&0x3FF; //The directory entry!
	TABLE = (address>>12)&0x3FF; //The table entry!
	
	byte effectiveUS;
	byte RW;
	RW = iswrite?1:0; //Are we trying to write?
	effectiveUS = getUserLevel(CPL); //Our effective user level!

	uint_32 temp;
	if (Paging_readTLB(-1, address, 1, effectiveUS, 1,0, &temp)) //Cache hit dirty for writes?
	{
		return 1; //Valid!
	}
	if (likely(RW==0)) //Are we reading? Allow all other combinations of dirty/read/write to be used for this!
	{
		if (Paging_readTLB(-1, address, 1, effectiveUS, 0,TLB_IGNOREREADMASK, &temp)) //Cache hit (non)dirty for reads/writes?
		{
			return 1; //Valid!
		}
	}
	if (isPrefetch) return 0; //Stop the prefetch when not in the TLB!
	//Check PDE
	PDE = memory_BIUdirectrdw(PDBR+(DIR<<2)); //Read the page directory entry!
	if (!(PDE&PXE_P)) //Not present?
	{
		raisePF(address,(RW<<1)|(effectiveUS<<2)); //Run a not present page fault!
		return 0; //We have an error, abort!
	}
	
	//Check PTE
	PTE = memory_BIUdirectrdw(((PDE&PXE_ADDRESSMASK)>>PXE_ADDRESSSHIFT)+(TABLE<<2)); //Read the page table entry!
	if (!(PTE&PXE_P)) //Not present?
	{
		raisePF(address,(RW<<1)|(effectiveUS<<2)); //Run a not present page fault!
		return 0; //We have an error, abort!
	}

	if (!verifyCPL(RW,effectiveUS,((PDE&PXE_RW)>>1),((PDE&PXE_US)>>2),((PTE&PXE_RW)>>1),((PTE&PXE_US)>>2))) //Protection fault on combined flags?
	{
		raisePF(address,PXE_P|(RW<<1)|(effectiveUS<<2)); //Run a not present page fault!
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
	if (!(PDE&PXE_A)) //Not accessed yet?
	{
		PDE |= PXE_A; //Accessed!
		memory_BIUdirectwdw(PDBR+(DIR<<2),PDE); //Update in memory!
	}
	if (PTEUPDATED) //Updated?
	{
		memory_BIUdirectwdw(((PDE&PXE_ADDRESSMASK)>>PXE_ADDRESSSHIFT)+(TABLE<<2),PTE); //Update in memory!
	}
	Paging_writeTLB(-1,address,RW,effectiveUS,(PTE&PTE_D)?1:0,(PTE&PXE_ADDRESSMASK)); //Save the PTE 32-bit address in the TLB!
	return 1; //Valid!
}

byte CPU_Paging_checkPage(uint_32 address, byte readflags, byte CPL)
{
	return (isvalidpage(address,(readflags==0),CPL,(readflags&0x10))==0); //Are we an invalid page? We've raised an error! Bit4 is set during Prefetch operations!
}

uint_32 mappage(uint_32 address, byte iswrite, byte CPL) //Maps a page to real memory when needed!
{
	uint_32 result; //What address?
	if (!is_paging()) return address; //Direct address when not paging!
	byte effectiveUS;
	byte RW;
	RW = iswrite?1:0; //Are we trying to write?
	effectiveUS = getUserLevel(CPL); //Our effective user level!
	retrymapping: //Retry the mapping when not cached!
	if (Paging_readTLB(-1,address,1,effectiveUS,1,0,&result)) //Cache hit for a written dirty entry? Match completely only!
	{
		return (result|(address&PXE_ACTIVEMASK)); //Give the actual address from the TLB!
	}
	else if (Paging_readTLB(-1,address,RW,effectiveUS,RW,RW?0:TLB_IGNOREREADMASK,&result)) //Cache hit for an the entry, any during reads, Write Dirty on write?
	{
		return (result|(address&PXE_ACTIVEMASK)); //Give the actual address from the TLB!
	}
	else
	{
		if (isvalidpage(address,iswrite,CPL,0)) //Retry checking if possible!
		{
			goto retrymapping;
		}
	}
	return address; //Untranslated!
}

byte Paging_TLBSet(uint_32 logicaladdress) //Automatic set determination when using a set number <0!
{
	return ((logicaladdress&0x30000000)>>28); //The set is determined by the upper 2 bits of the entry, the memory block!
}

byte Paging_oldestTLB(sbyte set) //Find a TLB to be used/overwritten!
{
	byte x,oldest=0;
	for (x=0;x<8;++x) //Check all TLBs!
	{
		if ((CPU[activeCPU].Paging_TLB.TLB[set][x].TAG&1)==0) //Unused?
		{
			return x; //Give the unused entry!
		}
		if (CPU[activeCPU].Paging_TLB.TLB[set][x].age==7) //Oldest?
		{
			oldest = x; //Oldest entry to give if nothing is available!
		}
	}
	return oldest; //Give the oldest entry!
}

uint_32 Paging_generateTAG(uint_32 logicaladdress, byte RW, byte US, byte Dirty)
{
	return ((logicaladdress&0xFFFFF000)|(Dirty<<3)|(RW<<2)|(US<<1)|1); //The used TAG!
}

byte Paging_matchTLBaddress(uint_32 logicaladdress, uint_32 TAG)
{
	return (((logicaladdress&0xFFFFF000)|1)==((TAG&0xFFFFF000)|(TAG&1))); //The used TAG matches on address and availability only! Ignore US/RW!
}

//Build for age entry!
#define AGEENTRY_AGEENTRY(age,entry) (signed2unsigned8(age)|(entry<<8))
//What to sort by!
#define AGEENTRY_SORT(entry) unsigned2signed8(entry)
//Deconstruct for age entry!
#define AGEENTRY_AGE(entry) unsigned2signed8(entry)
#define AGEENTRY_ENTRY(entry) (entry>>8)

#define SWAP(a,b) if (AGEENTRY_SORT(sortarray[b]) < AGEENTRY_SORT(sortarray[a])) { tmp = sortarray[a]; sortarray[a] = sortarray[b]; sortarray[b] = tmp; }

void Paging_refreshAges(sbyte TLB_set) //Refresh the ages, with the entry specified as newest!
{
	word sortarray[8];
	INLINEREGISTER word tmp;
	INLINEREGISTER byte x,y;
	x = 0;
	//Age bit 3 is assigned to become 8+(invalid/unused, which is moved to the end with value assigned 0)!
	do
	{
		sortarray[x]= AGEENTRY_AGEENTRY(((sbyte)(((CPU[activeCPU].Paging_TLB.TLB[TLB_set][x].TAG&1)^1)<<3))+(CPU[activeCPU].Paging_TLB.TLB[TLB_set][x].age),x); //Move unused entries to the end and what entry are we!
	} while (++x<8);

	//Sort the 8 items! Bose-Nelson Algorithm! Created using http://jgamble.ripco.net/cgi-bin/nw.cgi?inputs=8&algorithm=best&output=svg
	SWAP(0,1); SWAP(2,3); SWAP(4,5) SWAP(6,7);
	SWAP(0,2); SWAP(1,3); SWAP(4,6); SWAP(5,7);
	SWAP(1,2); SWAP(5,6); SWAP(0,4); SWAP(3,7);
	SWAP(1,5); SWAP(2,6);
	SWAP(1,4); SWAP(3,6);
	SWAP(2,4); SWAP(3,5);
	SWAP(3,4);

	y = 0; //Initialize the aged entry to apply!
	x = 0; //Initialize the sorted entry location/age to apply!
	do //Apply the new order!
	{
		CPU[activeCPU].Paging_TLB.TLB[TLB_set][AGEENTRY_ENTRY(sortarray[x])].age = (y>>(AGEENTRY_AGE(sortarray[x])&8)); //Generated age or unused age(0)!
		++y; //Next when valid entry!
	} while (++x<8);
}

void Paging_writeTLB(sbyte TLB_set, uint_32 logicaladdress, byte RW, byte US, byte Dirty, uint_32 result)
{
	uint_32 TAG;
	if (TLB_set < 0) TLB_set = Paging_TLBSet(logicaladdress); //Auto set?
	TAG = Paging_generateTAG(logicaladdress,RW,US,Dirty); //Generate a TAG!
	byte entry;
	entry = Paging_oldestTLB(TLB_set); //Get the oldest/unused TLB!
	CPU[activeCPU].Paging_TLB.TLB[TLB_set][entry].age = -1; //Clear the age: we're the new last used!
	CPU[activeCPU].Paging_TLB.TLB[TLB_set][entry].data = result; //The result for the lookup!
	CPU[activeCPU].Paging_TLB.TLB[TLB_set][entry].TAG = TAG; //The TAG to find it by!
	Paging_refreshAges(TLB_set); //Refresh the ages!
}

//RWDirtyMask: (PXE_RW|PTE_D) for ignoring R/W and Dirty, use them otherwise!
byte Paging_readTLB(sbyte TLB_set, uint_32 logicaladdress, byte RW, byte US, byte Dirty, uint_32 RWDirtyMask, uint_32 *result)
{
	INLINEREGISTER uint_32 TAG, TAGMask;
	if (TLB_set < 0) TLB_set = Paging_TLBSet(logicaladdress); //Auto set?
	TAG = Paging_generateTAG(logicaladdress,RW,US,Dirty); //Generate a TAG!
	TAGMask = ~RWDirtyMask; //Store for fast usage to mask the tag bits unused off!
	if (RWDirtyMask) //Used?
	{
		TAG &= TAGMask; //Ignoring these bits, so mask them off when comparing!
	}
	INLINEREGISTER byte entry=0;
	do //Check all entries!
	{
 		if (unlikely((CPU[activeCPU].Paging_TLB.TLB[TLB_set][entry].TAG&TAGMask)==TAG)) //Found?
		{
			*result = CPU[activeCPU].Paging_TLB.TLB[TLB_set][entry].data; //Give the stored data!
			if (unlikely(CPU[activeCPU].Paging_TLB.TLB[TLB_set][entry].age)) //Not the newest age(which is always 0)?
			{
				CPU[activeCPU].Paging_TLB.TLB[TLB_set][entry].age = -1; //Clear the age: we're the new last used!
				Paging_refreshAges(TLB_set); //Refresh the ages!
			}
			return 1; //Found!
		}
	} while (++entry<8);
	return 0; //Not found!
}

void Paging_Invalidate(uint_32 logicaladdress) //Invalidate a single address!
{
	INLINEREGISTER byte TLB_set;
	INLINEREGISTER byte entry;
	for (TLB_set = 0; TLB_set < 4; ++TLB_set) //Process all possible sets!
	{
		for (entry = 0; entry < 8; ++entry) //Check all entries!
		{
			if (Paging_matchTLBaddress(logicaladdress, CPU[activeCPU].Paging_TLB.TLB[TLB_set][entry].TAG)) //Matched?
			{
				CPU[activeCPU].Paging_TLB.TLB[TLB_set][entry].TAG = 0; //Clear the entry to unused!
			}
		}
	}
}

void Paging_clearTLB()
{
	memset(&CPU[activeCPU].Paging_TLB,0,sizeof(CPU[activeCPU].Paging_TLB)); //Reset fully and clear the TLB!
}

void Paging_initTLB()
{
	Paging_clearTLB(); //Clear the TLB!
}

void Paging_TestRegisterWritten(byte TR)
{
	byte P, D, DC, U, UC, W, WC;
	uint_32 logicaladdress, result;
	byte hit;
	if (unlikely(TR==6)) //TR6: Test command register
	{
		//We're given a command to execute!
		P = (CPU[activeCPU].registers->TR6&0x800)?1:0;
		D = (CPU[activeCPU].registers->TR6&0x400)?1:0;
		DC = (CPU[activeCPU].registers->TR6&0x200)?1:0;
		U = (CPU[activeCPU].registers->TR6&0x100)?1:0;
		UC = (CPU[activeCPU].registers->TR6&0x80)?1:0;
		W = (CPU[activeCPU].registers->TR6&0x40)?1:0;
		WC = (CPU[activeCPU].registers->TR6&0x20)?1:0;
		logicaladdress = CPU[activeCPU].registers->TR6&PXE_ADDRESSMASK; //What address!

		if (CPU[activeCPU].registers->TR6&1) //Lookup command?
		{
			if ((DC == (D ^ 1)) && (UC == (U ^ 1)) && (WC == (W ^ 1))) //Valid complements?
			{
				if (Paging_readTLB(0, logicaladdress, W, U, D, 0, &result)) //Read?
				{
					hit = 1; //Hit!
				}
				else if (Paging_readTLB(1, logicaladdress, W, U, D, 0, &result)) //Read?
				{
					hit = 2; //Hit!
				}
				else if (Paging_readTLB(2, logicaladdress, W, U, D, 0, &result)) //Read?
				{
					hit = 3; //Hit!
				}
				else if (Paging_readTLB(3, logicaladdress, W, U, D, 0, &result)) //Read?
				{
					hit = 4; //Hit!
				}
				if (hit==0) //Not hit?
				{
					noTRhit: //No hit after all?
					CPU[activeCPU].registers->TR7 &= ~0x10; //Not hit!
				}
				else //Hit?
				{
					CPU[activeCPU].registers->TR7 = (0x10 | ((hit - 1) << 2) | (result&PXE_ADDRESSMASK)); //Hit!
				}
			}
			else //Invalid components?
			{
				goto noTRhit; //Apply no hit!
			}
		}
		else //Write command?
		{
			if ((DC == (D ^ 1)) && (UC == (U ^ 1)) && (WC == (W ^ 1))) //Valid complements?
			{
				if (CPU[activeCPU].registers->TR6 & 0x10) //Hit?
				{
					Paging_writeTLB((CPU[activeCPU].registers->TR7 >> 2) & 3, logicaladdress, W, U, D, (result&PXE_ADDRESSMASK)); //Write to the associated block!
				}
			}
		}
	}
}