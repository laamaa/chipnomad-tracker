#if defined(__APPLE__) && defined(__MACH__)
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR

#import <UIKit/UIKit.h>
#include "haptic_feedback.h"

// Static feedback generator - created once and reused
static UIImpactFeedbackGenerator *impactGenerator = nil;

void hapticInit(void) {
    if (@available(iOS 10.0, *)) {
        // Use light impact for subtle button press feedback
        impactGenerator = [[UIImpactFeedbackGenerator alloc] initWithStyle:UIImpactFeedbackStyleLight];
        [impactGenerator prepare];
    }
}

void hapticTriggerButtonPress(void) {
    if (@available(iOS 10.0, *)) {
        if (impactGenerator) {
            [impactGenerator impactOccurred];
            // Prepare for next use
            [impactGenerator prepare];
        }
    }
}

void hapticCleanup(void) {
    if (@available(iOS 10.0, *)) {
        if (impactGenerator) {
            impactGenerator = nil;
        }
    }
}

#endif // TARGET_OS_IPHONE
#endif // __APPLE__
