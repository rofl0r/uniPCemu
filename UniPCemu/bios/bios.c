#include "headers/types.h" //Basic types etc.
#include "headers/basicio/io.h" //Basic I/O support for BIOS!
#include "headers/mmu/mmuhandler.h" //CRC32 support!
#include "headers/bios/bios.h" //BIOS basic type support etc!
#include "headers/bios/boot.h" //For booting disks!
#include "headers/cpu/cpu.h" //For some constants concerning CPU!
#include "headers/emu/gpu/gpu.h" //Need GPU comp!
#include "headers/support/zalloc.h" //Memory allocation: freemem function!
#include "headers/support/log.h" //Logging support!
#include "headers/emu/gpu/gpu_emu.h" //GPU emulator support!
#include "headers/hardware/8042.h" //Basic 8042 support for keyboard initialisation!
#include "headers/emu/emu_misc.h" //FILE_EXISTS support!
#include "headers/hardware/ports.h" //Port I/O support!
#include "headers/emu/sound.h" //Volume support!
#include "headers/hardware/midi/mididevice.h" //MIDI support!
#include "headers/hardware/ps2_keyboard.h" //For timeout support!
#include "headers/support/iniparser.h" //INI file parsing for our settings storage!

//Are we disabled?
#define __HW_DISABLED 0

//Log redirect, if enabled!
//#define LOG_REDIRECT

BIOS_Settings_TYPE BIOS_Settings; //Currently loaded settings!
byte exec_showchecksumerrors = 0; //Show checksum errors?

//Block size of memory (blocks of 16KB for IBM PC Compatibility)!
#define MEMORY_BLOCKSIZE_XT 0x4000
#define MEMORY_BLOCKSIZE_AT_LOW 0x10000
#define MEMORY_BLOCKSIZE_AT_HIGH 0x100000

//What file to use for saving the BIOS!
#define DEFAULT_SETTINGS_FILE "SETTINGS.INI"
#define DEFAULT_REDIRECT_FILE "redirect.txt"
#if defined(IS_WINDOWS) || defined(IS_PSP)
#define DEFAULT_ROOT_PATH "."
#else
//*nix? Use the root directory!
#define DEFAULT_ROOT_PATH "~/UniPCemu"
#endif

char BIOS_Settings_file[256] = DEFAULT_SETTINGS_FILE; //Our settings file!
char UniPCEmu_root_dir[256] = DEFAULT_ROOT_PATH; //Our root path!
byte UniPCEmu_root_dir_setting = 0; //The current root setting to be viewed!

//Android memory limit, in MB.
#define ANDROID_MEMORY_LIMIT 64

//All seperate paths used by the emulator!
extern char diskpath[256];
extern char soundfontpath[256];
extern char musicpath[256];
extern char capturepath[256];
extern char logpath[256];
extern char ROMpath[256];

void BIOS_updateDirectories()
{
#if defined(ANDROID) || defined(IS_LINUX)
	safestrcpy(diskpath,sizeof(diskpath),UniPCEmu_root_dir); //Root dir!
	safestrcat(diskpath,sizeof(diskpath),"/");
	safestrcpy(soundfontpath,sizeof(soundfontpath),diskpath); //Clone!
	safestrcpy(musicpath,sizeof(musicpath),diskpath); //Clone!
	safestrcpy(capturepath,sizeof(capturepath),diskpath); //Clone!
	safestrcpy(logpath,sizeof(logpath),diskpath); //Clone!
	safestrcpy(ROMpath,sizeof(ROMpath),diskpath); //Clone!
	//Now, create the actual subdirs!
	safestrcat(diskpath,sizeof(diskpath),"disks");
	safestrcat(soundfontpath,sizeof(soundfontpath),"soundfonts");
	safestrcat(musicpath,sizeof(musicpath),"music");
	safestrcat(capturepath,sizeof(capturepath),"captures");
	safestrcat(logpath,sizeof(logpath),"logs");
	safestrcat(ROMpath,sizeof(ROMpath),"ROM"); //ROM directory to use!
	//Now, all paths are loaded! Ready to run!
#endif
}

int storage_strpos(char *str, char ch)
{
	int pos=0;
	for (;(*str && (*str!=ch));++str,++pos); //Not found yet?
	if (*str==ch) return pos; //Found position!
	return -1; //Not found!
}

byte is_writablepath(char *path)
{
	char fullpath[256];
	FILE *f;
	memset(&fullpath,0,sizeof(fullpath)); //init!
	safestrcpy(fullpath,sizeof(fullpath),path); //Set the path!
	safestrcat(fullpath,sizeof(fullpath),"/"); //Add directory seperator!
	safestrcat(fullpath,sizeof(fullpath),"writable.txt"); //test file!
	f = fopen(fullpath,"wb");
	if (f)
	{
		fclose(f); //Close the file!
		delete_file(path,"writable.txt"); //Delete the file!
		return 1; //We're writable!
	}
	return 0; //We're not writable!
}

#if defined(ANDROID) || defined(IS_LINUX)
byte is_textcharacter(char c)
{
	if ((c>='a') && (c<='z')) return 1; //Text!
	if ((c>='A') && (c<='Z')) return 1; //Text!
	if ((c>='0') && (c<='9')) return 1; //Text!
	switch (c) //Remaining cases?
	{
		case '~':
		case '`':
		case '!':
		case '@':
		case '#':
		case '$':
		case '%':
		case '^':
		case '&':
		case '*':
		case '(':
		case ')':
		case '-':
		case '_':
		case '+':
		case '=':
		case '{':
		case '}':
		case '[':
		case ']':
		case '|':
		case '\\':
		case ':':
		case ';':
		case '"':
		case '\'':
		case '<':
		case ',':
		case '>':
		case '.':
		case '?':
			return 1; //Valid text for a filename to end in!
		default:
			break;
	}
	return 0; //Not a text character!
}
#endif

void BIOS_DetectStorage() //Auto-Detect the current storage to use, on start only!
{
	#if defined(ANDROID) || defined(IS_LINUX)
		FILE *f;
		byte is_redirected=0;
		is_redirected = 0; //Init redirect status for main directory!
		//Try external media first!
		char *environment;
		int multipathseperator; //Multi path seperator position in the current string! When set, it's changed into a NULL character to read out the current path in the list!

		char redirectdir[256]; //Redirect directory!
		int_32 redirectdirsize; //How much has been read?

		#ifdef PELYAS_SDL
		if (environment = getenv("SECONDARY_STORAGE")) //Autodetected try secondary storage?
		#else
		if ((environment = SDL_getenv("SECONDARY_STORAGE"))!=NULL) //Autodetected try secondary storage?
		#endif
		{
			scanNextSecondaryPath:
			if (environment=='\0') goto scanAndroiddefaultpath; //Start scanning the default path, nothing found!
			if ((multipathseperator = storage_strpos(environment,':'))!=-1) //Multiple environments left to check?
			{
				environment[multipathseperator] = '\0'; //Convert the seperator into an EOS for reading the current value out!
			}
			//Check the currently loaded path for writability!
			if (is_writablepath(environment)) //Writable?
			{
				safestrcpy(UniPCEmu_root_dir,sizeof(UniPCEmu_root_dir),environment); //Root path of the disk!
				if (multipathseperator!=-1) //To revert multiple path seperator?
				{
					environment[multipathseperator] = ':'; //Restore the path seperator from the EOS!
				}
				//Root directory loaded!
				safestrcat(UniPCEmu_root_dir,sizeof(UniPCEmu_root_dir),"/UniPCemu"); //Our storage path!
				domkdir(UniPCEmu_root_dir); //Make sure to create our parent directory, if needed!
				safestrcat(UniPCEmu_root_dir,sizeof(UniPCEmu_root_dir),"/files"); //Subdirectory to store the files!
				goto finishpathsetting;
			}
			//To check the next path?
			if (multipathseperator!=-1) //To revert multiple path seperator?
			{
				environment[multipathseperator] = ':'; //Restore the path seperator from the EOS!
				environment += (multipathseperator+1); //Skip past the multiple path seperator!
			}
			else goto scanAndroiddefaultpath; //Finished scanning without multiple paths left!
			goto scanNextSecondaryPath; //Scan the next path in the list!
		}

		#ifdef ANDROID
		scanAndroiddefaultpath:
		//Android changes the root path!
		#ifdef PELYAS_SDL
			safestrcpy(UniPCEmu_root_dir,sizeof(UniPCEmu_root_dir) getenv("SDCARD")); //path!
			safestrcat(UniPCEmu_root_dir,sizeof(UniPCEmu_root_dir), "/Android/data/com.unipcemu.app/files");
		#else
			if ((environment = SDL_getenv("SDCARD"))!=NULL) //Autodetected?
			{
				safestrcpy(UniPCEmu_root_dir,sizeof(UniPCEmu_root_dir), environment); //path!
				safestrcat(UniPCEmu_root_dir,sizeof(UniPCEmu_root_dir), "/Android/data/com.unipcemu.app/files");
			}
			else if (SDL_AndroidGetExternalStorageState() == (SDL_ANDROID_EXTERNAL_STORAGE_WRITE | SDL_ANDROID_EXTERNAL_STORAGE_READ)) //External settings exist?
			{
				if (SDL_AndroidGetExternalStoragePath()) //Try external.
				{
					safestrcpy(UniPCEmu_root_dir,sizeof(UniPCEmu_root_dir), SDL_AndroidGetExternalStoragePath()); //External path!
				}
				else if (SDL_AndroidGetInternalStoragePath()) //Try internal.
				{
					safestrcpy(UniPCEmu_root_dir,sizeof(UniPCEmu_root_dir), SDL_AndroidGetInternalStoragePath()); //Internal path!
				}
			}
			else
			{
				if (SDL_AndroidGetInternalStoragePath()) //Try internal.
				{
					safestrcpy(UniPCEmu_root_dir,sizeof(UniPCEmu_root_dir), SDL_AndroidGetInternalStoragePath()); //Internal path!
				}
			}
		#endif
		#endif

		finishpathsetting:
		safestrcpy(BIOS_Settings_file,sizeof(BIOS_Settings_file),UniPCEmu_root_dir); //Our settings file location!
		safestrcat(BIOS_Settings_file,sizeof(BIOS_Settings_file),"/"); //Inside the directory!
		safestrcat(BIOS_Settings_file,sizeof(BIOS_Settings_file),DEFAULT_SETTINGS_FILE); //Our settings file!
		domkdir(UniPCEmu_root_dir); //Auto-create our root directory!
		BIOS_updateDirectories(); //Update all directories!
		//Normal devices? Don't detect!

		//Check for redirection to apply!
		memset(&redirectdir,0,sizeof(redirectdir)); //Init!
		safestrcpy(redirectdir,sizeof(redirectdir),UniPCEmu_root_dir); //Check for redirects!
		safestrcat(redirectdir,sizeof(redirectdir),"/"); //Inside the directory!
		safestrcat(redirectdir,sizeof(redirectdir),DEFAULT_REDIRECT_FILE); //Our redirect file!
		#ifdef LOG_REDIRECT
		char temppath[256];
		memset(&temppath,0,sizeof(temppath)); //Set log path!
		safestrcpy(temppath,sizeof(temppath),logpath); //Set log path!
		safestrcat(temppath,sizeof(temppath),"/"); //Inside the directory!
		safestrcat(temppath,sizeof(temppath),DEFAULT_REDIRECT_FILE); //Log file for testing!
		char buffer[256];
		FILE *f2;
		char lb[2] = {0xD,0xA}; //Line break!
		memset(&buffer,0,sizeof(buffer)); //Init buffer!
		snprintf(buffer,sizeof(buffer),"Redirect file: %s",redirectdir);
		f2 = fopen(temppath,"wb"); //Log the filename!
		if (f2) //Valid?
		{
			fwrite(&buffer,1,safestrlen(buffer,sizeof(buffer)),f2); //Log!
			fwrite(&lb,1,sizeof(lb),f2); //Line break!
			fclose(f2); //Close!
		}
		#endif
		if (file_exists(redirectdir) && (is_redirected==0)) //Redirect for main directory?
		{
			#ifdef LOG_REDIRECT
			memset(&buffer,0,sizeof(buffer)); //Init buffer!
			snprintf(buffer,sizeof(buffer),"Attempting redirect...");
			f2 = fopen(temppath,"ab"); //Log the filename!
			if (f2) //Valid?
			{
				fwrite(&buffer,1,safestrlen(buffer,sizeof(buffer)),f2); //Log!
				fwrite(&lb,1,sizeof(lb),f2); //Line break!
				fclose(f2); //Close!
			}
			#endif
			f = fopen(redirectdir,"rb");
			if (f) //Valid?
			{
				#ifdef LOG_REDIRECT
				memset(&buffer,0,sizeof(buffer)); //Init buffer!
				snprintf(buffer,sizeof(buffer),"Valid file!");
				f2 = fopen(temppath,"ab"); //Log the filename!
				if (f2) //Valid?
				{
					fwrite(&buffer,1,safestrlen(buffer,sizeof(buffer)),f2); //Log!
					fwrite(&lb,1,sizeof(lb),f2); //Line break!
					fclose(f2); //Close!
				}
				#endif
				fseek(f,0,SEEK_END); //Goto EOF!
				if (((redirectdirsize = ftell(f))<sizeof(redirectdir)) && redirectdirsize) //Valid to read?
				{
					#ifdef LOG_REDIRECT
					memset(&buffer,0,sizeof(buffer)); //Init buffer!
					snprintf(buffer,sizeof(buffer),"Valid size!");
					f2 = fopen(temppath,"ab"); //Log the filename!
					if (f2) //Valid?
					{
						fwrite(&buffer,1,safestrlen(buffer,sizeof(buffer)),f2); //Log!
						fwrite(&lb,1,sizeof(lb),f2); //Line break!
						fclose(f2); //Close!
					}
					#endif
					fseek(f,0,SEEK_SET); //Goto BOF!
					memset(&redirectdir,0,sizeof(redirectdir)); //Clear for our result to be stored safely!
					if (fread(&redirectdir,1,redirectdirsize,f)==redirectdirsize) //Read?
					{
						#ifdef LOG_REDIRECT
						memset(&buffer,0,sizeof(buffer)); //Init buffer!
						snprintf(buffer,sizeof(buffer),"Valid content!");
						f2 = fopen(temppath,"ab"); //Log the filename!
						if (f2) //Valid?
						{
							fwrite(&buffer,1,safestrlen(buffer,sizeof(buffer)),f2); //Log!
							fwrite(&lb,1,sizeof(lb),f2); //Line break!
							fclose(f2); //Close!
						}
						#endif
						for (;safestrlen(redirectdir,sizeof(redirectdir));) //Valid to process?
						{
							switch (redirectdir[safestrlen(redirectdir,sizeof(redirectdir))-1]) //What is the final character?
							{
								case '/': //Invalid? Take it off!
									if (safestrlen(redirectdir,sizeof(redirectdir))>1) //More possible? Check for special path specification(e.g. :// etc.)!
									{
										if (!is_textcharacter(redirectdir[safestrlen(redirectdir,sizeof(redirectdir))-2])) //Not normal path?
										{
											redirectdir[safestrlen(redirectdir,sizeof(redirectdir))-1] = '\0'; //Take it off, we're specifying the final slash ourselves!
											goto redirect_validpath;
										}
									}
									//Invalid normal path: handle normally!
								case '\n':
								case '\r':
									redirectdir[safestrlen(redirectdir,sizeof(redirectdir))-1] = '\0'; //Take it off!
									break;
								default:
									redirect_validpath: //Apply valid directory for a root domain!
									#ifdef LOG_REDIRECT
									memset(&buffer,0,sizeof(buffer)); //Init buffer!
									snprintf(buffer,sizeof(buffer),"Trying: Redirecting to: %s",redirectdir); //Where are we redirecting to?
									f2 = fopen(temppath,"ab"); //Log the filename!
									if (f2) //Valid?
									{
										fwrite(&buffer,1,safestrlen(buffer,sizeof(buffer)),f2); //Log!
										fwrite(&lb,1,sizeof(lb),f2); //Line break!
										fclose(f2); //Close!
									}
									#endif
									if (is_writablepath(redirectdir)) //Writable path?
									{
										is_redirected = 1; //We're redirecting!
									}
									else
									{
										is_redirected = 0; //We're not redirecting after all!
									}
									goto finishredirect;
									break;
							}
						}
					}
				}
				finishredirect: //Finishing redirect!
				fclose(f); //Stop checking!
				#ifdef LOG_REDIRECT
				dolog("redirect","Content:%s/%i!",redirectdir,is_redirected);
				#endif
				if (is_redirected && redirectdir[0]) //To redirect?
				{
					safestrcpy(UniPCEmu_root_dir,sizeof(UniPCEmu_root_dir),redirectdir); //The new path to use!
					#ifdef LOG_REDIRECT
					memset(&buffer,0,sizeof(buffer)); //Init buffer!
					snprintf(buffer,sizeof(buffer),"Redirecting to: %s",UniPCEmu_root_dir); //Where are we redirecting to?
					f2 = fopen(temppath,"ab"); //Log the filename!
					if (f2) //Valid?
					{
						fwrite(&buffer,1,safestrlen(buffer,sizeof(buffer)),f2); //Log!
						fwrite(&lb,1,sizeof(lb),f2); //Line break!
						fclose(f2); //Close!
					}
					#endif
					goto finishpathsetting; //Go and apply the redirection!
				}
			}
		}
	#endif
}

