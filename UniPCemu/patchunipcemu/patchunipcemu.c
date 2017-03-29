#include <stdio.h> //I/O support!
#include <windows.h>

// Our application entry point.
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
  FILE *f;
  unsigned long int offset;
  WORD subsystem = 2; //The Windows GUI application subsystem!
  //First, 32-bit version!
  f = fopen("UniPCemu_x86.exe","rb+");
  if (!f) goto try64; //Abort if not found!
  fseek(f,0x3C,SEEK_SET); //Goto offset of our PE header location!
  if (fread(&offset,1,4,f)!=4) //Read the offset we need!
  {
    fclose(f);
    goto try64; //Abort!
  }
  fseek(f,offset+0x5C,SEEK_SET); //Goto offset of our subsystem!
  fwrite(&subsystem,1,2,f);
  fclose(f); //Finished!

  //Second, 64-bit version!
  try64:
  f = fopen("UniPCemu_x64.exe","rb+");
  if (!f) return 0; //Abort if not found!
  fseek(f,0x3C,SEEK_SET); //Goto offset of our PE header location!
  if (fread(&offset,1,4,f)!=4) //Read the offset we need!
  {
    fclose(f);
    return 0; //Abort!
  }
  fseek(f,offset+0x5C,SEEK_SET); //Goto offset of our subsystem!
  fwrite(&subsystem,1,2,f);
  fclose(f); //Finished!

  return 0; //Finished!
}
