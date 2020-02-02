#include <Arduino.h>
// библиотека для сна
#include <avr/sleep.h>
// библиотека для работы с лентой
#include "FastLED.h"
//----------------------------------------НАСТРОИВАЕМЫЕ ПАРАМЕТРЫ:
// задержка выключения
#define TIMER_DELAY 1800000
// задержка для анимации RGB светодиодов [ms]
#define THIS_DELAY 300
// число светодиодов в кольце/ленте
#define LED_COUNT 3
// пин, к которому подключен DIN светодиодной ленты
#define LED_DT 13
// пин, к которому подключен светодиод индикатор
#define LED_PIN 9
// Яркость мигающего диода таймера
#define PULSE_BRIGHTNES 3
// пин, к которому подключена кнопка (1йконтакт - пин, 2йконтакт - GND, INPUT_PULLUP, default - HIGH);
#define BUTTON_PIN 3
// максимальная яркость RGB свтодиодов (0 - 255)
#define MAX_BRIGHTNES 255
// гистерезис функции debounce
#define DEBOUNCE_DELAY 50
// тип светодиодной ленты
struct CRGB leds[LED_COUNT];
//  стартовая позиция для отсчёта прерывания на основе millis() в методе debounce();
unsigned long start = 0;
// время выключения
unsigned long shutDown = 0;
// перменные для pulse()
unsigned long pulseDelay;
`
//----------------------------------------FLAGS:
// флаг для buttonRoutine()
bool timerFlag = false;
bool pulseFlag = false;
byte buttonState;

byte *c;

// закрасить все диоды ленты в один цвет
void one_color_all(int cred, int cgrn, int cblu)
{
  for (int i = 0; i < LED_COUNT; i++)
  {
    leds[i].setRGB(cred, cgrn, cblu);
  }
}

// Иннициаллизация
void interrupt();
bool debounce();
void buttonRoutine();
byte *Wheel(byte WheelPos);
void heat();
void pwrDown();
void pulse();
void shutDownPoll();
void fadeOut();
void kill();

void setup()
{
  delay(500); // задержка включения для целей безопастности
  Serial.begin(9600);
  LEDS.setBrightness(MAX_BRIGHTNES);                  // ограничить максимальную яркость
  LEDS.addLeds<WS2811, LED_DT, GRB>(leds, LED_COUNT); // настрйоки для нашей ленты
  pinMode(BUTTON_PIN, INPUT_PULLUP);                  // defaul HIGH
  pinMode(LED_PIN, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), interrupt, LOW); // triggers when LOW
  one_color_all(0, 0, 0);                                             // погасить все светодиоды
  LEDS.show();
  heat();
  
}

void loop()
{

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
      shutDownPoll();
    }
    FastLED.show();
    delay(THIS_DELAY);

    // Serial.print(" Timer Flag = ");
    // Serial.println(timerFlag);
    
    // Serial.print("   Color = ");
    // Serial.println("");
    // Serial.print(*c);
    // Serial.print("  ");

    // Serial.print(*(c + 1));
    // Serial.print("  ");

    // Serial.println(*(c + 2));

    // Serial.print("  True Color = ");
    // Serial.println("");
    // byte red1 = *c;
    // Serial.print(red1);
    // Serial.print("  ");
    // byte green1 = *(c + 1);
    // Serial.print(green1);
    // Serial.print("  ");
    // byte blue1 = *(c + 2);
    // Serial.println(blue1);
    Serial.println(buttonState);
    Serial.println("==================================================================================");
  }
}

void interrupt()
{
  Serial.println("interrupt");
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
  if (buttonState == 0)
  {
    buttonState += 1;
    timerFlag = true;
    shutDown = millis() + TIMER_DELAY;
    pulseDelay = 100;
  }
  else if (buttonState == 1)
  {
    buttonState += 1;
    timerFlag = true;
    shutDown = millis() + TIMER_DELAY * 2;
    pulseDelay = 1000;
  }
  else if (buttonState == 2)
  {
    timerFlag = false;
    shutDown = 0;
    pulseDelay = 0;
    buttonState = 0;
    analogWrite(LED_PIN, 0);
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
        analogWrite(LED_PIN, PULSE_BRIGHTNES);
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

void pwrDown()
{
  analogWrite(LED_PIN, 0);
  fadeOut();
  timerFlag = false;
  one_color_all(0, 0, 0); // погасить все светодиоды
  LEDS.show();
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  sleep_cpu();
}

void shutDownPoll()
{
  if (timerFlag == true && shutDown != 0)
  { // если таймер влючен
    if (millis() >= shutDown)
    { // если время срабатывания таймера настало
      // Serial.println("Good night");
      pwrDown();
    }
  }
  Serial.print("До выключения (сек.) ");
  Serial.println((shutDown - millis()) / 1000);
  Serial.println(buttonState);
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

void kill()
{
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  sleep_cpu();
}