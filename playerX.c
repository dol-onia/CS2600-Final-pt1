#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define BROKER "34.94.22.125"
#define TOPIC_SUB "ttt/gameState"
#define TOPIC_PUB "ttt/playerX"

const char *moves[9] = {
    "00", "01", "02",
    "10", "11", "12",
    "20", "21", "22"
};

int count_char(const char *str, char ch) {
    int count = 0;
    while (*str) {
        if (*str++ == ch) count++;
    }
    return count;
}

int main() {
    srand(time(NULL)); // seed RNG once

    while (1) {
        // Get current board state
        FILE *fp = popen("mosquitto_sub -h " BROKER " -t " TOPIC_SUB " -C 1 -W 2", "r");
        if (!fp) {
            perror("Failed to subscribe");
            sleep(2);
            continue;
        }

        char board[64] = {0};
        if (!fgets(board, sizeof(board), fp)) {
            pclose(fp);
            sleep(2);
            continue;
        }
        pclose(fp);
        board[strcspn(board, "\r\n")] = '\0';

        if (strlen(board) != 9) {
            fprintf(stderr, "Malformed board: %s\n", board);
            sleep(2);
            continue;
        }

        int x_count = count_char(board, 'X');
        int o_count = count_char(board, 'O');

        // Only move if it's X's turn
        if (x_count > o_count) {
            sleep(1);
            continue;
        }

        // Get list of empty spots
        char *valid[9];
        int count = 0;
        for (int i = 0; i < 9; i++) {
            if (board[i] == ' ') {
                valid[count++] = (char *)moves[i];
            }
        }

        if (count == 0) {
            printf("X: No valid moves\n");
            sleep(2);
            continue;
        }

        const char *chosen = valid[rand() % count];
        printf("X playing: %s\n", chosen);

        // Publish move
        char cmd[128];
        snprintf(cmd, sizeof(cmd),
                 "mosquitto_pub -h %s -t %s -m %s",
                 BROKER, TOPIC_PUB, chosen);
        system(cmd);

        sleep(2); // wait before checking again
    }

    return 0;
}
