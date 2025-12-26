#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdarg.h>
#include "version.h"
#include "corelib_gfx.h"
#include <string.h>
#include <math.h>

#include "pitch_table_utils.h"
#include "virtual_buttons.h"

#define PRINT_BUFFER_SIZE (256)
#define TEXT_COLS (40)
#define TEXT_ROWS (20)

#define CHAR_X(x) ((x) * fontPixelW + offsetX)
#define CHAR_Y(y) ((y) * fontH + offsetY)

extern uint8_t font12x16[];
extern uint8_t font16x24[];
extern uint8_t font24x36[];
extern uint8_t font32x48[];
extern uint8_t font48x54[];

SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;
SDL_Haptic *hapticDevice = NULL;

static uint32_t fgColor = 0;
static uint32_t bgColor = 0;
static uint32_t cursorColor = 0;
static uint8_t *font = NULL;
static char printBuffer[PRINT_BUFFER_SIZE];
static int screenW;
static int screenH;
static int windowW;        // Actual window width
static int windowH;        // Actual window height
static int fontH;
static int fontW;
static int fontPixelW;
static int offsetX;
static int offsetY;
static int isDirty;

// Fixed backBuffer dimensions
static const int BACKBUFFER_W = 1280;
static const int BACKBUFFER_H = 960;

static SDL_Texture *backBuffer = NULL;
static SDL_Texture *buttonOverlay = NULL;

// Font texture optimization
static SDL_Texture *fontTexture = NULL;
static SDL_FRect charRects[95]; // ASCII 32-126

static char charBuffer[80];

SDL_Window *gfxGetWindow(void) {
    return window;
}

void gfxGetScreenSize(int *w, int *h) {
    // Return logical screen size (backBuffer dimensions)
    if (w) *w = BACKBUFFER_W;
    if (h) *h = BACKBUFFER_H;
}

static void selectFont(void) {
    // Use fixed backBuffer dimensions for font selection
    if (BACKBUFFER_W >= TEXT_COLS * 48 && BACKBUFFER_H >= TEXT_ROWS * 54) {
        font = font48x54;
        fontW = 6;
        fontPixelW = 48;
        fontH = 54;
    } else if (BACKBUFFER_W >= TEXT_COLS * 32 && BACKBUFFER_H >= TEXT_ROWS * 48) {
        font = font32x48;
        fontW = 4;
        fontPixelW = 32;
        fontH = 48;
    } else if (BACKBUFFER_W >= TEXT_COLS * 24 && BACKBUFFER_H >= TEXT_ROWS * 36) {
        font = font24x36;
        fontW = 3;
        fontPixelW = 24;
        fontH = 36;
    } else if (BACKBUFFER_W >= TEXT_COLS * 16 && BACKBUFFER_H >= TEXT_ROWS * 24) {
        font = font16x24;
        fontW = 2;
        fontPixelW = 16;
        fontH = 24;
    } else {
        font = font12x16;
        fontW = 2;
        fontPixelW = 12;
        fontH = 16;
    }
}

static void createFontTexture(void) {
    fontTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, fontPixelW * 95,
                                    fontH);
    SDL_SetTextureBlendMode(fontTexture, SDL_BLENDMODE_BLEND);

    SDL_SetRenderTarget(renderer, fontTexture);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);

    for (int ch = 0; ch < 95; ch++) {
        const int charX = ch * fontPixelW;
        charRects[ch] = (SDL_FRect){(float) charX, 0, (float) fontPixelW, (float) fontH};

        for (int l = 0; l < fontH; l++) {
            for (int c = 0; c < fontW; c++) {
                const uint8_t fontByte = font[ch * fontW * fontH + l * fontW + c];
                const int bitsToDraw = (c == fontW - 1 && fontPixelW % 8 != 0) ? fontPixelW % 8 : 8;
                uint8_t mask = 0x80;

                for (int b = 0; b < bitsToDraw; b++) {
                    if (fontByte & mask) {
                        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                        SDL_RenderPoint(renderer, (float) charX + (float) c * 8 + (float) b, (float) l);
                    }
                    mask >>= 1;
                }
            }
        }
    }

    SDL_SetRenderTarget(renderer, NULL);
}

// Modify all drawing functions to draw to the back buffer:
static void ensureBackBufferTarget(void) {
    SDL_SetRenderTarget(renderer, backBuffer);
}

