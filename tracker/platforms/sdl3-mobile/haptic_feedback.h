#ifndef HAPTIC_FEEDBACK_H
#define HAPTIC_FEEDBACK_H

// Initialize haptic feedback system
void hapticInit(void);

// Trigger haptic feedback for button press
// Safe to call even if haptics are not available
void hapticTriggerButtonPress(void);

// Cleanup haptic feedback resources
void hapticCleanup(void);

#endif // HAPTIC_FEEDBACK_H
