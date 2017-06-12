#include "headers/types.h" //Basic types!
#include "headers/cpu/cpu.h" //CPU!
#include "headers/bios/bios.h" //BIOS!
#include "headers/cpu/easyregs.h" //Easy registers!
#include "headers/cpu/cb_manager.h" //Callback support!
#include "headers/cpu/protection.h" //Protection support!
#include "headers/emu/debugger/debugger.h" //For logging registers!
#include "headers/cpu/multitasking.h" //Multitasking support!
#include "headers/mmu/mmuhandler.h" //Direct memory access support! 
#include "headers/support/log.h" //Logging support for debugging!
#include "headers/cpu/biu.h" //BIU support!
#include "headers/cpu/cpu_OP8086.h" //8086 support!

//Are we to disable NMI's from All(or Memory only)?
#define DISABLE_MEMNMI
//#define DISABLE_NMI
//Log the INT10h call to set 640x480x256 color mode.
//#define LOG_ET34K640480256_SET
//Log the INT calls and IRETs when defined.
//#define LOG_INTS

void CPU_setint(byte intnr, word segment, word offset) //Set real mode IVT entry!
{
	MMU_ww(-1,0x0000,((intnr<<2)|2),segment,0); //Copy segment!
	MMU_ww(-1,0x0000,(intnr<<2),offset,0); //Copy offset!
}

void CPU_getint(byte intnr, word *segment, word *offset) //Set real mode IVT entry!
{
	*segment = MMU_rw(-1,0x0000,((intnr<<2)|2),0,0); //Copy segment!
	*offset = MMU_rw(-1,0x0000,(intnr<<2),0,0); //Copy offset!
}

extern uint_32 destEIP;

//Interrupt support for timings!
extern byte CPU_interruptraised; //Interrupt raised flag?

word oldCS, oldIP, waitingforiret=0;

