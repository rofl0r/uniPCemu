#include "headers/types.h" //Basic typedefs!
#include "headers/emu/gpu/gpu_text.h" //Text editing!
#include "headers/emu/gpu/gpu_renderer.h" //Rendering support (for rendering the surface)!
#include "headers/emu/gpu/gpu_framerate.h" //Framerate surface rendering only!
#include "headers/support/log.h" //Logging!
#include "headers/emu/threads.h" //Thread support!
#include "headers/emu/sound.h" //Sound support!

/*

Our error handler!

raiseError: Raises an error!
parameters:
	source: The origin of the error (debugging purposes)
	text: see (s)printf parameters.

*/

byte ERROR_RAISED = 0; //No error raised by default!

extern GPU_TEXTSURFACE *frameratesurface; //The framerate!

void raiseError(char *source, char *text, ...)
{
	char msg[256];
	char result[256]; //Result!
	bzero(msg,sizeof(msg)); //Init!

	va_list args; //Going to contain the list!
	va_start (args, text); //Start list!
	vsprintf (msg, text, args); //Compile list!
	sprintf(result,"Error at %s: %s",source,msg); //Generate full message!
	va_end (args); //Destroy list!

	//Log the error!
	dolog("error","Error: %s",result); //Show the error in the log!

	dolog("error","Terminating threads...");
	termThreads(); //Stop all running threads!
	dolog("error","Stopping audio processing...");
	doneAudio(); //Stop audio processing!
	dolog("error","Terminating SDL...");
	//if (!THREADTEST) SDL_Quit(); //Stop SDL processing!
	dolog("error","Displaying message...");

	GPU_textgotoxy(frameratesurface,0,0); //Goto 0,0!
	GPU_textprintf(frameratesurface,RGB(0xFF,0xFF,0xFF),RGB(0x22,0x22,0x22),result); //Show the message on our debug screen!
	GPU_textrenderer(frameratesurface); //Render the framerate surface on top of all else!
	renderFramerateOnly(); //Render the framerate only!

	dolog("error","Waiting 5 seconds before quitting...");
	ERROR_RAISED = 1; //We've raised an error!
	delay(5000000); //Wait 5 seconds...
	//When we're exiting this thread, the main thread will become active, terminating the software!
	halt(0); //Just in case!
	sleep(); //Wait forever, just in case!
}
