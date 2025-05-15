#!/bin/bash
MODE="auto"
BROKER="localhost"
TOPIC="ttt/playerO"
STATE_TOPIC="ttt/gameState"
BOARD_FILE="/tmp/board_o.txt"

# 1. Get latest board state
mosquitto_sub -h "$BROKER" -t "$STATE_TOPIC" -C 1 > "$BOARD_FILE"
BOARD=$(tr -d '\r\n' < "$BOARD_FILE")
BOARD="${BOARD:0:9}"

# 2. Determine if it's O's turn
X_COUNT=$(echo "$BOARD" | grep -o X | wc -l)
O_COUNT=$(echo "$BOARD" | grep -o O | wc -l)

if [ "$X_COUNT" -le "$O_COUNT" ]; then
  echo "Not O's turn — exiting"
  exit 0
fi

# 3. Pick a valid move
MOVES=(00 01 02 10 11 12 20 21 22)
VALID=()
for i in {0..8}; do
  [[ "${BOARD:$i:1}" == " " ]] && VALID+=("${MOVES[i]}")
done
[ ${#VALID[@]} -eq 0 ] && echo "No valid moves" && exit 0

MOVE=${VALID[$((RANDOM % ${#VALID[@]}))]}
echo "O playing: $MOVE"

mosquitto_pub -h "$BROKER" -t "$TOPIC" -m "$MOVE"