void forceBIOSSave()
{
	BIOS_SaveData(); //Save the BIOS, ignoring the result!
}

extern byte is_XT; //Are we emulating a XT architecture?
extern byte is_Compaq; //Are we emulating a Compaq architecture?
extern byte is_PS2; //Are we emulating PS/2 architecture extensions?

void autoDetectArchitecture()
{
	is_XT = (BIOS_Settings.architecture==ARCHITECTURE_XT); //XT architecture?
	is_Compaq = 0; //Default to not being Compaq!
	is_PS2 = 0; //Default to not being PS/2!

	if (EMULATED_CPU >= CPU_80486) //Are we emulating a AT-only architecture CPU? 80386 is 32-bit, but there's the Inboard 386 for that to be emulated.
	{
		is_XT = 0; //We're forcing AT or PS/2 architecture!
	}
	if (BIOS_Settings.architecture==ARCHITECTURE_COMPAQ) //Compaq architecture?
	{
		is_XT = 0; //No XT!
		is_Compaq = 1; //Compaq!
	}
	if (BIOS_Settings.architecture==ARCHITECTURE_PS2) //PS/2 architecture?
	{
		is_PS2 = 1; //PS/2 extensions enabled!
		is_XT = 0; //AT compatible!
		is_Compaq = 1; //Compaq compatible!
	}
}

//Custom feof, because Windows feof seems to fail in some strange cases?
byte is_EOF(FILE *fp)
{
	byte res;
	long currentOffset = ftell(fp);

	fseek(fp, 0, SEEK_END);

	if(currentOffset >= ftell(fp))
		res = 1; //EOF!
    else
		res = 0; //Not EOF!
	fseek(fp, currentOffset, SEEK_SET);
	return res;
}

void autoDetectMemorySize(int tosave) //Auto detect memory size (tosave=save BIOS?)
{
	if (__HW_DISABLED) return; //Ignore updates to memory!
	debugrow("Detecting MMU memory size to use...");
	
	uint_32 freememory = freemem(); //The free memory available!
	int_32 memoryblocks;
	uint_64 maximummemory;
	byte AThighblocks; //Are we using AT high blocks instead of low blocks?
	byte memorylimitshift;
	memorylimitshift = 20; //Default to MB (2^20) chunks!
	
	#ifdef ANDROID
	maximummemory = ANDROID_MEMORY_LIMIT; //Default limit in MB!
	#else
	maximummemory = SHRT_MAX; //Default: maximum memory limit!
	#endif
	char limitfilename[256];
	memset(&limitfilename,0,sizeof(limitfilename)); //Init!
	safestrcpy(limitfilename,sizeof(limitfilename),UniPCEmu_root_dir); //Root directory!
	safestrcat(limitfilename,sizeof(limitfilename),"/memorylimit.txt"); //Limit file path!

	if (file_exists(limitfilename)) //Limit specified?
	{
		int memorylimitMB=SHRT_MAX;
		char memorylimitsize='?';
		byte limitread;
		FILE *f;
		f = fopen(limitfilename,"rb");
		limitread = 0; //Default: not read!
		if (f) //Valid file?
		{
			if (fscanf(f,"%d",&memorylimitMB)) //Read up to 4 bytes to the buffer!
			{
				if (is_EOF(f)) //Read until EOF? We're valid!
				{
					limitread = 1; //We're read!
				}
				else //Might have more?
				{
					if (fscanf(f,"%c",&memorylimitsize)) //Read size?
					{
						if (is_EOF(f)) //Read until EOF? We're valid!
						{
							limitread = 2; //We're read!
							switch (memorylimitsize) //What size?
							{
								case 'b':
								case 'B': //KB?
									memorylimitsize = 'B'; //Default to Bytes!
									break;
								case 'k':
								case 'K': //KB?
									memorylimitsize = 'K'; //Default to KB!
									break;
								case 'm':
								case 'M': //MB?
									memorylimitsize = 'M'; //Default to MB!
									break;
								case 'g':
								case 'G': //GB?
									memorylimitsize = 'G'; //Default to GB!
									break;
								default: //Unknown size?
									memorylimitsize = 'M'; //Default to MB!
									break;
							}
						}
					}
				}
			}
			fclose(f); //Close the file!
		}
		if (limitread) //Are we read?
		{
			maximummemory = (uint_32)memorylimitMB; //Set the memory limit, in MB!
			switch (memorylimitsize) //What shift to apply?
			{
				case 'B':
					memorylimitshift = 0; //No shift: we're in bytes!
					break;
				case 'K':
					memorylimitshift = 10; //Shift: we're in KB!
					break;
				case 'G':
					memorylimitshift = 30; //Shift: we're in GB!
					break;
				default:
				case 'M':
				case '?': //Unknown?
					memorylimitshift = 20; //Shift: we're in MB!
					break;			
			}
		}
	}
	maximummemory <<= memorylimitshift; //Convert to MB of memory limit!

	if (maximummemory<0x10000) //Nothing? use bare minumum!
	{
		maximummemory = 0x10000; //Bare minumum: 64KB + reserved memory!
	}

	maximummemory += FREEMEMALLOC; //Required free memory is always to be applied as a limit!

	if (((uint_64)freememory)>=maximummemory) //Limit broken?
	{
		freememory = (uint_32)maximummemory; //Limit the memory as specified!
	}

	//Architecture limits are placed on the detected memmory, which is truncated by the set memory limit!

	if (freememory>=FREEMEMALLOC) //Can we substract?
	{
		freememory -= FREEMEMALLOC; //What to leave!
	}
	else
	{
		freememory = 0; //Nothing to substract: ran out of memory!
	}

	autoDetectArchitecture(); //Detect the architecture to use!
	if (is_XT) //XT?
	{
		memoryblocks = SAFEDIV((freememory),MEMORY_BLOCKSIZE_XT); //Calculate # of free memory size and prepare for block size!
	}
	else //AT?
	{
		memoryblocks = SAFEDIV((freememory), MEMORY_BLOCKSIZE_AT_LOW); //Calculate # of free memory size and prepare for block size!
	}
	AThighblocks = 0; //Default: we're using low blocks!
	if (is_XT==0) //AT+?
	{
		if ((memoryblocks*MEMORY_BLOCKSIZE_AT_LOW)>=MEMORY_BLOCKSIZE_AT_HIGH) //Able to divide in big blocks?
		{
			memoryblocks = SAFEDIV((memoryblocks*MEMORY_BLOCKSIZE_AT_LOW),MEMORY_BLOCKSIZE_AT_HIGH); //Convert to high memory blocks!
			AThighblocks = 1; //Wer� using high blocks instead!
		}
	}
	if (memoryblocks<0) memoryblocks = 0; //No memory left?
	if (is_XT) //XT?
	{
		BIOS_Settings.memory = memoryblocks * MEMORY_BLOCKSIZE_XT; //Whole blocks of memory only!
	}
	else
	{
		BIOS_Settings.memory = memoryblocks * (AThighblocks?MEMORY_BLOCKSIZE_AT_HIGH:MEMORY_BLOCKSIZE_AT_LOW); //Whole blocks of memory only, either low memory or high memory blocks!
	}
	if (EMULATED_CPU<=CPU_NECV30) //80286-? We don't need more than 1MB memory(unusable memory)!
	{
		if (BIOS_Settings.memory>=0x100000) BIOS_Settings.memory = 0x100000; //1MB memory max!
	}
	else if (EMULATED_CPU<=CPU_80286) //80286-? We don't need more than 16MB memory(unusable memory)!
	{
		if (BIOS_Settings.memory>=0xF00000) BIOS_Settings.memory = 0x1000000; //16MB memory max!
	}
	if (!memoryblocks) //Not enough memory (at least 16KB or AT specs required)?
	{
		raiseError("Settings","Ran out of enough memory to use! Free memory: %u bytes",BIOS_Settings.memory); //Show error&quit: not enough memory to work with!
		sleep(); //Wait forever!
	}
	//dolog("BIOS","Detected memory: %u bytes",BIOS_Settings.memory);

	if ((uint_64)BIOS_Settings.memory>=((uint_64)4096<<20)) //Past 4G?
	{
		BIOS_Settings.memory = (uint_32)((((uint_64)4096)<<20)-MEMORY_BLOCKSIZE_AT_HIGH); //Limit to the max, just below 4G!
	}

	if (tosave)
	{
		forceBIOSSave(); //Force BIOS save!
	}
}



