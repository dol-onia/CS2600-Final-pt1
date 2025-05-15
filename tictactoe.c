#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/select.h>
#include <time.h>
#include <ctype.h>

#define BROKER "34.94.22.125"
#define TOPIC_GAMESTATE "ttt/gameState"
#define TOPIC_RESULT "ttt/result"
#define TOPIC_PLAYERX "ttt/playerX"
#define TOPIC_PLAYERO "ttt/playerO"

#define GCP_O_COMMAND "gcloud compute ssh dolonia0524@mqtt-server --quiet --zone=us-west2-c --command='bash ~/finalpt1/playerO.sh' > /dev/null 2>&1"

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
    if (po_pid > 0) {
        kill(po_pid, SIGTERM);
        waitpid(po_pid, NULL, 0);
    }
}

int is_valid_move(const char *move) {
    if (strlen(move) != 2) return 0;
    if (!isdigit(move[0]) || !isdigit(move[1])) return 0;
    int row = move[0] - '0';
    int col = move[1] - '0';
    return (row >= 0 && row <= 2 && col >= 0 && col <= 2);
}

int main() {
    int mode = 0;

    printf("===== TIC TAC TOE =====\n");
    printf("1) Human vs Bash AI (1-Player)\n");
    printf("2) Human vs Human\n");
    printf("3) C AI vs Bash AI (Auto)\n");
    printf("4) Exit\n");
    printf("Choose a mode: ");
    scanf("%d", &mode);

    if (mode < 1 || mode > 3) {
        printf("Exiting.\n");
        return 0;
    }

    // Reset board and result topics (clear retained messages)
    system("mosquitto_pub -r -h " BROKER " -t " TOPIC_GAMESTATE " -m \"         \"");
    system("mosquitto_pub -r -n -h " BROKER " -t " TOPIC_RESULT);
    system("mosquitto_pub -h " BROKER " -t " TOPIC_PLAYERX " -m \"reset\"");
    sleep(1); // Allow ESP32 to process reset and publish clean board

    if (mode == 1 || mode == 3) {
        // Launch Bash AI (playerO.sh) in background via gcloud
        if ((po_pid = fork()) == 0) {
            execlp("bash", "bash", "-l", "-c", GCP_O_COMMAND, NULL);
            perror("Failed to start playerO via gcloud SSH");
            exit(1);
        }
    }

    FILE *fp_board = popen("mosquitto_sub -h " BROKER " -t " TOPIC_GAMESTATE, "r");
    FILE *fp_result = popen("mosquitto_sub -h " BROKER " -t " TOPIC_RESULT, "r");

    if (!fp_board || !fp_result) {
        perror("Failed to subscribe");
        cleanup();
        return 1;
    }

    char board[64] = {0};
    char result[64] = {0};

    while (1) {
        fd_set fds;
        struct timeval tv = {1, 0};
        FD_ZERO(&fds);
        FD_SET(fileno(fp_board), &fds);
        FD_SET(fileno(fp_result), &fds);

        int maxfd = (fileno(fp_board) > fileno(fp_result)) ? fileno(fp_board) : fileno(fp_result);

        int ret = select(maxfd + 1, &fds, NULL, NULL, &tv);
        if (ret < 0) {
            perror("select error");
            break;
        }

        if (FD_ISSET(fileno(fp_result), &fds)) {
            if (fgets(result, sizeof(result), fp_result)) {
                result[strcspn(result, "\r\n")] = '\0';
                system("clear");
                printf("==> %s\n", result);
                break;
            }
        }

        if (FD_ISSET(fileno(fp_board), &fds) && strlen(result) == 0) {
            if (!fgets(board, sizeof(board), fp_board)) continue;
            board[strcspn(board, "\r\n")] = '\0';
            if (strlen(board) != 9) continue;

            system("clear");
            print_board(board);

            if (mode != 3) {
                int x_count = 0, o_count = 0;
                for (int i = 0; i < 9; i++) {
                    if (board[i] == 'X') x_count++;
                    if (board[i] == 'O') o_count++;
                }

                char move[8];
                if (mode == 2) {
                    if (x_count <= o_count) {
                        do {
                            printf("Your move (X): ");
                            scanf("%s", move);
                        } while (!is_valid_move(move));
                        char cmd[128];
                        snprintf(cmd, sizeof(cmd),
                                 "mosquitto_pub -h %s -t %s -m %s", BROKER, TOPIC_PLAYERX, move);
                        system(cmd);
                    } else {
                        do {
                            printf("Your move (O): ");
                            scanf("%s", move);
                        } while (!is_valid_move(move));
                        char cmd[128];
                        snprintf(cmd, sizeof(cmd),
                                 "mosquitto_pub -h %s -t %s -m %s", BROKER, TOPIC_PLAYERO, move);
                        system(cmd);
                    }
                } else if (mode == 1 && x_count <= o_count) {
                    do {
                        printf("Your move (X): ");
                        scanf("%s", move);
                    } while (!is_valid_move(move));
                    char cmd[128];
                    snprintf(cmd, sizeof(cmd),
                             "mosquitto_pub -h %s -t %s -m %s", BROKER, TOPIC_PLAYERX, move);
                    system(cmd);
                }
            }
        }
    }

    pclose(fp_board);
    pclose(fp_result);
    cleanup();
    return 0;
}
