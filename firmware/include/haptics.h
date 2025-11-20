/**
 * haptics.h
 * 
 * DRV2605L haptic motor driver
 */

#ifndef HAPTICS_H
#define HAPTICS_H

#include <stdint.h>

// Initialize haptic subsystem
void haptics_init(void);

// Trigger left haptic
void haptics_trigger_left(void);

// Trigger right haptic
void haptics_trigger_right(void);

// Trigger both haptics
void haptics_trigger_both(void);

// Light buzz for debug mode
void haptics_light_buzz(void);

#endif // HAPTICS_H
