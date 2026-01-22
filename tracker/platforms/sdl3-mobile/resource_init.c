#include <SDL3/SDL.h>
#include <string.h>
#include "resource_init.h"
#include "corelib/corelib_file.h"
#include "common.h"
#include "../../src/common.h"

#define MARKER_FILENAME ".chipnomad_initialized"
#define CURRENT_INIT_VERSION "1.1.0"

// Hardcoded file lists for copying from bundle
static const char* demoFiles[] = {
  "DEMO1.cnm",
  "SkyTrain Funk.cnm",
  NULL
};

static const char* pitchTableFiles[] = {
  "PT3-0.csv",
  "PT3-1.csv",
  "PT3-2.csv",
  "PT3-3.csv",
  "Just D Phrygian 177 433.csv",
  NULL
};

static const char* themeFiles[] = {
  "Default.cth",
  "nIkO.cth",
  NULL
};

// Compare version strings (simple strcmp-based comparison)
// Returns: <0 if v1 < v2, 0 if equal, >0 if v1 > v2
static int compareVersions(const char* v1, const char* v2) {
  int major1, minor1, patch1;
  int major2, minor2, patch2;

  // Parse first version
  if (sscanf(v1, "%d.%d.%d", &major1, &minor1, &patch1) != 3) {
    return -1;  // Invalid format, treat as older
  }

  // Parse second version
  if (sscanf(v2, "%d.%d.%d", &major2, &minor2, &patch2) != 3) {
    return 1;  // Invalid format, treat current as newer
  }

  if (major1 != major2) return major1 - major2;
  if (minor1 != minor2) return minor1 - minor2;
  return patch1 - patch2;
}

// Check if initialization is needed by comparing marker file version
static int needsInitialization(void) {
  const int fileId = fileOpen(MARKER_FILENAME, 0);  // Try to open for reading
  if (fileId < 0) {
    return 1;  // File doesn't exist - needs initialization
  }

  // Read version from marker file
  char storedVersion[32] = {0};
  int bytesRead = fileRead(fileId, storedVersion, sizeof(storedVersion) - 1);
  fileClose(fileId);

  if (bytesRead <= 0) {
    SDL_Log("Marker file empty or unreadable - needs initialization");
    return 1;
  }

  // Strip newline if present
  char* newline = strchr(storedVersion, '\n');
  if (newline) *newline = '\0';

  // Compare versions
  int cmp = compareVersions(storedVersion, CURRENT_INIT_VERSION);
  if (cmp < 0) {
    SDL_Log("Stored version %s is older than %s - needs initialization",
            storedVersion, CURRENT_INIT_VERSION);
    return 1;
  }

  SDL_Log("Stored version %s is up to date", storedVersion);
  return 0;  // Version is current or newer
}

// Create marker file to indicate resources have been initialized
static int createMarkerFile(void) {
  int fileId = fileOpen(MARKER_FILENAME, 1);  // Open for writing
  if (fileId < 0) {
    SDL_Log("Failed to create marker file %s", MARKER_FILENAME);
    return -1;
  }

  // Write current version
  fileWrite(fileId, (void*)CURRENT_INIT_VERSION, (int)strlen(CURRENT_INIT_VERSION));
  fileWrite(fileId, "\n", 1);
  fileClose(fileId);

  return 0;
}

// Copy a single file from bundle to Documents folder
// srcPath: absolute path in bundle
// destPath: relative path in Documents folder
static int copyFile(const char* srcPath, const char* destPath) {
  // Get Documents folder path
  const char* docsPath = SDL_GetUserFolder(SDL_FOLDER_DOCUMENTS);
  if (!docsPath) {
    SDL_Log("Failed to get documents path: %s", SDL_GetError());
    return -1;
  }

  // Build full destination path
  char fullDestPath[2048];
  SDL_snprintf(fullDestPath, sizeof(fullDestPath), "%s%s", docsPath, destPath);

  // Use SDL3's built-in copy function
  if (!SDL_CopyFile(srcPath, fullDestPath)) {
    SDL_Log("Failed to copy %s to %s: %s", srcPath, fullDestPath, SDL_GetError());
    return -1;
  }

  SDL_Log("Copied %s to %s", srcPath, destPath);
  return 0;
}

