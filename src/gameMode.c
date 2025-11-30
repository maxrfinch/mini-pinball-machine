#include "gameMode.h"
#include "modes/classicMode.h"

// ---------------------------------------------------------------------------
// Mode registry - array of all available game modes
// ---------------------------------------------------------------------------

static const GameModeConfig *modeRegistry[] = {
    &MODE_CONFIG_CLASSIC
};

static const int numModes = sizeof(modeRegistry) / sizeof(modeRegistry[0]);

// ---------------------------------------------------------------------------
// Helper function implementations
// ---------------------------------------------------------------------------

const GameModeConfig *GetModeConfig(GameMode mode) {
    // MODE_CLASSIC is 0, so we can use it directly as index
    if ((int)mode >= 0 && (int)mode < numModes) {
        return modeRegistry[(int)mode];
    }
    // Default to classic mode if invalid mode specified
    return modeRegistry[0];
}

int GetNumModes(void) {
    return numModes;
}
