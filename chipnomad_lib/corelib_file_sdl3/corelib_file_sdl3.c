#include <SDL3/SDL.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include "../corelib/corelib_file.h"

static SDL_IOStream* files[CORELIB_MAX_OPEN_FILES];
static int currentFileId = 0;
static char stringBuffer[1024];
static const char* documentsPath = NULL;

// Initialize documents path on first use
static const char* getDocumentsPath(void) {
  if (!documentsPath) {
    // For mobile builds, use Documents folder (visible in Files app)
    // For desktop builds during development, use preferences
#ifdef MOBILE_BUILD
    documentsPath = SDL_GetUserFolder(SDL_FOLDER_DOCUMENTS);
#else
    documentsPath = SDL_GetPrefPath("ChipNomad", "tracker");
#endif
    if (!documentsPath) {
      SDL_Log("Failed to get documents path: %s", SDL_GetError());
      return "";
    }
  }
  return documentsPath;
}

// Build full path from relative path
static void buildFullPath(const char* path, char* fullPath, size_t fullPathSize) {
  // If path is already absolute (starts with / or contains :), use as-is
  if (path[0] == '/' || (path[0] != '\0' && path[1] == ':')) {
    SDL_strlcpy(fullPath, path, fullPathSize);
  } else {
    // Relative path - prepend documents directory
    const char* docsPath = getDocumentsPath();
    SDL_snprintf(fullPath, fullPathSize, "%s%s", docsPath, path);
  }
}

int fileOpen(const char* path, int isWriting) {
  char fullPath[2048];
  buildFullPath(path, fullPath, sizeof(fullPath));

  int fileId = currentFileId;

  SDL_IOStream* stream = SDL_IOFromFile(fullPath, isWriting ? "wb" : "rb");
  if (!stream) {
    SDL_Log("Failed to open file %s: %s", fullPath, SDL_GetError());
    return -1;
  }

  files[fileId] = stream;

  currentFileId++;
  if (currentFileId == CORELIB_MAX_OPEN_FILES) {
    currentFileId = 0;
  }

  return fileId;
}

int fileClose(int fileId) {
  if (fileId < 0 || fileId >= CORELIB_MAX_OPEN_FILES || !files[fileId]) {
    return -1;
  }

  bool result = SDL_CloseIO(files[fileId]);
  files[fileId] = NULL;
  return result ? 0 : -1;
}

int fileRead(int fileId, void* buffer, int maxLength) {
  if (fileId < 0 || fileId >= CORELIB_MAX_OPEN_FILES || !files[fileId]) {
    return -1;
  }

  size_t bytesRead = SDL_ReadIO(files[fileId], buffer, maxLength);
  return (int)bytesRead;
}

char* fileReadString(int fileId) {
  if (fileId < 0 || fileId >= CORELIB_MAX_OPEN_FILES || !files[fileId]) {
    return NULL;
  }

  // Read line character by character until newline or buffer full
  int idx = 0;
  while (idx < 1023) {
    char c;
    size_t bytesRead = SDL_ReadIO(files[fileId], &c, 1);

    if (bytesRead == 0) {
      if (idx == 0) return NULL; // EOF with no data
      break; // EOF with some data
    }

    if (c == '\n') {
      break; // End of line
    }

    stringBuffer[idx++] = c;
  }
  stringBuffer[idx] = '\0';

  // Trim trailing whitespace
  idx--;
  while (idx >= 0 && (stringBuffer[idx] == ' ' || stringBuffer[idx] == '\t' ||
                       stringBuffer[idx] == '\r' || stringBuffer[idx] == '\n')) {
    stringBuffer[idx] = '\0';
    idx--;
  }

  return stringBuffer;
}

int fileWrite(int fileId, void* data, int length) {
  if (fileId < 0 || fileId >= CORELIB_MAX_OPEN_FILES || !files[fileId]) {
    return -1;
  }

  size_t bytesWritten = SDL_WriteIO(files[fileId], data, length);
  return (int)bytesWritten;
}

int filePrintf(int fileId, const char* format, ...) {
  static char writeBuffer[1024];

  va_list args;
  va_start(args, format);
  SDL_vsnprintf(writeBuffer, sizeof(writeBuffer), format, args);
  va_end(args);

  return fileWrite(fileId, writeBuffer, SDL_strlen(writeBuffer));
}

int fileSeek(int fileId, long offset, int whence) {
  if (fileId < 0 || fileId >= CORELIB_MAX_OPEN_FILES || !files[fileId]) {
    return -1;
  }

  // Convert stdio whence values to SDL values
  SDL_IOWhence sdlWhence;
  switch (whence) {
    case 0: // SEEK_SET
      sdlWhence = SDL_IO_SEEK_SET;
      break;
    case 1: // SEEK_CUR
      sdlWhence = SDL_IO_SEEK_CUR;
      break;
    case 2: // SEEK_END
      sdlWhence = SDL_IO_SEEK_END;
      break;
    default:
      return -1;
  }

  Sint64 result = SDL_SeekIO(files[fileId], offset, sdlWhence);
  return result >= 0 ? 0 : -1;
}