void BIOS_LoadDefaults(int tosave) //Load BIOS defaults, but not memory size!
{
	if (exec_showchecksumerrors)
	{
		printmsg(0xF,"\r\nSettings Checksum Error. "); //Checksum error.
	}
	
	uint_32 oldmem = BIOS_Settings.memory; //Memory installed!
	memset(&BIOS_Settings,0,sizeof(BIOS_Settings)); //Reset to empty!
	
	if (!file_exists(BIOS_Settings_file)) //New file?
	{
		BIOS_Settings.firstrun = 1; //We're the first run!
	}
	
	BIOS_Settings.memory = oldmem; //Keep this intact!
	//Now load the defaults.

	memset(&BIOS_Settings.floppy0[0],0,sizeof(BIOS_Settings.floppy0));
	BIOS_Settings.floppy0_readonly = 0; //Not read-only!
	memset(&BIOS_Settings.floppy1[0],0,sizeof(BIOS_Settings.floppy1));
	BIOS_Settings.floppy1_readonly = 0; //Not read-only!
	memset(&BIOS_Settings.hdd0[0],0,sizeof(BIOS_Settings.hdd0));
	BIOS_Settings.hdd0_readonly = 0; //Not read-only!
	memset(&BIOS_Settings.hdd1[0],0,sizeof(BIOS_Settings.hdd1));
	BIOS_Settings.hdd1_readonly = 0; //Not read-only!

	memset(&BIOS_Settings.cdrom0[0],0,sizeof(BIOS_Settings.cdrom0));
	memset(&BIOS_Settings.cdrom1[0],0,sizeof(BIOS_Settings.cdrom1));
//CD-ROM always read-only!

	memset(&BIOS_Settings.SoundFont[0],0,sizeof(BIOS_Settings.SoundFont)); //Reset the currently mounted soundfont!

	BIOS_Settings.bootorder = DEFAULT_BOOT_ORDER; //Default boot order!
	BIOS_Settings.emulated_CPU = DEFAULT_CPU; //Which CPU to be emulated?

	BIOS_Settings.debugmode = DEFAULT_DEBUGMODE; //Default debug mode!
	BIOS_Settings.executionmode = DEFAULT_EXECUTIONMODE; //Default execution mode!
	BIOS_Settings.debugger_log = DEFAULT_DEBUGGERLOG; //Default debugger logging!

	BIOS_Settings.GPU_AllowDirectPlot = DEFAULT_DIRECTPLOT; //Default: automatic 1:1 mapping!
	BIOS_Settings.aspectratio = DEFAULT_ASPECTRATIO; //Don't keep aspect ratio by default!
	BIOS_Settings.bwmonitor = DEFAULT_BWMONITOR; //Default B/W monitor setting!
	BIOS_Settings.SoundSource_Volume = DEFAULT_SSOURCEVOL; //Default soundsource volume knob!
	BIOS_Settings.GameBlaster_Volume = DEFAULT_BLASTERVOL; //Default Game Blaster volume knob!
	BIOS_Settings.ShowFramerate = DEFAULT_FRAMERATE; //Default framerate setting!
	BIOS_Settings.VGASynchronization = DEFAULT_VGASYNCHRONIZATION; //Default VGA synchronization setting!
	BIOS_Settings.diagnosticsportoutput_breakpoint = DEFAULT_DIAGNOSTICSPORTOUTPUT_BREAKPOINT; //Default breakpoint setting!
	BIOS_Settings.diagnosticsportoutput_timeout = DEFAULT_DIAGNOSTICSPORTOUTPUT_TIMEOUT; //Default breakpoint setting!
	BIOS_Settings.useDirectMIDI = DEFAULT_DIRECTMIDIMODE; //Default breakpoint setting!
	BIOS_Settings.BIOSROMmode = DEFAULT_BIOSROMMODE; //Default BIOS ROM mode setting!
	BIOS_Settings.modemlistenport = DEFAULT_MODEMLISTENPORT; //Default modem listen port!
	
	BIOS_Settings.version = BIOS_VERSION; //Current version loaded!
	keyboard_loadDefaults(); //Load the defaults for the keyboard!
	
	BIOS_Settings.useAdlib = 0; //Emulate Adlib?
	BIOS_Settings.useLPTDAC = 0; //Emulate Covox/Disney Sound Source?
	BIOS_Settings.usePCSpeaker = 0; //Sound PC Speaker?

	if (tosave) //Save settings?
	{
		forceBIOSSave(); //Save the BIOS!
	}
	if (exec_showchecksumerrors)
	{
		printmsg(0xF,"Defaults loaded.\r\n"); //Show that the defaults are loaded.
	}
}

int telleof(FILE *f) //Are we @eof?
{
	int curpos = 0; //Cur pos!
	int endpos = 0; //End pos!
	int result = 0; //Result!
	curpos = ftell(f); //Cur position!
	fseek(f,0,SEEK_END); //Goto EOF!
	endpos = ftell(f); //End position!

	fseek(f,curpos,SEEK_SET); //Return!
	result = (curpos==endpos); //@EOF?
	return result; //Give the result!
}

uint_32 BIOS_getChecksum() //Get the BIOS checksum!
{
	uint_32 result=0,total=sizeof(BIOS_Settings); //Initialise our info!
	byte *data = (byte *)&BIOS_Settings; //First byte of data!
	for (;total;) //Anything left?
	{
		result += (uint_32)*data++; //Add the data to the result!
		--total; //One byte of data processed!
	}
	return result; //Give the simple checksum of the loaded settings!
}

void loadBIOSCMOS(CMOSDATA *CMOS, char *section)
{
	word index;
	char field[256];
	CMOS->timedivergeance = get_private_profile_int64(section,"TimeDivergeance_seconds",0,BIOS_Settings_file);
	CMOS->timedivergeance2 = get_private_profile_int64(section,"TimeDivergeance_microseconds",0,BIOS_Settings_file);
	CMOS->s100 = (byte)get_private_profile_uint64(section,"s100",0,BIOS_Settings_file);
	CMOS->s10000 = (byte)get_private_profile_uint64(section,"s10000",0,BIOS_Settings_file);
	CMOS->centuryisbinary = (byte)get_private_profile_uint64(section,"centuryisbinary",0,BIOS_Settings_file);
	CMOS->cycletiming = (byte)get_private_profile_uint64(section,"cycletiming",0,BIOS_Settings_file);
	for (index=0;index<NUMITEMS(CMOS->DATA80.data);++index) //Process extra RAM data!
	{
		snprintf(field,sizeof(field),"RAM%02X",index); //The field!
		CMOS->DATA80.data[index] = (byte)get_private_profile_uint64(section,&field[0],0,BIOS_Settings_file);
	}
	for (index=0;index<NUMITEMS(CMOS->extraRAMdata);++index) //Process extra RAM data!
	{
		snprintf(field,sizeof(field),"extraRAM%02X",index); //The field!
		CMOS->extraRAMdata[index] = (byte)get_private_profile_uint64(section,&field[0],0,BIOS_Settings_file);
	}
}

char phonebookentry[256] = "";

byte loadedsettings_loaded = 0;
BIOS_Settings_TYPE loadedsettings; //The settings that have been loaded!

