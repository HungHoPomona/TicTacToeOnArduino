#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <stdlib.h> // For random()

#define SDA 13
#define SCL 14

const char *ssid = "MEOHEO";
const char *password = "0913753545";
const char *mqtt_server = "34.127.109.32";

WiFiClient espClient;
PubSubClient client(espClient);

LiquidCrystal_I2C lcd(0x27, 16, 2);

int board[3][3];
int currentPlayer = 1;
int xWins = 0, oWins = 0, draws = 0, gamesPlayed = 0;
bool gameOver = false;
bool awaitingMove = false;
char currentGameMode[32] = ""; // Global variable to track the current game mode

char lastMessage[32] = "";       // Global variable to store the latest message
bool newMessageReceived = false; // Flag to indicate a new message was received

// Function prototypes
void setup_wifi();
void resetBoard();
void checkWin();
void announceWinner(int winner);
void displayScore();
void printBoardToSerial();
void switchPlayers();
char symbolForPlayer(int player);
void reconnect();
void callback(char *topic, byte *message, unsigned int length);
void publishBoardState();
void displayMenu();
void handleMenuChoice(int choice);
void startPlayerVsBot();
void startTwoPlayers();
void startBotVsBot();
void resetScores();
void handlePlayerVsPlayerEnd();
void handlePlayerVsGCPEnd();
void handleBotVsBotEnd();

void setup_wifi()
{
    delay(10);
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
}

bool i2CAddrTest(uint8_t addr)
{
    Wire.beginTransmission(addr);
    return (Wire.endTransmission() == 0);
}

void publishBoardState()
{
    char boardState[64] = "";
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            char cell = board[i][j] == 1 ? 'X' : board[i][j] == 2 ? 'O'
                                                                  : '.';
            strncat(boardState, &cell, 1);
            if (j < 2)
                strncat(boardState, "|", 1);
        }
        if (i < 2)
            strncat(boardState, "\n", 1);
    }
    client.publish("tictactoe/boardState", boardState);
}

void callback(char *topic, byte *message, unsigned int length)
{
    char msg[length + 1];
    strncpy(msg, (char *)message, length);
    msg[length] = '\0';

    if (strcmp(topic, "tictactoe/menuChoice") == 0)
    {
        int choice = atoi(msg);
        handleMenuChoice(choice);
        return;
    }

    if (strcmp(topic, "tictactoe/keepPlaying") == 0)
    {
        strncpy(lastMessage, msg, sizeof(lastMessage) - 1);
        lastMessage[sizeof(lastMessage) - 1] = '\0';
        newMessageReceived = true;
        return;
    }

    if (!awaitingMove)
        return;

    Serial.printf("Message received on topic %s: %s\n", topic, msg);

    if ((currentPlayer == 1 && strcmp(topic, "tictactoe/playerX") != 0) ||
        (currentPlayer == 2 && strcmp(topic, "tictactoe/playerO") != 0))
    {
        Serial.println("Invalid move: Not this player's turn.");
        return;
    }

    int move = atoi(msg);
    int row = move / 3;
    int col = move % 3;

    if (row >= 0 && row < 3 && col >= 0 && col < 3 && board[row][col] == 0)
    {
        board[row][col] = currentPlayer;
        awaitingMove = false;
        printBoardToSerial();
        publishBoardState();
        checkWin();

        if (!gameOver)
            switchPlayers();
        else
            handlePlayerVsPlayerEnd();
    }
    else
    {
        Serial.println("Invalid move: Position already occupied or out of bounds.");
        client.publish("tictactoe/error", "Invalid move");
    }
}

void printBoardToSerial()
{
    Serial.println("Current Board:");
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            Serial.print(board[i][j] == 1 ? " X " : board[i][j] == 2 ? " O "
                                                                     : " . ");
            if (j < 2)
                Serial.print("|");
        }
        Serial.println();
        if (i < 2)
            Serial.println("---+---+---");
    }
    Serial.println();
}

char symbolForPlayer(int player)
{
    return player == 1 ? 'X' : player == 2 ? 'O'
                                           : ' ';
}

