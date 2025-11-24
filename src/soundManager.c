/*
 * soundManager.c
 * 
 * Audio system with dedicated haptics channel.
 * 
 * ARCHITECTURE:
 * - Uses raylib's standard audio system with pan-based channel routing
 * - LEFT CHANNEL  = HAPTICS ONLY
 *   - Procedurally generated low-frequency waveforms
 *   - Physically wired to TT25-8 shaker
 *   - No music or sound effects
 * - RIGHT CHANNEL = GAME AUDIO ONLY
 *   - All game music and sound effects mixed by raylib
 *   - Physically wired to speakers (Adafruit 1669)
 * 
 * IMPLEMENTATION:
 * 1. All game audio (Music + Sound) is panned fully right (pan = 1.0f)
 *    using SetMusicPan() and SetSoundPan()
 * 2. One dedicated AudioStream generates haptic waveforms and is panned
 *    fully left (pan = -1.0f) using SetAudioStreamPan()
 * 3. Raylib's internal mixer combines everything to the audio device
 * 
 * This split-channel design allows simultaneous haptic feedback
 * and audio output on hardware with a single stereo audio jack.
 */

#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <math.h>
#include <string.h>
#include "soundManager.h"

#ifndef PI
#define PI 3.14159265358979323846f
#endif

// Forward declaration for older raylib versions that use IsMusicStreamPlaying()
bool IsMusicStreamPlaying(Music music);

// Small compatibility wrapper for this raylib version
static bool IsMusicPlayingCompat(Music music) {
    // Use the raylib function that exists in this raylib build
    return IsMusicStreamPlaying(music);
}

// ============================================================================
// HAPTICS ENGINE
// ============================================================================

/*
 * Haptic effects are procedurally generated low-frequency waveforms
 * designed for a TT25-8 shaker in a small, rigid acrylic enclosure.
 * 
 * All frequencies are in the 40-80 Hz range (optimal for tactile response).
 * Durations are short (10-210ms) to provide crisp, non-intrusive feedback.
 * 
 * AMPLITUDE TIERS:
 * - Very soft:     0.15-0.25  (empty flipper)
 * - Soft/medium:   0.25-0.35  (light hits)
 * - Medium/strong: 0.35-0.50  (flipper+ball, strong hits)
 * - Special event: 0.40-0.70  (rising buzz for big moments)
 * 
 * EFFECT DESCRIPTIONS:
 * 1. FLIPPER_EMPTY: Subtle 60Hz sine tap (~12ms) when flipper pressed without ball
 * 2. FLIPPER_HIT: Sharper 60Hz square pulse (~20ms) when flipper strikes ball
 * 3. LAUNCH: Frequency sweep 40→60Hz (~90ms) with amplitude envelope for ball launch
 * 4. BUMPER_LIGHT: Gentle 55Hz sine (~22ms) for glancing bumper contacts
 * 5. BUMPER_SOLID: Strong 50Hz square with secondary pulse (~46ms total) for solid hits
 * 6. EXCITEMENT: Rising energy buzz 40→80Hz (~210ms) for multiball/jackpot events
 */

typedef enum {
    HAPTIC_NONE = 0,
    HAPTIC_FLIPPER_EMPTY,
    HAPTIC_FLIPPER_HIT,
    HAPTIC_LAUNCH,
    HAPTIC_BUMPER_LIGHT,
    HAPTIC_BUMPER_SOLID,
    HAPTIC_EXCITEMENT
} HapticEffectType;

typedef struct {
    HapticEffectType type;
    int active;
    float t;          // Elapsed time in seconds
    float phase;      // Oscillator phase for waveform generation
} HapticState;

static HapticState g_haptics = {0};

