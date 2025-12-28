// SDL3 haptic implementation for non-iOS platforms (Android, etc.)
// For iOS, see haptic_feedback_ios.m

#if !defined(__APPLE__) || !TARGET_OS_IPHONE

#include "haptic_feedback.h"
#include <SDL3/SDL.h>

// Haptic device - managed by this module
static SDL_Haptic *hapticDevice = NULL;

// Haptic feedback parameters
#define HAPTIC_STRENGTH 0.4f     // 40% intensity - subtle but noticeable
#define HAPTIC_DURATION_MS 40    // 40ms - brief tactile pulse

void hapticInit(void) {
    // Initialize haptic feedback using SDL3
    int numHaptics = 0;
    SDL_HapticID *haptics = SDL_GetHaptics(&numHaptics);
    if (haptics && numHaptics > 0) {
        hapticDevice = SDL_OpenHaptic(haptics[0]);
        if (hapticDevice) {
            if (SDL_InitHapticRumble(hapticDevice)) {
                SDL_Log("Haptic feedback initialized successfully");
            } else {
                SDL_Log("Failed to initialize haptic rumble: %s", SDL_GetError());
                SDL_CloseHaptic(hapticDevice);
                hapticDevice = NULL;
            }
        }
        SDL_free(haptics);
    } else {
        SDL_Log("No haptic devices found - vibration feedback disabled");
    }
}

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

void hapticCleanup(void) {
    if (hapticDevice) {
        SDL_CloseHaptic(hapticDevice);
        hapticDevice = NULL;
    }
}

#endif // !iOS
