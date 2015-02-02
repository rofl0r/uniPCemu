#ifndef SF2_H
#define SF2_H

#include "headers/types.h" //Basic types!

//RIFF IDs!

#define CKID_RIFF 0x46464952
#define CKID_LIST 0x5453494c

#define CKID_SFBK 0x6b626673
#define CKID_INFO 0x4f464e49
#define CKID_PDTA 0x61746470
#define CKID_IFIL 0x6c696669
#define CKID_INAM 0x4d414e49
#define CKID_PHDR 0x72646870

#define CKID_DLS  0x20534C44
#define CKID_COLH 0x686c6f63
#define CKID_VERS 0x73726576
#define CKID_LINS 0x736e696c
#define CKID_ICOP 0x504f4349
#define CKID_INS  0x20736e69
#define CKID_INSH 0x68736e69

//Our added RIFF IDs!


//Converter: http://www.dolcevie.com/js/converter.html
//Enter letters reversed! Hex normal! (tsil=list)

//Below all lower case!
#define CKID_SDTA 0x61746473
#define CKID_SMPL 0x6c706d73
#define CKID_SM24 0x34326d73
#define CKID_PBAG 0x67616270
#define CKID_PMOD 0x646f6d70
#define CKID_PGEN 0x6e656770
#define CKID_INST 0x74736e69
#define CKID_IBAG 0x67616269
#define CKID_IMOD 0x646f6d69
#define CKID_IGEN 0x6e656769
#define CKID_SHDR 0x72646873

/*
//Extra info (lower case)!
#define CKID_ISNG
#define CKID_IROM
#define CKID_IVER
//Rest info (upper case)!
#define CKID_ICRD
#define CKID_IENG
#define CKID_IPRD
#define CKID_ICOP
#define CKID_ICMT
#define CKID_ISFT
*/

/*

Tree of a soundfont RIFF (minimum required):

RIFF
	sfbk
		LIST
			INFO
				ifil
				isng
				INAM
			sdta
				smpl
				<sm24>
			pdta
				phdr
				pbag
				pmod
				pgen
				inst
				ibag
				imod
				igen
				shdr
*/



//Our patches to names!
#define DWORD uint_32
#define BYTE byte
#define CHAR sbyte
#define SHORT sword
#define WORD word

typedef uint_32 FOURCC; // Four-character code 
#include "headers/packed.h" //We're packed!
typedef struct  PACKED { 
 FOURCC ckID; // A chunk ID identifies the type of data within the chunk. 
 DWORD ckSize; // The size of the chunk data in bytes, excluding any pad byte. 
 // DATA THAT FOLLOWS = The actual data plus a pad byte if req’d to word align. 
} RIFF_DATAENTRY;
#include "headers/endpacked.h" //We're packed!

#include "headers/packed.h" //We're packed!
typedef struct  PACKED { 
     FOURCC ckID; 
     DWORD ckSize; 
     FOURCC fccType;          // RIFF form type 
} RIFF_LISTENTRY; //LIST/RIFF
#include "headers/endpacked.h" //We're packed!

typedef union
{
	RIFF_DATAENTRY *dataentry; //Data type!
	RIFF_LISTENTRY *listentry; //List type!
	void *voidentry; //Void type!
	byte *byteentry; //Byte type (same as void, but for visual c++)
} RIFF_ENTRY; //Data/List entry!

#include "headers/packed.h" //We're packed!
typedef struct PACKED
 { 
 BYTE byLo; 
 BYTE byHi; 
 } rangesType;
 #include "headers/endpacked.h" //We're packed!

#include "headers/packed.h" //We're packed!
typedef union PACKED
 { 
 rangesType ranges; 
 SHORT shAmount; 
 WORD wAmount;
 } genAmountType;
#include "headers/endpacked.h" //We're packed!

#include "headers/packed.h" //We're packed!
typedef enum PACKED
{
 monoSample = 1, 
 rightSample = 2, 
 leftSample = 4, 
 linkedSample = 8, 
 RomMonoSample = 0x8001, 
 RomRightSample = 0x8002, 
 RomLeftSample = 0x8004, 
 RomLinkedSample = 0x8008 
} SFSampleLink;
 #include "headers/endpacked.h" //We're packed!

#include "headers/packed.h" //We're packed!
typedef struct PACKED
{ 
 WORD wMajor; 
 WORD wMinor; 
} sfVersionTag;
#include "headers/endpacked.h" //We're packed!

