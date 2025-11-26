#ifndef HEADER_SCORES
#define HEADER_SCORES
#include "sqlite3.h"

// Maximum number of top scores to track
#define MAX_TOP_SCORES 10

typedef struct {
    char *scoreName;
    int scoreValue;
} ScoreObject;

typedef struct {
    int initialized;
    sqlite3 *db;
    sqlite3_stmt *insertStatement;
    sqlite3_stmt *getTopScoresStatement;
    int scoresUpdated;
    int numTopScores;
    ScoreObject *scores;
} ScoreHelper;

ScoreHelper *initScores();
void submitScore(ScoreHelper *helper, char *name, int score);
ScoreObject *getRankedScore(ScoreHelper *helper, int rank);
int isScoreInTopN(ScoreHelper *helper, int score, int n);
void shutdownScores(ScoreHelper *helper);
int sqlCallback(void *, int, char **, char **);

#endif
