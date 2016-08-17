#include "headers/types.h"

//LBA=Straight adress, much like flat memory
//CHS=Sectorized address, much like pointers.

//Convert Cylinder, Head, Sector to logical adress (LBA)

//Drive parameters:

//For nheads and nsectors, see Drive Parameters!
uint_32 CHS2LBA(word cylinder, byte head, byte sector, word nheads, uint_32 nsectors)
{
	return (sector-1)+(head*nsectors)+(cylinder*(nheads+1)*nsectors); //Calculate straight adress (LBA)
}

//Backwards comp.
void LBA2CHS(uint_32 LBA, word *cylinder, byte *head, byte *sector, word nheads, uint_32 nsectors)
{
	uint_32 cylhead;
	*sector = SAFEMOD(LBA,nsectors)+1;
	cylhead = SAFEDIV(LBA,nsectors); //Rest!
	*head = SAFEMOD(cylhead,(nheads+1));
	*cylinder = SAFEDIV(cylhead,(nheads+1));
}