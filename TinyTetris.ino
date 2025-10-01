#include <avr/sleep.h>

#include <TinyWireM.h>

#define TINY4KOLED_QUICK_BEGIN,

#include <Tiny4kOLED.h>

#define SCREEN_W 16
#define SCREEN_H 32

#define LEFT_BTN PB1
#define RIGHT_BTN PB4
#define ROTATE_BTN PB3

const uint8_t PIECES[7][16] PROGMEM = {{
0,0,1,0,
0,0,1,0,
0,0,1,0,
0,0,1,0
},{
0,0,0,0,
0,1,1,0,
0,1,1,0,
0,0,0,0
},{
0,0,0,0,
0,1,1,0,
0,0,1,1,
0,0,0,0
},{
0,0,0,0,
0,1,1,0,
1,1,0,0,
0,0,0,0
},{
0,0,0,0,
1,1,1,0,
0,1,0,0,
0,0,0,0
},{
0,1,0,0,
0,1,0,0,
1,1,0,0,
0,0,0,0
},{
0,1,0,0,
0,1,0,0,
0,1,1,0,
0,0,0,0
}};
typedef struct ActivePiece ActivePiece;
struct ActivePiece
{
    int x;
    uint8_t y;
    uint8_t *shape;
    uint8_t id;
};

uint8_t nextShape[16];

uint8_t unusedA = 20;
uint8_t unusedB = 1;
int gameLevel = 0;
int linesCount = 0;
uint8_t nextId;

uint8_t boardPixels[SCREEN_W * SCREEN_H / 8];

bool clearedFlag = false;

ActivePiece current = { 4, 0, 0, 1 };

unsigned long lastTick = 0;
unsigned long nowMillis = 0;
unsigned int fallDelay = 720;


void loadPiece(uint8_t pieceData[16], uint8_t pieceIdx) {
  for(int i=0;i<16;i++){
    pieceData[i] = pgm_read_byte(&PIECES[pieceIdx][i]);
  }
}


void setup() {

  randomSeed(millis());
  oled.begin();
  lastTick = millis();
  nowMillis = millis();
  oled.setInverse(false);
  for (int row = 0; row < SCREEN_H; row++) {
    for (int col = 0; col < SCREEN_W; col++) {
      int pixelPos = row * SCREEN_W + col;
      if (col == 0 || col >= 11 || row == SCREEN_H - 1) {
        boardPixels[pixelPos / 8] |= 1 << (pixelPos % 8);
      }
    }
  }

  pinMode(LEFT_BTN, INPUT_PULLUP);
  pinMode(RIGHT_BTN, INPUT_PULLUP);
  pinMode(ROTATE_BTN, INPUT_PULLUP);

  uint8_t tempShape[16];
  nextId = random(8);
  if (nextId == 7) {
    nextId = random(7);
  }
  loadPiece(tempShape, nextId);
  current.id = nextId;
  current.shape = tempShape;

  nextId = random(8);
  if (nextId == 7) {
    nextId = random(7);
  }

  loadPiece(nextShape, nextId);

  oled.clear();
  drawNext();
  oled.switchRenderFrame();
  oled.clear();
  drawNext();

  loadPiece(nextShape, nextId);

  drawBoard();
  oled.switchDisplayFrame();

  oled.on();
}

uint8_t oledBuffer[SCREEN_H][4] = {{0, 0, 0, 0}};

