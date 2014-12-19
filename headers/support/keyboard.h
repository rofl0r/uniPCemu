#ifndef KEYBOARD_H
#define KEYBOARD_H

void onKeyPress(char *key); //On key press/hold!
void onKeyRelease(char *key); //On key release!
void ReleaseKeys(); //Force release all normal keys (excluding ctrl,alt&shift) currently pressed!

#endif