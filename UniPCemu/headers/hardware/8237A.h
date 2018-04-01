#ifndef __HW_8237A_H
#define __HW_8237A_H

typedef void(*DMAWriteBHandler)(byte data); //Write handler to DMA hardware!
typedef byte(*DMAReadBHandler)(); //Read handler from DMA hardware!
typedef void(*DMAWriteWHandler)(word data); //Write handler to DMA hardware!
typedef word(*DMAReadWHandler)(); //Read handler from DMA hardware!
typedef void(*DMATickHandler)(); //Tick handler for DMA hardware!

void initDMA(); //Initialise the DMA support!
void doneDMA(); //Finish the DMA support!

void registerDMA8(byte channel, DMAReadBHandler readhandler, DMAWriteBHandler writehandler);
void registerDMA16(byte channel, DMAReadWHandler readhandler, DMAWriteWHandler writehandler);
void registerDMATick(byte channel, DMATickHandler DREQHandler, DMATickHandler DACKHandler, DMATickHandler TCHandler);

void DMA_SetDREQ(byte channel, byte DREQ); //Set DREQ from hardware!

//CPU related timing!
void updateDMA(uint_32 MHZ14passed, uint_32 CPUcyclespassed); //Tick the DMA controller when needed!
void cleanDMA(); //Skip all ticks up to now!

#endif