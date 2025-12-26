#ifndef __VIRTUAL_BUTTONS_H__
#define __VIRTUAL_BUTTONS_H__

#include "corelib_mainloop.h"
#include <SDL3/SDL.h>

// Button IDs matching the Key enum
typedef enum {
  VBUTTON_NONE = 0,
  VBUTTON_UP = keyUp,
  VBUTTON_DOWN = keyDown,
  VBUTTON_LEFT = keyLeft,
  VBUTTON_RIGHT = keyRight,
  VBUTTON_A = keyEdit,
  VBUTTON_B = keyOpt,
  VBUTTON_START = keyPlay,
  VBUTTON_SELECT = keyShift,
} VirtualButton;

// Button region structure
typedef struct {
  float x, y;           // Center position (normalized 0.0-1.0)
  float radius;         // Radius for circular buttons (normalized)
  VirtualButton button; // Button ID
  int isPressed;        // Current press state
} VirtualButtonRegion;

// Virtual buttons system state
struct VirtualButtonsState {
  VirtualButtonRegion *regions;
  int numRegions;
  int screenW;
  int screenH;
  int enabled;
};
typedef struct VirtualButtonsState VirtualButtonsState;

// Global virtual buttons state for use across the application (without modifying existing API)
VirtualButtonsState *getVirtualButtonsState();

// Initialize virtual buttons system
void virtualButtonsInit(int screenW, int screenH);

// Cleanup virtual buttons system
void virtualButtonsCleanup();

// Handle touch event (normalized coordinates 0.0-1.0)
// fingerId: SDL_FingerID to track which finger is pressing the button
VirtualButton virtualButtonsHandleTouch(float x, float y, int isDown, SDL_FingerID fingerId);

// Get button state
int virtualButtonsIsPressed(VirtualButton button);

// Enable/disable virtual buttons
void virtualButtonsSetEnabled(int enabled);

// Get button regions for rendering (exposed for graphics system)
VirtualButtonRegion* virtualButtonsGetRegions();

int virtualButtonsGetNumRegions();

#endif

