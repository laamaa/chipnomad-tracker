#include "virtual_buttons.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define MIN_BUTTON_SIZE_PX 44.0f  // iOS minimum touch target
#define BUTTON_PADDING 0.02f      // Padding between buttons (normalized)

static VirtualButtonsState vbuttons;

// Calculate distance between two points
static float distance(float x1, float y1, float x2, float y2) {
  float dx = x2 - x1;
  float dy = y2 - y1;
  return sqrtf(dx * dx + dy * dy);
}

// Check if point is in circular button region
static int isInCircle(float px, float py, float cx, float cy, float radius) {
  return distance(px, py, cx, cy) <= radius;
}


void virtualButtonsInit(int screenW, int screenH) {
  vbuttons.screenW = screenW;
  vbuttons.screenH = screenH;
  vbuttons.enabled = 1;
  
  // Allocate regions: D-pad (4 directions: Up, Down, Left, Right), A, B, Start, Select = 8 regions
  vbuttons.numRegions = 8;
  vbuttons.regions = (VirtualButtonRegion *)calloc(vbuttons.numRegions, sizeof(VirtualButtonRegion));
  
  if (!vbuttons.regions) {
    vbuttons.numRegions = 0;
    return;
  }
  
  // Calculate button size based on screen dimensions
  // Use minimum of width/height to ensure buttons fit on screen
  float minDimension = (float)(screenW < screenH ? screenW : screenH);
  float buttonSizePx = fmaxf(MIN_BUTTON_SIZE_PX, minDimension * 0.08f); // 8% of smaller dimension
  float buttonSize = buttonSizePx / minDimension; // Normalized size
  
  // Calculate button area (below 480px backBuffer)
  // Button area starts at normalized y = 480 / windowHeight
  const float BACKBUFFER_H = 480.0f;
  float buttonAreaStartY = BACKBUFFER_H / (float)screenH;
  float buttonAreaHeight = 1.0f - buttonAreaStartY;
  float buttonAreaCenterY = buttonAreaStartY + buttonAreaHeight * 0.5f;
  
  int idx = 0;
  
  // Game Boy-style layout in bottom area
  // D-pad on left side, centered vertically in button area
  float dPadX = 0.25f;  // 15% from left edge
  float dPadY = buttonAreaCenterY;  // Centered in button area
  float dPadSize = buttonSize * 1; // D-pad buttons are slightly larger
  float dPadSpacing = dPadSize; // Spacing between directions (creates proper cross shape)
  
  // Up
  vbuttons.regions[idx].x = dPadX;
  vbuttons.regions[idx].y = dPadY - dPadSpacing;
  vbuttons.regions[idx].radius = dPadSize;
  vbuttons.regions[idx].button = VBUTTON_UP;
  vbuttons.regions[idx].isPressed = 0;
  idx++;
  
  // Down
  vbuttons.regions[idx].x = dPadX;
  vbuttons.regions[idx].y = dPadY + dPadSpacing;
  vbuttons.regions[idx].radius = dPadSize;
  vbuttons.regions[idx].button = VBUTTON_DOWN;
  vbuttons.regions[idx].isPressed = 0;
  idx++;
  
  // Left
  vbuttons.regions[idx].x = dPadX - dPadSpacing*1.3f;
  vbuttons.regions[idx].y = dPadY;
  vbuttons.regions[idx].radius = dPadSize;
  vbuttons.regions[idx].button = VBUTTON_LEFT;
  vbuttons.regions[idx].isPressed = 0;
  idx++;
  
  // Right
  vbuttons.regions[idx].x = dPadX + dPadSpacing*1.3f;
  vbuttons.regions[idx].y = dPadY;
  vbuttons.regions[idx].radius = dPadSize;
  vbuttons.regions[idx].button = VBUTTON_RIGHT;
  vbuttons.regions[idx].isPressed = 0;
  idx++;
  
  // A button on right side (upper position in button area)
  const float aButtonX = 0.85f;  // 85% from left edge
  const float aButtonY = buttonAreaCenterY - buttonSize/2;  // Upper third of button area
  vbuttons.regions[idx].x = aButtonX;
  vbuttons.regions[idx].y = aButtonY;
  vbuttons.regions[idx].radius = buttonSize;
  vbuttons.regions[idx].button = VBUTTON_A;
  vbuttons.regions[idx].isPressed = 0;
  idx++;
  
  // B button on right side (lower position in button area, below A)
  const float bButtonX = 0.65f;  // 85% from left edge
  const float bButtonY = buttonAreaCenterY;  // Lower third of button area
  vbuttons.regions[idx].x = bButtonX;
  vbuttons.regions[idx].y = bButtonY;
  vbuttons.regions[idx].radius = buttonSize;
  vbuttons.regions[idx].button = VBUTTON_B;
  vbuttons.regions[idx].isPressed = 0;
  idx++;
  
  // Start button at bottom center-right
  const float startX = 0.6f;    // Slightly right of center
  const float startY = buttonAreaStartY + buttonAreaHeight * 0.9f;  // Near bottom of button area
  vbuttons.regions[idx].x = startX;
  vbuttons.regions[idx].y = startY;
  vbuttons.regions[idx].radius = buttonSize; // Slightly smaller
  vbuttons.regions[idx].button = VBUTTON_START;
  vbuttons.regions[idx].isPressed = 0;
  idx++;
  
  // Select button at bottom center-left
  const float selectX = 0.4f;  // Slightly left of center
  const float selectY = buttonAreaStartY + buttonAreaHeight * 0.9f;  // Same height as Start
  vbuttons.regions[idx].x = selectX;
  vbuttons.regions[idx].y = selectY;
  vbuttons.regions[idx].radius = buttonSize; // Slightly smaller
  vbuttons.regions[idx].button = VBUTTON_SELECT;
  vbuttons.regions[idx].isPressed = 0;
}

