#include <Mouse.h>
#include <Keyboard.h>

void setup() {
  Serial.begin(9600);
  Mouse.begin();
  Keyboard.begin();
}

void moveMouseInSteps(int x, int y) {
  const int step = 100;
  int stepsX = x / step;
  int stepsY = y / step;
  int remainderX = x % step;
  int remainderY = y % step;

  for (int i = 0; i < abs(stepsX); i++) {
    Mouse.move((stepsX > 0 ? step : -step), 0);
    delay(5);
  }

  for (int i = 0; i < abs(stepsY); i++) {
    Mouse.move(0, (stepsY > 0 ? step : -step));
    delay(5);
  }

  Mouse.move(remainderX, remainderY);
}

void handleMouseCommand(const String &command) {
  if (command.startsWith("move_")) {
    int firstUnderscore = command.indexOf('_');
    int secondUnderscore = command.indexOf('_', firstUnderscore + 1);
    int deltaX = command.substring(firstUnderscore + 1, secondUnderscore).toInt();
    int deltaY = command.substring(secondUnderscore + 1).toInt();
    moveMouseInSteps(deltaX * 0.6668, deltaY * 0.6668);
  } else if (command == "swipe_right") {
    Mouse.press(MOUSE_RIGHT);
    delay(50);
    Mouse.move(350, 0);
    delay(50);
    Mouse.release(MOUSE_RIGHT);
    delay(50);
  } else if (command == "mouse_left_press") {
    Mouse.press(MOUSE_LEFT);
    delay(5);
  } else if (command == "mouse_left_release") {
    Mouse.release(MOUSE_LEFT);
    delay(5);
  } else if (command == "mouse_right_press") {
    Mouse.press(MOUSE_RIGHT);
    delay(5);
  } else if (command == "mouse_right_release") {
    Mouse.release(MOUSE_RIGHT);
    delay(5);
  }
}

void handleKeyboardCommand(const String &command) {
  if (command.length() == 1 && isDigit(command[0])) {
    Keyboard.print(command);
    delay(5);
  } else if (command == "esc") {
    Keyboard.press(KEY_ESC);
    delay(5);
    Keyboard.release(KEY_ESC);
  } else if (command == "end") {
    Keyboard.press(KEY_END);
    delay(5);
    Keyboard.release(KEY_END);
  }
}

void loop() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    if (command.startsWith("move_") || command.startsWith("mouse_") || command == "swipe_right") {
      handleMouseCommand(command);
    } else {
      handleKeyboardCommand(command);
    }
  }
  delay(15);
}