// Draw a single virtual button
static void gfxDrawVirtualButton(VirtualButtonRegion *region, const int screenWidth, const int screenHeight) {
    if (!region || !renderer) return;

    // Convert normalized coordinates to screen coordinates
    const float centerX = region->x * (float)screenWidth;
    const float centerY = region->y * (float)screenHeight;
    const float radius = region->radius * (float)(screenWidth < screenHeight ? screenWidth : screenHeight);

    // Button colors
    const uint32_t buttonColor = 0x808080; // Gray
    const uint32_t pressedColor = 0x404040; // Darker gray when pressed
    const uint32_t borderColor = 0xFFFFFF; // White border

    const uint32_t currentColor = region->isPressed ? pressedColor : buttonColor;

    SDL_SetRenderTarget(renderer, buttonOverlay);

    // Draw button circle (semi-transparent)
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer,
                           (currentColor & 0xff0000) >> 16,
                           (currentColor & 0xff00) >> 8,
                           currentColor & 0xff,
                           180); // Alpha for semi-transparency

    // Draw filled circle (approximate with filled square - good enough for touch targets)
    SDL_FRect buttonRect = {
        centerX - radius,
        centerY - radius,
        radius * 2.0f,
        radius * 2.0f
    };
    SDL_RenderFillRect(renderer, &buttonRect);

    // Draw border (simple rectangle outline)
    SDL_SetRenderDrawColor(renderer,
                           (borderColor & 0xff0000) >> 16,
                           (borderColor & 0xff00) >> 8,
                           borderColor & 0xff,
                           200);

    // Draw border as 4 lines (top, bottom, left, right)
    float borderWidth = 2.0f;
    SDL_FRect borderRects[4] = {
        {buttonRect.x, buttonRect.y, buttonRect.w, borderWidth}, // top
        {buttonRect.x, buttonRect.y + buttonRect.h - borderWidth, buttonRect.w, borderWidth}, // bottom
        {buttonRect.x, buttonRect.y, borderWidth, buttonRect.h}, // left
        {buttonRect.x + buttonRect.w - borderWidth, buttonRect.y, borderWidth, buttonRect.h} // right
    };
    SDL_RenderFillRects(renderer, borderRects, 4);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

}

// Draw all virtual buttons
static void gfxDrawVirtualButtons() {
    VirtualButtonsState *state = getVirtualButtonsState();
    if (!state || !state->enabled) return;

    // Draw buttons to overlay texture
    SDL_SetRenderTarget(renderer, buttonOverlay);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0,SDL_ALPHA_TRANSPARENT);
    SDL_RenderClear(renderer);

    VirtualButtonRegion *regions = virtualButtonsGetRegions();
    int numRegions = virtualButtonsGetNumRegions();

    for (int i = 0; i < numRegions; i++) {
        gfxDrawVirtualButton(&regions[i], state->screenW, state->screenH);
    }

    // Reset render target
    SDL_SetRenderTarget(renderer, backBuffer);
}

// FIXME: On RG35XX+, the SDL2 seems to be built without haptic
// support enabled, so SDL_INIT_EVERYTHING will fail.
#ifdef PORTMASTER_BUILD
#define SDL_INIT_FLAGS (SDL_INIT_EVERYTHING & ~SDL_INIT_HAPTIC)
#else
#define SDL_INIT_FLAGS (SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_HAPTIC)
#endif

