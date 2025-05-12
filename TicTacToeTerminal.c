#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BROKER "34.53.109.99"
#define TOPIC_PLAYER_X "tictactoe/playerX"
#define TOPIC_PLAYER_O "tictactoe/playerO"
#define TOPIC_BOARD_STATE "tictactoe/boardState"
#define TOPIC_INFO "tictactoe/info"
#define TOPIC_ERROR "tictactoe/error"
#define TOPIC_TURN "tictactoe/turn"
#define TOPIC_ALL "tictactoe/#"

void publish_message(const char *topic, const char *message)
{
    char command[256];
    snprintf(command, sizeof(command), "mosquitto_pub -h %s -t %s -m \"%s\"", BROKER, topic, message);
    system(command);
}

void subscribe_and_wait(const char *topic, char *buffer, size_t buffer_size)
{
    char command[256];
    snprintf(command, sizeof(command), "mosquitto_sub -h %s -t %s -C 1", BROKER, topic);
    FILE *fp = popen(command, "r");
    if (fp == NULL)
    {
        fprintf(stderr, "Error: Failed to subscribe to topic %s\n", topic);
        exit(1);
    }
    fgets(buffer, buffer_size, fp);
    buffer[strcspn(buffer, "\n")] = '\0'; // Remove newline character
    pclose(fp);
}

void subscribe_and_display(const char *topic)
{
    char command[256];
    snprintf(command, sizeof(command), "mosquitto_sub -h %s -t %s", BROKER, topic);
    FILE *fp = popen(command, "r");
    if (fp == NULL)
    {
        fprintf(stderr, "Error: Failed to subscribe to topic %s\n", topic);
        exit(1);
    }

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), fp) != NULL)
    {
        buffer[strcspn(buffer, "\n")] = '\0'; // Remove newline character
        printf("Message received: %s\n", buffer);
    }

    pclose(fp);
}

void print_board(const char *board_state)
{
    printf("\nCurrent Board:\n%s\n", board_state);
}

void play_game()
{
    char board_state[256] = "";
    char turn[32] = "";
    int move;

    while (1)
    {
        subscribe_and_wait(TOPIC_BOARD_STATE, board_state, sizeof(board_state));
        print_board(board_state);

        subscribe_and_wait(TOPIC_TURN, turn, sizeof(turn));
        printf("It's your turn (%s)! Enter your move (0-8): ", strstr(turn, "playerX") ? "Player X" : "Player O");
        scanf("%d", &move);

        char move_str[3];
        snprintf(move_str, sizeof(move_str), "%d", move);

        if (strstr(turn, "playerX"))
        {
            publish_message(TOPIC_PLAYER_X, move_str);
        }
        else if (strstr(turn, "playerO"))
        {
            publish_message(TOPIC_PLAYER_O, move_str);
        }

        subscribe_and_wait(TOPIC_INFO, board_state, sizeof(board_state));
        if (strstr(board_state, "wins") || strstr(board_state, "draw"))
        {
            printf("%s\n", board_state);
            break;
        }
    }
}

int main()
{
    printf("Welcome to Tic Tac Toe Terminal!\n");
    printf("This program interacts with the Arduino kit via MQTT.\n");

    subscribe_and_display(TOPIC_ALL);

    play_game();

    printf("Thanks for playing!\n");
    return 0;
}
