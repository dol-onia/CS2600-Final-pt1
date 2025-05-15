#include <WiFi.h>
#include <PubSubClient.h>
#include <LiquidCrystal_I2C.h>


LiquidCrystal_I2C lcd(0x27, 16, 2);


const char* ssid = "moose i fone";
const char* password = "Wed051599!";
const char* mqtt_server = "test.mosquitto.org";
bool resetInProgress = false;


WiFiClient espClient;
PubSubClient client(espClient);


char board[3][3];
int gamesPlayed = 0;
int xWins = 0, oWins = 0, draws = 0;
bool isXTurn = true;
bool gameActive = true;


void clearBoard() {
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 3; ++j)
      board[i][j] = ' ';
}


void printBoard() {
  Serial.println("Board:");
  for (int i = 0; i < 3; ++i) {
    Serial.printf(" %c | %c | %c \n", board[i][0], board[i][1], board[i][2]);
    if (i < 2) Serial.println("---|---|---");
  }
}


bool checkWin(char symbol) {
  Serial.println("Checking win for:");
  Serial.println(symbol);
  printBoard();


  for (int i = 0; i < 3; ++i)
    if ((board[i][0] == symbol && board[i][1] == symbol && board[i][2] == symbol) ||
        (board[0][i] == symbol && board[1][i] == symbol && board[2][i] == symbol))
      return true;
  if ((board[0][0] == symbol && board[1][1] == symbol && board[2][2] == symbol) ||
      (board[0][2] == symbol && board[1][1] == symbol && board[2][0] == symbol))
    return true;
  return false;
}


bool isDraw() {
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 3; ++j)
      if (board[i][j] == ' ')
        return false;
  return true;
}


void publishBoard() {
  String state = "";
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 3; ++j)
      state += board[i][j];


  Serial.print("Publishing board: [");
  Serial.print(state);
  Serial.println("]");


  client.publish("ttt/gameState", state.c_str(), true);
}


void makeMove(char symbol, int row, int col) {
  if (!gameActive) {
    Serial.println("Game is resetting — move ignored.");
    return;
  }


  Serial.printf("Attempting move %c at %d,%d\n", symbol, row, col);


  if (row >= 0 && row < 3 && col >= 0 && col < 3 && board[row][col] == ' ') {
    board[row][col] = symbol;
    printBoard();
    publishBoard();


    if (checkWin(symbol)) {
      String msg = (symbol == 'X') ? "X wins" : "O wins";
      client.publish("ttt/result", msg.c_str());
      Serial.println(msg);
      if (symbol == 'X') {
        xWins++;
      } else {
        oWins++;
      }
      gamesPlayed++;


      Serial.printf("Games played: %d | X: %d | O: %d | Draws: %d\n", gamesPlayed, xWins, oWins, draws);
      gameActive = false;
      clearBoard();
      publishBoard();
      updateLCD();
      delay(250);
      gameActive = true;
      isXTurn = true;
    } else if (isDraw()) {
      client.publish("ttt/result", "Draw");
      Serial.println("Draw");
      draws++;
      gamesPlayed++;


      Serial.printf("Games played: %d | X: %d | O: %d | Draws: %d\n", gamesPlayed, xWins, oWins, draws);
      gameActive = false;
      clearBoard();
      publishBoard();
      updateLCD();
      delay(250);
      gameActive = true;
      isXTurn = true;
    } else {
      isXTurn = !isXTurn;
    }
  } else {
    Serial.println("Invalid move (occupied or out of bounds)");
  }
}


void callback(char* topic, byte* payload, unsigned int length) {
  String move = "";
  for (unsigned int i = 0; i < length; i++) {
    move += (char)payload[i];
  }


  int row = move.charAt(0) - '0';
  int col = move.charAt(1) - '0';


  if (strcmp(topic, "ttt/playerX") == 0 && isXTurn) {
    makeMove('X', row, col);
  } else if (strcmp(topic, "ttt/playerO") == 0 && !isXTurn) {
    makeMove('O', row, col);
  } else {
    Serial.println("Received move out of turn or unknown topic.");
  }


  if (strcmp(topic, "ttt/playerX") == 0 && strcmp(move.c_str(), "reset") == 0) {
    Serial.println("Recieved game reset command.");
    clearBoard();
    isXTurn = true;
    gameActive = true;
    publishBoard();
    return;
  }
}


void reconnect() {
  while (!client.connected()) {
    Serial.println("Attempting MQTT connection...");
    String clientId = "ESP32-TTT-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("MQTT connected.");
      client.subscribe("ttt/playerX");
      client.subscribe("ttt/playerO");
    } else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      Serial.println(" retrying in 5 seconds...");
      delay(5000);
    }
  }
}


void updateLCD() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("X:");
  lcd.print(xWins);
  lcd.print(" O:");
  lcd.print(oWins);


  lcd.setCursor(0, 1);
  lcd.print("D:");
  lcd.print(draws);
  lcd.print(" G:");
  lcd.print(gamesPlayed);
}


void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Booting...");


  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }


  Serial.println("\nWiFi connected.");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());


  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);


  clearBoard();
  Wire.begin(14, 13);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("TicTacToe Ready");
  delay(2000);
  lcd.clear();
  updateLCD();
}


void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}