void BIOS_LoadData() //Load BIOS settings!
{
	if (__HW_DISABLED) return; //Abort!
	FILE *f;
	byte defaultsapplied = 0; //Defaults have been applied?
	byte c;
	memset(&phonebookentry, 0, sizeof(phonebookentry)); //Init!

	if (loadedsettings_loaded)
	{
		memcpy(&BIOS_Settings,&loadedsettings,sizeof(BIOS_Settings)); //Reload from buffer!
		return;
	}
	f = fopen(BIOS_Settings_file,"rb"); //Open BIOS file!

	if (!f) //Not loaded?
	{
		BIOS_LoadDefaults(1); //Load the defaults, save!
		return; //We've loaded the defaults!
	}

	fclose(f); //Close the settings file!

	memset(&BIOS_Settings,0,sizeof(BIOS_Settings)); //Init settings to their defaults!

	//General
	BIOS_Settings.version = (byte)get_private_profile_uint64("general","version",BIOS_VERSION,BIOS_Settings_file);
	BIOS_Settings.firstrun = (byte)get_private_profile_uint64("general","firstrun",1,BIOS_Settings_file); //Is this the first run of this BIOS?
	BIOS_Settings.BIOSmenu_font = (byte)get_private_profile_uint64("general","settingsmenufont",0,BIOS_Settings_file); //The selected font for the BIOS menu!
	BIOS_Settings.backgroundpolicy = (byte)get_private_profile_uint64("general", "backgroundpolicy", DEFAULT_BACKGROUNDPOLICY, BIOS_Settings_file); //The selected font for the BIOS menu!

	//Machine
	BIOS_Settings.emulated_CPU = (word)get_private_profile_uint64("machine","cpu",DEFAULT_CPU,BIOS_Settings_file);
	BIOS_Settings.DataBusSize = (byte)get_private_profile_uint64("machine","databussize",0,BIOS_Settings_file); //The size of the emulated BUS. 0=Normal bus, 1=8-bit bus when available for the CPU!
	BIOS_Settings.memory = (uint_32)get_private_profile_uint64("machine","memory",0,BIOS_Settings_file);
	BIOS_Settings.architecture = (byte)get_private_profile_uint64("machine","architecture",ARCHITECTURE_XT,BIOS_Settings_file); //Are we using the XT/AT/PS/2 architecture?
	BIOS_Settings.executionmode = (byte)get_private_profile_uint64("machine","executionmode",DEFAULT_EXECUTIONMODE,BIOS_Settings_file); //What mode to execute in during runtime?
	BIOS_Settings.CPUSpeed = (uint_32)get_private_profile_uint64("machine","cpuspeed",0,BIOS_Settings_file);
	BIOS_Settings.ShowCPUSpeed = (byte)get_private_profile_uint64("machine","showcpuspeed",0,BIOS_Settings_file); //Show the relative CPU speed together with the framerate?
	BIOS_Settings.TurboCPUSpeed = (uint_32)get_private_profile_uint64("machine","turbocpuspeed",0,BIOS_Settings_file);
	BIOS_Settings.useTurboSpeed = (byte)get_private_profile_uint64("machine","useturbocpuspeed",0,BIOS_Settings_file); //Are we to use Turbo CPU speed?
	BIOS_Settings.clockingmode = (byte)get_private_profile_uint64("machine","clockingmode",DEFAULT_CLOCKINGMODE,BIOS_Settings_file); //Are we using the IPS clock?
	BIOS_Settings.BIOSROMmode = (byte)get_private_profile_uint64("machine","BIOSROMmode",DEFAULT_BIOSROMMODE,BIOS_Settings_file); //BIOS ROM mode.
	BIOS_Settings.InboardInitialWaitstates = (byte)get_private_profile_uint64("machine","inboardinitialwaitstates",DEFAULT_INBOARDINITIALWAITSTATES,BIOS_Settings_file); //Inboard 386 initial delay used?

	//Debugger
	BIOS_Settings.debugmode = (byte)get_private_profile_uint64("debugger","debugmode",DEFAULT_DEBUGMODE,BIOS_Settings_file);
	BIOS_Settings.debugger_log = (byte)get_private_profile_uint64("debugger","debuggerlog",DEFAULT_DEBUGGERLOG,BIOS_Settings_file);
	BIOS_Settings.debugger_logstates = (byte)get_private_profile_uint64("debugger","logstates",DEFAULT_DEBUGGERSTATELOG,BIOS_Settings_file); //Are we logging states? 1=Log states, 0=Don't log states!
	BIOS_Settings.debugger_logregisters = (byte)get_private_profile_uint64("debugger","logregisters",DEFAULT_DEBUGGERREGISTERSLOG,BIOS_Settings_file); //Are we logging states? 1=Log states, 0=Don't log states!
	BIOS_Settings.breakpoint = get_private_profile_uint64("debugger","breakpoint",0,BIOS_Settings_file); //The used breakpoint segment:offset and mode!
	BIOS_Settings.taskBreakpoint = get_private_profile_uint64("debugger","taskbreakpoint",0,BIOS_Settings_file); //The used breakpoint segment:offset and mode!
	BIOS_Settings.CR3breakpoint = get_private_profile_uint64("debugger","CR3breakpoint",0,BIOS_Settings_file); //The used breakpoint segment:offset and mode!
	BIOS_Settings.diagnosticsportoutput_breakpoint = (sword)get_private_profile_int64("debugger","diagnosticsport_breakpoint",DEFAULT_DIAGNOSTICSPORTOUTPUT_BREAKPOINT,BIOS_Settings_file); //Use a diagnostics port breakpoint?
	BIOS_Settings.diagnosticsportoutput_timeout = (uint_32)get_private_profile_uint64("debugger","diagnosticsport_timeout",DEFAULT_DIAGNOSTICSPORTOUTPUT_TIMEOUT,BIOS_Settings_file); //Breakpoint timeout used!
	BIOS_Settings.advancedlog = (byte)get_private_profile_uint64("debugger", "advancedlog", DEFAULT_ADVANCEDLOG, BIOS_Settings_file); //The selected font for the BIOS menu!

	//Video
	BIOS_Settings.VGA_Mode = (byte)get_private_profile_uint64("video","videocard",DEFAULT_VIDEOCARD,BIOS_Settings_file); //Enable VGA NMI on precursors?
	BIOS_Settings.CGAModel = (byte)get_private_profile_uint64("video","CGAmodel",DEFAULT_CGAMODEL,BIOS_Settings_file); //What kind of CGA is emulated? Bit0=NTSC, Bit1=New-style CGA
	BIOS_Settings.VRAM_size = (uint_32)get_private_profile_uint64("video","VRAM",0,BIOS_Settings_file); //(S)VGA VRAM size!
	BIOS_Settings.VGASynchronization = (byte)get_private_profile_uint64("video","synchronization",DEFAULT_VGASYNCHRONIZATION,BIOS_Settings_file); //VGA synchronization setting. 0=Automatic synchronization based on Host CPU. 1=Tight VGA Synchronization with the CPU.
	BIOS_Settings.GPU_AllowDirectPlot = (byte)get_private_profile_uint64("video","directplot",DEFAULT_DIRECTPLOT,BIOS_Settings_file); //Allow VGA Direct Plot: 1 for automatic 1:1 mapping, 0 for always dynamic, 2 for force 1:1 mapping?
	BIOS_Settings.aspectratio = (byte)get_private_profile_uint64("video","aspectratio",DEFAULT_ASPECTRATIO,BIOS_Settings_file); //The aspect ratio to use?
	BIOS_Settings.bwmonitor = (byte)get_private_profile_uint64("video","bwmonitor",DEFAULT_BWMONITOR,BIOS_Settings_file); //Are we a b/w monitor?
	BIOS_Settings.ShowFramerate = (byte)get_private_profile_uint64("video","showframerate",DEFAULT_FRAMERATE,BIOS_Settings_file); //Show the frame rate?

	//Sound
	BIOS_Settings.usePCSpeaker = (byte)get_private_profile_uint64("sound","speaker",1,BIOS_Settings_file); //Emulate PC Speaker sound?
	BIOS_Settings.useAdlib = (byte)get_private_profile_uint64("sound","adlib",1,BIOS_Settings_file); //Emulate Adlib?
	BIOS_Settings.useLPTDAC = (byte)get_private_profile_uint64("sound","LPTDAC",1,BIOS_Settings_file); //Emulate Covox/Disney Sound Source?
	get_private_profile_string("sound","soundfont","",&BIOS_Settings.SoundFont[0],sizeof(BIOS_Settings.SoundFont),BIOS_Settings_file); //Read entry!
	BIOS_Settings.useDirectMIDI = (byte)get_private_profile_uint64("sound","directmidi",DEFAULT_DIRECTMIDIMODE,BIOS_Settings_file); //Use Direct MIDI synthesis by using a passthrough to the OS?
	BIOS_Settings.useGameBlaster = (byte)get_private_profile_uint64("sound","gameblaster",1,BIOS_Settings_file); //Emulate Game Blaster?
	BIOS_Settings.GameBlaster_Volume = (uint_32)get_private_profile_uint64("sound","gameblaster_volume",100,BIOS_Settings_file); //The Game Blaster volume knob!
	BIOS_Settings.useSoundBlaster = (byte)get_private_profile_uint64("sound","soundblaster",DEFAULT_SOUNDBLASTER,BIOS_Settings_file); //Emulate Sound Blaster?
	BIOS_Settings.SoundSource_Volume = (uint_32)get_private_profile_uint64("sound","soundsource_volume",DEFAULT_SSOURCEVOL,BIOS_Settings_file); //The sound source volume knob!

	//Modem
	BIOS_Settings.modemlistenport = (word)get_private_profile_uint64("modem","listenport",DEFAULT_MODEMLISTENPORT,BIOS_Settings_file); //Modem listen port!
	for (c = 0; c < NUMITEMS(BIOS_Settings.phonebook); ++c) //Process all phonebook entries!
	{
		snprintf(phonebookentry, sizeof(phonebookentry), "phonebook%u", c); //The entry to use!
		get_private_profile_string("modem", phonebookentry, "", &BIOS_Settings.phonebook[c][0], sizeof(BIOS_Settings.phonebook[0]), BIOS_Settings_file); //Read entry!
	}

#ifdef PACKETSERVER_ENABLED
	BIOS_Settings.ethernetserver_settings.ethernetcard = get_private_profile_int64("modem","ethernetcard",-1,BIOS_Settings_file); //Ethernet card to use!
	get_private_profile_string("modem","MACaddress","",&BIOS_Settings.ethernetserver_settings.MACaddress[0],sizeof(BIOS_Settings.ethernetserver_settings.MACaddress),BIOS_Settings_file); //Read entry!
	get_private_profile_string("modem","gatewayMACaddress","",&BIOS_Settings.ethernetserver_settings.gatewayMACaddress[0],sizeof(BIOS_Settings.ethernetserver_settings.gatewayMACaddress),BIOS_Settings_file); //Read entry!
	get_private_profile_string("modem","username","",&BIOS_Settings.ethernetserver_settings.username[0],sizeof(BIOS_Settings.ethernetserver_settings.username),BIOS_Settings_file); //Read entry!
	get_private_profile_string("modem","password","",&BIOS_Settings.ethernetserver_settings.password[0],sizeof(BIOS_Settings.ethernetserver_settings.password),BIOS_Settings_file); //Read entry!
	get_private_profile_string("modem","IPaddress","",&BIOS_Settings.ethernetserver_settings.IPaddress[0],sizeof(BIOS_Settings.ethernetserver_settings.IPaddress),BIOS_Settings_file); //Read entry!
#endif

	//Disks
	get_private_profile_string("disks","floppy0","",&BIOS_Settings.floppy0[0],sizeof(BIOS_Settings.floppy0),BIOS_Settings_file); //Read entry!
	BIOS_Settings.floppy0_readonly = (byte)get_private_profile_uint64("disks","floppy0_readonly",0,BIOS_Settings_file);
	get_private_profile_string("disks","floppy1","",&BIOS_Settings.floppy1[0],sizeof(BIOS_Settings.floppy1),BIOS_Settings_file); //Read entry!
	BIOS_Settings.floppy1_readonly = (byte)get_private_profile_uint64("disks","floppy1_readonly",0,BIOS_Settings_file);
	get_private_profile_string("disks","hdd0","",&BIOS_Settings.hdd0[0],sizeof(BIOS_Settings.hdd0),BIOS_Settings_file); //Read entry!
	BIOS_Settings.hdd0_readonly = (byte)get_private_profile_uint64("disks","hdd0_readonly",0,BIOS_Settings_file);
	get_private_profile_string("disks","hdd1","",&BIOS_Settings.hdd1[0],sizeof(BIOS_Settings.hdd1),BIOS_Settings_file); //Read entry!
	BIOS_Settings.hdd1_readonly = (byte)get_private_profile_uint64("disks","hdd1_readonly",0,BIOS_Settings_file);
	get_private_profile_string("disks","cdrom0","",&BIOS_Settings.cdrom0[0],sizeof(BIOS_Settings.cdrom0),BIOS_Settings_file); //Read entry!
	get_private_profile_string("disks","cdrom1","",&BIOS_Settings.cdrom1[0],sizeof(BIOS_Settings.cdrom1),BIOS_Settings_file); //Read entry!


	//BIOS
	BIOS_Settings.bootorder = (byte)get_private_profile_uint64("bios","bootorder",DEFAULT_BOOT_ORDER,BIOS_Settings_file);

	//Input
	BIOS_Settings.input_settings.analog_minrange = (byte)get_private_profile_uint64("input","analog_minrange",0,BIOS_Settings_file); //Minimum adjustment x&y(0,0) for keyboard&mouse to change states (from center)
	BIOS_Settings.input_settings.fontcolor = (byte)get_private_profile_uint64("input","keyboard_fontcolor",0xFF,BIOS_Settings_file);
	BIOS_Settings.input_settings.bordercolor = (byte)get_private_profile_uint64("input","keyboard_bordercolor",0xFF,BIOS_Settings_file);
	BIOS_Settings.input_settings.activecolor = (byte)get_private_profile_uint64("input","keyboard_activecolor",0xFF,BIOS_Settings_file);
	BIOS_Settings.input_settings.specialcolor = (byte)get_private_profile_uint64("input","keyboard_specialcolor",0xFF,BIOS_Settings_file);
	BIOS_Settings.input_settings.specialbordercolor = (byte)get_private_profile_uint64("input","keyboard_specialbordercolor",0xFF,BIOS_Settings_file);
	BIOS_Settings.input_settings.specialactivecolor = (byte)get_private_profile_uint64("input","keyboard_specialactivecolor",0xFF,BIOS_Settings_file);
	for (c=0;c<6;++c) //Validate colors and set default colors when invalid!
	{
		if (BIOS_Settings.input_settings.colors[c]>0xF) keyboard_loadDefaultColor(c); //Set default color when invalid!
	}

	//Gamingmode
	char buttons[15][256] = {"start","left","up","right","down","ltrigger","rtrigger","triangle","circle","cross","square","analogleft","analogup","analogright","analogdown"}; //The names of all mappable buttons!
	byte button;
	char buttonstr[256];
	memset(&buttonstr,0,sizeof(buttonstr)); //Init button string!
	for (button=0;button<15;++button) //Process all buttons!
	{
		snprintf(buttonstr,sizeof(buttonstr),"gamingmode_map_%s_key",buttons[button]);
		BIOS_Settings.input_settings.keyboard_gamemodemappings[button] = (sword)get_private_profile_int64("gamingmode",buttonstr,-1,BIOS_Settings_file);
		snprintf(buttonstr,sizeof(buttonstr),"gamingmode_map_%s_shiftstate",buttons[button]);
		BIOS_Settings.input_settings.keyboard_gamemodemappings_alt[button] = (byte)get_private_profile_uint64("gamingmode",buttonstr,0,BIOS_Settings_file);
		snprintf(buttonstr,sizeof(buttonstr),"gamingmode_map_%s_mousebuttons",buttons[button]);
		BIOS_Settings.input_settings.mouse_gamemodemappings[button] = (byte)get_private_profile_uint64("gamingmode",buttonstr,0,BIOS_Settings_file);
	}

	BIOS_Settings.input_settings.gamingmode_joystick = (byte)get_private_profile_uint64("gamingmode","joystick",0,BIOS_Settings_file); //Use the joystick input instead of mapped input during gaming mode?

	//XTCMOS
	BIOS_Settings.got_XTCMOS = (byte)get_private_profile_uint64("XTCMOS","gotCMOS",0,BIOS_Settings_file); //Gotten an CMOS?
	loadBIOSCMOS(&BIOS_Settings.XTCMOS,"XTCMOS"); //Load the CMOS from the file!

	//ATCMOS
	BIOS_Settings.got_ATCMOS = (byte)get_private_profile_uint64("ATCMOS","gotCMOS",0,BIOS_Settings_file); //Gotten an CMOS?
	loadBIOSCMOS(&BIOS_Settings.ATCMOS,"ATCMOS"); //Load the CMOS from the file!

	//CompaqCMOS
	BIOS_Settings.got_CompaqCMOS = (byte)get_private_profile_uint64("CompaqCMOS","gotCMOS",0,BIOS_Settings_file); //Gotten an CMOS?
	loadBIOSCMOS(&BIOS_Settings.CompaqCMOS,"CompaqCMOS"); //The full saved CMOS!

	//PS2CMOS
	BIOS_Settings.got_PS2CMOS = (byte)get_private_profile_uint64("PS2CMOS","gotCMOS",0,BIOS_Settings_file); //Gotten an CMOS?
	loadBIOSCMOS(&BIOS_Settings.PS2CMOS,"PS2CMOS"); //Load the CMOS from the file!

	//BIOS settings have been loaded.

	if (BIOS_Settings.version!=BIOS_VERSION) //Not compatible with our version?
	{
		dolog("Settings","Error: Invalid settings version.");
		BIOS_LoadDefaults(1); //Load the defaults, save!
		return; //We've loaded the defaults because 
	}

	if (defaultsapplied) //To save, because defaults have been applied?
	{
		BIOS_SaveData(); //Save our Settings data!
	}

	memcpy(&loadedsettings, &BIOS_Settings, sizeof(BIOS_Settings)); //Reload from buffer!
	loadedsettings_loaded = 1; //Buffered in memory!
}

byte saveBIOSCMOS(CMOSDATA *CMOS, char *section, char *section_comment)
{
	word index;
	char field[256];
	if (!write_private_profile_int64(section,section_comment,"TimeDivergeance_seconds",CMOS->timedivergeance,BIOS_Settings_file)) return 0;
	if (!write_private_profile_int64(section,section_comment,"TimeDivergeance_microseconds",CMOS->timedivergeance2,BIOS_Settings_file)) return 0;
	if (!write_private_profile_uint64(section,section_comment,"s100",CMOS->s100,BIOS_Settings_file)) return 0;
	if (!write_private_profile_uint64(section,section_comment,"s10000",CMOS->s10000,BIOS_Settings_file)) return 0;
	if (!write_private_profile_uint64(section,section_comment,"centuryisbinary",CMOS->centuryisbinary,BIOS_Settings_file)) return 0;
	if (!write_private_profile_uint64(section,section_comment,"cycletiming",CMOS->cycletiming,BIOS_Settings_file)) return 0;
	for (index=0;index<NUMITEMS(CMOS->DATA80.data);++index) //Process extra RAM data!
	{
		snprintf(field,sizeof(field),"RAM%02X",index); //The field!
		if (!write_private_profile_uint64(section,section_comment,&field[0],CMOS->DATA80.data[index],BIOS_Settings_file)) return 0;
	}
	for (index=0;index<NUMITEMS(CMOS->extraRAMdata);++index) //Process extra RAM data!
	{
		snprintf(field,sizeof(field),"extraRAM%02X",index); //The field!
		if (!write_private_profile_uint64(section,section_comment,&field[0],CMOS->extraRAMdata[index],BIOS_Settings_file)) return 0;
	}
	return 1; //Successfully written!
}

extern char BOOT_ORDER_STRING[15][30]; //Boot order, string values!

extern char colors[0x10][15]; //All 16 colors!

//Comments to build:
char general_comment[4096] = "version: version number, DO NOT CHANGE\nfirstrun: 1 for opening the settings menu automatically, 0 otherwise\nsettingsmenufont: the font to use for the Settings menu: 0=Default, 1=Phoenix Laptop, 2=Phoenix - Award Workstation\nbackgroundpolicy: 0=Full halt, 1=Run without audio playing and recording, 2=Run without recording, 3=Run without rendering the display"; //General comment!
char machine_comment[4096] = ""; //Machine comment!
char debugger_comment[4096] = ""; //Debugger comment!
char video_comment[4096] = ""; //Video comment!
char sound_comment[4096] = ""; //Sound comment!
char modem_comment[4096] = ""; //Sound comment!
char disks_comment[4096] = ""; //Disks comment!
char bios_comment[4096] = ""; //BIOS comment!
char currentstr[4096] = ""; //Current boot order to dump!
char input_comment[4096] = ""; //Input comment!
char gamingmode_comment[4096] = ""; //Gamingmode comment!
char bioscomment_currentkey[4096] = "";
char buttons[15][256] = {"start","left","up","right","down","ltrigger","rtrigger","triangle","circle","cross","square","analogleft","analogup","analogright","analogdown"}; //The names of all mappable buttons!
char cmos_comment[4096] = ""; //PrimaryCMOS comment!

#ifdef PACKETSERVER_ENABLED
extern uint8_t maclocal_default[6]; //Default MAC of the sender!
#endif