int gfxSetup(int *screenWidth, int *screenHeight) {
    if (SDL_Init(SDL_INIT_FLAGS) == false) {
        SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "SDL3 Initialization Error: %s\n", SDL_GetError());
        return 1;
    }

    // Initialize haptic feedback for mobile devices
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

    sprintf(charBuffer, "%s v%s (%s)", appTitle, appVersion, appBuild);

    // Detect screen resolution if not provided or zero
    if (screenWidth == NULL || screenHeight == NULL || *screenWidth == 0 || *screenHeight == 0)
        {
        // Default to iPhone-like 9:21 aspect ratio
        screenW = 402;
        screenH = 874;

        // Update the pointers if they're not NULL
        if (screenWidth != NULL) *screenWidth = screenW;
        if (screenHeight != NULL) *screenHeight = screenH;
    } else {
        // TODO remove force setting
        screenW = 402;
        screenH = 874;
    }

    // Window flags
    uint32_t windowFlags = SDL_WINDOW_HIGH_PIXEL_DENSITY;

    // Desktop: resizable
    windowFlags |= SDL_WINDOW_RESIZABLE;

    if (!SDL_CreateWindowAndRenderer(charBuffer, screenW, screenH, windowFlags,
                                     &window, &renderer)) {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create window and renderer: %s",
                        SDL_GetError());
        return false;
    }

    // Check for high-DPI display and get actual drawable size
    int drawableW, drawableH;
    SDL_GetWindowSizeInPixels(window, &drawableW, &drawableH);
    if (drawableW != screenW || drawableH != screenH) {
        // High-DPI display detected, use drawable size for window dimensions
        windowW = drawableW;
        windowH = drawableH;
    } else {
        windowW = screenW;
        windowH = screenH;
    }

    // Use the app texture (backBuffer) size for the draw logic
    screenW = BACKBUFFER_W;
    screenH = BACKBUFFER_H;

    selectFont();

    // Center the text window within the 640x480 backBuffer
    const int textWindowW = TEXT_COLS * fontPixelW;
    const int textWindowH = TEXT_ROWS * fontH;
    offsetX = (BACKBUFFER_W - textWindowW) / 2;
    offsetY = (BACKBUFFER_H - textWindowH) / 2;

    createFontTexture();

    // Initialize virtual buttons system with full window dimensions
    virtualButtonsInit(windowW, windowH);

    // Create persistent back buffer at fixed 640x480
    backBuffer = SDL_CreateTexture(renderer,
                                   SDL_PIXELFORMAT_RGBA8888,
                                   SDL_TEXTUREACCESS_TARGET,
                                   BACKBUFFER_W, BACKBUFFER_H);
    if (!backBuffer) {
        SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Failed to create back buffer: %s", SDL_GetError());
        return 1;
    }

    SDL_SetTextureBlendMode(backBuffer,SDL_BLENDMODE_BLEND);

    // Create button overlay texture matching window size
    // Buttons will be drawn in the bottom portion
    buttonOverlay = SDL_CreateTexture(renderer,
                                      SDL_PIXELFORMAT_RGBA8888,
                                      SDL_TEXTUREACCESS_TARGET,
                                      windowW, windowH);
    if (!buttonOverlay) {
        SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Failed to create button overlay: %s", SDL_GetError());
        return 1;
    }

    SDL_SetTextureBlendMode(buttonOverlay,SDL_BLENDMODE_BLEND);

    // Initialize back buffer with background color (default to black if not set yet)
    SDL_SetRenderTarget(renderer, backBuffer);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_SetRenderTarget(renderer, NULL);
    
    // Initialize button overlay as transparent
    SDL_SetRenderTarget(renderer, buttonOverlay);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);
    SDL_SetRenderTarget(renderer, NULL);
    
    isDirty = 1;

    return 0;
}

