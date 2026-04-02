#include "corelib_mainloop.h"
#include <stdint.h>
#include <SDL3/SDL.h>

#include "corelib_gfx.h"
#include "corelib_input.h"
#include "corelib_keymap.h"
#include "virtual_buttons.h"
#include "haptic_feedback.h"
#include "../../src/corelib/corelib_assets.h"

#define FPS 60

#ifdef GAMEPAD_SUPPORT
static SDL_Gamepad* gamepad = NULL;

static void initGamepad(void) {
#ifdef TOUCH_INPUT
  virtualButtonsSetEnabled(1);
#endif
  int count = 0;
  SDL_JoystickID* gamepads = SDL_GetGamepads(&count);
  if (gamepads) {
    for (int i = 0; i < count; i++) {
      gamepad = SDL_OpenGamepad(gamepads[i]);
      if (gamepad) {
#ifdef TOUCH_INPUT
        virtualButtonsSetEnabled(0);
#endif
        break;
      }
    }
    SDL_free(gamepads);
  }
}
#endif

#ifdef TOUCH_INPUT
// Button index mapping for gfxSetButtonPressed
// Order: 0=Up, 1=Down, 2=Left, 3=Right, 4=Edit(A), 5=Opt(B), 6=Play(Start), 7=Shift(Select)
static int vbuttonToIndex(VirtualButton button) {
  switch (button) {
    case VBUTTON_UP: return 0;
    case VBUTTON_DOWN: return 1;
    case VBUTTON_LEFT: return 2;
    case VBUTTON_RIGHT: return 3;
    case VBUTTON_A: return 4;
    case VBUTTON_B: return 5;
    case VBUTTON_START: return 6;
    case VBUTTON_SELECT: return 7;
    default: return -1;
  }
}
#endif

void mainLoopRun(void (*draw)(void), void (*onEvent)(MainLoopEventData eventData)) {
  uint32_t delay = 1000 / FPS;
  uint32_t start;
  uint32_t busytime = 0;
  SDL_Event event;
  int menu = 0;
  MainLoopEventData eventData;

#ifdef MOBILE_LIFECYCLE
  int isBackgrounded = 0;
  int wakeRedrawFrames = 0;
#endif

  // Initialize bundled assets
  assetsInit();

#ifdef TOUCH_INPUT
  typedef struct {
    SDL_FingerID fingerId;
    int buttonIndex;
  } FingerButton;

  FingerButton activeFingers[10] = {0};
  int numActiveFingers = 0;
#endif

#ifdef GAMEPAD_SUPPORT
  initGamepad();
#endif

  while (1) {
    start = SDL_GetTicks();

    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT || (event.type == SDL_EVENT_KEY_DOWN &&
          (menu && event.key.key == BTN_X))) {
        eventData.type = eventExit;
        eventData.data.value = 0;
        onEvent(eventData);
#ifdef GAMEPAD_SUPPORT
        if (gamepad) {
          SDL_CloseGamepad(gamepad);
          gamepad = NULL;
        }
#endif
        return;
      }
#ifdef MOBILE_LIFECYCLE
      else if (event.type == SDL_EVENT_TERMINATING) {
        eventData.type = eventExit;
        eventData.data.value = 0;
        onEvent(eventData);
#ifdef GAMEPAD_SUPPORT
        if (gamepad) {
          SDL_CloseGamepad(gamepad);
          gamepad = NULL;
        }
#endif
        return;
      }
      else if (event.type == SDL_EVENT_DID_ENTER_BACKGROUND) {
        isBackgrounded = 1;
        eventData.type = eventSleep;
        eventData.data.value = 0;
        onEvent(eventData);
      }
      else if (event.type == SDL_EVENT_WILL_ENTER_FOREGROUND) {
        isBackgrounded = 0;
        eventData.type = eventWake;
        eventData.data.value = 0;
        onEvent(eventData);
        wakeRedrawFrames = FPS;
#ifdef GAMEPAD_SUPPORT
        if (gamepad && !SDL_GamepadConnected(gamepad)) {
          SDL_CloseGamepad(gamepad);
          gamepad = NULL;
        }
        if (!gamepad) {
          initGamepad();
        }
#endif
      }
#endif
      else if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
        if (event.key.key == BTN_MENU) {
          menu = event.type == SDL_EVENT_KEY_DOWN;
        } else {
          eventData.type = event.type == SDL_EVENT_KEY_DOWN ? eventKeyDown : eventKeyUp;
          eventData.data.input = (InputCode){inputKeyboard, event.key.key};
          onEvent(eventData);
        }
      }