// Generate one haptic sample for the left channel
// Returns value in range [-1.0, 1.0]
static float haptics_generate_sample(float sampleRate) {
    if (!g_haptics.active) return 0.0f;
    
    float dt = 1.0f / sampleRate;
    g_haptics.t += dt;
    
    float output = 0.0f;
    
    switch (g_haptics.type) {
        case HAPTIC_FLIPPER_EMPTY: {
            // ~60 Hz sine, 12.5ms duration, amplitude 0.2
            const float duration = 0.0125f;
            const float freq = 60.0f;
            const float amp = 0.2f;
            
            if (g_haptics.t < duration) {
                output = amp * sinf(g_haptics.phase);
                g_haptics.phase += 2.0f * PI * freq * dt;
            } else {
                g_haptics.active = 0;
            }
            break;
        }
        
        case HAPTIC_FLIPPER_HIT: {
            // ~60 Hz square pulse, 20ms duration, amplitude 0.4
            const float duration = 0.020f;
            const float freq = 60.0f;
            const float amp = 0.4f;
            
            if (g_haptics.t < duration) {
                // Square wave (clipped sine)
                float sine = sinf(g_haptics.phase);
                output = amp * (sine > 0.0f ? 1.0f : -1.0f);
                g_haptics.phase += 2.0f * PI * freq * dt;
            } else {
                g_haptics.active = 0;
            }
            break;
        }
        
        case HAPTIC_LAUNCH: {
            // Sweep from 40Hz to 60Hz over 90ms, amplitude ramps up then down
            const float duration = 0.090f;
            const float freq_start = 40.0f;
            const float freq_end = 60.0f;
            const float peak_amp = 0.4f;
            
            if (g_haptics.t < duration) {
                float progress = g_haptics.t / duration;
                float freq = freq_start + (freq_end - freq_start) * progress;
                
                // Amplitude envelope: ramp up to middle, then down
                float amp = peak_amp * sinf(PI * progress);
                
                output = amp * sinf(g_haptics.phase);
                g_haptics.phase += 2.0f * PI * freq * dt;
            } else {
                g_haptics.active = 0;
            }
            break;
        }
        
        case HAPTIC_BUMPER_LIGHT: {
            // ~55 Hz sine, 22.5ms duration, amplitude 0.27
            const float duration = 0.0225f;
            const float freq = 55.0f;
            const float amp = 0.27f;
            
            if (g_haptics.t < duration) {
                output = amp * sinf(g_haptics.phase);
                g_haptics.phase += 2.0f * PI * freq * dt;
            } else {
                g_haptics.active = 0;
            }
            break;
        }
        
        case HAPTIC_BUMPER_SOLID: {
            // ~50 Hz square, dual pulse: 27.5ms @ 0.45, then 8ms gap, then 11ms @ 0.25
            const float pulse1_duration = 0.0275f;
            const float gap_duration = 0.008f;
            const float pulse2_duration = 0.011f;
            const float freq = 50.0f;
            const float amp1 = 0.45f;
            const float amp2 = 0.25f;
            
            if (g_haptics.t < pulse1_duration) {
                // First pulse
                float sine = sinf(g_haptics.phase);
                output = amp1 * (sine > 0.0f ? 1.0f : -1.0f);
                g_haptics.phase += 2.0f * PI * freq * dt;
            } else if (g_haptics.t < pulse1_duration + gap_duration) {
                // Gap (silence)
                output = 0.0f;
            } else if (g_haptics.t < pulse1_duration + gap_duration + pulse2_duration) {
                // Second pulse
                float sine = sinf(g_haptics.phase);
                output = amp2 * (sine > 0.0f ? 1.0f : -1.0f);
                g_haptics.phase += 2.0f * PI * freq * dt;
            } else {
                g_haptics.active = 0;
            }
            break;
        }
        
        case HAPTIC_EXCITEMENT: {
            // Rising energy buzz: ~210ms total
            // Segment 1: 40->80Hz sweep, 150ms, amp 0.45
            // Segment 2: 70Hz sustain, 40ms, amp 0.35
            // Segment 3: 50Hz tail, 20ms, amp 0.25
            const float seg1_duration = 0.150f;
            const float seg2_duration = 0.040f;
            const float seg3_duration = 0.020f;
            
            if (g_haptics.t < seg1_duration) {
                // Segment 1: frequency sweep with moderate amplitude
                float progress = g_haptics.t / seg1_duration;
                float freq = 40.0f + (80.0f - 40.0f) * progress;
                float amp = 0.45f;
                
                output = amp * sinf(g_haptics.phase);
                g_haptics.phase += 2.0f * PI * freq * dt;
            } else if (g_haptics.t < seg1_duration + seg2_duration) {
                // Segment 2: sustain at 70Hz
                float freq = 70.0f;
                float amp = 0.35f;
                
                output = amp * sinf(g_haptics.phase);
                g_haptics.phase += 2.0f * PI * freq * dt;
            } else if (g_haptics.t < seg1_duration + seg2_duration + seg3_duration) {
                // Segment 3: tail at 50Hz
                float freq = 50.0f;
                float amp = 0.25f;
                
                output = amp * sinf(g_haptics.phase);
                g_haptics.phase += 2.0f * PI * freq * dt;
            } else {
                g_haptics.active = 0;
            }
            break;
        }
        
        default:
            g_haptics.active = 0;
            break;
    }
    
    return output;
}

