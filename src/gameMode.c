#include "gameMode.h"
#include "modes/classicMode.h"

// ---------------------------------------------------------------------------
// Mode registry - array of all available game modes
// Note: Mode entries must be in the same order as the GameMode enum in gameStruct.h
// ---------------------------------------------------------------------------

static const GameModeConfig *modeRegistry[] = {
    &MODE_CONFIG_CLASSIC    // MODE_CLASSIC = 0
};

static const int numModes = sizeof(modeRegistry) / sizeof(modeRegistry[0]);

// ---------------------------------------------------------------------------
// Helper function implementations
// ---------------------------------------------------------------------------

const GameModeConfig *GetModeConfig(GameMode mode) {
    // Use mode enum value as array index
    // Note: This requires modeRegistry to be in the same order as GameMode enum
    int modeIndex = (int)mode;
    if (modeIndex >= 0 && modeIndex < numModes) {
        return modeRegistry[modeIndex];
    }
    // Default to classic mode if invalid mode specified
    return modeRegistry[0];
}

int GetNumModes(void) {
    return numModes;
}
