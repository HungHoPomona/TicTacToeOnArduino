#!/bin/bash

BROKER="localhost"
TOPIC_PLAYER_X="tictactoe/playerX"
TOPIC_PLAYER_O="tictactoe/playerO"
TOPIC_BOARD_STATE="tictactoe/boardState"
TOPIC_MENU="tictactoe/#"
TOPIC_TURN="tictactoe/turn"

publish_move() {
    local player_topic=$1
    local move

    while true; do
        move=$((RANDOM % 9)) # Generate a random move between 0 and 8
        if [[ ! " ${used_moves[@]} " =~ " $move " ]]; then
            used_moves+=("$move") # Add the move to the list of used moves
            mosquitto_pub -h "$BROKER" -t "$player_topic" -m "$move"
            echo "Published move $move to $player_topic"
            break
        fi
    done
}

function display_updates() {
    echo "Subscribing to board state and menu updates..."
    mosquitto_sub -h $BROKER -t $TOPIC_BOARD_STATE -t $TOPIC_MENU &
    SUB_PID=$!
}

function play_as_player() {
    echo "Starting Two Players game..."
    while true; do
        TURN=$(mosquitto_sub -h $BROKER -t $TOPIC_TURN -C 1)
        if [[ "$TURN" == "tictactoe/playerX" ]]; then
            echo "Player X, it's your turn! Enter your move (0-8):"
            read move
            if [[ $move =~ ^[0-8]$ ]]; then
                mosquitto_pub -h $BROKER -t $TOPIC_PLAYER_X -m "$move"
                echo "Player X played move $move"
            else
                echo "Invalid move. Please enter a number between 0 and 8."
            fi
        elif [[ "$TURN" == "tictactoe/playerO" ]]; then
            echo "Player O, it's your turn! Enter your move (0-8):"
            read move
            if [[ $move =~ ^[0-8]$ ]]; then
                mosquitto_pub -h $BROKER -t $TOPIC_PLAYER_O -m "$move"
                echo "Player O played move $move"
            else
                echo "Invalid move. Please enter a number between 0 and 8."
            fi
        fi
    done
}

function play_as_bot_O() {
    MAX_GAMES=1
    games_played=0

    echo "Starting Tic Tac Toe game automation..."
    while [[ $games_played -lt $MAX_GAMES ]]; do
        echo "Game $((games_played + 1)) starting..."
        used_moves=() # Reset the list of used moves for the new game

        # Simulate a single game (maximum of 9 moves)
        for ((i = 0; i < 9; i++)); do
            if ((i % 2 == 0)); then
                publish_move "$TOPIC_PLAYER_O"
            fi
            sleep 3 # Delay between moves
        done

        ((games_played++))
        echo -e "\nGame $games_played completed.\n\n"
        sleep 5
    done
    echo "Finished Tic Tac Toe game automation."
}

function play_both_bots() {
    MAX_GAMES=1
    games_played=0

    echo "Starting Tic Tac Toe game automation..."
    while [[ $games_played -lt $MAX_GAMES ]]; do
        echo "Game $((games_played + 1)) starting..."
        used_moves=() # Reset the list of used moves for the new game

        # Simulate a single game (maximum of 9 moves)
        for ((i = 0; i < 9; i++)); do
            if ((i % 2 == 0)); then
                publish_move "$TOPIC_PLAYER_X"
            else
                publish_move "$TOPIC_PLAYER_O"
            fi
            sleep 3 # Delay between moves
        done

        ((games_played++))
        echo -e "\nGame $games_played completed.\n\n"
        sleep 5
    done
    echo "Finished Tic Tac Toe game automation."
}

echo "Select mode:"
echo "1. Play as Player X (Manual)"
echo "2. Play as Player O (Bot)"
echo "3. Bot vs Bot"
read choice

display_updates

case $choice in
    1) play_as_player ;;
    2) play_as_bot_O ;;
    3) play_both_bots ;;
    *) echo "Invalid choice"; kill $SUB_PID; exit 1 ;;
esac

kill $SUB_PID