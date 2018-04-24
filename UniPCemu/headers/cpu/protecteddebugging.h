#ifndef PROTECTEDMODEDEBUGGING_H
#define PROTECTEDMODEDEBUGGING_H

#include "headers/types.h" //Basic types!

#define PROTECTEDMODEDEBUGGER_TYPE_EXECUTION 0x00
#define PROTECTEDMODEDEBUGGER_TYPE_DATAWRITE 0x01
#define PROTECTEDMODEDEBUGGER_TYPE_DATAREAD 0x02
#define PROTECTEDMODEDEBUGGER_TYPE_IOREADWRITE 0x03

byte checkProtectedModeDebugger(uint_32 linearaddress, byte type); //Access at memory/IO port?
void protectedModeDebugger_taskswitch(); //Task switched?
void checkProtectedModeDebuggerAfter(); //Check after instruction for the protected mode debugger!
void protectedModeDebugger_taskswitched(); //Handle task switch debugger!

#endif