int fileDelete(const char* path) {
  char fullPath[2048];
  buildFullPath(path, fullPath, sizeof(fullPath));

  bool result = SDL_RemovePath(fullPath);
  return result ? 0 : -1;
}

int fileCreateDirectory(const char* path) {
  char fullPath[2048];
  buildFullPath(path, fullPath, sizeof(fullPath));

  bool result = SDL_CreateDirectory(fullPath);
  return result ? 0 : -1;
}

// Callback for SDL_EnumerateDirectory
typedef struct {
  FileEntry* entries;
  int count;
  int capacity;
  const char* extension;
  const char* basePath;
} EnumerateContext;

static SDL_EnumerationResult enumerateCallback(void* userdata, const char* dirname, const char* fname) {
  EnumerateContext* ctx = (EnumerateContext*)userdata;

  // Skip hidden files
  if (fname[0] == '.') {
    // Allow ".." for parent directory
    if (SDL_strcmp(fname, "..") != 0) {
      return SDL_ENUM_CONTINUE;
    }
  }

  // Build full path to check if it's a directory
  char fullPath[2048];
  SDL_snprintf(fullPath, sizeof(fullPath), "%s%s%s", dirname, PATH_SEPARATOR_STR, fname);

  SDL_PathInfo pathInfo;
  bool isDir = false;
  if (SDL_GetPathInfo(fullPath, &pathInfo)) {
    isDir = (pathInfo.type == SDL_PATHTYPE_DIRECTORY);
  }

  // Filter by extension if it's a file and extension is specified
  if (!isDir && ctx->extension && ctx->extension[0] != '\0') {
    const char* dot = SDL_strrchr(fname, '.');
    if (!dot) {
      return SDL_ENUM_CONTINUE;
    }

    // Check if extension matches (supports comma-separated list)
    int match = 0;
    const char* pos = ctx->extension;
    while (pos && *pos) {
      const char* comma = SDL_strchr(pos, ',');
      size_t extLen = comma ? (size_t)(comma - pos) : SDL_strlen(pos);

      if (SDL_strncasecmp(pos, dot, extLen) == 0 && dot[extLen] == '\0') {
        match = 1;
        break;
      }

      pos = comma ? comma + 1 : NULL;
    }

    if (!match) {
      return SDL_ENUM_CONTINUE;
    }
  }

  // Resize array if needed
  if (ctx->count >= ctx->capacity) {
    ctx->capacity *= 2;
    FileEntry* newEntries = (FileEntry*)SDL_realloc(ctx->entries, ctx->capacity * sizeof(FileEntry));
    if (!newEntries) {
      return SDL_ENUM_FAILURE;
    }
    ctx->entries = newEntries;
  }

  // Add entry
  SDL_strlcpy(ctx->entries[ctx->count].name, fname, sizeof(ctx->entries[ctx->count].name));
  ctx->entries[ctx->count].isDirectory = isDir ? 1 : 0;
  ctx->count++;

  return SDL_ENUM_CONTINUE;
}

FileEntry* fileListDirectory(const char* path, const char* extension, int* entryCount) {
  char fullPath[2048];
  buildFullPath(path, fullPath, sizeof(fullPath));

  EnumerateContext ctx;
  ctx.capacity = 100;
  ctx.count = 0;
  ctx.entries = (FileEntry*)SDL_malloc(ctx.capacity * sizeof(FileEntry));
  ctx.extension = extension;
  ctx.basePath = fullPath;

  if (!ctx.entries) {
    *entryCount = 0;
    return NULL;
  }

  bool result = SDL_EnumerateDirectory(fullPath, enumerateCallback, &ctx);

  if (!result) {
    SDL_Log("Failed to enumerate directory %s: %s", fullPath, SDL_GetError());
    SDL_free(ctx.entries);
    *entryCount = 0;
    return NULL;
  }

  *entryCount = ctx.count;
  return ctx.entries;
}

int fileGetCurrentDirectory(char* buffer, int bufferSize) {
  const char* docsPath = getDocumentsPath();
  SDL_strlcpy(buffer, docsPath, bufferSize);
  return 0;
}

int fileDirectoryExists(const char* path) {
  char fullPath[2048];
  buildFullPath(path, fullPath, sizeof(fullPath));

  SDL_PathInfo pathInfo;
  if (!SDL_GetPathInfo(fullPath, &pathInfo)) {
    return 0;
  }

  return (pathInfo.type == SDL_PATHTYPE_DIRECTORY) ? 1 : 0;
}
