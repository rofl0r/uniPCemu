#ifndef GPU_RENDERER_H
#define GPU_RENDERER_H

void renderScreenFrame(); //Render the screen frame!
int GPU_directRenderer(); //Plot directly 1:1 on-screen!
int GPU_fullRenderer();

/*

THE RENDERER!

*/

void renderHWFrame(); //Render a hardware frame!

uint_32 *get_rowempty(); //Gives an empty row (and clears it)!
void done_GPURenderer(); //Cleanup only!

/*

FPS LIMITER!

*/

void refreshscreen(); //Handler for a screen frame (60 fps) MAXIMUM.

#endif