void gfxCleanup(void) {
    if (backBuffer) SDL_DestroyTexture(backBuffer);
    if (buttonOverlay) SDL_DestroyTexture(buttonOverlay);
    if (fontTexture) SDL_DestroyTexture(fontTexture);
    virtualButtonsCleanup();
    if (hapticDevice) {
        SDL_CloseHaptic(hapticDevice);
        hapticDevice = NULL;
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
}

void gfxSetFgColor(const int rgb) {
    fgColor = rgb;
}

void gfxSetBgColor(const int rgb) {
    bgColor = rgb;
}

void gfxSetCursorColor(const int rgb) {
    cursorColor = rgb;
}

static void setColor(const uint32_t rgb) {
    SDL_SetRenderDrawColor(renderer, (rgb & 0xff0000) >> 16, (rgb & 0xff00) >> 8, rgb & 0xff, 255);
}

void gfxClear(void) {
    ensureBackBufferTarget();
    setColor(bgColor);
    SDL_RenderClear(renderer);
    isDirty = 1;
}

void gfxPoint(const int x, const int y, const uint32_t color) {
    ensureBackBufferTarget();
    setColor(color);
    SDL_RenderPoint(renderer, (float) x, (float) y);
    isDirty = 1;
}

void gfxClearRect(const int x, const int y, const int w, const int h) {
    ensureBackBufferTarget();
    const SDL_FRect rect = {CHAR_X(x), CHAR_Y(y), (float) (w * fontPixelW), (float) (h * fontH)};
    setColor(bgColor);
    SDL_RenderFillRect(renderer, &rect);
    isDirty = 1;
}

void gfxPrint(int x, int y, const char *text) {
    if (text == NULL) return;

    ensureBackBufferTarget();

    int cx = CHAR_X(x);
    int cy = CHAR_Y(y);
    const int len = (int) strlen(text);

    // Draw background rectangles first
    setColor(bgColor);
    for (int i = 0; i < len; i++) {
        if (text[i] == '\r' && text[i + 1] == '\n') {
            i++;
            cx = CHAR_X(x);
            cy += fontH;
            continue;
        }
        SDL_FRect bgRect = {(float) cx, (float) cy, (float) fontPixelW, (float) fontH};
        SDL_RenderFillRect(renderer, &bgRect);
        cx += fontPixelW;
        if (cx >= offsetX + TEXT_COLS * fontPixelW) {
            cx = CHAR_X(x);
            cy += fontH;
        }
    }

    // Draw characters
    cx = CHAR_X(x);
    cy = CHAR_Y(y);
    SDL_SetTextureColorMod(fontTexture, (fgColor >> 16) & 0xFF, (fgColor >> 8) & 0xFF, fgColor & 0xFF);

    for (int i = 0; i < len; i++) {
        const uint8_t C = text[i];
        if (C == '\r' && text[i + 1] == '\n') {
            i++;
            cx = CHAR_X(x);
            cy += fontH;
            if (cy >= offsetY + TEXT_ROWS * fontH) {
                cy = CHAR_Y(y);
            }
            continue;
        }

        if (C >= 32 && C <= 126) {
            SDL_FRect dstRect = {(float) cx, (float) cy, (float) fontPixelW, (float) fontH};
            SDL_RenderTexture(renderer, fontTexture, &charRects[C - 32], &dstRect);
        }

        cx += fontPixelW;
        if (cx >= offsetX + TEXT_COLS * fontPixelW) {
            cx = CHAR_X(x);
            cy += fontH;
        }
    }
    isDirty = 1;
}

void gfxPrintf(const int x, const int y, const char *format, ...) {
    ensureBackBufferTarget();
    va_list args;
    va_start(args, format);
    vsnprintf(printBuffer, 256, format, args);
    va_end(args);
    gfxPrint(x, y, printBuffer);
}

void gfxCursor(int x, int y, int w) {
    ensureBackBufferTarget();
    const SDL_FRect rect = {CHAR_X(x), CHAR_Y(y) + fontH - 1, (float) (w * fontPixelW), 1};
    setColor(cursorColor);
    SDL_RenderFillRect(renderer, &rect);
    isDirty = 1;
}

void gfxRect(const int x, const int y, const int w, const int h) {
    ensureBackBufferTarget();
    const float cx = CHAR_X(x);
    const float cy = CHAR_Y(y);
    const float cw = (float) (w * fontPixelW);
    const float ch = (float) (h * fontH);
    setColor(fgColor);

    const SDL_FRect rects[4] = {
        {cx, cy, cw, 1}, // top
        {cx, cy + ch - 1, cw, 1}, // bottom
        {cx, cy, 1, ch}, // left
        {cx + cw - 1, cy, 1, ch} // right
    };
    SDL_RenderFillRects(renderer, rects, 4);
    isDirty = 1;
}

// Modify gfxUpdateScreen to copy back buffer to screen:
void gfxUpdateScreen(void) {
    if (!isDirty) {
        return;
    }

    // Draw virtual buttons overlay onto its own texture
    gfxDrawVirtualButtons();

    // Set render target to screen (NULL = default render target)
    SDL_SetRenderTarget(renderer, NULL);
    
    // Clear screen with background color
    setColor(bgColor);
    SDL_RenderClear(renderer);
    
    // Calculate horizontal offset to center 640px backBuffer in window
    //float backBufferOffsetX = (windowW - BACKBUFFER_W) / 2.0f;

    // Render back buffer in top portion (centered horizontally)
    const float scale = (float)windowW / (float)BACKBUFFER_W;
    SDL_FRect backBufferDst = {
        //backBufferOffsetX,
        0.0f,
        160.0f,
        (float)windowW,
        (float)BACKBUFFER_H * scale
    };
    SDL_RenderTexture(renderer, backBuffer, NULL, &backBufferDst);
    
    // Render button overlay in bottom portion (if enabled)
    // Buttons are drawn to overlay using full window coordinates, positioned in bottom area
    VirtualButtonsState *state = getVirtualButtonsState();
    if (state && state->enabled) {
        float buttonAreaHeight = (float)(windowH - BACKBUFFER_H);
        if (buttonAreaHeight > 0) {
            // Render only the bottom portion of the button overlay (below 480px)
            SDL_FRect buttonOverlaySrc = {
                0.0f,
                (float)BACKBUFFER_H,  // Source starts at y=480 in overlay texture
                (float)windowW,
                buttonAreaHeight
            };
            SDL_FRect buttonOverlayDst = {
                0.0f,
                (float)BACKBUFFER_H,  // Destination starts at y=480 on screen
                (float)windowW,
                buttonAreaHeight
            };
            SDL_RenderTexture(renderer, buttonOverlay, &buttonOverlaySrc, &buttonOverlayDst);
        }
    }
    
    // Present to screen
    SDL_RenderPresent(renderer);

    isDirty = 0;
}
