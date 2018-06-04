all: ../../../projects_build/UniPCemu/testremainder

../../../projects_build/UniPCemu/testremainder: testremainder.asm
	nasm -i. -f bin testremainder.asm -w-all -l ../../../projects_build/UniPCemu/testremainder.lst -o ../../../projects_build/UniPCemu/testremainder.bin