// Trigger a haptic effect (resets and starts the effect)
static void haptics_trigger(HapticEffectType type) {
    g_haptics.type = type;
    g_haptics.active = 1;
    g_haptics.t = 0.0f;
    g_haptics.phase = 0.0f;
}

// ============================================================================
// AUDIO STREAM MANAGEMENT
// ============================================================================

#define SAMPLE_RATE 48000
#define CHANNELS 2
#define AUDIO_BUFFER_SIZE 4096

static AudioStream g_hapticsStream = {0};
static int g_audioInitialized = 0;
static short g_audioBuffer[AUDIO_BUFFER_SIZE * CHANNELS];  // Pre-allocated buffer for haptics

// ============================================================================
// INITIALIZATION
// ============================================================================

SoundManager *initSound(){
    SoundManager *sound = malloc(sizeof(SoundManager));
    if (!sound) {
        fprintf(stderr, "ERROR: Failed to allocate SoundManager\n");
        return NULL;
    }
    
    // Initialize raylib audio device
    InitAudioDevice();
    
    // Create dedicated AudioStream for haptics, panned fully left
    g_hapticsStream = LoadAudioStream(SAMPLE_RATE, 16, CHANNELS);
    SetAudioStreamVolume(g_hapticsStream, 1.0f);
    SetAudioStreamPan(g_hapticsStream, -1.0f);  // Pan fully left for haptics
    PlayAudioStream(g_hapticsStream);
    g_audioInitialized = 1;
    
    // Initialize haptics to idle
    g_haptics.active = 0;
    g_haptics.type = HAPTIC_NONE;
    g_haptics.t = 0.0f;
    g_haptics.phase = 0.0f;
    
    // Load music streams and pan them fully right for game audio
    sound->menuMusic = LoadMusicStream("Resources/Audio/1.mp3");
    sound->gameMusic = LoadMusicStream("Resources/Audio/5.mp3");
    SetMusicPan(sound->menuMusic, 1.0f);  // Pan fully right for speakers
    SetMusicPan(sound->gameMusic, 1.0f);  // Pan fully right for speakers
    
    // Load sound effects and allocate arrays for polyphonic playback
    sound->redPowerup = malloc(sizeof(Sound) * 4);
    sound->bluePowerup = malloc(sizeof(Sound) * 4);
    sound->slowdown = malloc(sizeof(Sound) * 4);
    sound->speedup = malloc(sizeof(Sound) * 4);
    sound->upperBouncer = malloc(sizeof(Sound) * 4);
    sound->click = malloc(sizeof(Sound) * 4);
    sound->bounce1 = malloc(sizeof(Sound) * 4);
    sound->bounce2 = malloc(sizeof(Sound) * 4);
    sound->flipper = malloc(sizeof(Sound) * 4);
    sound->waterSplash = malloc(sizeof(Sound) * 4);
    
    // Check for allocation failures
    if (!sound->redPowerup || !sound->bluePowerup || !sound->slowdown || !sound->speedup ||
        !sound->upperBouncer || !sound->click || !sound->bounce1 || !sound->bounce2 ||
        !sound->flipper || !sound->waterSplash) {
        fprintf(stderr, "ERROR: Failed to allocate sound arrays\n");
        // Clean up any successful allocations (free() handles NULL safely)
        if (sound->redPowerup) free(sound->redPowerup);
        if (sound->bluePowerup) free(sound->bluePowerup);
        if (sound->slowdown) free(sound->slowdown);
        if (sound->speedup) free(sound->speedup);
        if (sound->upperBouncer) free(sound->upperBouncer);
        if (sound->click) free(sound->click);
        if (sound->bounce1) free(sound->bounce1);
        if (sound->bounce2) free(sound->bounce2);
        if (sound->flipper) free(sound->flipper);
        if (sound->waterSplash) free(sound->waterSplash);
        free(sound);
        return NULL;
    }
    
    // Load sound effects and pan them all fully right for game audio
    sound->launch = LoadSound("Resources/Audio/Click_Heavy_00.wav");
    sound->water = LoadSound("Resources/Audio/water.wav");
    SetSoundPan(sound->launch, 1.0f);  // Pan fully right for speakers
    SetSoundPan(sound->water, 1.0f);   // Pan fully right for speakers
    
    for (int i = 0; i < 4; i++){
        sound->redPowerup[i] = LoadSound("Resources/Audio/redPowerup.wav");
        sound->bluePowerup[i] = LoadSound("Resources/Audio/redPowerup.wav");
        sound->slowdown[i] = LoadSound("Resources/Audio/slowdown.wav");
        sound->speedup[i] = LoadSound("Resources/Audio/speedup.ogg");
        sound->upperBouncer[i] = LoadSound("Resources/Audio/upperBouncer.wav");
        sound->click[i] = LoadSound("Resources/Audio/Typewriter_02.wav");
        sound->bounce1[i] = LoadSound("Resources/Audio/Bounce3.wav");
        sound->bounce2[i] = LoadSound("Resources/Audio/redPowerup3.wav");
        sound->flipper[i] = LoadSound("Resources/Audio/Slide_Sharp_02.wav");
        sound->waterSplash[i] = LoadSound("Resources/Audio/water2.wav");
        
        // Pan all sounds fully right for game audio on speakers
        SetSoundPan(sound->redPowerup[i], 1.0f);
        SetSoundPan(sound->bluePowerup[i], 1.0f);
        SetSoundPan(sound->slowdown[i], 1.0f);
        SetSoundPan(sound->speedup[i], 1.0f);
        SetSoundPan(sound->upperBouncer[i], 1.0f);
        SetSoundPan(sound->click[i], 1.0f);
        SetSoundPan(sound->bounce1[i], 1.0f);
        SetSoundPan(sound->bounce2[i], 1.0f);
        SetSoundPan(sound->flipper[i], 1.0f);
        SetSoundPan(sound->waterSplash[i], 1.0f);
    }
    
    return sound;
}

