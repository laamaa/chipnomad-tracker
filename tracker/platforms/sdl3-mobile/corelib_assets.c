#include <SDL3/SDL.h>
#include <string.h>
#include "corelib_assets.h"
#include "corelib/corelib_file.h"
#include "common.h"
#include "../../src/common.h"

#define MARKER_FILENAME ".chipnomad_initialized"
#define CURRENT_INIT_VERSION "2.0.0"

// Asset directories to copy from bundle to Documents
// Each entry is: { bundleSubdir, destSubdir }
static const char* assetDirs[][2] = {
  { "projects",     "Demos" },
  { "pitch-tables", "pitch-tables" },
  { "themes",       "themes" },
  { NULL, NULL }
};

// Callback context for SDL_EnumerateDirectory
typedef struct {
  const char* bundleDir;   // Full path to bundle subdirectory
  const char* destSubdir;  // Relative destination subdirectory
  int successCount;
  int failCount;
} CopyContext;

// Compare version strings
// Returns: <0 if v1 < v2, 0 if equal, >0 if v1 > v2
static int compareVersions(const char* v1, const char* v2) {
  int major1, minor1, patch1;
  int major2, minor2, patch2;

  if (sscanf(v1, "%d.%d.%d", &major1, &minor1, &patch1) != 3) {
    return -1;
  }

  if (sscanf(v2, "%d.%d.%d", &major2, &minor2, &patch2) != 3) {
    return 1;
  }

  if (major1 != major2) return major1 - major2;
  if (minor1 != minor2) return minor1 - minor2;
  return patch1 - patch2;
}

static int needsInitialization(void) {
  const int fileId = fileOpen(MARKER_FILENAME, 0);
  if (fileId < 0) {
    return 1;
  }

  char storedVersion[32] = {0};
  int bytesRead = fileRead(fileId, storedVersion, sizeof(storedVersion) - 1);
  fileClose(fileId);

  if (bytesRead <= 0) {
    SDL_Log("Marker file empty or unreadable - needs initialization");
    return 1;
  }

  char* newline = strchr(storedVersion, '\n');
  if (newline) *newline = '\0';

  int cmp = compareVersions(storedVersion, CURRENT_INIT_VERSION);
  if (cmp < 0) {
    SDL_Log("Stored version %s is older than %s - needs initialization",
            storedVersion, CURRENT_INIT_VERSION);
    return 1;
  }

  SDL_Log("Stored version %s is up to date", storedVersion);
  return 0;
}

static int createMarkerFile(void) {
  int fileId = fileOpen(MARKER_FILENAME, 1);
  if (fileId < 0) {
    SDL_Log("Failed to create marker file %s", MARKER_FILENAME);
    return -1;
  }

  fileWrite(fileId, (void*)CURRENT_INIT_VERSION, (int)strlen(CURRENT_INIT_VERSION));
  fileWrite(fileId, "\n", 1);
  fileClose(fileId);

  return 0;
}

// Copy a single file from bundle to Documents folder
static int copyFile(const char* srcPath, const char* destPath) {
  const char* docsPath = SDL_GetUserFolder(SDL_FOLDER_DOCUMENTS);
  if (!docsPath) {
    SDL_Log("Failed to get documents path: %s", SDL_GetError());
    return -1;
  }

  char fullDestPath[2048];
  SDL_snprintf(fullDestPath, sizeof(fullDestPath), "%s%s", docsPath, destPath);

  if (!SDL_CopyFile(srcPath, fullDestPath)) {
    SDL_Log("Failed to copy %s to %s: %s", srcPath, fullDestPath, SDL_GetError());
    return -1;
  }

  SDL_Log("Copied %s to %s", srcPath, destPath);
  return 0;
}

