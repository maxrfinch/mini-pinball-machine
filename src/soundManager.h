#ifndef HEADER_SOUND
#define HEADER_SOUND

#include "gameStruct.h"

// Core audio system functions
SoundManager *initSound();
void updateSound(SoundManager *soundManager, GameStruct *game);
void shutdownSound(SoundManager *soundManager);

// Existing game audio functions (mixed to mono, sent to right channel)
void playSlowdownSound(SoundManager *sound);
void playSpeedupSound(SoundManager *sound);
void playRedPowerupSound(SoundManager *sound);
void playBluePowerupSound(SoundManager *sound);
void playUpperBouncerSound(SoundManager *sound);
void playClick(SoundManager *sound);
void playBounce(SoundManager *sound);
void playBounce2(SoundManager *sound);
void playLaunch(SoundManager *sound);
void playFlipper(SoundManager *sound);
void playWater(SoundManager *sound);
void playWaterSplash(SoundManager *sound);

// Haptic effect functions (procedural waveforms, sent to left channel)
// These trigger tactile feedback on the TT25-8 shaker
void sound_play_haptic_flipper_empty(SoundManager *sound);  // Flipper press without ball contact
void sound_play_haptic_flipper_hit(SoundManager *sound);     // Flipper strikes ball
void sound_play_haptic_launch(SoundManager *sound);          // Ball launch event
void sound_play_haptic_bumper_light(SoundManager *sound);    // Light/glancing bumper hit
void sound_play_haptic_bumper_solid(SoundManager *sound);    // Solid bumper/slingshot hit
void sound_play_haptic_excitement(SoundManager *sound);      // Big event (multiball, jackpot, etc.)

// Volume control functions (affects game audio only, not haptics)
float sound_getGameVolume(SoundManager *sound);              // Get current game volume (0.0 to 1.0)
void sound_setGameVolume(SoundManager *sound, float volume); // Set game volume (0.0 to 1.0)

#endif
