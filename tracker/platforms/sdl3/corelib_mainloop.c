#include "corelib_mainloop.h"
#include <stdint.h>
#include <SDL3/SDL.h>

#include "corelib_gfx.h"
#include "virtual_buttons.h"
#ifdef MOBILE_BUILD
#include "virtual_buttons.h"
#endif

#define FPS 60

// Keyboard mapping
#if defined(DESKTOP_BUILD) || defined(PORTMASTER_BUILD)
// Desktop and PortMaster mapping
#define BTN_UP          (SDLK_UP)
#define BTN_DOWN        (SDLK_DOWN)
#define BTN_LEFT        (SDLK_LEFT)
#define BTN_RIGHT       (SDLK_RIGHT)
#define BTN_A           (SDLK_X)
#define BTN_B           (SDLK_Z)
#define BTN_X           (SDLK_C)
#define BTN_Y           (SDLK_A)
#define BTN_L1          (SDLK_LSHIFT)
#define BTN_R1          (SDLK_LSHIFT)
#define BTN_L2          0
#define BTN_R2          0
#define BTN_SELECT      (SDLK_LSHIFT)
#define BTN_START       (SDLK_SPACE)
#define BTN_MENU        (SDLK_LCTRL)
#define BTN_VOLUME_UP   0
#define BTN_VOLUME_DOWN 0
#define BTN_POWER       0
#define BTN_EXIT        0
#else
// RG35xx (old) mapping
#define BTN_UP          119
#define BTN_DOWN        115
#define BTN_LEFT        113
#define BTN_RIGHT       100
#define BTN_A           97
#define BTN_B           98
#define BTN_X           120
#define BTN_Y           121
#define BTN_L1          104
#define BTN_R1          108
#define BTN_L2          106
#define BTN_R2          107
#define BTN_SELECT      110
#define BTN_START       109
#define BTN_MENU        117
#define BTN_VOLUME_UP   114
#define BTN_VOLUME_DOWN 116
#define BTN_POWER       0
#define BTN_EXIT        0
#endif

static int decodeKey(int sym) {
  // If a key is not recognized, return zero
  if (sym == BTN_UP) return keyUp;
  if (sym == BTN_DOWN) return keyDown;
  if (sym == BTN_LEFT) return keyLeft;
  if (sym == BTN_RIGHT) return keyRight;
  if (sym == BTN_A) return keyEdit;
  if (sym == BTN_B) return keyOpt;
  if (sym == BTN_START) return keyPlay;
  if (sym == BTN_SELECT || sym == BTN_R1 || sym == BTN_L1) return keyShift;
  if (sym == BTN_VOLUME_UP) return keyVolumeUp;
  if (sym == BTN_VOLUME_DOWN) return keyVolumeDown;
  return 0;
}

void mainLoopRun(void (*draw)(void), void (*onEvent)(enum MainLoopEvent event, int value, void* userdata)) {
  uint32_t delay = 1000 / FPS;
  uint32_t start;
  uint32_t busytime = 0;
  SDL_Event event;
  int menu = 0;
  
  // Track active touches for multi-touch support
  SDL_FingerID activeTouches[10]; // Support up to 10 simultaneous touches
  enum Key touchKeys[10]; // Map touch IDs to keys
  int numActiveTouches = 0;

  while (1) {
    start = SDL_GetTicks();

    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT || (event.type == SDL_EVENT_KEY_DOWN && (
        (event.key.key == BTN_POWER) ||
        (event.key.key == BTN_EXIT) ||
        (menu && event.key.key == BTN_X)))) {
        onEvent(eventExit, 0, NULL);
        return;
      }
      if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
        if (event.key.key == BTN_MENU) {
          menu = event.type == SDL_EVENT_KEY_DOWN;
        } else {
          enum Key key = decodeKey(event.key.key);
          if (key != -1 ) onEvent(event.type == SDL_EVENT_KEY_DOWN ? eventKeyDown : eventKeyUp, key, NULL);
        }
      }

      // Handle touch events
      if (event.type == SDL_EVENT_FINGER_DOWN || event.type == SDL_EVENT_FINGER_UP || event.type == SDL_EVENT_FINGER_MOTION) {
        SDL_FingerID fingerId = event.tfinger.fingerID;
        float x = event.tfinger.x; // Normalized coordinates (0.0-1.0)
        float y = event.tfinger.y;
        int isDown = (event.type == SDL_EVENT_FINGER_DOWN);
        int isUp = (event.type == SDL_EVENT_FINGER_UP);
        
        // Handle touch
        const VirtualButton button = virtualButtonsHandleTouch(x, y, isDown);
        
        if (button != VBUTTON_NONE) {
          enum Key key = (enum Key)button;
          
          if (isDown) {
            // Find or add touch tracking
            int touchIndex = -1;
            for (int i = 0; i < numActiveTouches; i++) {
              if (activeTouches[i] == fingerId) {
                touchIndex = i;
                break;
              }
            }
            if (touchIndex == -1 && numActiveTouches < 10) {
              touchIndex = numActiveTouches++;
              activeTouches[touchIndex] = fingerId;
            }
            if (touchIndex >= 0) {
              touchKeys[touchIndex] = key;
              onEvent(eventKeyDown, key, NULL);
            }
          } else if (isUp) {
            // Find and release touch
            for (int i = 0; i < numActiveTouches; i++) {
              if (activeTouches[i] == fingerId) {
                onEvent(eventKeyUp, touchKeys[i], NULL);
                // Remove from active touches
                for (int j = i; j < numActiveTouches - 1; j++) {
                  activeTouches[j] = activeTouches[j + 1];
                  touchKeys[j] = touchKeys[j + 1];
                }
                numActiveTouches--;
                break;
              }
            }
          }
        }
      }

    }
    onEvent(eventTick, 0, NULL);

    draw();
    gfxUpdateScreen();

    busytime = SDL_GetTicks() - start;
    if (delay > busytime) {
      SDL_Delay(delay - busytime);
    }
  }
}

void mainLoopDelay(int ms) {
  SDL_Delay(ms);
}

void mainLoopQuit(void) {
  SDL_Quit();
}

void mainLoopTriggerQuit(void) {
  SDL_Event quitEvent;
  quitEvent.type = SDL_EVENT_QUIT;
  SDL_PushEvent(&quitEvent);
}
