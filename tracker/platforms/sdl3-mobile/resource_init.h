#ifndef __RESOURCE_INIT_H__
#define __RESOURCE_INIT_H__

// Initialize bundled resources on first run
// Copies demo files, pitch tables, and other resources from app bundle
// to SDL_FOLDER_DOCUMENTS if they don't already exist
// Returns 0 on success, -1 on error (non-fatal)
int resourcesInitFirstRun(void);

#endif
