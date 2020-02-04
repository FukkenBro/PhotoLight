#include <Arduino.h>
// библиотека для сна
#include <avr/sleep.h>
// библиотека для работы с лентой
#include "FastLED.h"

//НЕИЗМЕНЯЕМЫЕ ПАРАМЕТРЫ: ========================================================
// число светодиодов в кольце/ленте
#define LED_COUNT 3
// пин, к которому подключен DIN светодиодной ленты
#define LED_DT 13
// пин, к которому подключен светодиод индикатор
#define LED_PIN 9
// пин, к которому подключена кнопка (1йконтакт - пин, 2йконтакт - GND, INPUT_PULLUP, default - HIGH);
#define BUTTON_PIN 3
// максимальная яркость RGB свтодиодов (0 - 255)
#define MAX_BRIGHTNES 255
// тип светодиодной ленты
struct CRGB leds[LED_COUNT];
//время удержания кнопки для регистрации длинного нажатия (default - 150)
#define HOLD_DURATION 150

//НАСТРОИВАЕМЫЕ ПАРАМЕТРЫ:=========================================================
// задержка выключения [ms] (default 180 000ms = 30min)
#define TIMER_DELAY 3000
// задержка для анимации RGB светодиодов [ms]
#define THIS_DELAY 300

//НАСТРОИВАЕМЫЕ ПАРАМЕТРЫ:=========================================================

// перменные для debounce() .......................................................
// стартовая позиция для отсчёта прерывания на основе millis() в методе debounce();
unsigned long start = 0;
// гистерезис функции debounce
#define DEBOUNCE_DELAY 50
volatile int pressTimer = 0;
//переменные для poll()............................................................
// время выключения
unsigned long shutDown = 0;

// перменные для pulse() ..........................................................
// Яркость мигающего диода таймера
#define PULSE_BRIGHTNES 250
unsigned long pulseDelay;
unsigned long pulseStart;
byte pulseFade = 0;
byte pulseFadeStep = 0;
unsigned long pulseStartTime = millis();
int tmp = 0;
int mode = 1;
//FLAGS: ==========================================================================
// флаг для buttonRoutine()
bool timerFlag = false;
bool pulseFlag = false;
byte buttonState;
byte *c;
bool reset = false;

bool forceExit = false;

// Иннициаллизация функций ========================================================
void interrupt();
bool debounce();
byte *Wheel(byte WheelPos);
void heat();
void pwrDown();
void pulse();
void poll();
void fadeOut();
void kill();
void animation1(byte speedMultiplier);
void animation2(byte hueDelta, byte speedMultiplier);
void resetCheck();
void onButtonClick();
void onButtonHold();
void resetColor();
// закрасить все диоды ленты в один цвет
void one_color_all(int cred, int cgrn, int cblu)
{
  for (int i = 0; i < LED_COUNT; i++)
  {
    leds[i].setRGB(cred, cgrn, cblu);
  }
}

//SETUP ===========================================================================
void setup()
{
  delay(100);
  one_color_all(0, 0, 0); // погасить все светодиоды
  LEDS.show();
  Serial.begin(9600);
  LEDS.setBrightness(MAX_BRIGHTNES);                  // ограничить максимальную яркость
  LEDS.addLeds<WS2811, LED_DT, GRB>(leds, LED_COUNT); // настрйоки для нашей ленты
  pinMode(BUTTON_PIN, INPUT_PULLUP);                  // defaul HIGH
  pinMode(LED_PIN, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), interrupt, CHANGE); // triggers when LOW
  heat();
}

//LOOP ============================================================================
void loop()
{
  forceExit = false;
  if (mode == 1)
  {
    resetColor();
    Serial.println("Animation in mode 1");
    animation1(1);
    return;
  }
  else if (mode == 2)
  {
    resetColor();
    Serial.println("Animation in mode 2");
    animation1(5);
    return;
  }
  else if (mode == 3)
  {
    resetColor();
    Serial.println("Animation in mode 3");
    animation1(20);
    return;
  }
  else if (mode == 4)
  {
    resetColor();
    Serial.println("Animation in mode 4");
    animation2(3, 1);
    return;
  }
  else if (mode == 5)
  {
    resetColor();
    Serial.println("Animation in mode 5");
    animation2(3, 5);
    return;
  }
  else if (mode == 6)
  {
    resetColor();
    Serial.println("Animation in mode 6");
    animation2(3, 20);
    return;
  }
  else
  {
    resetColor();
    Serial.println("defaul mode animation");
    animation1(1);
    return;
  }
}

