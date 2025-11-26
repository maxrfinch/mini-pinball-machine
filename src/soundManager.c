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
 * 1. All game audio (Music + Sound) is panned fully right (pan = 0.0f)
 *    using SetMusicPan() and SetSoundPan()
 * 2. One dedicated AudioStream generates haptic waveforms and is written
 *    directly to the left channel (right=0.0) in the buffer.
 * 3. Raylib's internal mixer combines everything to the audio device
 *
 * // NOTE: In this raylib build, pan=0.0 maps to full RIGHT, pan=1.0 maps to full LEFT.
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

 // TT25-8 TRANSDUCER NOTES:
// - Usable frequency response: ~20–80 Hz
// - Resonant peak (Fs): ~40 Hz
// - Most efficient / strongest tactile band: 40–70 Hz
// - Can reproduce 20–30 Hz but output drops (cabinet amplification may help)
// - Above ~80 Hz becomes more “buzz” than “thump”
// These limits should guide haptic effect design: keep primary impact
// frequencies in 50–80 Hz and use 40–70 Hz for strongest transient hits.


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
            // Light but noticeable tap: ~120 Hz, ~50 ms, small decay envelope (slightly stronger)
            const float duration = 0.050f;
            const float freq = 120.0f;
            const float baseAmp = 1.0f;

            if (g_haptics.t < duration) {
                float progress = g_haptics.t / duration;           // 0..1
                float env = 1.0f - progress;                       // simple decay
                float amp = baseAmp * env;
                output = amp * sinf(g_haptics.phase);
                g_haptics.phase += 2.0f * PI * freq * dt;
            } else {
                g_haptics.active = 0;
            }
            break;
        }
        
        case HAPTIC_FLIPPER_HIT: {
            // Thumpy hit: ~130 Hz, ~64 ms, stronger peak to approach launch feel (~100%)
            const float duration = 0.064f;
            const float freq = 130.0f;
            const float baseAmp = 1.0f;

            if (g_haptics.t < duration) {
                float progress = g_haptics.t / duration;   // 0..1
                // Fast decay so it feels like a knock, not a buzz
                float env = 1.0f - progress * progress;    // quadratic decay
                float amp = baseAmp * env;

                output = amp * sinf(g_haptics.phase);
                g_haptics.phase += 2.0f * PI * freq * dt;
            } else {
                g_haptics.active = 0;
            }
            break;
        }
        
        case HAPTIC_LAUNCH: {
            // Sweep from 45Hz to 90Hz over 190ms, amplitude ramps up then down
            const float duration = 0.190f;
            const float freq_start = 45.0f;
            const float freq_end = 90.0f;
            const float peak_amp = 1.0f;
            
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
            // Light bumper: ~150 Hz, ~40ms, decaying sine tap
            const float duration = 0.080f;
            const float freq = 70.0f;
            const float baseAmp = 1.0f;
            
            if (g_haptics.t < duration) {
                float progress = g_haptics.t / duration;   // 0..1
                float env = 1.0f - progress * progress;    // quick decay
                float amp = baseAmp * env;
                output = amp * sinf(g_haptics.phase);
                g_haptics.phase += 2.0f * PI * freq * dt;
            } else {
                g_haptics.active = 0;
            }
            break;
        }
        
        case HAPTIC_BUMPER_SOLID: {
            // Double knock: sine-based pulses with envelopes to avoid buzz
            const float pulse1_duration = 0.104f;
            const float gap_duration   = 0.012f;
            const float pulse2_duration = 0.032f;
            const float freq = 60.0f;      // more tactile for limited power
            const float baseAmp1 = 1.0f;
            const float baseAmp2 = 1.0f;

            if (g_haptics.t < pulse1_duration) {
                // First pulse with fast decay envelope
                float progress = g_haptics.t / pulse1_duration;   // 0..1
                float env = 1.0f - progress * progress;
                float amp = baseAmp1 * env;
                output = amp * sinf(g_haptics.phase);
                g_haptics.phase += 2.0f * PI * freq * dt;
            } else if (g_haptics.t < pulse1_duration + gap_duration) {
                // Gap (silence)
                output = 0.0f;
            } else if (g_haptics.t < pulse1_duration + gap_duration + pulse2_duration) {
                // Second pulse with its own envelope
                float local_t = g_haptics.t - (pulse1_duration + gap_duration);
                float progress = local_t / pulse2_duration;       // 0..1
                float env = 1.0f - progress * progress;
                float amp = baseAmp2 * env;
                output = amp * sinf(g_haptics.phase);
                g_haptics.phase += 2.0f * PI * freq * dt;
            } else {
                g_haptics.active = 0;
            }
            break;
        }
        
        case HAPTIC_EXCITEMENT: {
            // Rising energy buzz: ~420ms total
            // Segment 1: 40->80Hz sweep, 300ms, amp 1.0
            // Segment 2: 70Hz sustain, 80ms, amp 1.0
            // Segment 3: 50Hz tail, 40ms, amp 1.0
            const float seg1_duration = 0.300f;
            const float seg2_duration = 0.080f;
            const float seg3_duration = 0.040f;
            
            if (g_haptics.t < seg1_duration) {
                // Segment 1: frequency sweep with maximum amplitude
                float progress = g_haptics.t / seg1_duration;
                float freq = 40.0f + (80.0f - 40.0f) * progress;
                float amp = 1.0f;
                
                output = amp * sinf(g_haptics.phase);
                g_haptics.phase += 2.0f * PI * freq * dt;
            } else if (g_haptics.t < seg1_duration + seg2_duration) {
                // Segment 2: sustain at 70Hz
                float freq = 70.0f;
                float amp = 1.0f;
                
                output = amp * sinf(g_haptics.phase);
                g_haptics.phase += 2.0f * PI * freq * dt;
            } else if (g_haptics.t < seg1_duration + seg2_duration + seg3_duration) {
                // Segment 3: tail at 50Hz
                float freq = 50.0f;
                float amp = 1.0f;
                
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
#define SOUND_POOL_SIZE 8  // Number of polyphonic slots per sound effect

static AudioStream g_hapticsStream = {0};
static int g_audioInitialized = 0;
static short g_audioBuffer[AUDIO_BUFFER_SIZE * CHANNELS];  // Pre-allocated buffer for haptics

// ============================================================================
// INITIALIZATION
// ============================================================================

SoundManager *initSound(){
    SoundManager *sound = malloc(sizeof(SoundManager));
    if (!sound) {
        fprintf(stderr, "ERROR: Failed to allocate memory for SoundManager\n");
        return NULL;
    }
    
    // Initialize game volume to 100%
    sound->gameVolume = 1.0f;
    
    // Initialize round-robin indices for polyphonic playback
    sound->redPowerupIndex = 0;
    sound->bluePowerupIndex = 0;
    sound->slowdownIndex = 0;
    sound->speedupIndex = 0;
    sound->upperBouncerIndex = 0;
    sound->clickIndex = 0;
    sound->bounce1Index = 0;
    sound->bounce2Index = 0;
    sound->flipperIndex = 0;
    sound->waterSplashIndex = 0;
    
    // Initialize raylib audio device
    InitAudioDevice();
    
    // Create dedicated AudioStream for haptics (dedicated left channel)
    SetAudioStreamBufferSizeDefault(AUDIO_BUFFER_SIZE);
    g_hapticsStream = LoadAudioStream(SAMPLE_RATE, 16, CHANNELS);
    SetAudioStreamVolume(g_hapticsStream, 1.0f);
    // No need to set pan: we write explicit stereo samples (left=haptics, right=0.0)
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
    SetMusicPan(sound->menuMusic, 0.0f);  // Pan fully right for speakers
    SetMusicPan(sound->gameMusic, 0.0f);  // Pan fully right for speakers
    SetMusicVolume(sound->menuMusic, sound->gameVolume);
    SetMusicVolume(sound->gameMusic, sound->gameVolume);
    
    // Load sound effects and allocate arrays for polyphonic playback
    sound->redPowerup = malloc(sizeof(Sound) * SOUND_POOL_SIZE);
    sound->bluePowerup = malloc(sizeof(Sound) * SOUND_POOL_SIZE);
    sound->slowdown = malloc(sizeof(Sound) * SOUND_POOL_SIZE);
    sound->speedup = malloc(sizeof(Sound) * SOUND_POOL_SIZE);
    sound->upperBouncer = malloc(sizeof(Sound) * SOUND_POOL_SIZE);
    sound->click = malloc(sizeof(Sound) * SOUND_POOL_SIZE);
    sound->bounce1 = malloc(sizeof(Sound) * SOUND_POOL_SIZE);
    sound->bounce2 = malloc(sizeof(Sound) * SOUND_POOL_SIZE);
    sound->flipper = malloc(sizeof(Sound) * SOUND_POOL_SIZE);
    sound->waterSplash = malloc(sizeof(Sound) * SOUND_POOL_SIZE);
    
    // Check for allocation failures
    if (!sound->redPowerup || !sound->bluePowerup || !sound->slowdown || !sound->speedup ||
        !sound->upperBouncer || !sound->click || !sound->bounce1 || !sound->bounce2 ||
        !sound->flipper || !sound->waterSplash) {
        fprintf(stderr, "ERROR: Failed to allocate memory for sound effect arrays\n");
        // Clean up any successful allocations (free() handles NULL safely)
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
        free(sound);
        // Clean up audio resources
        if (g_audioInitialized) {
            StopAudioStream(g_hapticsStream);
            UnloadAudioStream(g_hapticsStream);
            g_audioInitialized = 0;
        }
        CloseAudioDevice();
        return NULL;
    }
    
    // Load sound effects and pan them all fully right for game audio
    sound->launch = LoadSound("Resources/Audio/Click_Heavy_00.wav");
    sound->water = LoadSound("Resources/Audio/water.wav");
    SetSoundPan(sound->launch, 0.0f);  // Pan fully right for speakers
    SetSoundPan(sound->water, 0.0f);   // Pan fully right for speakers
    SetSoundVolume(sound->launch, sound->gameVolume);
    SetSoundVolume(sound->water, sound->gameVolume);
    
    for (int i = 0; i < SOUND_POOL_SIZE; i++){
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
        SetSoundPan(sound->redPowerup[i], 0.0f);
        SetSoundPan(sound->bluePowerup[i], 0.0f);
        SetSoundPan(sound->slowdown[i], 0.0f);
        SetSoundPan(sound->speedup[i], 0.0f);
        SetSoundPan(sound->upperBouncer[i], 0.0f);
        SetSoundPan(sound->click[i], 0.0f);
        SetSoundPan(sound->bounce1[i], 0.0f);
        SetSoundPan(sound->bounce2[i], 0.0f);
        SetSoundPan(sound->flipper[i], 0.0f);
        SetSoundPan(sound->waterSplash[i], 0.0f);
        
        // Set volume for all sounds
        SetSoundVolume(sound->redPowerup[i], sound->gameVolume);
        SetSoundVolume(sound->bluePowerup[i], sound->gameVolume);
        SetSoundVolume(sound->slowdown[i], sound->gameVolume);
        SetSoundVolume(sound->speedup[i], sound->gameVolume);
        SetSoundVolume(sound->upperBouncer[i], sound->gameVolume);
        SetSoundVolume(sound->click[i], sound->gameVolume);
        SetSoundVolume(sound->bounce1[i], sound->gameVolume);
        SetSoundVolume(sound->bounce2[i], sound->gameVolume);
        SetSoundVolume(sound->flipper[i], sound->gameVolume);
        SetSoundVolume(sound->waterSplash[i], sound->gameVolume);
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
    } else if (game->gameState == 1 || game->gameState == 2 || game->gameState == 3){
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
        // Apply both game volume and music volume multiplier
        SetMusicVolume(sound->gameMusic, sound->gameMusicVolume * sound->gameVolume);
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
//
// Round-robin playback with SOUND_POOL_SIZE slots prevents audio cutoff
// during rapid succession. With 8 slots, sounds can overlap naturally
// without needing to stop previous instances in most cases.

// Helper macro for round-robin sound playback
#define PLAY_SOUND_ROUND_ROBIN(soundArray, indexVar) do { \
    int idx = (indexVar); \
    StopSound((soundArray)[idx]); \
    PlaySound((soundArray)[idx]); \
    (indexVar) = (idx + 1) % SOUND_POOL_SIZE; \
} while(0)

void playBounce(SoundManager *sound){
    PLAY_SOUND_ROUND_ROBIN(sound->bounce1, sound->bounce1Index);
}

void playBounce2(SoundManager *sound){
    PLAY_SOUND_ROUND_ROBIN(sound->bounce2, sound->bounce2Index);
}

void playClick(SoundManager *sound){
    PLAY_SOUND_ROUND_ROBIN(sound->click, sound->clickIndex);
}

void playSlowdownSound(SoundManager *sound){
    PLAY_SOUND_ROUND_ROBIN(sound->slowdown, sound->slowdownIndex);
}

void playSpeedupSound(SoundManager *sound){
    PLAY_SOUND_ROUND_ROBIN(sound->speedup, sound->speedupIndex);
}

void playRedPowerupSound(SoundManager *sound){
    PLAY_SOUND_ROUND_ROBIN(sound->redPowerup, sound->redPowerupIndex);
}

void playBluePowerupSound(SoundManager *sound){
    PLAY_SOUND_ROUND_ROBIN(sound->bluePowerup, sound->bluePowerupIndex);
}

void playUpperBouncerSound(SoundManager *sound){
    PLAY_SOUND_ROUND_ROBIN(sound->upperBouncer, sound->upperBouncerIndex);
}

void playLaunch(SoundManager *sound){
    StopSound(sound->launch);
    PlaySound(sound->launch);
}

void playFlipper(SoundManager *sound){
    PLAY_SOUND_ROUND_ROBIN(sound->flipper, sound->flipperIndex);
}

void playWater(SoundManager *sound){
    StopSound(sound->water);
    PlaySound(sound->water);
}

void playWaterSplash(SoundManager *sound){
    PLAY_SOUND_ROUND_ROBIN(sound->waterSplash, sound->waterSplashIndex);
}

// ============================================================================
// HAPTIC EFFECT TRIGGER FUNCTIONS
// ============================================================================
// These functions trigger procedural haptic effects on the left channel.
// They are safe to call from game logic at any time and do not block.
//
// NOTE: The SoundManager parameter is unused but kept for consistency with
// other sound API functions (playBounce, playFlipper, etc.). This allows
// callers to use the same pattern for all audio calls.

void sound_play_haptic_flipper_empty(SoundManager *sound) {
    (void)sound;  // Unused - haptics use global state
    haptics_trigger(HAPTIC_FLIPPER_EMPTY);
}

void sound_play_haptic_flipper_hit(SoundManager *sound) {
    (void)sound;  // Unused - haptics use global state
    haptics_trigger(HAPTIC_FLIPPER_HIT);
}

void sound_play_haptic_launch(SoundManager *sound) {
    (void)sound;  // Unused - haptics use global state
    haptics_trigger(HAPTIC_LAUNCH);
}

void sound_play_haptic_bumper_light(SoundManager *sound) {
    (void)sound;  // Unused - haptics use global state
    haptics_trigger(HAPTIC_BUMPER_LIGHT);
}

void sound_play_haptic_bumper_solid(SoundManager *sound) {
    (void)sound;  // Unused - haptics use global state
    haptics_trigger(HAPTIC_BUMPER_SOLID);
}

void sound_play_haptic_excitement(SoundManager *sound) {
    (void)sound;  // Unused - haptics use global state
    haptics_trigger(HAPTIC_EXCITEMENT);
}

// ============================================================================
// VOLUME CONTROL
// ============================================================================

float sound_getGameVolume(SoundManager *sound) {
    if (!sound) return 1.0f;
    return sound->gameVolume;
}

void sound_setGameVolume(SoundManager *sound, float volume) {
    if (!sound) return;
    
    // Clamp volume to valid range [0.0, 1.0]
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;
    
    sound->gameVolume = volume;
    
    // Update music volumes
    SetMusicVolume(sound->menuMusic, volume);
    SetMusicVolume(sound->gameMusic, volume * sound->gameMusicVolume);
    
    // Update individual sound volumes
    SetSoundVolume(sound->launch, volume);
    SetSoundVolume(sound->water, volume);
    
    // Update sound array volumes
    for (int i = 0; i < SOUND_POOL_SIZE; i++) {
        SetSoundVolume(sound->redPowerup[i], volume);
        SetSoundVolume(sound->bluePowerup[i], volume);
        SetSoundVolume(sound->slowdown[i], volume);
        SetSoundVolume(sound->speedup[i], volume);
        SetSoundVolume(sound->upperBouncer[i], volume);
        SetSoundVolume(sound->click[i], volume);
        SetSoundVolume(sound->bounce1[i], volume);
        SetSoundVolume(sound->bounce2[i], volume);
        SetSoundVolume(sound->flipper[i], volume);
        SetSoundVolume(sound->waterSplash[i], volume);
    }
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
    for (int i = 0; i < SOUND_POOL_SIZE; i++) {
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
    
    // Free the sound manager structure itself
    free(sound);
    
    // Close audio device
    CloseAudioDevice();
}