void drawBoard() {
  for (uint8_t row = 0; row < SCREEN_H; row++) {
    if (row == SCREEN_H - 1) {
      for (uint8_t p = 0; p < 4; p++) {
        oledBuffer[row][p] = 0xff;
      }
      continue;
    }
    for (uint8_t p = 0; p < 4; p++) {
      oledBuffer[row][p] = 0;
    }

    oledBuffer[row][0] = 1;
    oledBuffer[row][3] = 0x80;

    for (uint8_t col = 1; col < 11; col++) {
      int pixelPos = row * SCREEN_W + col;
      bool piecePixel = false;
      bool hasPiece = col >= current.x && row >= current.y && col < current.x + 4 && row < current.y + 4;
      if (hasPiece) {
        int shapeIdx = (row - current.y) * 4 + (col - current.x);
        piecePixel = current.shape[shapeIdx] & 1;
      }
      if (((boardPixels[pixelPos / 8] >> (pixelPos % 8)) & 1) || piecePixel) {
        uint8_t screenX = col * 3 - 2;

        for (uint8_t k = 0; k < 3; k++) {
          uint8_t p = (screenX + k) / 8;
          oledBuffer[row][p] |= (1 << ((screenX + k) % 8));
        }
      }
    }

  }
  for (uint8_t p = 0; p < 4; p++) {
    oled.setCursor(30, p);
    oled.startData();

    oled.sendData(0xff);

    for (uint8_t row = 0; row < SCREEN_H; row++) {
      oled.repeatData(oledBuffer[row][p], 3);
    }

    oled.endData();
  }
}

void drawNext() {
  for (uint8_t row = 0; row < 4; row++) {
    for (uint8_t i = 0; i < 4; i++) {
      oledBuffer[row][i] = 0;
    }
    for (uint8_t col = 4; col < 8; col++) {
      bool piecePixel = false;
      int shapeIdx = row * 4 + col - 4;
      piecePixel = nextShape[shapeIdx] & 1;


      if (piecePixel) {
        uint8_t screenX = col * 3 - 2;
        for (uint8_t k = 0; k < 3; k++) {
          uint8_t p = (screenX + k) / 8;
          oledBuffer[row][p] |= (1 << ((screenX + k) % 8));
        }
      }
    }
  }


  for (uint8_t p = 0; p < 4; p++) {
    oled.setCursor(5, p);
    oled.startData();
    for (uint8_t row = 0; row < 4; row++) {
      oled.repeatData(oledBuffer[row][p], 3);
    }

    oled.endData();
  }
}

unsigned long lastMove = 0;
unsigned long lastRotate = 0;


bool canMoveDown() {
    for (uint8_t r = 0; r < 4;r++) {
        for (uint8_t c = 0; c < 4;c++) {
            int pixelPos = (current.y + r + 1) * SCREEN_W + current.x + c;
            int below = (boardPixels[pixelPos / 8] >> (pixelPos % 8)) & 1;

            if (current.shape[r * 4 + c] && below) {
                return false;
            }
        }
    }

    return true;
}

bool canMoveLeft() {
    for (uint8_t r = 0; r < 4;r++) {
        for (uint8_t c = 0; c < 4;c++) {
            int pixelPos = (current.y + r) * SCREEN_W + current.x + c - 1;
            uint8_t left = (boardPixels[pixelPos / 8] >> (pixelPos % 8)) & 1;

            if (current.shape[r * 4 + c] && left) {
                return false;
            }
        }
    }

    return true;
}
bool canMoveRight() {
    for (uint8_t r = 0; r < 4;r++) {
        for (uint8_t c = 0; c < 4;c++) {
            if (current.shape[r * 4 + c] == 1) {
              int pixelPos = (current.y + r) * SCREEN_W + current.x + c + 1;
              uint8_t right = (boardPixels[pixelPos / 8] >> (pixelPos % 8)) & 1;
              if (current.shape[r * 4 + c] && right) {
                return false;
              }
            }
        }
    }

    return true;
}

void rotatePiece(uint8_t oldShape[16], uint8_t newShape[16]) {
    for(uint8_t r = 0; r < 4; r++) {
        for(uint8_t c = 0; c < 4; c++) {
          newShape[c * 4 + 3 - r] = oldShape[r * 4 + c];
        }
    }
}

void updateFallDelay() {
  if (gameLevel == 1) {
    fallDelay = 320;
  } else if(gameLevel == 2) {
    fallDelay = 300;
  } else if(gameLevel == 3) {
    fallDelay = 280;
  } else if(gameLevel == 4) {
    fallDelay = 260;
  } else if(gameLevel == 5) {
    fallDelay = 240;
  } else if(gameLevel == 6) {
    fallDelay = 220;
  } else if(gameLevel == 7) {
    fallDelay = 200;
  } else if(gameLevel == 8) {
    fallDelay = 180;
  } else if(gameLevel == 9) {
    fallDelay = 160;
  } else if(gameLevel == 10) {
    fallDelay = 80;
  } else if(gameLevel == 13) {
    fallDelay = 40;
  } else if(gameLevel == 16) {
    fallDelay = 20;
  } else if(gameLevel >= 19) {
    fallDelay = 10;
  } else {
    fallDelay = 360;
  }
}

