#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "scores.h"
#include <unistd.h>  // for access() and unlink()

ScoreHelper *initScores(){
    ScoreHelper *helper = malloc(sizeof(ScoreHelper));
    helper->scoresUpdated = 0;
    helper->numTopScores = 0;
    helper->scores = malloc(sizeof(ScoreObject) * MAX_TOP_SCORES);
    helper->initialized = 0;

    int rc = sqlite3_open("Resources/scores.db", &helper->db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(helper->db));
        sqlite3_close(helper->db);
    } else {
        helper->initialized = 1;
        // Create sqlite table if it does not exist.
        char *sql = "CREATE TABLE IF NOT EXISTS Score (id INTEGER PRIMARY KEY, name TEXT, score INTEGER);";
        rc = sqlite3_exec(helper->db, sql, NULL, NULL, NULL);

        // Blank scores with null score objects.
        for (int i = 0; i < MAX_TOP_SCORES; i++){
            helper->scores[i].scoreName = (char*) malloc(12*sizeof(char));
            helper->scores[i].scoreValue = 0;
        }
        // As a test, show top MAX_TOP_SCORES scores
        for (int i = 1; i <= MAX_TOP_SCORES; i++){
            ScoreObject *score = getRankedScore(helper,i);
            if (score != NULL){
                printf("%d %s = %d\n",i,score->scoreName,score->scoreValue);
            } else {
                printf("%d NULL\n",i);
            }
        }
    }
    return helper;
}

void shutdownScores(ScoreHelper *helper){
    if (helper->initialized == 1){
        sqlite3_close(helper->db);
        helper->initialized = 0;
    }
    
    // Free allocated score name strings
    if (helper->scores != NULL) {
        for (int i = 0; i < MAX_TOP_SCORES; i++) {
            if (helper->scores[i].scoreName != NULL) {
                free(helper->scores[i].scoreName);
                helper->scores[i].scoreName = NULL;
            }
        }
        // Free the scores array itself
        free(helper->scores);
        helper->scores = NULL;
    }
    
    // Free the helper structure itself
    free(helper);
}
void submitScore(ScoreHelper *helper, char *name, int score){
    if (helper->initialized == 1){
        // invalidate scores
        helper->scoresUpdated = 0;
        int rc = sqlite3_prepare_v2(helper->db, "insert into Score(name,score) values (?,?)", -1, &helper->insertStatement, NULL);
        printf("insert: %d\n",rc);
        sqlite3_bind_text(helper->insertStatement, 1, name, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(helper->insertStatement, 2, score);
        rc = sqlite3_step(helper->insertStatement);
        sqlite3_finalize(helper->insertStatement);
        printf("submitted score status = %d\n",rc);
        
        // NEW CODE: Check if we need to delete the old 3rd place photo
        // Query for the 4th place entry (if it exists, it was displaced from 3rd)
        sqlite3_stmt *checkStmt;
        const char *checkSql = "SELECT name, score FROM Score ORDER BY score DESC LIMIT 1 OFFSET 3;";
        if (sqlite3_prepare_v2(helper->db, checkSql, -1, &checkStmt, NULL) == SQLITE_OK) {
            if (sqlite3_step(checkStmt) == SQLITE_ROW) {
                // There's a 4th place entry - it was bumped from 3rd
                const char *oldName = (const char*)sqlite3_column_text(checkStmt, 0);
                int oldScore = sqlite3_column_int(checkStmt, 1);
                
                // Sanitize the name the same way as in menu.c
                char sanitizedName[64];
                int nameLen = strlen(oldName);
                if (nameLen >= 64) nameLen = 63;
                
                for (int i = 0; i < nameLen; i++) {
                    char c = toupper(oldName[i]);
                    if (c == ' ' || c < 'A' || c > 'Z') {
                        sanitizedName[i] = '_';
                    } else {
                        sanitizedName[i] = c;
                    }
                }
                sanitizedName[nameLen] = '\0';
                
                // Delete the old photo
                char photoPath[256];
                snprintf(photoPath, sizeof(photoPath), "Resources/Photos/%s_%d.png", sanitizedName, oldScore);
                if (access(photoPath, F_OK) == 0) {
                    unlink(photoPath);
                    printf("Deleted old 3rd place photo: %s\n", photoPath);
                }
            }
            sqlite3_finalize(checkStmt);
        }
    }
}

ScoreObject *getRankedScore(ScoreHelper *helper, int rank){
    if (helper->initialized == 1){
        // if score table is invalid, update it.
        if (helper->scoresUpdated == 0){
            char *sql1 = "drop table if exists SortedScores;";
            char *sql2 = "create table SortedScores as select name, score from Score ORDER BY score DESC LIMIT 10;";
            char *sql3 = "select rowid, name, score from SortedScores order by rowid;";
            printf("Loading score table\n");
            helper->numTopScores = 0;
            int rc1 = sqlite3_exec(helper->db, sql1, NULL, NULL, NULL);
            int rc2 = sqlite3_exec(helper->db, sql2, NULL, NULL, NULL);
            int rc3 = sqlite3_exec(helper->db, sql3, sqlCallback, helper, NULL);
            printf("%d %d %d\n",rc1,rc2,rc3);
            helper->scoresUpdated = 1;
        }
        // Check that number range is valid.
        if (rank > 0 && rank <= helper->numTopScores){
            return &helper->scores[rank - 1];
        } else {
            return NULL;
        }
    } else {
        return NULL;
    }
}

// Check if a score would be in the top N scores
int isScoreInTopN(ScoreHelper *helper, int score, int n) {
    if (helper->initialized != 1 || n <= 0 || n > MAX_TOP_SCORES) {
        return 0;
    }
    
    // Ensure scores are up to date
    if (helper->scoresUpdated == 0) {
        // Force update by calling getRankedScore for rank 1
        getRankedScore(helper, 1);
    }
    
    // If we have fewer than n scores, the new score automatically qualifies
    if (helper->numTopScores < n) {
        return 1;
    }
    
    // Check if score is greater than the nth score
    if (n <= helper->numTopScores) {
        return score > helper->scores[n - 1].scoreValue;
    }
    
    return 0;
}

// Called for each row in the score query
int sqlCallback(void *helper, int argc, char **argv, char **azColName){
    ScoreHelper *tempHelper = (ScoreHelper*)helper;
    char *rowId = argv[0];
    int rowIdInt = atoi(rowId) - 1;
    char *scoreName = argv[1];
    char *scoreValue = argv[2];
    printf("%d %s %s \n",rowIdInt, scoreName, scoreValue);
    if (rowIdInt >= 0 && rowIdInt < MAX_TOP_SCORES){
        strcpy(tempHelper->scores[rowIdInt].scoreName, scoreName);
        int j = 0;
        while (tempHelper->scores[rowIdInt].scoreName[j]) {
            tempHelper->scores[rowIdInt].scoreName[j] = toupper(tempHelper->scores[rowIdInt].scoreName[j]);
            j++;
        }
        tempHelper->scores[rowIdInt].scoreValue = atoi(scoreValue);
        tempHelper->numTopScores++;
    }
    return 0;
}
