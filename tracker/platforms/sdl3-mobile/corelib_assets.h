#ifndef __CORELIB_ASSETS_H__
#define __CORELIB_ASSETS_H__

// Initialize bundled assets
// Copies bundled resources from app bundle to Documents folder
// on first run or version upgrade
// Returns 0 on success, non-zero on error
int assetsInit(void);

#endif
