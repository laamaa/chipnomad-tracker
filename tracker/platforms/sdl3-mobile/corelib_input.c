#include "corelib_input.h"
#include "common.h"
#include <SDL3/SDL.h>
#include <string.h>
#include <locale.h>
#include <ctype.h>
#include "corelib_keymap.h"

// Keyboard layout detection (only used for first-launch default key mapping)
typedef enum {
  LAYOUT_QWERTY = 0,
  LAYOUT_QWERTZ = 1
} KeyboardLayout;

static KeyboardLayout detectKeyboardLayout(void) {
  const char* locale = NULL;

  setlocale(LC_CTYPE, "");
  locale = setlocale(LC_CTYPE, NULL);
  if (!locale || strcmp(locale, "C") == 0 || strcmp(locale, "POSIX") == 0) {
    locale = getenv("LC_ALL");
    if (!locale) locale = getenv("LC_CTYPE");
    if (!locale) locale = getenv("LANG");
  }

  if (locale) {
    char localeLower[32];
    strncpy(localeLower, locale, sizeof(localeLower) - 1);
    localeLower[sizeof(localeLower) - 1] = '\0';
    for (int i = 0; localeLower[i]; i++) {
      localeLower[i] = tolower(localeLower[i]);
    }

    // QWERTZ regions
    if (strstr(localeLower, "de_") || strstr(localeLower, "de.") ||
        strstr(localeLower, "_at") || strstr(localeLower, ".at") ||
        strstr(localeLower, "_ch") || strstr(localeLower, ".ch") ||
        strstr(localeLower, "cs_") || strstr(localeLower, "cs.") ||
        strstr(localeLower, "_cz") || strstr(localeLower, ".cz") ||
        strstr(localeLower, "sk_") || strstr(localeLower, "sk.") ||
        strstr(localeLower, "_sk") || strstr(localeLower, ".sk") ||
        strstr(localeLower, "pl_") || strstr(localeLower, "pl.") ||
        strstr(localeLower, "_pl") || strstr(localeLower, ".pl") ||
        strstr(localeLower, "hu_") || strstr(localeLower, "hu.") ||
        strstr(localeLower, "_hu") || strstr(localeLower, ".hu")) {
      return LAYOUT_QWERTZ;
    }
  }

  return LAYOUT_QWERTY;
}

void inputInitDefaultKeyMapping(void) {
  KeyboardLayout layout = detectKeyboardLayout();

  // Keyboard mappings (slot 0)
  appSettings.keyMapping.keyUp[0] = (InputCode){inputKeyboard, BTN_UP};
  appSettings.keyMapping.keyDown[0] = (InputCode){inputKeyboard, BTN_DOWN};
  appSettings.keyMapping.keyLeft[0] = (InputCode){inputKeyboard, BTN_LEFT};
  appSettings.keyMapping.keyRight[0] = (InputCode){inputKeyboard, BTN_RIGHT};
  appSettings.keyMapping.keyOpt[0] = (InputCode){inputKeyboard, (layout == LAYOUT_QWERTZ) ? SDLK_Y : BTN_B};
  appSettings.keyMapping.keyPlay[0] = (InputCode){inputKeyboard, BTN_START};
  appSettings.keyMapping.keyShift[0] = (InputCode){inputKeyboard, BTN_SELECT};
  appSettings.keyMapping.keyEdit[0] = (InputCode){inputKeyboard, BTN_A};

  // Gamepad mappings (slot 1)
  appSettings.keyMapping.keyUp[1] = (InputCode){inputGamepad, SDL_GAMEPAD_BUTTON_DPAD_UP};
  appSettings.keyMapping.keyDown[1] = (InputCode){inputGamepad, SDL_GAMEPAD_BUTTON_DPAD_DOWN};
  appSettings.keyMapping.keyLeft[1] = (InputCode){inputGamepad, SDL_GAMEPAD_BUTTON_DPAD_LEFT};
  appSettings.keyMapping.keyRight[1] = (InputCode){inputGamepad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT};
  appSettings.keyMapping.keyEdit[1] = (InputCode){inputGamepad, SDL_GAMEPAD_BUTTON_SOUTH};
  appSettings.keyMapping.keyOpt[1] = (InputCode){inputGamepad, SDL_GAMEPAD_BUTTON_EAST};
  appSettings.keyMapping.keyPlay[1] = (InputCode){inputGamepad, SDL_GAMEPAD_BUTTON_START};
  appSettings.keyMapping.keyShift[1] = (InputCode){inputGamepad, SDL_GAMEPAD_BUTTON_BACK};

  // Slot 2 empty
  appSettings.keyMapping.keyUp[2] = (InputCode){inputNone, 0};
  appSettings.keyMapping.keyDown[2] = (InputCode){inputNone, 0};
  appSettings.keyMapping.keyLeft[2] = (InputCode){inputNone, 0};
  appSettings.keyMapping.keyRight[2] = (InputCode){inputNone, 0};
  appSettings.keyMapping.keyEdit[2] = (InputCode){inputNone, 0};
  appSettings.keyMapping.keyOpt[2] = (InputCode){inputNone, 0};
  appSettings.keyMapping.keyPlay[2] = (InputCode){inputNone, 0};
  appSettings.keyMapping.keyShift[2] = (InputCode){inputNone, 0};
}

const char* inputGetKeyName(InputCode input) {
  if (input.deviceType == inputNone) return "---";

  if (input.deviceType == inputGamepad) {
    switch (input.code) {
      case SDL_GAMEPAD_BUTTON_SOUTH: return "Pad A";
      case SDL_GAMEPAD_BUTTON_EAST: return "Pad B";
      case SDL_GAMEPAD_BUTTON_WEST: return "Pad X";
      case SDL_GAMEPAD_BUTTON_NORTH: return "Pad Y";
      case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: return "Pad L1";
      case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return "Pad R1";
      case SDL_GAMEPAD_BUTTON_BACK: return "PadSel";
      case SDL_GAMEPAD_BUTTON_START: return "PadStrt";
      case SDL_GAMEPAD_BUTTON_DPAD_UP: return "Pad Up";
      case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return "PadDown";
      case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return "PadLeft";
      case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return "PadRght";
      default: return "Pad ?";
    }
  }

  // Keyboard - custom short names for common keys
  switch (input.code) {
    case SDLK_LSHIFT: return "LShift";
    case SDLK_RSHIFT: return "RShift";
    case SDLK_LCTRL: return "LCtrl";
    case SDLK_RCTRL: return "RCtrl";
    case SDLK_LALT: return "LAlt";
    case SDLK_RALT: return "RAlt";
    case SDLK_SPACE: return "Space";
    case SDLK_RETURN: return "Return";
    case SDLK_BACKSPACE: return "BkSpace";
    case SDLK_ESCAPE: return "Escape";
  }

  const char* name = SDL_GetKeyName(input.code);
  if (name && name[0]) return name;

  return "???";
}
