#include "powerups.h"
#include "constants.h"
#include "game.h"
#include <math.h>

void Powerups_Init(GameStruct *game, PowerupSystem *ps) {
    ps->slowMotionFactor = 1.0f;
    ps->iceOverlayAlpha = 0.0f;
    ps->multiballOverlayY = 20.0f + worldHeight;
    
    game->slowMotion = 0;
    game->slowMotionCounter = 0;
    game->slowMoPowerupAvailable = 1;
    game->slowMoCooldownTimer = 0.0f;
    game->slowMoCooldownBaselineLives = game->numLives;
    game->slowMoExplosionEffect = 0.0f;
}

void Powerups_Update(GameStruct *game,
                     PowerupSystem *ps,
                     InputManager *input,
                     SoundManager *sound,
                     float effectiveTimestep) {
    
    // Validate slowMotionFactor to prevent bad timesteps
    if (!isfinite(ps->slowMotionFactor) || ps->slowMotionFactor <= 0.0f) {
        ps->slowMotionFactor = 1.0f;
    }

    // Update ice overlay based on slow-motion state
    if (game->slowMotion == 1) {
        ps->slowMotionFactor = 0.3f;
        ps->iceOverlayAlpha += 0.01f;
        if (ps->iceOverlayAlpha >= 1.0f) {
            ps->iceOverlayAlpha = 1.0f;
        }
    } else {
        if (ps->slowMotionFactor < 1.0f) {
            ps->slowMotionFactor += 0.05f;
            if (ps->slowMotionFactor > 1.0f) {
                ps->slowMotionFactor = 1.0f;
            }
        }
        ps->iceOverlayAlpha -= 0.01f;
        if (ps->iceOverlayAlpha <= 0.0f) {
            ps->iceOverlayAlpha = 0.0f;
        }
    }

    // Update slow-motion counter
    if (game->slowMotionCounter > 0) {
        game->slowMotionCounter--;
        if (game->slowMotionCounter <= 0) {
            game->slowMotion = 0;
            playSpeedupSound(sound);
            
            // Start cooldown timer after slow-mo ends
            if (game->slowMoPowerupAvailable == 0) {
                game->slowMoCooldownTimer = slowMoCooldownDuration;
                game->slowMoCooldownBaselineLives = game->numLives;
            }
        }
    }
    
    // Update explosion effect decay (affected by slow-motion for consistency)
    if (game->slowMoExplosionEffect > 0.0f) {
        game->slowMoExplosionEffect -= 0.05f * ps->slowMotionFactor;
        if (game->slowMoExplosionEffect < 0.0f) {
            game->slowMoExplosionEffect = 0.0f;
        }
    }
    
    // Update slow-mo cooldown timer (stay-alive requirement)
    if (game->slowMoCooldownTimer > 0.0f && game->slowMoPowerupAvailable == 0) {
        // Count down using effectiveTimestep for consistency
        game->slowMoCooldownTimer -= effectiveTimestep;
        
        // Check if ball was lost (lives decreased)
        if (game->numLives < game->slowMoCooldownBaselineLives) {
            // Ball lost - reset timer and update baseline
            game->slowMoCooldownTimer = slowMoCooldownDuration;
            game->slowMoCooldownBaselineLives = game->numLives;
        }
        
        // Check if cooldown complete
        if (game->slowMoCooldownTimer <= 0.0f) {
            game->slowMoCooldownTimer = 0.0f;
            game->slowMoPowerupAvailable = 1;
            // TODO: play slow-mo ready sound
        }
    }

    // Update red and blue powerup overlays
    if (game->redPowerupOverlay > 0.0f) {
        game->redPowerupOverlay -= 0.02f * ps->slowMotionFactor;
        if (game->redPowerupOverlay <= 0.0f) {
            game->redPowerupOverlay = 0.0f;
        }
    }
    if (game->bluePowerupOverlay > 0.0f) {
        game->bluePowerupOverlay -= 0.04f * ps->slowMotionFactor;
        if (game->bluePowerupOverlay <= 0.0f) {
            game->bluePowerupOverlay = 0.0f;
        }
    }
    
    // Store slowMotionFactor back to game for physics integration
    game->slowMotionFactor = ps->slowMotionFactor;
}

void Powerups_CheckMultiball(GameStruct *game, Bumper *bumpers, SoundManager *sound, InputManager *input) {
    // Check powerups before dispensing balls
    if (game->ballPowerupState == 0 && !bumpers[7].enabled && !bumpers[8].enabled && !bumpers[9].enabled){
        // spawn balls (multiball powerup)
        Game_SpawnMultiBalls(game, 89.5 - ballSize / 2, 160, 3, 0, -220, 1);
        playBluePowerupSound(sound);
        sound_play_haptic_excitement(sound);
        game->bluePowerupOverlay = 1.0f;
        game->ballPowerupState = -1;
        game->gameScore += 500;
        if (game->waterPowerupState == 0){
            game->powerupScore += 500;
        }
        // Send multiball animation to controller
        inputSendMultiballAnimation(input);
    } else if (game->ballPowerupState == -1){
        // Check if there are no balls left. Then powerup resets and bumpers reset.
        if (game->numBalls == 0){
            game->ballPowerupState = 0;
            bumpers[7].enabled = 1;
            bumpers[8].enabled = 1;
            bumpers[9].enabled = 1;
        }
    }
}

void Powerups_CheckBumperPowerup(GameStruct *game, Bumper *bumpers, SoundManager *sound) {
    if (game->bumperPowerupState == 0 && !bumpers[4].enabled && !bumpers[5].enabled && !bumpers[6].enabled){
        // spawn bumpers
        game->bumperPowerupState = -1;
        bumpers[10].enabled = 1;
        bumpers[11].enabled = 1;
        bumpers[12].enabled = 1;
        bumpers[13].enabled = 1;
        playRedPowerupSound(sound);
        sound_play_haptic_excitement(sound);
        game->redPowerupOverlay = 1.0f;
        game->gameScore += 500;
        if (game->waterPowerupState == 0){
            game->powerupScore += 500;
        }
    } else if (game->bumperPowerupState == -1){
        if (!bumpers[10].enabled && !bumpers[11].enabled && !bumpers[12].enabled && !bumpers[13].enabled){
            game->bumperPowerupState = 0;
            bumpers[4].enabled = 1;
            bumpers[5].enabled = 1;
            bumpers[6].enabled = 1;
            game->redPowerupOverlay = 1.0f;
        }
    }
}
