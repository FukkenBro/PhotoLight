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

//НАСТРОИВАЕМЫЕ ПАРАМЕТРЫ:=========================================================
// задержка выключения [ms] (default 180 000ms = 30min)
#define TIMER_DELAY 1800000
// задержка для анимации RGB светодиодов [ms]
#define THIS_DELAY 300

//НАСТРОИВАЕМЫЕ ПАРАМЕТРЫ:=========================================================

// перменные для debounce() .......................................................
// стартовая позиция для отсчёта прерывания на основе millis() в методе debounce();
unsigned long start = 0;
// гистерезис функции debounce
#define DEBOUNCE_DELAY 50

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


//FLAGS: ==========================================================================
// флаг для buttonRoutine()
bool timerFlag = false;
bool pulseFlag = false;
byte buttonState;
byte *c;
bool reset = false;

// Иннициаллизация функций ========================================================
void interrupt();
bool debounce();
void buttonRoutine();
byte *Wheel(byte WheelPos);
void heat();
void pwrDown();
void pulse();
void poll();
void fadeOut();
void kill();
void animation1();
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
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), interrupt, LOW); // triggers when LOW
  heat();
}

//LOOP ============================================================================
void loop()
{
 animation1();
}

//FUNCTIONS =======================================================================
void animation1(){
 uint16_t i, j;
  for (j = 0; j < 256 * 5; j++)
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
    FastLED.show();
    delay(THIS_DELAY);
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

void interrupt()
{
  if (!debounce())
  {
    return;
  }
  else
  {
    buttonRoutine();
  }
}

bool debounce()
{
  unsigned long current = millis();
  if (abs(current - start) > DEBOUNCE_DELAY)
  {
    start = millis();
    return true;
  }
  else
  {
    start = millis();
    return false;
  }
}

void buttonRoutine()
{
  if (reset == true)
  {
    reset = false;
    asm volatile("  jmp 0");
  }
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
  // Serial.print("До выключения (сек.) ");
  // Serial.println((shutDown - millis()) / 1000);
  // Serial.println(buttonState);
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