// ============================================================================
// UPDATE
// ============================================================================

void updateSound(SoundManager *sound, GameStruct *game){
    // Update music streams based on game mode
    if (game->gameState == 0){
        if (!IsMusicPlayingCompat(sound->menuMusic)){
            PlayMusicStream(sound->menuMusic);
            StopMusicStream(sound->gameMusic);
        }
        UpdateMusicStream(sound->menuMusic);
    } else if (game->gameState == 1 || game->gameState == 2){
        if (!IsMusicPlayingCompat(sound->gameMusic)){
            PlayMusicStream(sound->gameMusic);
            StopMusicStream(sound->menuMusic);
        }
        UpdateMusicStream(sound->gameMusic);
        if (game->gameState == 1){
            sound->gameMusicVolume = 1.0f;
        } else {
            sound->gameMusicVolume -= 0.01f;
            if (sound->gameMusicVolume < 0.3f){
                sound->gameMusicVolume = 0.3f;
            }
        }
        SetMusicVolume(sound->gameMusic, sound->gameMusicVolume);
        if (game->slowMotionFactor < 1.0f){
            SetMusicPitch(sound->gameMusic, 0.7f);
        } else {
            SetMusicPitch(sound->gameMusic, 1.0f);
        }
    }
    
    // Update haptics audio stream (panned left via SetAudioStreamPan)
    // This stream is mixed by raylib alongside all game audio
    if (g_audioInitialized && IsAudioStreamProcessed(g_hapticsStream)) {
        // Fill buffer with haptic waveforms
        // Note: We generate stereo frames (left=haptic, right=0) and rely on
        // SetAudioStreamPan(-1.0f) to route the signal to the left channel only
        for (int i = 0; i < AUDIO_BUFFER_SIZE; i++) {
            // Generate haptic sample for left channel
            float hapticSample = haptics_generate_sample((float)SAMPLE_RATE);
            
            // Right channel is silent in our buffer
            float silentSample = 0.0f;
            
            // Convert float samples [-1.0, 1.0] to 16-bit signed integers
            short leftSample = (short)(hapticSample * 32767.0f);
            short rightSample = (short)(silentSample * 32767.0f);
            
            g_audioBuffer[i * 2 + 0] = leftSample;   // Left = haptics
            g_audioBuffer[i * 2 + 1] = rightSample;  // Right = 0
        }
        
        // Submit buffer to haptics stream (raylib mixes it with game audio)
        UpdateAudioStream(g_hapticsStream, g_audioBuffer, AUDIO_BUFFER_SIZE);
    }
}

