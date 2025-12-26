#include "haptic_feedback.h"
#include <SDL3/SDL.h>

// External reference to haptic device (defined in corelib_gfx.c)
extern SDL_Haptic *hapticDevice;

// Haptic feedback parameters
#define HAPTIC_STRENGTH 0.4f     // 40% intensity - subtle but noticeable
#define HAPTIC_DURATION_MS 40    // 40ms - brief tactile pulse

void hapticTriggerButtonPress(void) {
    if (!hapticDevice) {
        return; // Gracefully do nothing if haptics unavailable
    }

    // Play brief rumble effect
    if (!SDL_PlayHapticRumble(hapticDevice, HAPTIC_STRENGTH, HAPTIC_DURATION_MS)) {
        // Vibration failed - log once but don't spam logs
        static int errorLogged = 0;
        if (!errorLogged) {
            SDL_Log("Haptic rumble failed: %s", SDL_GetError());
            errorLogged = 1;
        }
    }
}
