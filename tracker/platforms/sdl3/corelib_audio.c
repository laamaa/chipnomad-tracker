#include "corelib_audio.h"
#include <SDL3/SDL.h>
#include <stdio.h>

static SDL_AudioStream* audioStream = NULL;

static void sdlAudioCallback(void* userdata, SDL_AudioStream* stream, int additional_amount, int total_amount) {
  AudioCallback* callback = userdata;

  // Calculate how many samples we need
  int samples_needed = additional_amount / sizeof(int16_t) / 2; // Stereo samples

  if (samples_needed <= 0) return;

  // Allocate temporary buffer
  int16_t* buffer = (int16_t*)SDL_malloc(additional_amount);
  if (!buffer) return;

  // Call the user's audio callback to fill the buffer
  callback(buffer, samples_needed);

  // Put the data into the stream
  SDL_PutAudioStreamData(stream, buffer, additional_amount);

  SDL_free(buffer);
}

int audioSetup(AudioCallback* audioCallback, int sampleRate, int bufferSize) {
  if (SDL_Init(SDL_INIT_AUDIO) == false) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "SDL3 audio initialization Error: %s\n", SDL_GetError());
    return 1;
  }
  SDL_AudioSpec spec;
  SDL_memset(&spec, 0, sizeof(spec));
  spec.freq = sampleRate;
  spec.format = SDL_AUDIO_S16;
  spec.channels = 2;

  char bufferSizeStr[256];
  SDL_snprintf(bufferSizeStr, sizeof(bufferSizeStr), "%d", bufferSize);
  if (bufferSize > 0) {
    SDL_LogInfo(SDL_LOG_CATEGORY_AUDIO, "Setting requested audio device sample frames to %d",
                bufferSize);
    SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, bufferSizeStr);
  }

  // Open audio device and create stream
  audioStream = SDL_OpenAudioDeviceStream(
    SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
    &spec,
    sdlAudioCallback,
    audioCallback
  );

  if (!audioStream) {
    fprintf(stderr, "Failed to open audio: %s\n", SDL_GetError());
    return 1;
  }

  // Resume the stream to start playback
  SDL_ResumeAudioStreamDevice(audioStream);

  return 0;
}

void audioPause(int isPaused) {
  if (!audioStream) return;

  if (isPaused) {
    SDL_PauseAudioStreamDevice(audioStream);
  } else {
    SDL_ResumeAudioStreamDevice(audioStream);
  }
}

void audioCleanup(void) {
  if (audioStream) {
    SDL_DestroyAudioStream(audioStream);
    audioStream = NULL;
  }
}