//FUNCTIONS =======================================================================
void animation1(byte speedMultiplier)
{
  uint16_t i, j;
  for (j = 0; j < 256 * 1; j++)
  { // 5 cycles of all colors on wheel
    for (i = 0; i < LED_COUNT; i++)
    {
      c = Wheel(((i * 256 / LED_COUNT) + j) & 255);
      one_color_all(*c, *(c + 1), *(c + 2));
    }
    if (timerFlag == true)
    {
      pulse();
      poll();
    }
    if (forceExit)
    {
      return;
    }
    FastLED.show();
    delay(THIS_DELAY / speedMultiplier);
  }
}

void animation2(byte hueDelta, byte speedMultiplier)
{
  uint16_t i, j;
  for (j = 0; j < 256 * 5; j++)
  { // 5 cycles of all colors on wheel
    for (i = 0; i < LED_COUNT; i++)
    {
      c = Wheel(((i * 256 / hueDelta) + j) & 255);
      leds[i].setRGB(*c, *(c + 1), *(c + 2));
    }
    if (timerFlag == true)
    {
      pulse();
      poll();
    }
    if (forceExit)
    {
      return;
    }
    FastLED.show();
    delay(THIS_DELAY / speedMultiplier);
  }
}

byte *Wheel(byte WheelPos)
{
  static byte c[3];
  if (WheelPos < 85)
  {
    c[0] = WheelPos * 3;
    c[1] = 255 - WheelPos * 3;
    c[2] = 0;
  }
  else if (WheelPos < 170)
  {
    WheelPos -= 85;
    c[0] = 255 - WheelPos * 3;
    c[1] = 0;
    c[2] = WheelPos * 3;
  }
  else
  {
    WheelPos -= 170;
    c[0] = 0;
    c[1] = WheelPos * 3;
    c[2] = 255 - WheelPos * 3;
  }
  return c;
}

void heat()
{
  for (int i = 0; i <= 255; i++)
  {
    for (int j = 0; j < LED_COUNT; j++)
    {
      one_color_all(0, 0, i);
    }
    FastLED.show();
    delay(5);
  }
}