int BIOS_SaveData() //Save BIOS settings!
{
	byte c;
	if (__HW_DISABLED) return 1; //Abort!
	if ((memcmp(&loadedsettings,&BIOS_Settings,sizeof(BIOS_Settings))==0) && loadedsettings_loaded) //Unchanged from buffer?
	{
		return 1; //Nothing to do! Report success!
	}
	
	memset(&phonebookentry, 0, sizeof(phonebookentry)); //Init!

	delete_file(NULL,BIOS_Settings_file); //We're rewriting the file entirely, also updating the comments if required!

	//General
	char *general_commentused=NULL;
	if (general_comment[0]) general_commentused = &general_comment[0];
	if (!write_private_profile_uint64("general",general_commentused,"version",BIOS_VERSION,BIOS_Settings_file)) return 0;
	if (!write_private_profile_uint64("general",general_commentused,"firstrun",BIOS_Settings.firstrun,BIOS_Settings_file)) return 0; //Is this the first run of this BIOS?
	if (!write_private_profile_uint64("general",general_commentused,"settingsmenufont",BIOS_Settings.BIOSmenu_font,BIOS_Settings_file)) return 0; //The selected font for the BIOS menu!
	if (!write_private_profile_uint64("general", general_commentused, "backgroundpolicy", BIOS_Settings.backgroundpolicy, BIOS_Settings_file)) return 0; //The selected font for the BIOS menu!

	//Machine
	memset(&machine_comment,0,sizeof(machine_comment)); //Init!
	safestrcat(machine_comment,sizeof(machine_comment),"cpu: 0=8086/8088, 1=NEC V20/V30, 2=80286, 3=80386, 4=80486\n");
	safestrcat(machine_comment,sizeof(machine_comment),"databussize: 0=Full sized data bus of 16/32-bits, 1=Reduced data bus size\n");
	safestrcat(machine_comment,sizeof(machine_comment),"memory: memory size in bytes\n");
	safestrcat(machine_comment,sizeof(machine_comment),"architecture: 0=XT, 1=AT, 2=Compaq Deskpro 386, 3=Compaq Deskpro 386 with PS/2 mouse\n");
	safestrcat(machine_comment,sizeof(machine_comment),"executionmode: 0=Use emulator internal BIOS, 1=Run debug directory files, else TESTROM.DAT at 0000:0000, 2=Run TESTROM.DAT at 0000:0000, 3=Debug video card output, 4=Load BIOS from ROM directory as BIOSROM.u* and OPTROM.*, 5=Run sound test\n");
	safestrcat(machine_comment,sizeof(machine_comment),"cpuspeed: 0=default, otherwise, limited to n cycles(>=0)\n");
	safestrcat(machine_comment,sizeof(machine_comment),"showcpuspeed: 0=Don't show, 1=Show\n");
	safestrcat(machine_comment,sizeof(machine_comment),"turbocpuspeed: 0=default, otherwise, limit to n cycles(>=0)\n");
	safestrcat(machine_comment,sizeof(machine_comment),"useturbocpuspeed: 0=Don't use, 1=Use\n");
	safestrcat(machine_comment,sizeof(machine_comment),"clockingmode: 0=Cycle-accurate clock, 1=IPS clock\n");
	safestrcat(machine_comment,sizeof(machine_comment),"BIOSROMmode: 0=Normal BIOS ROM, 1=Diagnostic ROM, 2=Enforce normal U-ROMs\n");
	safestrcat(machine_comment,sizeof(machine_comment),"inboardinitialwaitstates: 0=Default waitstates, 1=No waitstates");
	char *machine_commentused=NULL;
	if (machine_comment[0]) machine_commentused = &machine_comment[0];
	if (!write_private_profile_uint64("machine",machine_commentused,"cpu",BIOS_Settings.emulated_CPU,BIOS_Settings_file)) return 0;
	if (!write_private_profile_uint64("machine",machine_commentused,"databussize",BIOS_Settings.DataBusSize,BIOS_Settings_file)) return 0; //The size of the emulated BUS. 0=Normal bus, 1=8-bit bus when available for the CPU!
	if (!write_private_profile_uint64("machine",machine_commentused,"memory",BIOS_Settings.memory,BIOS_Settings_file)) return 0;
	if (!write_private_profile_uint64("machine",machine_commentused,"architecture",BIOS_Settings.architecture,BIOS_Settings_file)) return 0; //Are we using the XT/AT/PS/2 architecture?
	if (!write_private_profile_uint64("machine",machine_commentused,"executionmode",BIOS_Settings.executionmode,BIOS_Settings_file)) return 0; //What mode to execute in during runtime?
	if (!write_private_profile_uint64("machine",machine_commentused,"cpuspeed",BIOS_Settings.CPUSpeed,BIOS_Settings_file)) return 0;
	if (!write_private_profile_uint64("machine",machine_commentused,"showcpuspeed",BIOS_Settings.ShowCPUSpeed,BIOS_Settings_file)) return 0; //Show the relative CPU speed together with the framerate?
	if (!write_private_profile_uint64("machine",machine_commentused,"turbocpuspeed",BIOS_Settings.TurboCPUSpeed,BIOS_Settings_file)) return 0;
	if (!write_private_profile_uint64("machine",machine_commentused,"useturbocpuspeed",BIOS_Settings.useTurboSpeed,BIOS_Settings_file)) return 0; //Are we to use Turbo CPU speed?
	if (!write_private_profile_uint64("machine",machine_commentused,"clockingmode",BIOS_Settings.clockingmode,BIOS_Settings_file)) return 0; //Are we using the IPS clock?
	if (!write_private_profile_uint64("machine",machine_commentused,"BIOSROMmode",BIOS_Settings.BIOSROMmode,BIOS_Settings_file)) return 0; //BIOS ROM mode.
	if (!write_private_profile_uint64("machine",machine_commentused,"inboardinitialwaitstates",BIOS_Settings.InboardInitialWaitstates,BIOS_Settings_file)) return 0; //Inboard 386 initial delay used?

	//Debugger
	memset(&debugger_comment,0,sizeof(debugger_comment)); //Init!
	safestrcat(debugger_comment,sizeof(debugger_comment),"debugmode: 0=Disabled, 1=Enabled, RTrigger=Step, 2=Enabled, Step through, 3=Enabled, just run, ignore shoulder buttons, 4=Enabled, just run, don't show, ignore shoulder buttons\n");
	safestrcat(debugger_comment,sizeof(debugger_comment),"debuggerlog: 0=Don't log, 1=Only when debugging, 2=Always log, 3=Interrupt calls only, 4=BIOS Diagnostic codes only, 5=Always log, no register state, 6=Always log, even during skipping, 7=Always log, even during skipping, single line format, 8=Only when debugging, single line format, 9=Always log, even during skipping, single line format, simplified, 10=Only when debugging, single line format, simplified, 11=Always log, common log format, 12=Always log, even during skipping, common log format, 13=Only when debugging, common log format\n");
	safestrcat(debugger_comment,sizeof(debugger_comment),"logstates: 0=Disabled, 1=Enabled\n");
	safestrcat(debugger_comment,sizeof(debugger_comment),"logregisters: 0=Disabled, 1=Enabled\n");
	safestrcat(debugger_comment,sizeof(debugger_comment),"breakpoint: bits 60-61: 0=Not set, 1=Real mode, 2=Protected mode, 3=Virtual 8086 mode; bit 59: Break on CS only; bit 58: Break on mode only. bit 57: Break on EIP only. bits 32-47: segment, bits 31-0: offset(truncated to 16-bits in Real/Virtual 8086 mode\n");
	safestrcat(debugger_comment,sizeof(debugger_comment),"taskbreakpoint: bits 60-61: 0=Not set, 1=Enabled; bit 59: Break on TR only; bit 57: Break on base address only. bits 32-47: TR segment, bits 31-0: base address within the descriptor cache\n");
	safestrcat(debugger_comment,sizeof(debugger_comment),"CR3breakpoint: bits 60-61: 0=Not set, 1=Enabled; bits 31-0: Base address\n");
	safestrcat(debugger_comment,sizeof(debugger_comment),"diagnosticsport_breakpoint: -1=Disabled, 0-255=Value to trigger the breakpoint\n");
	safestrcat(debugger_comment,sizeof(debugger_comment),"diagnosticsport_timeout: 0=At first instruction, 1+: At the n+1th instruction\n");
	safestrcat(debugger_comment, sizeof(debugger_comment), "advancedlog: 0=Disable advanced logging, 1: Use advanced logging");
	char *debugger_commentused=NULL;
	if (debugger_comment[0]) debugger_commentused = &debugger_comment[0];
	if (!write_private_profile_uint64("debugger",debugger_commentused,"debugmode",BIOS_Settings.debugmode,BIOS_Settings_file)) return 0;
	if (!write_private_profile_uint64("debugger",debugger_commentused,"debuggerlog",BIOS_Settings.debugger_log,BIOS_Settings_file)) return 0;
	if (!write_private_profile_uint64("debugger",debugger_commentused,"logstates",BIOS_Settings.debugger_logstates,BIOS_Settings_file)) return 0; //Are we logging states? 1=Log states, 0=Don't log states!
	if (!write_private_profile_uint64("debugger",debugger_commentused,"logregisters",BIOS_Settings.debugger_logregisters,BIOS_Settings_file)) return 0; //Are we logging states? 1=Log states, 0=Don't log states!
	if (!write_private_profile_uint64("debugger",debugger_commentused,"breakpoint",BIOS_Settings.breakpoint,BIOS_Settings_file)) return 0; //The used breakpoint segment:offset and mode!
	if (!write_private_profile_uint64("debugger",debugger_commentused,"taskbreakpoint",BIOS_Settings.taskBreakpoint,BIOS_Settings_file)) return 0; //The used breakpoint segment:offset and enable!
	if (!write_private_profile_uint64("debugger",debugger_commentused,"CR3breakpoint",BIOS_Settings.CR3breakpoint,BIOS_Settings_file)) return 0; //The used breakpoint offset ans enable!
	if (!write_private_profile_int64("debugger",debugger_commentused,"diagnosticsport_breakpoint",BIOS_Settings.diagnosticsportoutput_breakpoint,BIOS_Settings_file)) return 0; //Use a diagnostics port breakpoint?
	if (!write_private_profile_uint64("debugger",debugger_commentused,"diagnosticsport_timeout",BIOS_Settings.diagnosticsportoutput_timeout,BIOS_Settings_file)) return 0; //Breakpoint timeout used!
	if (!write_private_profile_uint64("debugger", debugger_commentused, "advancedlog", BIOS_Settings.advancedlog, BIOS_Settings_file)) return 0; //Advanced logging feature!

	//Video
	memset(&video_comment,0,sizeof(video_comment)); //Init!
	safestrcat(video_comment,sizeof(video_comment),"videocard: 0=Pure VGA, 1=VGA with NMI, 2=VGA with CGA, 3=VGA with MDA, 4=Pure CGA, 5=Pure MDA, 6=Tseng ET4000, 7=Tseng ET3000, 8=Pure EGA\n");
	safestrcat(video_comment,sizeof(video_comment),"CGAmodel: 0=Old-style RGB, 1=Old-style NTSC, 2=New-style RGB, 3=New-style NTSC\n");
	safestrcat(video_comment,sizeof(video_comment),"VRAM: Ammount of VRAM installed, in bytes\n");
	safestrcat(video_comment,sizeof(video_comment),"synchronization: 0=Old synchronization depending on host, 1=Synchronize depending on host, 2=Full CPU synchronization\n");
	safestrcat(video_comment,sizeof(video_comment),"directplot: 0=Disabled, 1=Automatic, 2=Forced\n");
	safestrcat(video_comment,sizeof(video_comment),"aspectratio: 0=Fullscreen stretching, 1=Keep the same, 2=Force 4:3(VGA), 3=Force CGA, 4=Force 4:3(SVGA 768p), 5=Force 4:3(SVGA 1080p), 6=Force 4K\n");
	safestrcat(video_comment,sizeof(video_comment),"bwmonitor: 0=Color, 1=B/W monitor: white, 2=B/W monitor: green, 3=B/W monitor: amber\n");
	safestrcat(video_comment,sizeof(video_comment),"showframerate: 0=Disabled, otherwise Enabled");
	char *video_commentused=NULL;
	if (video_comment[0]) video_commentused = &video_comment[0];
	if (!write_private_profile_uint64("video",video_commentused,"videocard",BIOS_Settings.VGA_Mode,BIOS_Settings_file)) return 0; //Enable VGA NMI on precursors?
	if (!write_private_profile_uint64("video",video_commentused,"CGAmodel",BIOS_Settings.CGAModel,BIOS_Settings_file)) return 0; //What kind of CGA is emulated? Bit0=NTSC, Bit1=New-style CGA
	if (!write_private_profile_uint64("video",video_commentused,"VRAM",BIOS_Settings.VRAM_size,BIOS_Settings_file)) return 0; //(S)VGA VRAM size!
	if (!write_private_profile_uint64("video",video_commentused,"synchronization",BIOS_Settings.VGASynchronization,BIOS_Settings_file)) return 0; //VGA synchronization setting. 0=Automatic synchronization based on Host CPU. 1=Tight VGA Synchronization with the CPU.
	if (!write_private_profile_uint64("video",video_commentused,"directplot",BIOS_Settings.GPU_AllowDirectPlot,BIOS_Settings_file)) return 0; //Allow VGA Direct Plot: 1 for automatic 1:1 mapping, 0 for always dynamic, 2 for force 1:1 mapping?
	if (!write_private_profile_uint64("video",video_commentused,"aspectratio",BIOS_Settings.aspectratio,BIOS_Settings_file)) return 0; //The aspect ratio to use?
	if (!write_private_profile_uint64("video",video_commentused,"bwmonitor",BIOS_Settings.bwmonitor,BIOS_Settings_file)) return 0; //Are we a b/w monitor?
	if (!write_private_profile_uint64("video",video_commentused,"showframerate",BIOS_Settings.ShowFramerate,BIOS_Settings_file)) return 0; //Show the frame rate?

	//Sound
	memset(&sound_comment,0,sizeof(sound_comment)); //Init!
	safestrcat(sound_comment,sizeof(sound_comment),"speaker: 0=Disabled, 1=Enabled\n");
	safestrcat(sound_comment,sizeof(sound_comment),"adlib: 0=Disabled, 1=Enabled\n");
	safestrcat(sound_comment,sizeof(sound_comment),"LPTDAC: 0=Disabled, 1=Enabled\n");
	safestrcat(sound_comment,sizeof(sound_comment),"soundfont: The path to the soundfont file. Empty for none.\n");
	safestrcat(sound_comment,sizeof(sound_comment),"directmidi: 0=Disabled, 1=Enabled\n");
	safestrcat(sound_comment,sizeof(sound_comment),"gameblaster: 0=Disabled, 1=Enabled\n");
	safestrcat(sound_comment,sizeof(sound_comment),"gameblaster_volume: Volume of the game blaster, in percent(>=0)\n");
	safestrcat(sound_comment,sizeof(sound_comment),"soundblaster: 0=Disabled, 1=Version 1.5, 2=Version 2.0\n");
	safestrcat(sound_comment,sizeof(sound_comment),"soundsource_volume: Volume of the sound source, in percent(>=0)");
	char *sound_commentused=NULL;
	if (sound_comment[0]) sound_commentused = &sound_comment[0];
	if (!write_private_profile_uint64("sound",sound_commentused,"speaker",BIOS_Settings.usePCSpeaker,BIOS_Settings_file)) return 0; //Emulate PC Speaker sound?
	if (!write_private_profile_uint64("sound",sound_commentused,"adlib",BIOS_Settings.useAdlib,BIOS_Settings_file)) return 0; //Emulate Adlib?
	if (!write_private_profile_uint64("sound",sound_commentused,"LPTDAC",BIOS_Settings.useLPTDAC,BIOS_Settings_file)) return 0; //Emulate Covox/Disney Sound Source?
	if (!write_private_profile_string("sound",sound_commentused,"soundfont",&BIOS_Settings.SoundFont[0],BIOS_Settings_file)) return 0; //Read entry!
	if (!write_private_profile_uint64("sound",sound_commentused,"directmidi",BIOS_Settings.useDirectMIDI,BIOS_Settings_file)) return 0; //Use Direct MIDI synthesis by using a passthrough to the OS?
	if (!write_private_profile_uint64("sound",sound_commentused,"gameblaster",BIOS_Settings.useGameBlaster,BIOS_Settings_file)) return 0; //Emulate Game Blaster?
	if (!write_private_profile_uint64("sound",sound_commentused,"gameblaster_volume",BIOS_Settings.GameBlaster_Volume,BIOS_Settings_file)) return 0; //The Game Blaster volume knob!
	if (!write_private_profile_uint64("sound",sound_commentused,"soundblaster",BIOS_Settings.useSoundBlaster,BIOS_Settings_file)) return 0; //Emulate Sound Blaster?
	if (!write_private_profile_uint64("sound",sound_commentused,"soundsource_volume",BIOS_Settings.SoundSource_Volume,BIOS_Settings_file)) return 0; //The sound source volume knob!

	//Modem
	memset(&modem_comment,0,sizeof(modem_comment)); //Init!
	memset(currentstr,0,sizeof(currentstr)); //Init!
	snprintf(modem_comment,sizeof(modem_comment),"listenport: listen port to listen on when not connected(defaults to %u)\n",DEFAULT_MODEMLISTENPORT);
	snprintf(currentstr, sizeof(currentstr), "phonebook0-%u: Phonebook entry #n\n", (byte)(NUMITEMS(BIOS_Settings.phonebook)-1)); //Information about the phonebook!
	safestrcat(modem_comment, sizeof(modem_comment), currentstr); //MAC address information!
#ifdef PACKETSERVER_ENABLED
	safestrcat(modem_comment,sizeof(modem_comment),"ethernetcard: -1 for disabled(use normal emulation), 0-254 selected and use a network card, 255 to generate a list of network cards to select\n");
	snprintf(currentstr,sizeof(currentstr),"MACaddress: MAC address to emulate as a virtual NIC and send/receive packets on(defaults to %02x:%02x:%02x:%02x:%02x:%02x)\n",maclocal_default[0],maclocal_default[1],maclocal_default[2],maclocal_default[3],maclocal_default[4],maclocal_default[5]);
	safestrcat(modem_comment,sizeof(modem_comment),currentstr); //MAC address information!
	safestrcat(modem_comment,sizeof(modem_comment),"gatewayMACaddress: gateway MAC address to send/receive packets on\n");
	safestrcat(modem_comment,sizeof(modem_comment),"username: set username and password to non-empty values for a credential protected server\n");
	safestrcat(modem_comment,sizeof(modem_comment),"password: set username and password to non-empty values for a credential protected server\n");
	safestrcat(modem_comment,sizeof(modem_comment),"IPaddress: static IP address to use for this NIC (account). Format 0123456789AB for IP 012.345.678.9AB\n");
#endif
	char *modem_commentused=NULL;
	if (modem_comment[0]) modem_commentused = &modem_comment[0];
	if (!write_private_profile_uint64("modem",modem_commentused,"listenport",BIOS_Settings.modemlistenport,BIOS_Settings_file)) return 0; //Modem listen port!
	for (c = 0; c < NUMITEMS(BIOS_Settings.phonebook); ++c) //Process all phonebook entries!
	{
		snprintf(phonebookentry, sizeof(phonebookentry), "phonebook%u", c); //The entry to use!
		if (!write_private_profile_string("modem",modem_commentused,phonebookentry,&BIOS_Settings.phonebook[c][0],BIOS_Settings_file)) return 0; //Entry!
	}
#ifdef PACKETSERVER_ENABLED
	if (!write_private_profile_int64("modem",modem_commentused,"ethernetcard",BIOS_Settings.ethernetserver_settings.ethernetcard,BIOS_Settings_file)) return 0; //Ethernet card to use!
	if (!write_private_profile_string("modem",modem_commentused,"MACaddress",&BIOS_Settings.ethernetserver_settings.MACaddress[0],BIOS_Settings_file)) return 0; //MAC address to use!
	if (!write_private_profile_string("modem",modem_commentused,"gatewayMACaddress",&BIOS_Settings.ethernetserver_settings.gatewayMACaddress[0],BIOS_Settings_file)) return 0; //MAC address to use!
	if (!write_private_profile_string("modem",modem_commentused,"username",&BIOS_Settings.ethernetserver_settings.username[0],BIOS_Settings_file)) return 0; //MAC address to use!
	if (!write_private_profile_string("modem",modem_commentused,"password",&BIOS_Settings.ethernetserver_settings.password[0],BIOS_Settings_file)) return 0; //MAC address to use!
	if (!write_private_profile_string("modem",modem_commentused,"IPaddress",&BIOS_Settings.ethernetserver_settings.IPaddress[0],BIOS_Settings_file)) return 0; //MAC address to use!
#endif

	//Disks
	memset(&disks_comment,0,sizeof(disks_comment)); //Init!
	safestrcat(disks_comment,sizeof(disks_comment),"floppy[number]/hdd[number]/cdrom[number]: The disk to be mounted. Empty for none.\n");
	safestrcat(disks_comment,sizeof(disks_comment),"floppy[number]_readonly/hdd[number]_readonly: 0=Writable, 1=Read-only");
	char *disks_commentused=NULL;
	if (disks_comment[0]) disks_commentused = &disks_comment[0];
	if (!write_private_profile_string("disks",disks_commentused,"floppy0",&BIOS_Settings.floppy0[0],BIOS_Settings_file)) return 0; //Read entry!
	if (!write_private_profile_uint64("disks",disks_commentused,"floppy0_readonly",BIOS_Settings.floppy0_readonly,BIOS_Settings_file)) return 0;
	if (!write_private_profile_string("disks",disks_commentused,"floppy1",&BIOS_Settings.floppy1[0],BIOS_Settings_file)) return 0; //Read entry!
	if (!write_private_profile_uint64("disks",disks_commentused,"floppy1_readonly",BIOS_Settings.floppy1_readonly,BIOS_Settings_file)) return 0;
	if (!write_private_profile_string("disks",disks_commentused,"hdd0",&BIOS_Settings.hdd0[0],BIOS_Settings_file)) return 0; //Read entry!
	if (!write_private_profile_uint64("disks",disks_commentused,"hdd0_readonly",BIOS_Settings.hdd0_readonly,BIOS_Settings_file)) return 0;
	if (!write_private_profile_string("disks",disks_commentused,"hdd1",&BIOS_Settings.hdd1[0],BIOS_Settings_file)) return 0; //Read entry!
	if (!write_private_profile_uint64("disks",disks_commentused,"hdd1_readonly",BIOS_Settings.hdd1_readonly,BIOS_Settings_file)) return 0;
	if (!write_private_profile_string("disks",disks_commentused,"cdrom0",&BIOS_Settings.cdrom0[0],BIOS_Settings_file)) return 0; //Read entry!
	if (!write_private_profile_string("disks",disks_commentused,"cdrom1",&BIOS_Settings.cdrom1[0],BIOS_Settings_file)) return 0; //Read entry!

	//BIOS
	memset(&bios_comment,0,sizeof(bios_comment)); //Init!
	safestrcat(bios_comment,sizeof(bios_comment),"bootorder: The boot order of the internal BIOS:\n");
	byte currentitem;
	for (currentitem=0;currentitem<NUMITEMS(BOOT_ORDER_STRING);++currentitem)
	{
		snprintf(currentstr,sizeof(currentstr),"%u=%s\n",currentitem,BOOT_ORDER_STRING[currentitem]); //A description of all boot orders!
		safestrcat(bios_comment,sizeof(bios_comment),currentstr); //Add the string!
	}
	char *bios_commentused=NULL;
	if (bios_comment[0]) bios_commentused = &bios_comment[0];
	if (!write_private_profile_uint64("bios",bios_commentused,"bootorder",BIOS_Settings.bootorder,BIOS_Settings_file)) return 0;

	//Input
	memset(&currentstr,0,sizeof(currentstr)); //Current boot order to dump!
	memset(&gamingmode_comment,0,sizeof(gamingmode_comment)); //Gamingmode comment!
	memset(&input_comment,0,sizeof(input_comment)); //Init!
	safestrcat(input_comment,sizeof(input_comment),"analog_minrange: Minimum range for the analog stick to repond. 0-255\n");
	safestrcat(input_comment,sizeof(input_comment),"Color codes are as follows:");
	for (currentitem=0;currentitem<0x10;++currentitem)
	{
		snprintf(currentstr,sizeof(currentstr),"\n%u=%s",currentitem,colors[currentitem]); //A description of all colors!
		safestrcat(input_comment,sizeof(input_comment),currentstr); //Add the string!
	}
	safestrcat(input_comment,sizeof(input_comment),"\n\n"); //Empty line!
	safestrcat(input_comment,sizeof(input_comment),"keyboard_fontcolor: font color for the (PSP) OSK.\n");
	safestrcat(input_comment,sizeof(input_comment),"keyboard_bordercolor: border color for the (PSP) OSK.\n");
	safestrcat(input_comment,sizeof(input_comment),"keyboard_activecolor: active color for the (PSP) OSK. Also color of pressed keys on the touch OSK.\n");
	safestrcat(input_comment,sizeof(input_comment),"keyboard_specialcolor: font color for the LEDs.\n");
	safestrcat(input_comment,sizeof(input_comment),"keyboard_specialbordercolor: border color for the LEDs.\n");
	safestrcat(input_comment,sizeof(input_comment),"keyboard_specialactivecolor: active color for the LEDs.\n");
	char *input_commentused=NULL;
	if (input_comment[0]) input_commentused = &input_comment[0];
	if (!write_private_profile_uint64("input",input_commentused,"analog_minrange",BIOS_Settings.input_settings.analog_minrange,BIOS_Settings_file)) return 0; //Minimum adjustment x&y(0,0) for keyboard&mouse to change states (from center)
	if (!write_private_profile_uint64("input",input_commentused,"keyboard_fontcolor",BIOS_Settings.input_settings.fontcolor,BIOS_Settings_file)) return 0;
	if (!write_private_profile_uint64("input",input_commentused,"keyboard_bordercolor",BIOS_Settings.input_settings.bordercolor,BIOS_Settings_file)) return 0;
	if (!write_private_profile_uint64("input",input_commentused,"keyboard_activecolor",BIOS_Settings.input_settings.activecolor,BIOS_Settings_file)) return 0;
	if (!write_private_profile_uint64("input",input_commentused,"keyboard_specialcolor",BIOS_Settings.input_settings.specialcolor,BIOS_Settings_file)) return 0;
	if (!write_private_profile_uint64("input",input_commentused,"keyboard_specialbordercolor",BIOS_Settings.input_settings.specialbordercolor,BIOS_Settings_file)) return 0;
	if (!write_private_profile_uint64("input",input_commentused,"keyboard_specialactivecolor",BIOS_Settings.input_settings.specialactivecolor,BIOS_Settings_file)) return 0;
	
	//Gamingmode
	memset(&bioscomment_currentkey,0,sizeof(bioscomment_currentkey)); //Init!
	for (currentitem=0;currentitem<104;++currentitem) //Give translations for all keys!
	{
		safestrcpy(currentstr,sizeof(currentstr),""); //Init current string!
		safestrcpy(bioscomment_currentkey,sizeof(bioscomment_currentkey),""); //Init current string!
		if (EMU_keyboard_handler_idtoname(currentitem,&bioscomment_currentkey[0])) //Name retrieved?
		{
			snprintf(currentstr,sizeof(currentstr),"Key number %u is %s\n",currentitem,bioscomment_currentkey); //Generate a key row!
			safestrcat(gamingmode_comment,sizeof(gamingmode_comment),currentstr); //Add the key to the list!
		}
	}
	safestrcat(gamingmode_comment,sizeof(gamingmode_comment),"gamingmode_map_[key]_key: The key to be mapped. -1 for unmapped. Otherwise, the key number(0-103)\n");
	snprintf(currentstr,sizeof(currentstr),"gamingmode_map_[key]_shiftstate: The summed state of ctrl/alt/shift keys to be pressed. %u=Ctrl, %u=Alt, %u=Shift. 0/empty=None.\n",SHIFTSTATUS_CTRL,SHIFTSTATUS_ALT,SHIFTSTATUS_SHIFT);
	safestrcat(gamingmode_comment,sizeof(gamingmode_comment),currentstr);
	safestrcat(gamingmode_comment,sizeof(gamingmode_comment),"gamingmode_map_[key]_mousebuttons: The summed state of mouse buttons to be pressed(0=None pressed, 1=Left, 2=Right, 4=Middle).\n");
	safestrcat(gamingmode_comment,sizeof(gamingmode_comment),"joystick: 0=Normal gaming mode mapped input, 1=Joystick, Cross=Button 1, Circle=Button 2, 2=Joystick, Cross=Button 2, Circle=Button 1, 3=Joystick, Gravis Gamepad, 4=Joystick, Gravis Analog Pro, 5=Joystick, Logitech WingMan Extreme Digital");
	char *gamingmode_commentused=NULL;
	if (gamingmode_comment[0]) gamingmode_commentused = &gamingmode_comment[0];
	byte button;
	char buttonstr[256];
	memset(&buttonstr,0,sizeof(buttonstr)); //Init button string!
	for (button=0;button<15;++button) //Process all buttons!
	{
		snprintf(buttonstr,sizeof(buttonstr),"gamingmode_map_%s_key",buttons[button]);
		if (!write_private_profile_int64("gamingmode",gamingmode_commentused,buttonstr,BIOS_Settings.input_settings.keyboard_gamemodemappings[button],BIOS_Settings_file)) return 0;
		snprintf(buttonstr,sizeof(buttonstr),"gamingmode_map_%s_shiftstate",buttons[button]);
		if (!write_private_profile_uint64("gamingmode",gamingmode_commentused,buttonstr,BIOS_Settings.input_settings.keyboard_gamemodemappings_alt[button],BIOS_Settings_file)) return 0;
		snprintf(buttonstr,sizeof(buttonstr),"gamingmode_map_%s_mousebuttons",buttons[button]);
		if (!write_private_profile_uint64("gamingmode",gamingmode_commentused,buttonstr,BIOS_Settings.input_settings.mouse_gamemodemappings[button],BIOS_Settings_file)) return 0;
	}

	if (!write_private_profile_uint64("gamingmode",gamingmode_commentused,"joystick",BIOS_Settings.input_settings.gamingmode_joystick,BIOS_Settings_file)) return 0; //Use the joystick input instead of mapped input during gaming mode?

	//CMOS
	memset(&cmos_comment,0,sizeof(cmos_comment)); //Init!
	safestrcat(cmos_comment,sizeof(cmos_comment),"gotCMOS: 0=Don't load CMOS. 1=CMOS data is valid and to be loaded.\n");
	safestrcat(cmos_comment,sizeof(cmos_comment),"TimeDivergeance_seconds: Time to be added to get the emulated time, in seconds.\n");
	safestrcat(cmos_comment,sizeof(cmos_comment),"TimeDivergeance_microseconds: Time to be added to get the emulated time, in microseconds.\n");
	safestrcat(cmos_comment,sizeof(cmos_comment),"s100: 100th second register content on XT RTC (0-255, Usually BCD stored as integer)\n");
	safestrcat(cmos_comment,sizeof(cmos_comment),"s10000: 10000th second register content on XT RTC (0-255, Usually BCD stored as integer)\n");
	safestrcat(cmos_comment,sizeof(cmos_comment),"RAM[hexnumber]: The contents of the CMOS RAM location(0-255)\n");
	safestrcat(cmos_comment,sizeof(cmos_comment),"extraRAM[hexnumber]: The contents of the extra RAM location(0-255)");
	safestrcat(cmos_comment,sizeof(cmos_comment),"centuryisbinary: The contents of the century byte is to be en/decoded as binary(value 1) instead of BCD(value 0), not to be used as a century byte.");
	safestrcat(cmos_comment,sizeof(cmos_comment),"cycletiming: 0=Time divergeance is relative to realtime. Not 0=Time is relative to 1-1-1970 midnight and running on the CPU timing.");
	char *cmos_commentused=NULL;
	if (cmos_comment[0]) cmos_commentused = &cmos_comment[0];

	//XTCMOS
	if (!write_private_profile_uint64("XTCMOS",cmos_commentused,"gotCMOS",BIOS_Settings.got_XTCMOS,BIOS_Settings_file)) return 0; //Gotten an CMOS?
	if (!saveBIOSCMOS(&BIOS_Settings.XTCMOS,"XTCMOS",cmos_commentused)) return 0; //Load the CMOS from the file!

	//ATCMOS
	if (!write_private_profile_uint64("ATCMOS",cmos_commentused,"gotCMOS",BIOS_Settings.got_ATCMOS,BIOS_Settings_file)) return 0; //Gotten an CMOS?
	if (!saveBIOSCMOS(&BIOS_Settings.ATCMOS,"ATCMOS",cmos_commentused)) return 0; //Load the CMOS from the file!

	//CompaqCMOS
	if (!write_private_profile_uint64("CompaqCMOS",cmos_commentused,"gotCMOS",BIOS_Settings.got_CompaqCMOS,BIOS_Settings_file)) return 0; //Gotten an CMOS?
	if (!saveBIOSCMOS(&BIOS_Settings.CompaqCMOS,"CompaqCMOS",cmos_commentused)) return 0; //The full saved CMOS!

	//PS2CMOS
	if (!write_private_profile_uint64("PS2CMOS",cmos_commentused,"gotCMOS",BIOS_Settings.got_PS2CMOS,BIOS_Settings_file)) return 0; //Gotten an CMOS?
	if (!saveBIOSCMOS(&BIOS_Settings.PS2CMOS,"PS2CMOS",cmos_commentused)) return 0; //The full saved CMOS!

	memcpy(&loadedsettings,&BIOS_Settings,sizeof(BIOS_Settings)); //Reload from buffer!
	loadedsettings_loaded = 1; //Buffered in memory!

	//Fully written!
	return 1; //BIOS Written & saved successfully!
}