#include "headers/packed.h" //We're packed!
typedef struct PACKED
{ 
 CHAR achPresetName[20]; 
 WORD wPreset; 
 WORD wBank; 
 WORD wPresetBagNdx; 
 DWORD dwLibrary; 
 DWORD dwGenre; 
 DWORD dwMorphology; 
} sfPresetHeader;
#include "headers/endpacked.h" //We're packed!

#include "headers/packed.h" //We're packed!
typedef struct PACKED
{ 
 WORD wGenNdx; 
 WORD wModNdx; 
} sfPresetBag;
#include "headers/endpacked.h" //We're packed!

typedef word SFGenerator;
typedef word SFModulator;
typedef word SFTransform;

#include "headers/packed.h" //We're packed!
typedef struct PACKED
{ 
 SFModulator sfModSrcOper; 
 SFGenerator sfModDestOper; 
 SHORT modAmount; 
 SFModulator sfModAmtSrcOper; 
 SFTransform sfModTransOper; 
} sfModList;
#include "headers/endpacked.h" //We're packed!

#include "headers/packed.h" //We're packed!
typedef struct PACKED
{ 
 SFGenerator sfGenOper; 
 genAmountType genAmount; 
} sfGenList;
#include "headers/endpacked.h" //We're packed!

#include "headers/packed.h" //We're packed!
typedef struct PACKED
{ 
 CHAR achInstName[20]; 
 WORD wInstBagNdx; 
} sfInst;
#include "headers/endpacked.h" //We're packed!

#include "headers/packed.h" //We're packed!
typedef struct PACKED
{ 
 WORD wInstGenNdx; 
 WORD wInstModNdx; 
} sfInstBag;
#include "headers/endpacked.h" //We're packed!

#include "headers/packed.h" //We're packed!
typedef struct PACKED
{ 
 SFGenerator sfGenOper; 
 genAmountType genAmount; 
} sfInstGenList;
#include "headers/endpacked.h" //We're packed!

#include "headers/packed.h" //We're packed!
typedef struct PACKED
{ 
 CHAR achSampleName[20]; 
 DWORD dwStart; 
 DWORD dwEnd; 
 DWORD dwStartloop; 
 DWORD dwEndloop; 
 DWORD dwSampleRate; 
 BYTE byOriginalPitch; 
 CHAR chPitchCorrection; 
 WORD wSampleLink; 
 WORD sfSampleType; 
} sfSample;
#include "headers/endpacked.h" //We're packed!

//Our own defines!

//ID lookup generators.
#define GEN_INSTRUMENT 41
#define GEN_SAMPLEID 53

//Different kinds of tuning for getting the right tone of an sample.
//Coarse tune: in semitones
#define GEN_COARSETUNE 51
//Fine tune: in cents
#define GEN_FINETUNE 52
//Scale tuning: How much does the MIDI key number affect pitch. 0=None, 100=Full semitone.
#define GEN_SCALETUNING 56

//Loop modes:
#define GEN_SAMPLEMODES 54
//Different sample modes:
#define GEN_SAMPLEMODES_NOLOOP 0
#define GEN_SAMPLEMODES_LOOP 1
#define GEN_SAMPLEMODES_NOLOOP2 2
#define GEN_SAMPLEMODES_LOOPUNTILDEPRESSDONE 3

//Panning
#define GEN_PAN 17
//Pan are 0.1% units where the sound is send to (left or right speaker). Left=(50-percentage number)%, Right=(50+percentage number)%

//Sample address generators (all signed values)
#define GEN_STARTADDRESSOFFSET 0
#define GEN_ENDADDRESSOFFSET 1
#define GEN_STARTLOOPADDRESSOFFSET 2
#define GEN_ENDLOOPADDRESSOFFSET 3
#define GEN_STARTADDRESSCOARSEOFFSET 4
#define GEN_ENDADDRESSCOARSEOFFSET 12
#define GEN_STARTLOOPADDRESSCOARSEOFFSET 45
#define GEN_ENDLOOPADDRESSCOARSEOFFSET 50

//Key/velocity for note lookup
#define GEN_KEYNUM 46
#define GEN_OVERRIDINGROOTKEY 58
#define GEN_VELOCITY 47

//Special also for checks!
#define GEN_KEYRANGE 43
#define GEN_VELOCITYRANGE 44

#include "headers/packed.h" //We're packed!
typedef struct PACKED
{
	uint_32 filesize; //The total filesize!
	RIFF_ENTRY rootentry; //Root (SFBK) entry!
	RIFF_ENTRY pcmdata; //16-bit (SMPL) audio entry!
	RIFF_ENTRY pcm24data; //24-bit (SMPL) audio extension of 16-bit audio entry!
} RIFFHEADER; //RIFF data header!
#include "headers/endpacked.h" //We're packed!