#ifdef GAMEPAD_SUPPORT
      if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN || event.type == SDL_EVENT_GAMEPAD_BUTTON_UP) {
        eventData.type = event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN ? eventKeyDown : eventKeyUp;
        eventData.data.input = (InputCode){inputGamepad, event.gbutton.button};
        onEvent(eventData);
      }
      else if (event.type == SDL_EVENT_GAMEPAD_ADDED) {
        if (!gamepad) {
          gamepad = SDL_OpenGamepad(event.gdevice.which);
#ifdef TOUCH_INPUT
          if (gamepad) {
            virtualButtonsSetEnabled(0);
          }
#endif
        }
      }
      else if (event.type == SDL_EVENT_GAMEPAD_REMOVED) {
        if (gamepad && event.gdevice.which == SDL_GetGamepadID(gamepad)) {
          SDL_CloseGamepad(gamepad);
          gamepad = NULL;
#ifdef TOUCH_INPUT
          virtualButtonsSetEnabled(1);
#endif
        }
      }
#endif
#ifdef TOUCH_INPUT
      if (event.type == SDL_EVENT_FINGER_DOWN) {
        float x = event.tfinger.x;
        float y = event.tfinger.y;

        VirtualButton button = virtualButtonsHandleTouch(x, y, 1, event.tfinger.fingerID);
        if (button != VBUTTON_NONE && numActiveFingers < 10) {
          int btnIdx = vbuttonToIndex(button);
          activeFingers[numActiveFingers].fingerId = event.tfinger.fingerID;
          activeFingers[numActiveFingers].buttonIndex = btnIdx;
          numActiveFingers++;
          if (btnIdx >= 0) gfxSetButtonPressed(btnIdx, 1);
          hapticTriggerButtonPress();
          eventData.type = eventKeyDown;
          eventData.data.input = (InputCode){inputLogical, (int32_t)button};
          onEvent(eventData);
        }
      }
      else if (event.type == SDL_EVENT_FINGER_UP) {
        virtualButtonsHandleTouch(event.tfinger.x, event.tfinger.y, -1, event.tfinger.fingerID);
        for (int i = 0; i < numActiveFingers; i++) {
          if (activeFingers[i].fingerId == event.tfinger.fingerID) {
            if (activeFingers[i].buttonIndex >= 0) {
              gfxSetButtonPressed(activeFingers[i].buttonIndex, 0);
            }
            // Look up the original button key from the region index
            VirtualButtonRegion *regions = virtualButtonsGetRegions();
            int btnIdx = activeFingers[i].buttonIndex;
            int32_t key = 0;
            if (btnIdx >= 0 && regions) {
              key = (int32_t)regions[btnIdx].button;
            }
            eventData.type = eventKeyUp;
            eventData.data.input = (InputCode){inputLogical, key};
            onEvent(eventData);
            for (int j = i; j < numActiveFingers - 1; j++) {
              activeFingers[j] = activeFingers[j + 1];
            }
            numActiveFingers--;
            break;
          }
        }
      }
      else if (event.type == SDL_EVENT_FINGER_MOTION) {
        virtualButtonsHandleTouch(event.tfinger.x, event.tfinger.y, 0, event.tfinger.fingerID);
      }
#endif
    }

#ifdef MOBILE_LIFECYCLE
    if (wakeRedrawFrames > 0) {
      eventData.type = eventFullRedraw;
      eventData.data.value = 0;
      onEvent(eventData);
      wakeRedrawFrames--;
    }
#endif

    eventData.type = eventTick;
    eventData.data.value = 0;
    onEvent(eventData);

#ifdef MOBILE_LIFECYCLE
    if (!isBackgrounded) {
#endif
      draw();
      gfxUpdateScreen();
#ifdef MOBILE_LIFECYCLE
    }
#endif

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