uint_32 BIOS_GetMMUSize() //For MMU!
{
	if (__HW_DISABLED) return MBMEMORY; //Abort with default value (1MB memory)!
	return BIOS_Settings.memory; //Use all available memory always!
}

extern byte backgroundpolicy; //Background task policy. 0=Full halt of the application, 1=Keep running without video and audio muted, 2=Keep running with audio playback, recording muted, 3=Keep running fully without video.

void BIOS_ValidateData() //Validates all data and unmounts/remounts if needed!
{
	char soundfont[256];
	if (__HW_DISABLED) return; //Abort!
	//Mount all devices!
	iofloppy0(BIOS_Settings.floppy0,0,BIOS_Settings.floppy0_readonly,0);
	iofloppy1(BIOS_Settings.floppy1,0,BIOS_Settings.floppy1_readonly,0);
	iohdd0(BIOS_Settings.hdd0,0,BIOS_Settings.hdd0_readonly,0);
	iohdd1(BIOS_Settings.hdd1,0,BIOS_Settings.hdd1_readonly,0);
	iocdrom0(BIOS_Settings.cdrom0,0,1,0);
	iocdrom1(BIOS_Settings.cdrom1,0,1,0);

	byte buffer[512]; //Little buffer for checking the files!
	int bioschanged = 0; //BIOS changed?
	bioschanged = 0; //Reset if the BIOS is changed!

	//dolog("IO","Checking FLOPPY A (%s)...",BIOS_Settings.floppy0);
	if ((!readdata(FLOPPY0,&buffer,0,sizeof(buffer))) && (strcmp(BIOS_Settings.floppy0,"")!=0)) //No disk mounted but listed?
	{
		if (!getDSKimage(FLOPPY0)) //NOT a DSK image?
		{
			memset(&BIOS_Settings.floppy0[0],0,sizeof(BIOS_Settings.floppy0)); //Unmount!
			BIOS_Settings.floppy0_readonly = 0; //Reset readonly flag!
			//dolog("BIOS","Floppy A invalidated!");
			bioschanged = 1; //BIOS changed!
		}
	}
	//dolog("IO","Checking FLOPPY B (%s)...",BIOS_Settings.floppy1);
	if ((!readdata(FLOPPY1,&buffer,0,sizeof(buffer))) && (strcmp(BIOS_Settings.floppy1,"")!=0)) //No disk mounted but listed?
	{
		if (!getDSKimage(FLOPPY1)) //NOT a DSK image?
		{
			memset(&BIOS_Settings.floppy1[0],0,sizeof(BIOS_Settings.floppy1)); //Unmount!
			BIOS_Settings.floppy1_readonly = 0; //Reset readonly flag!
			//dolog("BIOS","Floppy B invalidated!");
			bioschanged = 1; //BIOS changed!
		}
	}
	
	//dolog("IO","Checking First HDD (%s)...",BIOS_Settings.hdd0);
	if ((!readdata(HDD0,&buffer,0,sizeof(buffer))) && (strcmp(BIOS_Settings.hdd0,"")!=0)) //No disk mounted but listed?
	{
		memset(&BIOS_Settings.hdd0[0],0,sizeof(BIOS_Settings.hdd0)); //Unmount!
		BIOS_Settings.hdd0_readonly = 0; //Reset readonly flag!
		//dolog("BIOS","First HDD invalidated!");
		bioschanged = 1; //BIOS changed!
	}
	
	//dolog("IO","Checking Second HDD (%s)...",BIOS_Settings.hdd1);
	if ((!readdata(HDD1,&buffer,0,sizeof(buffer))) && (strcmp(BIOS_Settings.hdd1,"")!=0)) //No disk mounted but listed?
	{
		memset(&BIOS_Settings.hdd1[0],0,sizeof(BIOS_Settings.hdd1)); //Unmount!
		BIOS_Settings.hdd1_readonly = 0; //Reset readonly flag!
		//dolog("BIOS","Second HDD invalidated!");
		bioschanged = 1; //BIOS changed!
	}
	//dolog("IO","Checking First CD-ROM (%s)...",BIOS_Settings.cdrom0);
	if ((!readdata(CDROM0,&buffer,0,sizeof(buffer))) && (strcmp(BIOS_Settings.cdrom0,"")!=0)) //No disk mounted but listed?
	{
		memset(&BIOS_Settings.cdrom0[0],0,sizeof(BIOS_Settings.cdrom0)); //Unmount!
		bioschanged = 1; //BIOS changed!
		//dolog("BIOS","First CD-ROM invalidated!");
	}
	
	//dolog("IO","Checking Second CD-ROM (%s)...",BIOS_Settings.cdrom1);
	if ((!readdata(CDROM1,&buffer,0,sizeof(buffer))) && (strcmp(BIOS_Settings.cdrom1,"")!=0)) //No disk mounted but listed?
	{
		memset(&BIOS_Settings.cdrom1[0],0,sizeof(BIOS_Settings.cdrom1)); //Unmount!
		bioschanged = 1; //BIOS changed!
		//dolog("BIOS","Second CD-ROM invalidated!");
	}

	//Unmount/remount!
	iofloppy0(BIOS_Settings.floppy0,0,BIOS_Settings.floppy0_readonly,0);
	iofloppy1(BIOS_Settings.floppy1,0,BIOS_Settings.floppy1_readonly,0);
	iohdd0(BIOS_Settings.hdd0,0,BIOS_Settings.hdd0_readonly,0);
	iohdd1(BIOS_Settings.hdd1,0,BIOS_Settings.hdd1_readonly,0);
	iocdrom0(BIOS_Settings.cdrom0,0,1,0); //CDROM always read-only!
	iocdrom1(BIOS_Settings.cdrom1,0,1,0); //CDROM always read-only!

	if (BIOS_Settings.SoundFont[0]) //Gotten a soundfont set?
	{
		memset(&soundfont, 0, sizeof(soundfont)); //Init!
		safestrcpy(soundfont,sizeof(soundfont), soundfontpath); //The path to the soundfont!
		safestrcat(soundfont,sizeof(soundfont), "/");
		safestrcat(soundfont,sizeof(soundfont), BIOS_Settings.SoundFont); //The full path to the soundfont!
		if (!FILE_EXISTS(soundfont)) //Not found?
		{
			memset(BIOS_Settings.SoundFont, 0, sizeof(BIOS_Settings.SoundFont)); //Invalid soundfont!
			bioschanged = 1; //BIOS changed!
		}
	}

	if (BIOS_Settings.useDirectMIDI && !directMIDISupported()) //Unsupported Direct MIDI?
	{
		BIOS_Settings.useDirectMIDI = 0; //Unsupported: disable the functionality!
		bioschanged = 1; //BIOS changed!
	}

	if (BIOS_Settings.DataBusSize > 1) //Invalid bus size?
	{
		BIOS_Settings.DataBusSize = 0; //Default bus size!
		bioschanged = 1; //BIOS changed!
	}

	if (bioschanged)
	{
		forceBIOSSave(); //Force saving!
	}
}

