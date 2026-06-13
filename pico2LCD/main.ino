#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735

// Screen Setup
const uint16_t Pixel_Color_White = 0xFFFF;
uint16_t Display_Backround_Color = Pixel_Color_White;

int currentColor; // var to indicate current colors[] index
uint16_t colors[7] = {0x0000, 0xF800, 0xFFE0, 0xFBE0,
                      0x7E0,  0x1F,   0xF81F}; // Selectable colors
                                               /*
                                                 Pixel_Color_Black, Pixel_Color_Red, Pixel_Color_Yellow,
                                                 Pixel_Color_Orange, Pixel_Color_Green, Pixel_Color_Blue,
                                                 Pixel_Color_Pink
                                               */

#define TFT_CS 10   // Hallowing display control pins: chip select
#define TFT_RST 9   // Display reset
#define TFT_DC 8    // Display data/command select
#define TFT_MOSI 11 // Data out
#define TFT_SCLK 13 // Clock out

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK,
                                      TFT_RST); // Init of tft screen
int MAX_X_RES = tft.width();
int MAX_Y_RES = tft.height();

// Current cursor position stars in the center of the screen
int cursorX = tft.width() / 2;
int cursorY = tft.height() / 2;

// Encoder Setup

struct encoder {
  int clk;
  int dt;
  int currentStateCLK;
  int lastStateCLK;
};

#define PIN_ENCODER_X_CLK 2
#define PIN_ENCODER_X_DT 3
#define PIN_ENCODER_X_SWITCH 7
#define PIN_ENCODER_Y_CLK 4
#define PIN_ENCODER_Y_DT 5
#define PIN_ENCODER_Y_SWITCH 6

int button_X;
int button_Y;
encoder X_Encoder = {PIN_ENCODER_X_CLK, PIN_ENCODER_X_DT, 0, 0};
encoder Y_Encoder = {PIN_ENCODER_Y_CLK, PIN_ENCODER_Y_DT, 0, 0};

void setup() {

  // Screen Setup
  tft.initR(INITR_BLACKTAB);
  tft.fillScreen(Display_Backround_Color); // fills scrren white
  currentColor = 0;                        // sets cursor color to black

  // Encoder Setup
  pinMode(PIN_ENCODER_X_SWITCH, INPUT_PULLUP);
  pinMode(X_Encoder.clk, INPUT);
  pinMode(X_Encoder.dt, INPUT);
  X_Encoder.lastStateCLK = digitalRead(X_Encoder.clk);

  pinMode(PIN_ENCODER_Y_SWITCH, INPUT_PULLUP);
  pinMode(Y_Encoder.clk, INPUT);
  pinMode(Y_Encoder.dt, INPUT);
  Y_Encoder.lastStateCLK = digitalRead(Y_Encoder.clk);
}

void loop() {

  // draws a pixel at the current cursor location with the current colors[]
  // index
  tft.drawPixel(cursorX, cursorY, colors[currentColor]);
  X_Encoder.currentStateCLK = digitalRead(X_Encoder.clk);
  Y_Encoder.currentStateCLK = digitalRead(Y_Encoder.clk);

  if (X_Encoder.currentStateCLK != X_Encoder.lastStateCLK &&
      X_Encoder.currentStateCLK == 1) {
    if (digitalRead(X_Encoder.dt) != X_Encoder.currentStateCLK) {
      cursorX++;
    } else {
      cursorX--;
    }
  }

  if (Y_Encoder.currentStateCLK != Y_Encoder.lastStateCLK &&
      Y_Encoder.currentStateCLK == 1) {
    if (digitalRead(Y_Encoder.dt) != Y_Encoder.currentStateCLK) {
      cursorY++;
    } else {
      cursorY--;
    }
  }

  X_Encoder.lastStateCLK = X_Encoder.currentStateCLK;
  Y_Encoder.lastStateCLK = Y_Encoder.currentStateCLK;
  button_X = digitalRead(PIN_ENCODER_X_SWITCH);
  // Button control
  while (button_X == LOW) { // while x-button is pressed the following runs
    button_X = digitalRead(PIN_ENCODER_X_SWITCH); // reads the left button
    button_Y = digitalRead(PIN_ENCODER_Y_SWITCH); // reads the right button
    Y_Encoder.currentStateCLK = digitalRead(Y_Encoder.clk);
    // Clear
    if (button_Y == LOW) { // If y-button is pressed
      tft.fillScreen(
          Display_Backround_Color); // Fills the screen with background color
    }
    // Color Selecting
    else { // If y-button is not pressed
      if (Y_Encoder.currentStateCLK != Y_Encoder.lastStateCLK &&
          Y_Encoder.currentStateCLK == 1) {
        if (digitalRead(Y_Encoder.dt) != Y_Encoder.currentStateCLK) {
          currentColor--;
          if (currentColor == -1)
            currentColor = 6;
        } else {
          currentColor++;
          if (currentColor == 7)
            currentColor = 0;
        }
      }
    }
    Y_Encoder.lastStateCLK = Y_Encoder.currentStateCLK;
  }
}