// ============================================================================
// GAME AUDIO PLAYBACK FUNCTIONS
// ============================================================================
// These functions play sound effects through raylib's normal audio system.
// All sounds are panned fully right (pan=1.0f) during initialization via
// SetSoundPan(), which routes them to the right channel (speakers).
// Raylib's mixer combines these with the music (also panned right) and
// the haptics stream (panned left).

void playBounce(SoundManager *sound){
    for (int i = 0; i < 4; i++){
        if (!IsSoundPlaying(sound->bounce1[i])){
            PlaySound(sound->bounce1[i]);
            return;
        }
    }
}

void playBounce2(SoundManager *sound){
    for (int i = 0; i < 4; i++){
        if (!IsSoundPlaying(sound->bounce2[i])){
            PlaySound(sound->bounce2[i]);
            return;
        }
    }
}

void playClick(SoundManager *sound){
    for (int i = 0; i < 4; i++){
        if (!IsSoundPlaying(sound->click[i])){
            PlaySound(sound->click[i]);
            return;
        }
    }
}

void playSlowdownSound(SoundManager *sound){
    for (int i = 0; i < 4; i++){
        if (!IsSoundPlaying(sound->slowdown[i])){
            PlaySound(sound->slowdown[i]);
            return;
        }
    }
}

void playSpeedupSound(SoundManager *sound){
    for (int i = 0; i < 4; i++){
        if (!IsSoundPlaying(sound->speedup[i])){
            PlaySound(sound->speedup[i]);
            return;
        }
    }
}

void playRedPowerupSound(SoundManager *sound){
    for (int i = 0; i < 4; i++){
        if (!IsSoundPlaying(sound->redPowerup[i])){
            PlaySound(sound->redPowerup[i]);
            return;
        }
    }
}

void playBluePowerupSound(SoundManager *sound){
    for (int i = 0; i < 4; i++){
        if (!IsSoundPlaying(sound->bluePowerup[i])){
            PlaySound(sound->bluePowerup[i]);
            return;
        }
    }
}

void playUpperBouncerSound(SoundManager *sound){
    for (int i = 0; i < 4; i++){
        if (!IsSoundPlaying(sound->upperBouncer[i])){
            PlaySound(sound->upperBouncer[i]);
            return;
        }
    }
}