// Callback for SDL_EnumerateDirectory - called for each entry in a directory
static SDL_EnumerationResult copyEnumerateCallback(void* userdata, const char* dirname, const char* fname) {
  CopyContext* ctx = (CopyContext*)userdata;

  // Skip hidden files
  if (fname[0] == '.') {
    return SDL_ENUM_CONTINUE;
  }

  char srcPath[2048];
  SDL_snprintf(srcPath, sizeof(srcPath), "%s%s%s", dirname, PATH_SEPARATOR_STR, fname);

  // Check if this entry is a directory (for subdirectories like IDEColorThemes/)
  SDL_PathInfo pathInfo;
  if (SDL_GetPathInfo(srcPath, &pathInfo) && pathInfo.type == SDL_PATHTYPE_DIRECTORY) {
    // Recursively copy subdirectory
    char subDestDir[2048];
    SDL_snprintf(subDestDir, sizeof(subDestDir), "%s%s%s", ctx->destSubdir, PATH_SEPARATOR_STR, fname);

    // Create the subdirectory in Documents
    fileCreateDirectory(subDestDir);

    CopyContext subCtx = {
      .bundleDir = srcPath,
      .destSubdir = subDestDir,
      .successCount = 0,
      .failCount = 0
    };

    SDL_EnumerateDirectory(srcPath, copyEnumerateCallback, &subCtx);
    ctx->successCount += subCtx.successCount;
    ctx->failCount += subCtx.failCount;

    return SDL_ENUM_CONTINUE;
  }

  // Regular file - copy it
  char destPath[2048];
  SDL_snprintf(destPath, sizeof(destPath), "%s%s%s", ctx->destSubdir, PATH_SEPARATOR_STR, fname);

  if (copyFile(srcPath, destPath) == 0) {
    ctx->successCount++;
  } else {
    ctx->failCount++;
    SDL_Log("Warning: Failed to copy %s", fname);
  }

  return SDL_ENUM_CONTINUE;
}

// Copy all files from a bundle subdirectory to a Documents subdirectory
static int copyDirectory(const char* bundlePath, const char* bundleSubdir, const char* destSubdir) {
  // Create destination directory
  if (fileCreateDirectory(destSubdir) != 0) {
    SDL_Log("Warning: Failed to create directory %s", destSubdir);
  }

  char fullBundleDir[2048];
  SDL_snprintf(fullBundleDir, sizeof(fullBundleDir), "%s%s", bundlePath, bundleSubdir);

  CopyContext ctx = {
    .bundleDir = fullBundleDir,
    .destSubdir = destSubdir,
    .successCount = 0,
    .failCount = 0
  };

  if (!SDL_EnumerateDirectory(fullBundleDir, copyEnumerateCallback, &ctx)) {
    SDL_Log("Failed to enumerate bundle directory %s: %s", fullBundleDir, SDL_GetError());
    return -1;
  }

  SDL_Log("Copied %d files to %s (%d failed)", ctx.successCount, destSubdir, ctx.failCount);
  return ctx.successCount > 0 ? 0 : -1;
}

// Copy all bundled resources to Documents folder
static int copyBundledResources(void) {
  const char* bundlePath = SDL_GetBasePath();
  if (!bundlePath) {
    SDL_Log("Failed to get bundle path: %s", SDL_GetError());
    return -1;
  }

  SDL_Log("Bundle path: %s", bundlePath);

  int totalSuccess = 0;

  // Enumerate and copy each asset directory
  for (int i = 0; assetDirs[i][0] != NULL; i++) {
    SDL_Log("Copying %s...", assetDirs[i][0]);
    if (copyDirectory(bundlePath, assetDirs[i][0], assetDirs[i][1]) == 0) {
      totalSuccess++;
    }
  }

  // Create empty instruments directory
  SDL_Log("Creating instruments directory...");
  if (fileCreateDirectory("instruments") == 0) {
    SDL_Log("Created instruments directory");
  } else {
    SDL_Log("Warning: Failed to create instruments directory");
  }

  return totalSuccess > 0 ? 0 : -1;
}

int assetsInit(void) {
  SDL_Log("Checking if resource initialization needed...");

  if (!needsInitialization()) {
    SDL_Log("Resources up to date - skipping initialization");
    return 0;
  }

  SDL_Log("Initialization needed - copying bundled resources...");

  if (copyBundledResources() != 0) {
    SDL_Log("Warning: Resource copy had errors, but continuing");
  }

  if (createMarkerFile() != 0) {
    SDL_Log("Warning: Failed to create marker file");
  }

  // Update default paths in settings
  SDL_Log("Updating default paths in settings...");
  strncpy(appSettings.projectPath, PATH_SEPARATOR_STR "Demos", PATH_LENGTH);
  strncpy(appSettings.pitchTablePath, PATH_SEPARATOR_STR "pitch-tables", PATH_LENGTH);
  strncpy(appSettings.instrumentPath, PATH_SEPARATOR_STR "instruments", PATH_LENGTH);
  strncpy(appSettings.themePath, PATH_SEPARATOR_STR "themes", PATH_LENGTH);

  appSettings.projectPath[PATH_LENGTH] = '\0';
  appSettings.pitchTablePath[PATH_LENGTH] = '\0';
  appSettings.instrumentPath[PATH_LENGTH] = '\0';
  appSettings.themePath[PATH_LENGTH] = '\0';

  if (settingsSave() != 0) {
    SDL_Log("Warning: Failed to save settings");
  }

  SDL_Log("Resource initialization complete");
  return 0;
}