// Copy all files from a bundle subdirectory to a Documents subdirectory
// bundleSubdir: subdirectory in app bundle (e.g., "projects")
// destSubdir: subdirectory in Documents (e.g., "Demos")
// fileList: NULL-terminated array of filenames to copy
static int copyDirectory(const char* bundlePath, const char* bundleSubdir,
                        const char* destSubdir, const char** fileList) {
  int successCount = 0;
  int failCount = 0;

  // Create destination directory
  if (fileCreateDirectory(destSubdir) != 0) {
    SDL_Log("Warning: Failed to create directory %s", destSubdir);
    // Continue anyway - directory might already exist
  }

  // Copy each file
  for (int i = 0; fileList[i] != NULL; i++) {
    char srcPath[2048];
    char destPath[2048];

    // Build source path: bundlePath/bundleSubdir/filename
    SDL_snprintf(srcPath, sizeof(srcPath), "%s%s%s%s",
                bundlePath, bundleSubdir, PATH_SEPARATOR_STR, fileList[i]);

    // Build destination path: destSubdir/filename
    SDL_snprintf(destPath, sizeof(destPath), "%s%s%s",
                destSubdir, PATH_SEPARATOR_STR, fileList[i]);

    if (copyFile(srcPath, destPath) == 0) {
      successCount++;
    } else {
      failCount++;
      SDL_Log("Warning: Failed to copy %s", fileList[i]);
    }
  }

  SDL_Log("Copied %d files to %s (%d failed)", successCount, destSubdir, failCount);
  return successCount > 0 ? 0 : -1;  // Success if at least one file copied
}

// Copy all bundled resources to Documents folder
static int copyBundledResources(void) {
  // Get app bundle path
  const char* bundlePath = SDL_GetBasePath();
  if (!bundlePath) {
    SDL_Log("Failed to get bundle path: %s", SDL_GetError());
    return -1;
  }

  SDL_Log("Bundle path: %s", bundlePath);

  int totalSuccess = 0;

  // Copy demo projects to Demos folder
  SDL_Log("Copying demo projects...");
  if (copyDirectory(bundlePath, ".", "Demos", demoFiles) == 0) {
    totalSuccess++;
  }

  // Copy pitch tables
  SDL_Log("Copying pitch tables...");
  if (copyDirectory(bundlePath, ".", "pitch-tables", pitchTableFiles) == 0) {
    totalSuccess++;
  }

  // Copy themes
  SDL_Log("Copying themes...");
  if (copyDirectory(bundlePath, ".", "themes", themeFiles) == 0) {
    totalSuccess++;
  }

  // Create empty instruments directory for future use
  SDL_Log("Creating instruments directory...");
  if (fileCreateDirectory("instruments") == 0) {
    SDL_Log("Created instruments directory");
  } else {
    SDL_Log("Warning: Failed to create instruments directory");
  }

  return totalSuccess > 0 ? 0 : -1;  // Success if at least one category copied
}

// Main entry point - initialize bundled resources on first run or version upgrade
int resourcesInitFirstRun(void) {
  SDL_Log("Checking if resource initialization needed...");

  // Check if initialization is needed (first run or version upgrade)
  if (!needsInitialization()) {
    SDL_Log("Resources up to date - skipping initialization");
    return 0;  // Already initialized with current version
  }

  SDL_Log("Initialization needed - copying bundled resources...");

  // Copy bundled resources
  if (copyBundledResources() != 0) {
    SDL_Log("Warning: Resource copy had errors, but continuing");
    // Don't return error - partial success is acceptable
  }

  // Create marker file to prevent re-copying on next launch
  if (createMarkerFile() != 0) {
    SDL_Log("Warning: Failed to create marker file");
    // Don't return error - resources were copied successfully
  }

  // Update default paths in settings
  SDL_Log("Updating default paths in settings...");
  strncpy(appSettings.projectPath, PATH_SEPARATOR_STR "Demos", PATH_LENGTH);
  strncpy(appSettings.pitchTablePath, PATH_SEPARATOR_STR "pitch-tables", PATH_LENGTH);
  strncpy(appSettings.instrumentPath, PATH_SEPARATOR_STR "instruments", PATH_LENGTH);
  strncpy(appSettings.themePath, PATH_SEPARATOR_STR "themes", PATH_LENGTH);


  // Ensure null termination
  appSettings.projectPath[PATH_LENGTH] = '\0';
  appSettings.pitchTablePath[PATH_LENGTH] = '\0';
  appSettings.instrumentPath[PATH_LENGTH] = '\0';
  appSettings.themePath[PATH_LENGTH] = '\0';

  // Save updated settings
  if (settingsSave() != 0) {
    SDL_Log("Warning: Failed to save settings");
    // Don't return error - settings will be saved later
  }

  SDL_Log("Resource initialization complete");
  return 0;
}