void switchPlayers()
{
    currentPlayer = (currentPlayer == 1) ? 2 : 1;
    awaitingMove = true;
    displayScore();
    delay(3000);
}

void reconnect()
{
    while (!client.connected())
    {
        Serial.print("Attempting MQTT connection...");
        if (client.connect("ESP32Client"))
        {
            Serial.println("connected");
            client.subscribe("tictactoe/playerX");
            client.subscribe("tictactoe/playerO");
            client.subscribe("tictactoe/menuChoice");
            client.subscribe("tictactoe/keepPlaying");
        }
        else
        {
            Serial.printf("failed, rc=%d. Retrying in 5 seconds...\n", client.state());
            delay(5000);
        }
    }
}

void setup()
{
    Serial.begin(115200);
    Serial.println("Booting...");
    Wire.begin(SDA, SCL);
    Serial.println("I2C initialized");
    if (!i2CAddrTest(0x27))
        lcd = LiquidCrystal_I2C(0x3F, 16, 2);
    lcd.init();
    lcd.backlight();

    setup_wifi();
    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);

    resetBoard();
}

void displayMenu()
{
    char menu[256];
    snprintf(menu, sizeof(menu),
             "\n--- Tic Tac Toe Menu ---\n"
             "1. Player vs Bash Script (GCP)\n"
             "2. Two Players on Different Computers\n"
             "3. GCP vs GCP\n"
             "Enter your choice: ");
    client.publish("tictactoe/menu", menu); // Publish the menu to the MQTT topic
    Serial.println(menu);                   // Display the menu on the serial monitor
}

void startPlayerVsBot()
{
    Serial.println("Player vs Bash Script (GCP) selected.");
    client.publish("tictactoe/mode", "Player vs GCP");

    currentPlayer = 1;   // Player X starts
    awaitingMove = true; // Ensure the game waits for Player X's move
    resetBoard();

    Serial.println("Game started! Here is the initial board:");
    printBoardToSerial();

    // Notify Player X to start
    client.publish("tictactoe/turn", "tictactoe/playerX");
    Serial.println("Player X, it's your turn. Publish your move to the appropriate topic.");

    // Main game loop
    while (!gameOver)
    {
        if (awaitingMove)
        {
            char topic[32];
            sprintf(topic, "tictactoe/player%c", currentPlayer == 1 ? 'X' : 'O');
            client.publish("tictactoe/turn", topic);
            Serial.printf("Waiting for Player %c's move via MQTT...\n", currentPlayer == 1 ? 'X' : 'O');

            // Wait for the move to be processed
            while (awaitingMove)
            {
                client.loop(); // Process incoming MQTT messages
                delay(100);    // Avoid busy-waiting
            }
        }

        if (gameOver)
        {
            displayScore();
            delay(1500);
            resetBoard();
            gameOver = false;

            client.publish("tictactoe/gameNumber", String(gamesPlayed).c_str());
            Serial.println("Game over. Restarting Player vs GCP mode...");
            break; // Exit the loop to allow restarting the game
        }

        // If it's the bot's turn, simulate a move
        if (currentPlayer == 2 && !awaitingMove)
        {
            int botMove = random(0, 9); // Generate a random move
            int row = botMove / 3;
            int col = botMove % 3;

            if (board[row][col] == 0) // Ensure the move is valid
            {
                board[row][col] = currentPlayer;
                Serial.printf("Bot (Player O) played move %d (row: %d, col: %d)\n", botMove, row, col);
                printBoardToSerial();
                publishBoardState();
                checkWin();

                if (!gameOver)
                    switchPlayers();
            }
        }
    }
}

void startTwoPlayers()
{
    Serial.println("Two Players on Different Computers selected.");
    client.publish("tictactoe/mode", "Two Players");
    currentPlayer = 1;   // Player X starts
    awaitingMove = true; // Ensure the game waits for Player X's move
    resetBoard();
    Serial.println("Game started! Here is the initial board:");
    printBoardToSerial();
    Serial.printf("Player %c, it's your turn. Publish your move to the appropriate topic.\n", symbolForPlayer(currentPlayer));
    client.publish("tictactoe/turn", "tictactoe/playerX");
}

