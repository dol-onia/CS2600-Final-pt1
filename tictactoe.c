#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#define BROKER "34.94.22.125"
#define TOPIC_GAMESTATE "ttt/gameState"
#define TOPIC_RESULT    "ttt/result"

pid_t px_pid = 0;
pid_t po_pid = 0;

void print_board(const char *board) {
    printf("-------------\n");
    for (int i = 0; i < 3; i++) {
        printf("| %c | %c | %c |\n",
            board[i * 3 + 0],
            board[i * 3 + 1],
            board[i * 3 + 2]);
        printf("-------------\n");
    }
}

void cleanup() {
    if (px_pid > 0) kill(px_pid, SIGTERM);
    if (po_pid > 0) kill(po_pid, SIGTERM);
}

int main() {
    int mode = 0;

    printf("===== TIC TAC TOE =====\n");
    printf("1) Human vs Human\n");
    printf("2) Human vs Auto\n");
    printf("3) Auto vs Auto\n");
    printf("4) Exit\n");
    printf("Choose a mode: ");
    scanf("%d", &mode);

    if (mode < 1 || mode > 3) {
        printf("Exiting.\n");
        return 0;
    }

    // Fork player X
    if ((px_pid = fork()) == 0) {
        execlp("bash", "bash", "playerX.sh", (mode == 3 ? "auto" : "human"), NULL);
        perror("Failed to start playerX.sh");
        exit(1);
    }

    // Fork player O
    if ((po_pid = fork()) == 0) {
        execlp("bash", "bash", "playerO.sh", (mode == 1 ? "human" : "auto"), NULL);
        perror("Failed to start playerO.sh");
        exit(1);
    }

    FILE *fp_board = popen("mosquitto_sub -h " BROKER " -t " TOPIC_GAMESTATE, "r");
    FILE *fp_result = popen("mosquitto_sub -h " BROKER " -t " TOPIC_RESULT " -C 1", "r");
    if (!fp_board || !fp_result) {
        perror("Failed to subscribe to MQTT");
        cleanup();
        return 1;
    }

    char board[16] = {0};
    char result[64] = {0};

    while (fgets(board, sizeof(board), fp_board)) {
        board[strcspn(board, "\r\n")] = '\0';
        if (strlen(board) != 9) continue;
        system("clear");
        print_board(board);

        fd_set fds;
        struct timeval tv = {0, 100000};
        FD_ZERO(&fds);
        FD_SET(fileno(fp_result), &fds);
        if (select(fileno(fp_result)+1, &fds, NULL, NULL, &tv) > 0) {
            if (fgets(result, sizeof(result), fp_result)) {
                result[strcspn(result, "\r\n")] = '\0';
                printf("==> %s\n", result);
                break;
            }
        }
    }

    pclose(fp_board);
    pclose(fp_result);
    cleanup();
    return 0;
}