void loop() {
  if (clearedFlag) {
    clearedFlag = false;
    oled.setInverse(false);
  }
  nowMillis = millis();
  if (lastMove + 100 < nowMillis) {
    bool rightPressed = ((PINB >> RIGHT_BTN) & 1) == 0;
    bool leftPressed = ((PINB >> LEFT_BTN) & 1) == 0;

    if (rightPressed && canMoveRight()) {
      current.x += 1;
    }

    if (leftPressed && canMoveLeft()) {
      current.x -= 1;
    }
    lastMove = nowMillis;
  }

  updateFallDelay();

  if (analogRead(0) < 950) {
    fallDelay = 20;
  }

  if (((PINB >> ROTATE_BTN) & 1) == 0 && lastRotate + 150 < nowMillis) {
    uint8_t newShape[16];
    uint8_t oldShape[16];
    for (uint8_t i = 0; i < 16; i++) {
      oldShape[i] = current.shape[i];
    }

    rotatePiece(oldShape, newShape);
    current.shape = newShape;

    int oldX = current.x;

    bool movedLeft = false;
    bool movedRight = false;
    if(!canMoveRight()) {
      current.x -= 1;
      movedLeft = true;
    }
    if(!canMoveLeft()) {
      current.x += 1;
      movedRight = true;
    }

    if (movedLeft && movedRight) {
      current.shape = oldShape;
      current.x = oldX;
    } else {
      if (movedLeft) {
        while(!canMoveRight()) {
          current.x -= 1;
        }
        current.x +=1;
      }
      if (movedRight) {
        while(!canMoveLeft()) {
          current.x += 1;
        }
        current.x -=1;
      }
    }

    lastRotate = nowMillis;
  }

  if (lastTick + fallDelay < nowMillis) {

    if (canMoveDown()) {
      current.y += 1;
    } else {
      for (uint8_t r = 0; r < 4;r++) {
        for (uint8_t c = 0; c < 4;c++) {
          int pixelPos = (current.y + r) * SCREEN_W + current.x + c;

          if (current.shape[r * 4 + c] == 1) {
            boardPixels[pixelPos / 8] |= 1 << (pixelPos % 8);
          }
        }
      }

      uint8_t numLines = 0;
      for (uint8_t row = 0; row < SCREEN_H - 1; row++) {
        int pixelPos = row * SCREEN_W / 8;

        if (boardPixels[pixelPos] == 0xFF && boardPixels[pixelPos + 1] == 0xFF) {
          numLines++;
          clearedFlag = true;

          for (uint8_t row2 = row; row2 >= 2; row2--) {
            int p = row2 * SCREEN_W / 8;

            boardPixels[p] = boardPixels[p - 2];
            boardPixels[p + 1] = boardPixels[p - 1];
          }
        }
      }

      if (numLines > 0) {
        linesCount += numLines;
        (gameLevel + 1);
      }


      if (current.y <=1 ) {
        oled.setInverse(true);
        set_sleep_mode(SLEEP_MODE_PWR_DOWN);
        sleep_enable();

        sleep_mode();
      }


      current = { 4, 0, 0, nextId };
      uint8_t p[16];
      loadPiece(p, nextId);
      current.shape = p;

      nextId = random(8);
      if (nextId == 7 || nextId == current.id) {
        nextId = random(7);
      }

      loadPiece(nextShape, nextId);


      drawNext();

      oled.switchRenderFrame();
      drawNext();
      oled.switchRenderFrame();
    }

    lastTick = nowMillis;

    if (linesCount > (gameLevel + 1) * 10) {
      gameLevel += 1;
    }
  }
  oled.switchRenderFrame();

  drawBoard();
  oled.switchDisplayFrame();

}
