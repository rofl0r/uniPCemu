#include "headers/types.h" //Basic types!
#include "headers/cpu/cpu.h" //CPU!
#include "headers/cpu/easyregs.h" //Easy registers!
#include "headers/cpu/protection.h" //Protection support!
#include "headers/emu/debugger/debugger.h" //For logging registers!
#include "headers/mmu/mmuhandler.h" //Direct memory access support! 
#include "headers/support/log.h" //Logging support for debugging!
#include "headers/cpu/cpu_OP8086.h" //8086 support!
#include "headers/cpu/cpu_execution.h" //Execution phase support!
#include "headers/cpu/biu.h" //PIQ flushing support!
#include "headers/cpu/multitasking.h" //Task switching/faulting support!

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

extern byte singlestep; //Enable EMU-driven single step!
extern byte allow_debuggerstep; //Disabled by default: needs to be enabled by our BIOS!
extern byte advancedlog; //Advanced log setting

extern byte MMU_logging; //Are we logging from the MMU?
extern byte REPPending; //Pending REP reset?
word destINTCS, destINTIP;
byte CPU_customint(byte intnr, word retsegment, uint_32 retoffset, int_64 errorcode, byte is_interrupt) //Used by soft (below) and exceptions/hardware!
{
	byte checkinterruptstep;
	char errorcodestr[256];
	word destCS;
	CPU[activeCPU].executed = 0; //Default: still busy executing!
	CPU_interruptraised = 1; //We've raised an interrupt!
	REPPending = CPU[activeCPU].repeating = 0; //Not repeating anymore!
	if ((getcpumode()==CPU_MODE_REAL) || (errorcode==-4)) //Use IVT structure in real mode only, or during VME when processing real-mode style interrupts(errorcode of -4)!
	{
		if (((errorcode == -4)?0x3FF:(CPU[activeCPU].registers->IDTR.limit))<((intnr<<2)|3)) //IVT limit too low?
		{
			if ((MMU_logging == 1) && advancedlog) //Are we logging?
			{
				dolog("debugger","#DF fault(-1)!");
			}

			if (CPU_faultraised(8)) //Able to fault?
			{
				CPU_executionphase_startinterrupt(8,2,0); //IVT limit problem or double fault redirect!
				return 0; //Abort!
			}
			else return 0; //Abort on triple fault!
		}
		//errorcode=-4: VME extensions are used!
		#ifdef LOG_ET34K640480256_SET
		if ((intnr==0x10) && (CPU[activeCPU].registers->AX==0x002E) && (errorcode==-1)) //Setting the video mode to 0x2E?
		{
			waitingforiret = 1; //Waiting for IRET!
			oldIP = retoffset;
			oldCS = retsegment; //Backup the return position!
		}
		#endif
		checkinterruptstep = 0; //Init!
		if (CPU[activeCPU].internalinterruptstep == checkinterruptstep)
		{
			if (errorcode == -4) //VME extensions require paging check as well?
			{
				if (checkMMUaccess16(-1, 0, (intnr << 2), 1|0xA0, getCPL(), 0, 0 | 0x8)) return 0; //Direct access Fault on IP?
				if (checkMMUaccess16(-1, 0, ((intnr << 2)|2), 1|0xA0, getCPL(), 0, 0 | 0x8)) return 0; //Direct access Fault on CS?
			}
			if (checkStackAccess(3, 1, 0)) return 0; //We must allow three pushes to succeed to be able to throw an interrupt!
		}
		word flags;
		flags = REG_FLAGS; //Default flags!
		if ((getcpumode() == CPU_MODE_8086) && (errorcode == -4) && (FLAG_PL != 3)) //Use virtual interrupt flag instead?
		{
			flags = (flags&~F_IF) | (FLAG_VIF ? F_IF : 0); //Replace the interrupt flag on the stack image with the Virtual Interrupt flag instead!
			flags |= 0x3000; //IOPL of 3 on the stack image!
		}

		if (CPU8086_internal_interruptPUSHw(checkinterruptstep,&flags,0)) return 0; //Busy pushing flags!
		checkinterruptstep += 2;
		if (CPU8086_internal_interruptPUSHw(checkinterruptstep,&retsegment,0)) return 0; //Busy pushing return segment!
		checkinterruptstep += 2;
		word retoffset16 = (retoffset&0xFFFF);
		if (CPU8086_internal_interruptPUSHw(checkinterruptstep,&retoffset16,0)) return 0; //Busy pushing return offset!
		checkinterruptstep += 2;
		if (CPU[activeCPU].internalinterruptstep==checkinterruptstep) //Handle specific EU timings here?
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
		++checkinterruptstep;
		if (CPU8086_internal_stepreadinterruptw(checkinterruptstep,-1,0,(intnr<<2)+((errorcode!=-4)?CPU[activeCPU].registers->IDTR.base:0),&destINTIP,0)) return 0; //Read destination IP!
		checkinterruptstep += 2;
		if (CPU8086_internal_stepreadinterruptw(checkinterruptstep,-1,0,((intnr<<2)|2) + ((errorcode!=-4)?CPU[activeCPU].registers->IDTR.base:0),&destINTCS,0)) return 0; //Read destination CS!
		checkinterruptstep += 2;

		if ((getcpumode() == CPU_MODE_8086) && (errorcode == -4) && (FLAG_PL != 3)) //Use virtual interrupt flag instead?
		{
			FLAGW_VIF(0); //We're calling the interrupt!
		}
		else //Normal interrupt flag handling!
		{
			FLAGW_IF(0); //We're calling the interrupt!
		}
		FLAGW_TF(0); //We're calling an interrupt, resetting debuggers!
		
		if (EMULATED_CPU>=CPU_80486)
		{
			FLAGW_AC(0); //Clear Alignment Check flag too!
		}

		//Load EIP and CS destination to use from the original 16-bit data!
		destEIP = (uint_32)destINTIP;
		destCS = destINTCS;
		cleardata(&errorcodestr[0],sizeof(errorcodestr)); //Clear the error code!
		if (errorcode==-1) //No error code?
		{
			safestrcpy(errorcodestr,sizeof(errorcodestr),"-1");
		}
		else
		{
			snprintf(errorcodestr,sizeof(errorcodestr),"%08X",(uint_32)errorcode); //The error code itself!
		}
		#ifdef LOG_INTS
		dolog("cpu","Interrupt %02X=%04X:%08X@%04X:%04X(%02X); ERRORCODE: %s; STACK=%04X:%08X",intnr,destCS,destEIP,CPU[activeCPU].registers->CS,CPU[activeCPU].registers->EIP,CPU[activeCPU].lastopcode,errorcodestr,REG_SS,REG_ESP); //Log the current info of the call!
		#endif
		if ((MMU_logging == 1) && advancedlog) dolog("debugger","Interrupt %02X=%04X:%08X@%04X:%04X(%02X); ERRORCODE: %s",intnr,destINTCS,destEIP,CPU[activeCPU].registers->CS,CPU[activeCPU].registers->EIP,CPU[activeCPU].lastopcode,errorcodestr); //Log the current info of the call!
		if (segmentWritten(CPU_SEGMENT_CS,destCS,0)) return 1; //Interrupt to position CS:EIP/CS:IP in table.
		CPU_flushPIQ(-1); //We're jumping to another address!
		CPU[activeCPU].executed = 1; //We've executed: process the next instruction!
		CPU_interruptcomplete(); //Prepare us for new instructions!

		//No error codes are pushed in (un)real mode! Only in protected mode!
		return 1; //OK!
	}
	//Use Protected mode IVT?
	return CPU_ProtectedModeInterrupt(intnr,retsegment,retoffset,errorcode,is_interrupt); //Execute the protected mode interrupt!
}