word destINTCS, destINTIP;
byte CPU_customint(byte intnr, word retsegment, uint_32 retoffset, int_64 errorcode) //Used by soft (below) and exceptions/hardware!
{
	char errorcodestr[256];
	word destCS;
	CPU_interruptraised = 1; //We've raised an interrupt!
	if (getcpumode()==CPU_MODE_REAL) //Use IVT structure in real mode only!
	{
		if (CPU[activeCPU].registers->IDTR.limit<((intnr<<2)|3)) //IVT limit too low?
		{
			if (CPU_faultraised(8)) //Able to fault?
			{
				return CPU_INT(8,-1); //IVT limit problem or double fault redirect!
			}
			else return 0; //Abort on triple fault!
		}
		#ifdef LOG_ET34K640480256_SET
		if ((intnr==0x10) && (CPU[activeCPU].registers->AX==0x002E) && (errorcode==-1)) //Setting the video mode to 0x2E?
		{
			waitingforiret = 1; //Waiting for IRET!
			oldIP = retoffset;
			oldCS = retsegment; //Backup the return position!
		}
		#endif
		if (EMULATED_CPU>=CPU_80286) //80286+ CPU?
		{
			CPU_PUSH16(&REG_FLAGS); //Push flags!
			CPU_PUSH16(&retsegment); //Push segment!
			word retoffset16 = (retoffset&0xFFFF);
			CPU_PUSH16(&retoffset16);
		}
		else //Cycle-accurate way?
		{
			if (CPU8086_internal_interruptPUSHw(0,&REG_FLAGS)) return 0; //Busy pushing flags!
			if (CPU8086_internal_interruptPUSHw(2,&retsegment)) return 0; //Busy pushing return segment!
			word retoffset16 = (retoffset&0xFFFF);
			if (CPU8086_internal_interruptPUSHw(4,&retoffset16)) return 0; //Busy pushing return offset!
		}
		FLAGW_IF(0); //We're calling the interrupt!
		FLAGW_TF(0); //We're calling an interrupt, resetting debuggers!
		//Now, jump to it!
		if (EMULATED_CPU>=CPU_80286) //80286+ CPU?
		{
			destINTIP = memory_directrw((intnr<<2)+CPU[activeCPU].registers->IDTR.base); //JUMP to position CS:EIP/CS:IP in table.
			destINTCS = memory_directrw(((intnr<<2)|2) + CPU[activeCPU].registers->IDTR.base); //Destination CS!
		}
		else //Cycle-accurate way?
		{
			if (CPU[activeCPU].internalinterruptstep==6) //Handle specific EU timings here?
			{
				if (EMULATED_CPU==CPU_8086) //Known timings in between?
				{
					CPU[activeCPU].cycles_OP += 36; //We take 20 cycles to execute on a 8086/8088 EU!
					++CPU[activeCPU].internalinterruptstep; //Next step to be taken!
					CPU[activeCPU].executed = 0; //We haven't executed!
					return 0; //Waiting to complete!
				}
				else ++CPU[activeCPU].internalinterruptstep; //Skip anyways!
			}
			if (CPU8086_internal_stepreadinterruptw(7,-2,0,(intnr<<2)+CPU[activeCPU].registers->IDTR.base,&destINTIP,0)) return 0; //Read destination IP!
			if (CPU8086_internal_stepreadinterruptw(9,-2,0,((intnr<<2)|2) + CPU[activeCPU].registers->IDTR.base,&destINTCS,0)) return 0; //Read destination CS!
		}
		//Load EIP and CS destination to use from the original 16-bit data!
		destEIP = (uint_32)destINTIP;
		destCS = destINTCS;
		cleardata(&errorcodestr[0],sizeof(errorcodestr)); //Clear the error code!
		if (errorcode==-1) //No error code?
		{
			strcpy(errorcodestr,"-1");
		}
		else
		{
			sprintf(errorcodestr,"%08X",(uint_32)errorcode); //The error code itself!
		}
		#ifdef LOG_INTS
		dolog("cpu","Interrupt %02X=%04X:%08X@%04X:%04X(%02X); ERRORCODE: %s",intnr,destCS,destEIP,CPU[activeCPU].registers->CS,CPU[activeCPU].registers->EIP,CPU[activeCPU].lastopcode,errorcodestr); //Log the current info of the call!
		#endif
		if (debugger_logging()) dolog("debugger","Interrupt %02X=%04X:%08X@%04X:%04X(%02X); ERRORCODE: %s",intnr,destINTCS,destEIP,CPU[activeCPU].registers->CS,CPU[activeCPU].registers->EIP,CPU[activeCPU].lastopcode,errorcodestr); //Log the current info of the call!
		segmentWritten(CPU_SEGMENT_CS,destCS,0); //Interrupt to position CS:EIP/CS:IP in table.
		CPU_flushPIQ(-1); //We're jumping to another address!
		//No error codes are pushed in (un)real mode! Only in protected mode!
		return 1; //OK!
	}
	//Use Protected mode IVT?
	return CPU_ProtectedModeInterrupt(intnr,retsegment,retoffset,errorcode); //Execute the protected mode interrupt!
}


byte CPU_INT(byte intnr, int_64 errorcode) //Call an software interrupt; WARNING: DON'T HANDLE ANYTHING BUT THE REGISTERS ITSELF!
{
	//Now, jump to it!
	return CPU_customint(intnr,REG_CS,REG_EIP,errorcode); //Execute real interrupt, returning to current address!
}

byte NMIMasked = 0; //Are NMI masked?

