#ifndef GPU_FRAMERATE_H
#define GPU_FRAMERATE_H

void updateFramerateBar(); //Update/show the framerate bar if enabled!
void GPU_Framerate_Thread(); //One second has passed thread (called every second!)?
void finish_screen(); //Extra stuff after rendering!
void updateFramerateBar_Thread();
void GPU_FrameRendered(); //A frame has been rendered?
void initFramerate(); //Initialise framerate support!
void doneFramerate(); //Remove the framerate support!
void renderFramerate(); //Render the framerate on the surface of the current rendering!

void renderFramerateOnly(); //Render the framerate only, black background!
void logVGASpeed(); //Log VGA at max speed!

/*

Frameskip support!

*/

void setGPUFrameskip(byte Frameskip);

#endif