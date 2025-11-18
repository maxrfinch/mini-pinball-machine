#include "powerups.h"
#include "constants.h"
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