//Basic open/close functions for soundfonts!
RIFFHEADER *readSF(char *filename); //Open, read and validate a soundfont!
void closeSF(RIFFHEADER **sf); //Close the soundfont!

//All different soundfont blocks to read for MIDI!

/* Presets */
//PHDR
byte getSFPreset(RIFFHEADER *sf, uint_32 preset, sfPresetHeader *result); //Retrieves a preset from a soundfont!
byte isValidPreset(sfPresetHeader *preset); //Valid for playback?

//PBAG
byte getSFPresetBag(RIFFHEADER *sf,word wPresetBagNdx, sfPresetBag *result);
byte isPresetBagNdx(RIFFHEADER *sf, uint_32 preset, word wPresetBagNdx);

//PMOD
byte getSFPresetMod(RIFFHEADER *sf, word wPresetModNdx, sfModList *result);
byte isPresetModNdx(RIFFHEADER *sf, word preset, word wPresetBagNdx, word wPresetModNdx);

//PGEN
byte getSFPresetGen(RIFFHEADER *sf, word wPresetGenNdx, sfGenList *result);
byte isPresetGenNdx(RIFFHEADER *sf, word preset, word wPresetBagNdx, word wPresetGenNdx);

/* Instruments */

//INST
byte getSFInstrument(RIFFHEADER *sf, word Instrument, sfInst *result);

//IBAG
byte getSFInstrumentBag(RIFFHEADER *sf, word wInstBagNdx, sfInstBag *result);
byte isInstrumentBagNdx(RIFFHEADER *sf, word Instrument, word wInstBagNdx);

//IMOD
byte getSFInstrumentMod(RIFFHEADER *sf, word wInstrumentModNdx, sfModList *result);
byte isInstrumentModNdx(RIFFHEADER *sf, word Instrument, word wInstrumentBagNdx, word wInstrumentModNdx);

//IGEN
byte getSFInstrumentGen(RIFFHEADER *sf, word wInstGenNdx, sfInstGenList *result);
byte isInstrumentGenNdx(RIFFHEADER *sf, word Instrument, word wInstrumentBagNdx, word wInstrumentGenNdx);

/* Samples */

//Sample information and samples themselves!
byte getSFSampleInformation(RIFFHEADER *sf, word Sample, sfSample *result);
byte getSFsample(RIFFHEADER *sf, uint_32 sample, short *result); //Get a 16/24-bit sample!

/* Global and validation of zones */

byte isGlobalPresetZone(RIFFHEADER *sf, uint_32 preset, word PBag);
byte isGlobalInstrumentZone(RIFFHEADER *sf, word instrument, word IBag);
byte isValidPresetZone(RIFFHEADER *sf, uint_32 preset, word PBag);
byte isValidInstrumentZone(RIFFHEADER *sf, word instrument, word IBag);

/* Finally: some lookup functions for contents within the bags! */

byte lookupSFPresetMod(RIFFHEADER *sf, uint_32 preset, word PBag, SFModulator sfModSrcOper, sfModList *result);
byte lookupSFPresetGen(RIFFHEADER *sf, uint_32 preset, word PBag, SFGenerator sfGenOper, sfGenList *result);
byte lookupSFInstrumentMod(RIFFHEADER *sf, word instrument, word IBag, SFModulator sfModSrcOper, sfModList *result);
byte lookupSFInstrumentGen(RIFFHEADER *sf, word instrument, word IBag, SFGenerator sfGenOper, sfInstGenList *result);

byte lookupPresetByInstrument(RIFFHEADER *sf, word preset, word bank, uint_32 *result);
byte lookupPBagByMIDIKey(RIFFHEADER *sf, uint_32 preset, byte MIDIKey, byte MIDIVelocity, word *result);
byte lookupIBagByMIDIKey(RIFFHEADER *sf, word instrument, byte MIDIKey, byte MIDIVelocity, word *result, byte RequireInstrument);

/* Global and normal lookup of data (global variations of the normal support) */
byte lookupSFPresetModGlobal(RIFFHEADER *sf, uint_32 preset, word PBag, SFModulator sfModSrcOper, sfModList *result);
byte lookupSFPresetGenGlobal(RIFFHEADER *sf, word preset, word PBag, SFGenerator sfGenOper, sfGenList *result);
byte lookupSFInstrumentModGlobal(RIFFHEADER *sf, uint_32 instrument, word IBag, SFModulator sfModSrcOper, sfModList *result);
byte lookupSFInstrumentGenGlobal(RIFFHEADER *sf, word instrument, word IBag, SFGenerator sfGenOper, sfInstGenList *result);

#endif