void startBotVsBot()
{
    Serial.println("GCP vs GCP selected.");
    client.publish("tictactoe/mode", "GCP vs GCP");
    currentPlayer = 1;   // GCP X starts
    awaitingMove = true; // Wait for GCP moves
    resetBoard();
    Serial.println("Game started! Here is the initial board:");
    printBoardToSerial();
    Serial.println("Waiting for GCP bots to play...");
}

void handleMenuChoice(int choice)
{
    switch (choice)
    {
    case 1:
        strcpy(currentGameMode, "Player vs GCP");
        startPlayerVsBot();
        break;
    case 2:
        strcpy(currentGameMode, "Two Players");
        startTwoPlayers();
        break;
    case 3:
        strcpy(currentGameMode, "GCP vs GCP");
        startBotVsBot();
        break;
    default:
        Serial.println("Invalid choice. Please try again.");
        client.publish("tictactoe/error", "Invalid menu choice");
        return;
    }
}

void resetScores()
{
    xWins = 0;
    oWins = 0;
    draws = 0;
    gamesPlayed = 0;
    Serial.println("Scores have been reset.");
    client.publish("tictactoe/info", "Scores have been reset.");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Scores Reset!");
    delay(2000);
}

void handlePlayerVsPlayerEnd()
{
    // Ask the user if they want to keep playing or return to the menu
    Serial.println("Do you want to keep playing? (yes/no)");
    client.publish("tictactoe/info", "Do you want to keep playing? (yes/no)");

    while (true)
    {
        client.loop(); // Process incoming MQTT messages
        delay(100);    // Avoid busy-waiting

        if (newMessageReceived)
        {
            newMessageReceived = false; // Reset the flag

            if (strcmp(lastMessage, "yes") == 0)
            {
                Serial.println("Continuing Player vs Player mode...");
                client.publish("tictactoe/info", "Continuing Player vs Player mode...");
                startTwoPlayers(); // Restart Player vs Player mode
                break;
            }
            else if (strcmp(lastMessage, "no") == 0)
            {
                Serial.println("Returning to menu and resetting scores...");
                client.publish("tictactoe/info", "Returning to menu and resetting scores...");
                resetScores(); // Reset scores and return to menu
                break;
            }
            else
            {
                Serial.println("Invalid input received via MQTT. Please send 'yes' or 'no'.");
                client.publish("tictactoe/error", "Invalid input. Please send 'yes' or 'no'.");
            }
        }
    }
}

void handlePlayerVsGCPEnd()
{
    Serial.println("Do you want to keep playing? (yes/no)");
    client.publish("tictactoe/info", "Do you want to keep playing? (yes/no)");

    while (true)
    {
        client.loop(); // Process incoming MQTT messages
        delay(100);

        if (newMessageReceived)
        {
            newMessageReceived = false; // Reset the flag

            if (strcmp(lastMessage, "yes") == 0)
            {
                Serial.println("Continuing Player vs Player mode...");
                client.publish("tictactoe/info", "Continuing Player vs Player mode...");
                startTwoPlayers(); // Restart Player vs Player mode
                break;
            }
            else if (strcmp(lastMessage, "no") == 0)
            {
                Serial.println("Returning to menu and resetting scores...");
                client.publish("tictactoe/info", "Returning to menu and resetting scores...");
                resetScores(); // Reset scores and return to menu
                break;
            }
            else
            {
                Serial.println("Invalid input received via MQTT. Please send 'yes' or 'no'.");
                client.publish("tictactoe/error", "Invalid input. Please send 'yes' or 'no'.");
            }
        }

        if (Serial.available() > 0)
        {
            String input = Serial.readStringUntil('\n');
            input.trim();

            if (input.equalsIgnoreCase("yes"))
            {
                Serial.println("Continuing Player vs GCP mode...");
                client.publish("tictactoe/info", "Continuing Player vs GCP mode...");
                startPlayerVsBot(); // Restart Player vs GCP mode
                break;
            }
            else if (input.equalsIgnoreCase("no"))
            {
                Serial.println("Returning to menu and resetting scores...");
                client.publish("tictactoe/info", "Returning to menu and resetting scores...");
                resetScores(); // Reset scores and return to menu
                break;
            }
            else
            {
                Serial.println("Invalid input. Please type 'yes' or 'no'.");
                client.publish("tictactoe/error", "Invalid input. Please type 'yes' or 'no'.");
            }
        }
    }
}