void fadeOut()
{
  byte red = *c;
  byte green = *(c + 1);
  byte blue = *(c + 2);
  for (; (red >= 0) && (green >= 0) && (blue >= 0);)
  {
    if (red > 0)
    {
      red--;
    }
    if (green > 0)
    {
      green--;
    }
    if (blue > 0)
    {
      blue--;
    }
    one_color_all(red, green, blue);
    FastLED.show();
    delay(5);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Button input related values
static const int STATE_NORMAL = 0; // no button activity
static const int STATE_SHORT = 1;  // short button press
static const int STATE_LONG = 2;   // long button press
volatile int resultButton = 0;     // global value set by checkButton()

void interrupt()
{
  /*
  * This function implements software debouncing for a two-state button.
  * It responds to a short press and a long press and identifies between
  * the two states. Your sketch can continue processing while the button
  * function is driven by pin changes.
  */

  const unsigned long LONG_DELTA = 500ul;               // hold seconds for a long press
  const unsigned long DEBOUNCE_DELTA = 50ul;            // debounce time
  static int lastButtonStatus = HIGH;                   // HIGH indicates the button is NOT pressed
  int buttonStatus;                                     // button atate Pressed/LOW; Open/HIGH
  static unsigned long longTime = 0ul, shortTime = 0ul; // future times to determine if button has been poressed a short or long time
  boolean Released = true, Transition = false;          // various button states
  boolean timeoutShort = false, timeoutLong = false;    // flags for the state of the presses

  buttonStatus = digitalRead(BUTTON_PIN); // read the button state on the pin "BUTTON_PIN"
  timeoutShort = (millis() > shortTime);  // calculate the current time states for the button presses
  timeoutLong = (millis() > longTime);

  if (buttonStatus != lastButtonStatus)
  { // reset the timeouts if the button state changed
    shortTime = millis() + DEBOUNCE_DELTA;
    longTime = millis() + LONG_DELTA;
  }

  Transition = (buttonStatus != lastButtonStatus);   // has the button changed state
  Released = (Transition && (buttonStatus == HIGH)); // for input pullup circuit

  lastButtonStatus = buttonStatus; // save the button status

  if (!Transition)
  { //without a transition, there's no change in input
    // if there has not been a transition, don't change the previous result
    resultButton = STATE_NORMAL | resultButton;
    return;
  }

  if (timeoutLong && Released)
  {                                           // long timeout has occurred and the button was just released
    resultButton = STATE_LONG | resultButton; // ensure the button result reflects a long press
    onButtonHold();
  }
  else if (timeoutShort && Released)
  {                                            // short timeout has occurred (and not long timeout) and button was just released
    resultButton = STATE_SHORT | resultButton; // ensure the button result reflects a short press
    onButtonClick();
  }
  else
  {                                             // else there is no change in status, return the normal state
    resultButton = STATE_NORMAL | resultButton; // with no change in status, ensure no change in button status
  }
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////

void pulse()
{
  if (pulseDelay != 0)
  {
    // мерцание
    unsigned long current = millis();
    if (abs(current - pulseStart) > pulseDelay)
    {
      if (pulseFlag == false)
      {
        pulseFade = constrain((pulseFade + pulseFadeStep), 0, 250);
        analogWrite(LED_PIN, constrain((PULSE_BRIGHTNES - pulseFade), 1, 255));
        pulseFlag = true;
      }
      else
      {
        analogWrite(LED_PIN, 0);
        pulseFlag = false;
        pulseStart = millis();
      }
    }
  }
}

void poll()
{
  if (timerFlag == true && shutDown != 0)
  { // если таймер влючен
    if (millis() >= shutDown)
    { // если время срабатывания таймера настало
      pwrDown();
    }
  }
  Serial.print("До выключения (сек.) ");
  Serial.println((shutDown - millis()) / 1000);
}

void pwrDown()
{
  reset = true;
  analogWrite(LED_PIN, 0);
  fadeOut();
  timerFlag = false;
  one_color_all(0, 0, 0); // погасить все светодиоды
  LEDS.show();
  delay(1000);
  kill();
}

void kill()
{
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  sleep_cpu();
}

void resetCheck()
{
  if (reset == true)
  {
    reset = false;
    asm volatile("  jmp 0");
  }
}

void onButtonClick()
{
  resetCheck();
  if (buttonState == 0)
  {
    pulseFade = 0;
    buttonState += 1;
    timerFlag = true;
    shutDown = millis() + TIMER_DELAY;
    pulseDelay = 100;
    pulseFadeStep = 2;
  }
  else if (buttonState == 1)
  {
    pulseFade = 0;
    buttonState += 1;
    timerFlag = true;
    shutDown = millis() + TIMER_DELAY * 2;
    pulseDelay = 1000;
    pulseFadeStep = 10;
  }
  else if (buttonState == 2)
  {
    // cброс всех флажков
    timerFlag = false;
    analogWrite(LED_PIN, 0);
    shutDown = 0;
    pulseDelay = 0;
    buttonState = 0;
    pulseFade = 0;
    pulseFadeStep = 0;
  }
}

void onButtonHold()
{
  resetCheck();
  mode++;
  if (mode == 7)
  {
    mode = 1;
  }
  Serial.print("NOW MODE ");
  Serial.println(mode);
  for (size_t i = 255; i > 0; i--)
  {
    analogWrite(LED_PIN, i);
    delay(10);
  }
  analogWrite(LED_PIN, 0);
  forceExit = true;
}

void resetColor()
{
  for (size_t i = 0; i < 3; i++)
  {
    c[i] = 0;
  }
}