void virtualButtonsCleanup() {
  if (vbuttons.regions) {
    free(vbuttons.regions);
    vbuttons.regions = NULL;
  }
  vbuttons.numRegions = 0;
}

VirtualButton virtualButtonsHandleTouch(const float x, const float y, const int isDown) {
  if (!vbuttons.enabled || !vbuttons.regions) {
    return VBUTTON_NONE;
  }
  
  VirtualButton pressedButton = VBUTTON_NONE;
  
  // Check all button regions
  for (int i = 0; i < vbuttons.numRegions; i++) {
    VirtualButtonRegion *region = &vbuttons.regions[i];
    if (isInCircle(x, y, region->x, region->y, region->radius)) {
      pressedButton = region->button;
      region->isPressed = isDown;
      break; // Only one button can be pressed at a time per touch
    }
  }
  
  // Handle button release - clear pressed state for the released button
  if (!isDown && pressedButton != VBUTTON_NONE) {
    for (int i = 0; i < vbuttons.numRegions; i++) {
      if (vbuttons.regions[i].button == pressedButton) {
        vbuttons.regions[i].isPressed = 0;
        break;
      }
    }
  }
  
  return pressedButton;
}

int virtualButtonsIsPressed(VirtualButton button) {
  if (!vbuttons.regions) return 0;
  
  for (int i = 0; i < vbuttons.numRegions; i++) {
    if (vbuttons.regions[i].button == button && vbuttons.regions[i].isPressed) {
      return 1;
    }
  }
  return 0;
}

void virtualButtonsSetEnabled(int enabled) {
  vbuttons.enabled = enabled;
}

// Get button regions for rendering (exposed for graphics system)
VirtualButtonRegion* virtualButtonsGetRegions() {
  return vbuttons.regions;
}

int virtualButtonsGetNumRegions() {
  return vbuttons.numRegions;
}

VirtualButtonsState *getVirtualButtonsState() {
  return &vbuttons;
}