void handleBotVsBotEnd()
{
    loop();
}

void loop()
{
    if (!client.connected())
        reconnect();
    client.loop();

    static bool menuSelected = false;
    if (!menuSelected)
    {
        displayMenu();
        Serial.println("Waiting for menu input via MQTT...");
        while (!menuSelected)
        {
            client.loop(); // Process incoming MQTT messages
            delay(100);    // Avoid busy-waiting
        }
    }

    if (gamesPlayed >= 100)
    {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Games Complete!");
        lcd.setCursor(0, 1);
        lcd.printf("X:%d O:%d", xWins, oWins);
        return;
    }

    if (gameOver)
    {
        displayScore();
        delay(1500);
        resetBoard();
        gameOver = false;

        client.publish("tictactoe/gameNumber", String(gamesPlayed).c_str());

        // Handle end of game for different modes
        if (strcmp(currentGameMode, "Two Players") == 0)
        {
            handlePlayerVsPlayerEnd();
        }
        else if (strcmp(currentGameMode, "Player vs GCP") == 0)
        {
            handlePlayerVsGCPEnd();
        }
        else
        {
            handleBotVsBotEnd();
        }
    }

    if (awaitingMove)
    {
        char topic[32];
        sprintf(topic, "tictactoe/player%c", currentPlayer == 1 ? 'X' : 'O');
        client.publish("tictactoe/turn", topic);
        Serial.printf("Waiting for Player %c's move via MQTT...\n", currentPlayer == 1 ? 'X' : 'O');
        while (awaitingMove)
        {
            client.loop(); // Process incoming MQTT messages
            delay(100);    // Avoid busy-waiting
        }
    }
}

void resetBoard()
{
    memset(board, 0, sizeof(board));
    printBoardToSerial();
    publishBoardState(); // Publish the initial board state
}

void checkWin()
{
    for (int i = 0; i < 3; i++)
    {
        if (board[i][0] != 0 && board[i][0] == board[i][1] && board[i][1] == board[i][2])
        {
            announceWinner(board[i][0]);
            return;
        }
        if (board[0][i] != 0 && board[0][i] == board[1][i] && board[1][i] == board[2][i])
        {
            announceWinner(board[0][i]);
            return;
        }
    }
    if (board[0][0] != 0 && board[0][0] == board[1][1] && board[1][1] == board[2][2])
    {
        announceWinner(board[0][0]);
        return;
    }
    if (board[0][2] != 0 && board[0][2] == board[1][1] && board[1][1] == board[2][0])
    {
        announceWinner(board[0][2]);
        return;
    }

    bool full = true;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            if (board[i][j] == 0)
                full = false;

    if (full)
    {
        draws++;
        gameOver = true;
        gamesPlayed++;
    }
}

void announceWinner(int winner)
{
    gameOver = true;

    char winnerMsg[32];

    if (winner == 1)
    {
        xWins++;
        snprintf(winnerMsg, sizeof(winnerMsg), "Player X Wins!");
    }
    else if (winner == 2)
    {
        oWins++;
        snprintf(winnerMsg, sizeof(winnerMsg), "Player O Wins!");
    }

    gamesPlayed++;

    // Publish winner message to MQTT
    client.publish("tictactoe/turn", winnerMsg);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.printf("Player %c Wins!", winner == 1 ? 'X' : 'O');
    lcd.setCursor(0, 1);
    lcd.printf("X:%d O:%d", xWins, oWins);

    Serial.printf("Player %c Wins!\n", winner == 1 ? 'X' : 'O');
    delay(5000);
}

void displayScore()
{
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.printf("Game %d of 100", gamesPlayed + 1);
    lcd.setCursor(0, 1);
    lcd.printf("X:%d  O:%d  D:%d", xWins, oWins, draws);
}
```