extern byte advancedlog; //Advanced log setting!

void BIOS_LoadIO(int showchecksumerrors) //Loads basic I/O drives from BIOS!
{
	if (__HW_DISABLED) return; //Abort!
	ioInit(); //Reset I/O system!
	exec_showchecksumerrors = showchecksumerrors; //Allow checksum errors to be shown!
	BIOS_LoadData();//Load BIOS options!
	BIOS_ValidateData(); //Validate all data!
	GPU_AspectRatio(BIOS_Settings.aspectratio); //Keep the aspect ratio?
	exec_showchecksumerrors = 0; //Don't Allow checksum errors to be shown!
	backgroundpolicy = MIN(BIOS_Settings.backgroundpolicy,3); //Load the new background policy!
	advancedlog = LIMITRANGE(BIOS_Settings.advancedlog,0,1);
}

void BIOS_ShowBIOS() //Shows mounted drives etc!
{
	uint_32 blocksize;
	if (__HW_DISABLED) return; //Abort!
	exec_showchecksumerrors = 0; //No checksum errors to show!
	BIOS_LoadData();
	BIOS_ValidateData(); //Validate all data before continuing!

	printmsg(0xF,"Memory installed: ");
	blocksize = (is_XT) ? MEMORY_BLOCKSIZE_XT : MEMORY_BLOCKSIZE_AT_LOW; //What block size is used?
	printmsg(0xE,"%u blocks (%uKB / %uMB)\r\n",SAFEDIV(BIOS_GetMMUSize(),blocksize),(BIOS_GetMMUSize()/1024),(BIOS_GetMMUSize()/MBMEMORY));

	printmsg(0xF,"\r\n"); //A bit of space between memory and disks!
	int numdrives = 0;
	if (strcmp(BIOS_Settings.hdd0,"")!=0) //Have HDD0?
	{
		printmsg(0xF,"Primary master: %s",BIOS_Settings.hdd0);
		if (BIOS_Settings.hdd0_readonly) //Read-only?
		{
			printmsg(0x4," <R>");
		}
		printmsg(0xF,"\r\n"); //Newline!
		++numdrives;
	}
	if (strcmp(BIOS_Settings.hdd1,"")!=0) //Have HDD1?
	{
		printmsg(0xF,"Primary slave: %s",BIOS_Settings.hdd1);
		if (BIOS_Settings.hdd1_readonly) //Read-only?
		{
			printmsg(0x4," <R>");
		}
		printmsg(0xF,"\r\n"); //Newline!
		++numdrives;
	}
	if (strcmp(BIOS_Settings.cdrom0,"")!=0) //Have CDROM0?
	{
		printmsg(0xF,"Secondary master: %s\r\n",BIOS_Settings.cdrom0);
		++numdrives;
	}
	if (strcmp(BIOS_Settings.cdrom1,"")!=0) //Have CDROM1?
	{
		printmsg(0xF,"Secondary slave: %s\r\n",BIOS_Settings.cdrom1);
		++numdrives;
	}

	if (((strcmp(BIOS_Settings.floppy0,"")!=0) || (strcmp(BIOS_Settings.floppy1,"")!=0)) && numdrives>0) //Have drives and adding floppy?
	{
		printmsg(0xF,"\r\n"); //Insert empty row between floppy and normal disks!
	}

	if (strcmp(BIOS_Settings.floppy0,"")!=0) //Have FLOPPY0?
	{
		printmsg(0xF,"Floppy disk detected: %s",BIOS_Settings.floppy0);
		if (BIOS_Settings.floppy0_readonly) //Read-only?
		{
			printmsg(0x4," <R>");
		}
		printmsg(0xF,"\r\n"); //Newline!
		++numdrives;
	}

	if (strcmp(BIOS_Settings.floppy1,"")!=0) //Have FLOPPY1?
	{
		printmsg(0xF,"Floppy disk detected: %s",BIOS_Settings.floppy1);
		if (BIOS_Settings.floppy1_readonly) //Read-only?
		{
			printmsg(0x4," <R>");
		}
		printmsg(0xF,"\r\n"); //Newline!
		++numdrives;
	}

	if (BIOS_Settings.emulated_CPU==CPU_8086) //8086?
	{
		if (BIOS_Settings.DataBusSize) //8-bit bus?
		{
			printmsg(0xF, "Installed CPU: Intel 8088\r\n"); //Emulated CPU!
		}
		else //16-bit bus?
		{
			printmsg(0xF,"Installed CPU: Intel 8086\r\n"); //Emulated CPU!
		}
	}
	else if (BIOS_Settings.emulated_CPU==CPU_NECV30) //NECV30?
	{
		if (BIOS_Settings.DataBusSize) //8-bit bus?
		{
			printmsg(0xF, "Installed CPU: NEC V20\r\n"); //Emulated CPU!
		}
		else //16-bit bus?
		{
			printmsg(0xF, "Installed CPU: NEC V30\r\n"); //Emulated CPU!
		}
	}
	else if (BIOS_Settings.emulated_CPU == CPU_80286) //80286?
	{
		printmsg(0xF, "Installed CPU: Intel 80286\r\n"); //Emulated CPU!
	}
	else if (BIOS_Settings.emulated_CPU == CPU_80386) //80386?
	{
		printmsg(0xF, "Installed CPU: Intel 80386\r\n"); //Emulated CPU!
	}
	else if (BIOS_Settings.emulated_CPU == CPU_80486) //80486?
	{
		printmsg(0xF, "Installed CPU: Intel 80486\r\n"); //Emulated CPU!
	}
	else if (BIOS_Settings.emulated_CPU == CPU_PENTIUM) //80286?
	{
		printmsg(0xF, "Installed CPU: Intel Pentium(unfinished)\r\n"); //Emulated CPU!
	}
	else //Unknown CPU?
	{
		printmsg(0x4,"Installed CPU: Unknown\r\n"); //Emulated CPU!
	}

	if (numdrives==0) //No drives?
	{
		printmsg(0x4,"Warning: no drives have been detected!\r\nPlease enter settings and specify some disks.\r\n");
	}
}