word INTreturn_CS=0xCCCC;
uint_32 INTreturn_EIP=0xCCCCCCCC;

byte CPU_INT(byte intnr, int_64 errorcode, byte is_interrupt) //Call an software interrupt; WARNING: DON'T HANDLE ANYTHING BUT THE REGISTERS ITSELF!
{
	//Now, jump to it!
	return CPU_customint(intnr,INTreturn_CS,INTreturn_EIP,errorcode,is_interrupt); //Execute real interrupt, returning to current address!
}

byte NMIMasked = 0; //Are NMI masked?

extern word CPU_exec_CS; //OPCode CS
extern uint_32 CPU_exec_EIP; //OPCode EIP

word IRET_IP=0, IRET_CS=0, IRET_FLAGS=0;

void CPU_IRET()
{
	word V86SegRegs[4]; //All V86 mode segment registers!
	byte V86SegReg; //Currently processing segment register!
	byte oldCPL = getCPL(); //Original CPL
	word tempCS, tempSS;
	uint_32 tempEFLAGS;
	//Special effect: re-enable NMI! This isn't dependent on the instruction succeeding or not(nor documented, just when it's executed)!
	NMIMasked = 0; //We're allowing NMI again!

	if (getcpumode()==CPU_MODE_REAL) //Use IVT?
	{
		//uint_32 backupESP = REG_ESP;
		if (CPU[activeCPU].stackchecked==0) { if (checkStackAccess(3,0,CPU_Operand_size[activeCPU])) { return; } ++CPU[activeCPU].stackchecked; } //3 Word POPs!
		if (CPU8086_internal_POPw(0,&IRET_IP,CPU_Operand_size[activeCPU])) return; //POP IP!
		if (CPU8086_internal_POPw(2,&IRET_CS,CPU_Operand_size[activeCPU])) return; //POP CS!
		if (CPU8086_internal_POPw(4,&IRET_FLAGS,CPU_Operand_size[activeCPU])) return; //POP FLAGS!
		destEIP = (uint_32)IRET_IP; //POP IP!
		if (segmentWritten(CPU_SEGMENT_CS, IRET_CS, 3)) return; //We're loading because of an IRET!
		CPU_flushPIQ(-1); //We're jumping to another address!
		REG_FLAGS = IRET_FLAGS; //Pop flags!
		#ifdef LOG_INTS
		dolog("cpu","IRET@%04X:%08X to %04X:%04X; STACK=%04X:%08X",CPU_exec_CS,CPU_exec_EIP,CPU[activeCPU].registers->CS,CPU[activeCPU].registers->EIP,tempSS,backupESP); //Log the current info of the call!
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
		return; //Finished!
	}

	//Use protected mode IRET?
	if (FLAG_V8) //Virtual 8086 mode?
	{
		//According to: http://x86.renejeschke.de/html/file_module_x86_id_145.html
		if (FLAG_PL==3) //IOPL==3? Processor is in virtual-8086 mode when IRET is executed and stays in virtual-8086 mode
		{
			if (CPU_Operand_size[activeCPU]) //32-bit operand size?
			{
				if (checkStackAccess(3,0,1)) return; //3 DWord POPs!
				destEIP = CPU_POP32();
				tempCS = (CPU_POP32()&0xFFFF);
				tempEFLAGS = CPU_POP32();
				if (segmentWritten(CPU_SEGMENT_CS,tempCS,3)) return; //Jump to the CS, IRET style!
				//VM&IOPL aren't changed by the POP!
				tempEFLAGS = (tempEFLAGS&~(0x23000|F_VIF|F_VIP))|(REG_EFLAGS&(0x23000|F_VIF|F_VIP)); //Don't modfiy changed flags that we're not allowed to!
				REG_EFLAGS = tempEFLAGS; //Restore EFLAGS!
				updateCPUmode();
			}
			else //16-bit operand size?
			{
				if (checkStackAccess(3,0,0)) return; //3 Word POPs!
				destEIP = (uint_32)CPU_POP16(0);
				tempCS = CPU_POP16(0);
				tempEFLAGS = CPU_POP16(0);
				if (segmentWritten(CPU_SEGMENT_CS, tempCS, 3)) return; //Jump to the CS, IRET style!
				//VM&IOPL aren't changed by the POP!
				tempEFLAGS = (tempEFLAGS&~(0x23000|F_VIF|F_VIP))|(REG_EFLAGS&(0x23000|F_VIF|F_VIP)); //Don't modfiy changed flags that we're not allowed to!
				REG_FLAGS = tempEFLAGS; //Restore FLAGS, leave high DWord unmodified(VM, IOPL, VIP and VIF are unmodified, only bits 0-15)!
			}
		}
		else //PL!=3?
		{
			if (((CPU[activeCPU].registers->CR4 & 1) && (EMULATED_CPU>=CPU_PENTIUM)) && (CPU_Operand_size[activeCPU]==0)) //Pentium with VME enabled, executing 16-bit IRET?
			{
				//Execute 16-bit POP normally
				if (checkStackAccess(3, 0, 0)) return; //3 Word POPs!
				destEIP = (uint_32)CPU_POP16(0);
				tempCS = CPU_POP16(0);
				tempEFLAGS = CPU_POP16(0);
				if (segmentWritten(CPU_SEGMENT_CS, tempCS, 3)) return; //Jump to the CS, IRET style!
				//VM&IOPL aren't changed by the POP!
				if (tempEFLAGS&F_TF) //Trap flag set?
				{
					THROWDESCGP(0, 0, 0); //Throw #GP(0) to trap to the VM monitor!
				}
				else //VIF from IF!
				{
					FLAGW_VIF((tempEFLAGS&F_IF) ? 1 : 0); //Virtual interrupt flag from Interrupt flag on the stack!
				}
				tempEFLAGS = (tempEFLAGS&~(0x23000 | F_VIF | F_VIP | F_IF)) | (REG_EFLAGS&(0x23000 | F_VIF | F_VIP | F_IF)); //Don't modfiy changed flags that we're not allowed to!
				if (FLAG_VIP && FLAG_VIF) //Virtual interrupt flag has been set by software while pending?
				{
					THROWDESCGP(0, 0, 0); //Throw #GP(0) to trap to the VM monitor!
				}
				else //Finish normally!
				{
					REG_FLAGS = tempEFLAGS; //Restore FLAGS, leave high DWord unmodified(VM, IOPL, VIP and VIF are unmodified, only bits 0-15)!
				}
			}
			else //Normal handling?
			{
				THROWDESCGP(0, 0, 0); //Throw #GP(0) to trap to the VM monitor!
			}
		}
		return; //Finished!
	}

	//NT flag is set? If so, perform a task switch back to the task we're nested in(undocumented)! http://nicolascormier.com/documentation/hardware/microprocessors/intel/i80386/Chap15.html 
	if (FLAG_NT && (getcpumode() != CPU_MODE_REAL)) //Protected mode Nested Task IRET?
	{
		SEGMENT_DESCRIPTOR newdescriptor; //Temporary storage!
		word desttask;
		sbyte loadresult;
		desttask = MMU_rw(CPU_SEGMENT_TR, CPU[activeCPU].registers->TR, 0, 0,0); //Read the destination task!
		if ((loadresult = LOADDESCRIPTOR(CPU_SEGMENT_TR, desttask, &newdescriptor,3))<=0) //Error loading new descriptor? The backlink is always at the start of the TSS!
		{
			if (loadresult == -1) return; //Abort on page fault!
			CPU_TSSFault(desttask,0,(desttask&4)?EXCEPTION_TABLE_LDT:EXCEPTION_TABLE_GDT); //Throw error!
			return; //Error, by specified reason!
		}
		CPU_executionphase_starttaskswitch(CPU_SEGMENT_TR,&newdescriptor,&CPU[activeCPU].registers->TR,desttask,3,0,-1); //Execute an IRET to the interrupted task!
		return; //Finished!
	}

	//Normal protected mode IRET?
	uint_32 tempesp;
	if (CPU_Operand_size[activeCPU]) //32-bit?
	{
		if (checkStackAccess(3,0,1)) return; //Top 12 bytes!
	}
	else //16-bit?
	{
		if (checkStackAccess(3,0,0)) return; //Top 6 bytes!
	}
			
	if (CPU_Operand_size[activeCPU]) //32-bit mode?
	{
		destEIP = CPU_POP32(); //POP EIP!
	}
	else
	{
		destEIP = (uint_32)CPU_POP16(0); //POP IP!
	}
	tempCS = CPU_POP16(CPU_Operand_size[activeCPU]); //CS to be loaded!
	if (CPU_Operand_size[activeCPU]) //32-bit mode?
	{
		tempEFLAGS = CPU_POP32(); //Pop eflags!
	}
	else
	{
		tempEFLAGS = (uint_32)CPU_POP16(0); //Pop flags!
	}

	if ((tempEFLAGS&0x20000) && (!oldCPL)) //Returning to virtual 8086 mode?
	{
		if (checkStackAccess(6,0,1)) return; //First level IRET data?
		tempesp = CPU_POP32(); //POP ESP!
		tempSS = (CPU_POP32()&0xFFFF); //POP SS!
		for (V86SegReg=0;V86SegReg<NUMITEMS(V86SegRegs);)//POP required remaining registers into buffers first!
		{
			V86SegRegs[V86SegReg++] = (CPU_POP32()&0xFFFF); //POP segment register! Throw away high word!
		}
		REG_EFLAGS = tempEFLAGS; //Set EFLAGS to the tempEFLAGS
		updateCPUmode(); //Update the CPU mode to return to Virtual 8086 mode!
		//Load POPped registers into the segment registers, CS:EIP and SS:ESP in V86 mode(raises no faults) to restore the task.
		if (segmentWritten(CPU_SEGMENT_CS,tempCS,3)) return; //We're loading because of an IRET!
		if (segmentWritten(CPU_SEGMENT_SS,tempSS,0)) return; //Load SS!
		REG_ESP = tempesp; //Set the new ESP of the V86 task!
		if (segmentWritten(CPU_SEGMENT_ES,V86SegRegs[0],0)) return; //Load ES!
		if (segmentWritten(CPU_SEGMENT_DS,V86SegRegs[1],0)) return; //Load DS!
		if (segmentWritten(CPU_SEGMENT_FS, V86SegRegs[2], 0)) return; //Load FS!
		if (segmentWritten(CPU_SEGMENT_GS,V86SegRegs[3],0)) return; //Load GS!
	}
	else //Normal protected mode return?
	{
		if (CPU_Operand_size[activeCPU]==0) tempEFLAGS |= (REG_EFLAGS&0xFFFF0000); //Pop flags only, not EFLAGS!
		//Check unchanging bits!
		tempEFLAGS = (tempEFLAGS&~F_V8)|(REG_EFLAGS&F_V8); //When returning to a V86-mode task from a non-PL0 handler, the VM flag isn't updated, so it stays in protected mode!
		if (getCPL())
		{
			tempEFLAGS = (tempEFLAGS&~F_IOPL) | (REG_EFLAGS&F_IOPL); //Disallow IOPL being changed!
			if (EMULATED_CPU >= CPU_PENTIUM) //VIP and VIF as well are protected?
			{
				tempEFLAGS = (tempEFLAGS&~(F_VIP|F_VIF)) | (REG_EFLAGS&(F_VIP|F_VIF)); //Disallow VIP&VIF being changed!
			}
		}
		if (getCPL()>FLAG_PL) tempEFLAGS = (tempEFLAGS&~F_IF)|(REG_EFLAGS&F_IF); //Disallow IF being changed!

		//Flags are OK now!
		REG_EFLAGS = tempEFLAGS; //Restore EFLAGS normally.
		updateCPUmode();
		if (segmentWritten(CPU_SEGMENT_CS,tempCS,3)) return; //We're loading because of an IRET!
		CPU_flushPIQ(-1); //We're jumping to another address!

		if ((tempEFLAGS&(F_VIP | F_VIF)) == (F_VIP | F_VIF)) //VIP and VIF both set on the new code?
		{
			CPU_commitState(); //Commit to the new instruction!
			THROWDESCGP(0, 0, 0); //#GP(0)!
		}
	}
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
			if ((MMU_logging == 1) && advancedlog) //Are we logging?
			{
				dolog("debugger","#NMI fault(-1)!");
			}

			if (CPU_faultraised(EXCEPTION_NMI))
			{
				CPU_executionphase_startinterrupt(EXCEPTION_NMI,2,-1); //Return to opcode!
			}
			CPU[activeCPU].cycles_HWOP = 50; /* Normal interrupt as hardware interrupt */
			return 0; //We're handled!
		}
	}
	return 1; //Unhandled NMI!
}
