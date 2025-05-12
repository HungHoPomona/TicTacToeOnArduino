#!/bin/bash

# Add PATH for CRON environment
export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin

echo "Starting Tic Tac Toe game automation..."

# MQTT broker details
BROKER="localhost"
TOPIC_PLAYER_X="tictactoe/playerX"
TOPIC_PLAYER_O="tictactoe/playerO"

# Game variables
MAX_GAMES=100
games_played=0

# Function to publish a random move that doesn't collide with previous moves
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

# Play 100 games
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