void playLaunch(SoundManager *sound){
    if (!IsSoundPlaying(sound->launch)){
        PlaySound(sound->launch);
        return;
    }
}

void playFlipper(SoundManager *sound){
    for (int i = 0; i < 4; i++){
        if (!IsSoundPlaying(sound->flipper[i])){
            PlaySound(sound->flipper[i]);
            return;
        }
    }
}

void playWater(SoundManager *sound){
    if (!IsSoundPlaying(sound->water)){
        PlaySound(sound->water);
        return;
    }
}

void playWaterSplash(SoundManager *sound){
    for (int i = 0; i < 4; i++){
        if (!IsSoundPlaying(sound->waterSplash[i])){
            PlaySound(sound->waterSplash[i]);
            return;
        }
    }
}

// ============================================================================
// HAPTIC EFFECT TRIGGER FUNCTIONS
// ============================================================================
// These functions trigger procedural haptic effects on the left channel.
// They are safe to call from game logic at any time and do not block.

void sound_play_haptic_flipper_empty(SoundManager *sound) {
    (void)sound;  // Unused parameter, kept for API consistency
    haptics_trigger(HAPTIC_FLIPPER_EMPTY);
}

void sound_play_haptic_flipper_hit(SoundManager *sound) {
    (void)sound;  // Unused parameter, kept for API consistency
    haptics_trigger(HAPTIC_FLIPPER_HIT);
}

void sound_play_haptic_launch(SoundManager *sound) {
    (void)sound;  // Unused parameter, kept for API consistency
    haptics_trigger(HAPTIC_LAUNCH);
}

void sound_play_haptic_bumper_light(SoundManager *sound) {
    (void)sound;  // Unused parameter, kept for API consistency
    haptics_trigger(HAPTIC_BUMPER_LIGHT);
}

void sound_play_haptic_bumper_solid(SoundManager *sound) {
    (void)sound;  // Unused parameter, kept for API consistency
    haptics_trigger(HAPTIC_BUMPER_SOLID);
}

void sound_play_haptic_excitement(SoundManager *sound) {
    (void)sound;  // Unused parameter, kept for API consistency
    haptics_trigger(HAPTIC_EXCITEMENT);
}

// ============================================================================
// SHUTDOWN
// ============================================================================

void shutdownSound(SoundManager *sound){
    // Stop and unload haptics audio stream
    if (g_audioInitialized) {
        StopAudioStream(g_hapticsStream);
        UnloadAudioStream(g_hapticsStream);
        g_audioInitialized = 0;
    }
    
    // Check if sound manager is valid (could be NULL if initSound failed)
    if (!sound) {
        CloseAudioDevice();
        return;
    }
    
    // Unload music streams
    UnloadMusicStream(sound->menuMusic);
    UnloadMusicStream(sound->gameMusic);
    
    // Unload individual sounds
    UnloadSound(sound->launch);
    UnloadSound(sound->water);
    
    // Unload sound arrays
    for (int i = 0; i < 4; i++) {
        UnloadSound(sound->redPowerup[i]);
        UnloadSound(sound->bluePowerup[i]);
        UnloadSound(sound->slowdown[i]);
        UnloadSound(sound->speedup[i]);
        UnloadSound(sound->upperBouncer[i]);
        UnloadSound(sound->click[i]);
        UnloadSound(sound->bounce1[i]);
        UnloadSound(sound->bounce2[i]);
        UnloadSound(sound->flipper[i]);
        UnloadSound(sound->waterSplash[i]);
    }
    
    // Free allocated arrays
    free(sound->redPowerup);
    free(sound->bluePowerup);
    free(sound->slowdown);
    free(sound->speedup);
    free(sound->upperBouncer);
    free(sound->click);
    free(sound->bounce1);
    free(sound->bounce2);
    free(sound->flipper);
    free(sound->waterSplash);
    
    // Close audio device
    CloseAudioDevice();
}