//Defines for booting!
#define BOOT_FLOPPY 0
#define BOOT_HDD 1
#define BOOT_CDROM 2
#define BOOT_NONE 3

//Boot order for boot sequence!
byte BOOT_ORDER[15][3] =
{
//First full categories (3 active)
	{BOOT_FLOPPY, BOOT_CDROM, BOOT_HDD}, //Floppy, Cdrom, Hdd?
	{BOOT_FLOPPY, BOOT_HDD, BOOT_CDROM}, //Floppy, Hdd, Cdrom?
	{BOOT_CDROM, BOOT_FLOPPY, BOOT_HDD}, //Cdrom, Floppy, Hdd?
	{BOOT_CDROM, BOOT_HDD, BOOT_FLOPPY}, //Cdrom, Hdd, Floppy?
	{BOOT_HDD, BOOT_FLOPPY, BOOT_CDROM}, //Hdd, Floppy, Cdrom?
	{BOOT_HDD, BOOT_CDROM, BOOT_FLOPPY}, //Hdd, Cdrom, Floppy?
//Now advanced categories (2 active)!
	{BOOT_FLOPPY, BOOT_CDROM, BOOT_NONE}, //Floppy, Cdrom?
	{BOOT_FLOPPY, BOOT_HDD, BOOT_NONE}, //Floppy, Hdd?
	{BOOT_CDROM, BOOT_FLOPPY, BOOT_NONE}, //Cdrom, Floppy?
	{BOOT_CDROM, BOOT_HDD, BOOT_NONE}, //Cdrom, Hdd?
	{BOOT_HDD, BOOT_FLOPPY, BOOT_NONE}, //Hdd, Floppy?
	{BOOT_HDD, BOOT_CDROM, BOOT_NONE}, //Hdd, Cdrom?
//Finally single categories (1 active)
	{BOOT_FLOPPY, BOOT_NONE, BOOT_NONE}, //Floppy only?
	{BOOT_CDROM, BOOT_NONE, BOOT_NONE}, //CDROM only?
	{BOOT_HDD, BOOT_NONE, BOOT_NONE} //HDD only?
};

//Boot order (string representation)
char BOOT_ORDER_STRING[15][30] =
{
//Full categories (3 active)
	"FLOPPY, CDROM, HDD",
	"FLOPPY, HDD, CDROM",
	"CDROM, FLOPPY, HDD",
	"CDROM, HDD, FLOPPY",
	"HDD, FLOPPY, CDROM",
	"HDD, CDROM, FLOPPY",
//Advanced categories (2 active)
	"FLOPPY, CDROM",
	"FLOPPY, HDD",
	"CDROM, FLOPPY",
	"CDROM, HDD",
	"HDD, FLOPPY",
	"HDD, CDROM",
//Finally single categories (1 active)
	"FLOPPY ONLY",
	"CDROM ONLY",
	"HDD ONLY",
};

//Try to boot a category (BOOT_FLOPPY, BOOT_HDD, BOOT_CDROM)

int try_boot(byte category)
{
	if (__HW_DISABLED) return 0; //Abort!
	switch (category)
	{
	case BOOT_FLOPPY: //Boot FLOPPY?
		if (CPU_boot(FLOPPY0)) //Try floppy0!
		{
			return 1; //OK: booted!
		}
		else
		{
			return CPU_boot(FLOPPY1); //Try floppy1!
		}
	case BOOT_HDD: //Boot HDD?
		if (CPU_boot(HDD0)) //Try hdd0!
		{
			return 1; //OK: booted!
		}
		else
		{
			return CPU_boot(HDD1); //Try hdd1!
		}
	case BOOT_CDROM: //Boot CDROM?
		if (CPU_boot(CDROM0)) //Try cdrom0!
		{
			return 1; //OK: booted!
		}
		else
		{
			return CPU_boot(CDROM1); //Try cdrom1!
		}
	case BOOT_NONE: //No device?
		break; //Don't boot!
	default: //Default?
		break; //Don't boot!
	}
	return 0; //Not booted!
}

/*

boot_system: boots using BIOS boot order!
returns: TRUE on booted, FALSE on no bootable disk found.

*/

int boot_system()
{
	if (__HW_DISABLED) return 0; //Abort!
	int c;
	for (c=0; c<3; c++) //Try 3 boot devices!
	{
		if (try_boot(BOOT_ORDER[BIOS_Settings.bootorder][c])) //Try boot using currently specified boot order!
		{
			return 1; //Booted!
		}
	}
	return 0; //Not booted at all!
}

/*

Basic BIOS Keyboard support!

*/

void BIOS_writeKBDCMD(byte cmd)
{
	if (__HW_DISABLED) return; //Abort!
	write_8042(0x60,cmd); //Write the command directly to the controller!
}

extern byte force8042; //Force 8042 style handling?

void BIOSKeyboardInit() //BIOS part of keyboard initialisation!
{
	if (__HW_DISABLED) return; //Abort!
	if (is_XT) return;
	byte result; //For holding the result from the hardware!
	force8042 = 1; //We're forcing 8042 style init!

	for (;(PORT_IN_B(0x64) & 0x2);) //Wait for output of data?
	{
		update8042(KEYBOARD_DEFAULTTIMEOUT); //Update the keyboard when allowed!
		updatePS2Keyboard(KEYBOARD_DEFAULTTIMEOUT); //Update the keyboard when allowed!
	}

	BIOS_writeKBDCMD(0xED); //Set/reset status indicators!

	for (;(PORT_IN_B(0x64) & 0x2);) //Wait for output of data?
	{
		update8042(KEYBOARD_DEFAULTTIMEOUT); //Update the keyboard when allowed!
		updatePS2Keyboard(KEYBOARD_DEFAULTTIMEOUT); //Update the keyboard when allowed!
	}

	for (;!(PORT_IN_B(0x64) & 0x1);) //Wait for input data?
	{
		updatePS2Keyboard(KEYBOARD_DEFAULTTIMEOUT); //Update the keyboard when allowed!
		update8042(KEYBOARD_DEFAULTTIMEOUT); //Update the keyboard when allowed!
	}

	result = PORT_IN_B(0x60); //Check the result!
	if (result!=0xFA) //NAC?
	{
		raiseError("Keyboard BIOS initialisation","Set/reset status indication command result: %02X",result);
	}

	write_8042(0x60,0x02); //Turn on NUM LOCK led!

	for (;(PORT_IN_B(0x64) & 0x2);) //Wait for output of data?
	{
		update8042(KEYBOARD_DEFAULTTIMEOUT); //Update the keyboard when allowed!
		updatePS2Keyboard(KEYBOARD_DEFAULTTIMEOUT); //Update the keyboard when allowed!
	}

	for (;!(PORT_IN_B(0x64) & 0x1);) //Wait for input data?
	{
		updatePS2Keyboard(KEYBOARD_DEFAULTTIMEOUT); //Update the keyboard when allowed!
		update8042(KEYBOARD_DEFAULTTIMEOUT); //Update the keyboard when allowed!
	}

	if (!(PORT_IN_B(0x64)&0x1)) //No input data?
	{
		raiseError("Keyboard BIOS initialisation","No turn on NUM lock led result!");
	}
	result = PORT_IN_B(0x60); //Must be 0xFA!
	if (result!=0xFA) //Error?
	{
		raiseError("Keyboard BIOS initialisation","Couldn't turn on Num Lock LED! Result: %02X",result);
	}

	PORT_OUT_B(0x64, 0xAE); //Enable first PS/2 port!

	for (;(PORT_IN_B(0x64) & 0x2);) //Wait for output of data?
	{
		update8042(KEYBOARD_DEFAULTTIMEOUT); //Update the keyboard when allowed!
		updatePS2Keyboard(KEYBOARD_DEFAULTTIMEOUT); //Update the keyboard when allowed!
	}

	BIOS_writeKBDCMD(0xF4); //Enable scanning!

	for (;(PORT_IN_B(0x64) & 0x2);) //Wait for output of data?
	{
		update8042(KEYBOARD_DEFAULTTIMEOUT); //Update the keyboard when allowed!
		updatePS2Keyboard(KEYBOARD_DEFAULTTIMEOUT); //Update the keyboard when allowed!
	}

	PORT_OUT_B(0x64, 0x20); //Read PS2ControllerConfigurationByte!
	for (;(PORT_IN_B(0x64) & 0x2);) //Wait for output of data?
	{
		update8042(KEYBOARD_DEFAULTTIMEOUT); //Update the keyboard when allowed!
		updatePS2Keyboard(KEYBOARD_DEFAULTTIMEOUT); //Update the keyboard when allowed!
	}


	byte PS2ControllerConfigurationByte;
	PS2ControllerConfigurationByte = PORT_IN_B(0x60); //Read result!

	PS2ControllerConfigurationByte |= 1; //Enable our interrupt!
	PORT_OUT_B(0x64, 0x60); //Write PS2ControllerConfigurationByte!
	for (;(PORT_IN_B(0x64) & 0x2);) //Wait for output of data?
	{
		update8042(KEYBOARD_DEFAULTTIMEOUT); //Update the keyboard when allowed!
		updatePS2Keyboard(KEYBOARD_DEFAULTTIMEOUT); //Update the keyboard when allowed!
	}
	PORT_OUT_B(0x60, PS2ControllerConfigurationByte); //Write the new configuration byte!
	for (;(PORT_IN_B(0x64) & 0x2);) //Wait for output of data?
	{
		update8042(KEYBOARD_DEFAULTTIMEOUT); //Update the keyboard when allowed!
		updatePS2Keyboard(KEYBOARD_DEFAULTTIMEOUT); //Update the keyboard when allowed!
	}
	force8042 = 0; //Disable 8042 style init!
}