void CPU_IRET()
{
	word V86SegRegs[5]; //All V86 mode segment registers!
	byte V86SegReg; //Currently processing segment register!
	byte oldCPL = getCPL(); //Original CPL
	word tempCS, tempSS;
	uint_32 tempEFLAGS;
	if (getcpumode()==CPU_MODE_REAL) //Use IVT?
	{
		if (checkStackAccess(3,0,0)) return; //3 Word POPs!
		destEIP = CPU_POP16(); //POP IP!
		segmentWritten(CPU_SEGMENT_CS,CPU_POP16(),3); //We're loading because of an IRET!
		CPU_flushPIQ(-1); //We're jumping to another address!
		if (CPU[activeCPU].faultraised==0) //No fault raised?
		{
			REG_FLAGS = CPU_POP16(); //Pop flags!
		}
		#ifdef LOG_INTS
		dolog("cpu","IRET to %04X:%04X",CPU[activeCPU].registers->CS,CPU[activeCPU].registers->EIP); //Log the current info of the call!
		#endif
		#ifdef LOG_ET34K640480256_SET
		if (waitingforiret) //Waiting for IRET?
		{
			//if ((REG_CS==oldCS) && (REG_IP==oldIP)) //Returned?
			{
				waitingforiret = 0; //We're finished with the logging information!
			}
		}
		#endif
	}
	else //Use protected mode IRET?
	{
		if (FLAG_V8) //Virtual 8086 mode?
		{
			//According to: http://x86.renejeschke.de/html/file_module_x86_id_145.html
			if (FLAG_PL==3) //IOPL==3? Processor is in virtual-8086 mode when IRET is executed and stays in virtual-8086 mode
			{
				if (CODE_SEGMENT_DESCRIPTOR_D_BIT()) //32-bit operand size?
				{
					if (checkStackAccess(3,0,1)) return; //3 DWord POPs!
					destEIP = CPU_POP32();
					tempCS = (CPU_POP32()&0xFFFF);
					tempEFLAGS = CPU_POP32();
					segmentWritten(CPU_SEGMENT_CS,tempCS,3); //Jump to the CS, IRET style!
					if (CPU[activeCPU].faultraised) return; //Abort on fault!
					REG_EFLAGS = tempEFLAGS; //Restore EFLAGS!
				}
				else //16-bit operand size?
				{
					if (checkStackAccess(3,0,0)) return; //3 Word POPs!
					destEIP = CPU_POP16();
					tempCS = CPU_POP16();
					tempEFLAGS = CPU_POP16();
					segmentWritten(CPU_SEGMENT_CS,tempCS,3); //Jump to the CS, IRET style!
					if (CPU[activeCPU].faultraised) return; //Abort on fault!
					REG_FLAGS = tempEFLAGS; //Restore FLAGS, leave high DWord unmodified(VM, IOPL, VIP and VIF are unmodified, only bits 0-15)!
				}
			}
			else
			{
				THROWDESCGP(0,0,0); //Throw #GP(0) to trap to the VM monitor!
			}
			return; //Abort!
		}

		//Normal protected mode?
		if (FLAG_NT && (getcpumode() != CPU_MODE_REAL)) //Protected mode Nested Task IRET?
		{
			SEGDESCRIPTOR_TYPE newdescriptor; //Temporary storage!
			word desttask;
			desttask = MMU_rw(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, 0, 0,0); //Read the destination task!
			if (!LOADDESCRIPTOR(CPU_SEGMENT_TR, desttask, &newdescriptor)) //Error loading new descriptor? The backlink is always at the start of the TSS!
			{
				CPU_TSSFault(desttask,0,(desttask&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!
				return; //Error, by specified reason!
			}
			CPU_switchtask(CPU_SEGMENT_TR,&newdescriptor,&CPU[activeCPU].registers->TR,desttask,3,0,-1); //Execute an IRET to the interrupted task!
		}
		else //Normal IRET?
		{
			uint_32 tempesp;
			if (CODE_SEGMENT_DESCRIPTOR_D_BIT()) //32-bit?
			{
				if (checkStackAccess(3,0,1)) return; //Top 12 bytes!
			}
			else //16-bit?
			{
				if (checkStackAccess(3,0,0)) return; //Top 6 bytes!
			}
			
			if (CODE_SEGMENT_DESCRIPTOR_D_BIT()) //32-bit mode?
			{
				destEIP = CPU_POP32(); //POP EIP!
			}
			else
			{
				destEIP = (uint_32)CPU_POP16(); //POP IP!
			}
			tempCS = CPU_POP16(); //CS to be loaded!
			if (CODE_SEGMENT_DESCRIPTOR_D_BIT()) //32-bit mode?
			{
				tempEFLAGS = CPU_POP32(); //Pop flags!
			}
			else
			{
				tempEFLAGS = (uint_32)CPU_POP16(); //Pop flags!
			}

			if ((tempEFLAGS&0x20000) && (!oldCPL)) //Returning to virtual 8086 mode?
			{
				if (checkStackAccess(12,0,1)) return; //First level IRET data?
				tempesp = CPU_POP32(); //POP ESP!
				tempSS = (CPU_POP32()&0xFFFF); //POP SS!
				for (V86SegReg=0;V86SegReg<NUMITEMS(V86SegRegs);++V86SegReg)//POP required remaining registers into buffers first!
				{
					V86SegRegs[V86SegReg] = (CPU_POP32()&0xFFFF); //POP segment register! Throw away high word!
				}
				REG_EFLAGS = tempEFLAGS; //Set EFLAGS to the tempEFLAGS
				updateCPUmode(); //Update the CPU mode to return to Virtual 8086 mode!
				//Load POPped registers into the segment registers, CS:EIP and SS:ESP in V86 mode(raises no faults) to restore the task.
				segmentWritten(CPU_SEGMENT_CS,tempCS,3); //We're loading because of an IRET!
				segmentWritten(CPU_SEGMENT_SS,tempSS,0); //Load SS!
				REG_ESP = tempesp; //Set the new ESP of the V86 task!
				segmentWritten(CPU_SEGMENT_ES,V86SegRegs[0],0); //Load ES!
				segmentWritten(CPU_SEGMENT_DS,V86SegRegs[1],0); //Load DS!
				segmentWritten(CPU_SEGMENT_FS,V86SegRegs[2],0); //Load FS!
				segmentWritten(CPU_SEGMENT_GS,V86SegRegs[3],0); //Load GS!
			}
			else //Normal protected mode return?
			{
				REG_EFLAGS = tempEFLAGS; //Restore EFLAGS normally.
				segmentWritten(CPU_SEGMENT_CS,tempCS,3); //We're loading because of an IRET!
				CPU_flushPIQ(-1); //We're jumping to another address!
			}
		}
	}
	//Special effect: re-enable NMI!
	NMIMasked = 0; //We're allowing NMI again!
}

extern byte SystemControlPortA; //System control port A data!
extern byte SystemControlPortB; //System control port B data!
extern byte PPI62; //For XT support!
byte NMI = 1; //NMI Disabled?

extern word CPU_exec_CS;
extern uint_32 CPU_exec_EIP;

byte execNMI(byte causeisMemory) //Execute an NMI!
{
	byte doNMI = 0;
	if (causeisMemory) //I/O error on memory?
	{
		if (EMULATED_CPU >= CPU_80286) //AT?
		{
			if ((SystemControlPortB & 4)==0) //Parity check enabled(the enable bits are reversed according to the AT BIOS)?
			{
				SystemControlPortB |= 0x80; //Signal a Memory error!
				doNMI = 1; //Allow NMI, if enabled!
			}
		}
		else //XT?
		{
			if ((SystemControlPortB & 0x10)==0) //Enabled?
			{
				PPI62 |= 0x80; //Signal a Memory error on a XT!
				doNMI = 1; //Allow NMI, if enabled!
			}
		}
		#ifdef DISABLE_MEMNMI
			return 1; //We don't handle any NMI's from Bus or Memory through the NMI PIN!
		#endif
	}
	else //Cause is I/O?
	{
		//Bus error?
		if (EMULATED_CPU >= CPU_80286) //AT?
		{
			if ((SystemControlPortB & 8)==0) //Channel check enabled(the enable bits are reversed according to the AT BIOS)?
			{
				SystemControlPortB |= 0x40; //Signal a Bus error!
				doNMI = 1; //Allow NMI, if enabled!
			}
		}
		else //XT?
		{
			if ((SystemControlPortB & 0x20)==0) //Parity check enabled?
			{
				PPI62 |= 0x40; //Signal a Parity error on a XT!
				doNMI = 1; //Allow NMI, if enabled!
			}
		}
	}

#ifdef DISABLE_NMI
	return 1; //We don't handle any NMI's from Bus or Memory through the NMI PIN!
#endif
	if (!NMI && !NMIMasked) //NMI interrupt enabled and not masked off?
	{
		NMIMasked = 1; //Mask future NMI!
		if (doNMI && CPU[activeCPU].allowInterrupts) //I/O error on memory or bus?
		{
			if (CPU_faultraised(EXCEPTION_NMI))
			{
				CPU_customint(EXCEPTION_NMI, CPU_exec_CS, CPU_exec_EIP,0); //Return to opcode!
			}
			CPU[activeCPU].cycles_HWOP = 50; /* Normal interrupt as hardware interrupt */
			return 0; //We're handled!
		}
	}
	return 1; //Unhandled NMI!
}
