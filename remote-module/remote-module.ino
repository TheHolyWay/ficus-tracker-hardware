#include <RCSwitch.h>
#include <OneWire.h>

#include <avr/wdt.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>

#define WDTO_INFINITE 255
#define SLEEP_PERIOD WDTO_8S
#define SKIP_WDT_WAKEUPS 2 // x SLEEP_PERIOD + SLEEP_PERIOD

#define MOISTURE_SENSOR_PIN A1 //pin P2
#define LIGHT_SENSOR_PIN A2 //pin P4
#define TEMPERATURE_SENSOR_PIN 3
#define SENSORS_POWER_PIN 0

#define SERIAL_NUM 001 //value from 1 to 999  

#define SOIL_MOISTURE_SENSOR_ID 1
#define LIGHT_SENSOR_ID 2
#define TEMPERATURE_SENSOR_ID 3

volatile byte wakeup_counter = SKIP_WDT_WAKEUPS + 1;

ISR(WDT_vect) {
  wakeup_counter++;
}

RCSwitch mySwitch = RCSwitch();
OneWire  ds(TEMPERATURE_SENSOR_PIN);

void setup() {
  pinMode(SENSORS_POWER_PIN, OUTPUT);
  pinMode(TEMPERATURE_SENSOR_PIN, INPUT);

  digitalWrite(SENSORS_POWER_PIN, LOW);
  mySwitch.enableTransmit(1);
}

void loop() {
  if (wakeup_counter > SKIP_WDT_WAKEUPS) {
     process();
     wakeup_counter = 0;
  }

  sleep();
}

void process() {
  digitalWrite(SENSORS_POWER_PIN, HIGH); //Enable sensors
  delay(10);

  long moistureValue = analogRead(MOISTURE_SENSOR_PIN) - 25; //-25 for use three numbers for value. For moisture we will never get value less that 200.
  long light = analogRead(LIGHT_SENSOR_PIN);
  long temparature = temp() + 300; //temparature now is multiplied on 10 (for pass tenths in long).  +300 for negative values. We wanna transmit only positive values. Range from -30 to 70 will be avaulavle

  randomSeed(light + temparature);

  long message = (SERIAL_NUM * 10000) + (SOIL_MOISTURE_SENSOR_ID * 1000) + moistureValue;
  mySwitch.send(message, 24);

  message = (SERIAL_NUM * 10000) + (LIGHT_SENSOR_ID * 1000) + light;
  mySwitch.send(message, 24);

  message = (SERIAL_NUM * 10000) + (TEMPERATURE_SENSOR_PIN * 1000) + temparature;
  mySwitch.send(message, 24);

  digitalWrite(SENSORS_POWER_PIN, LOW); // Disable sensors
}

int temp() {
  byte i;
  byte present = 0;
  byte data[12];
  byte addr[8];
  int celsius;

  if (!ds.search(addr)) {
    ds.reset_search();
    return -1000;
  }

  ds.reset();
  ds.select(addr);
  ds.write(0x44);        // start conversion, with parasite power on at the end
  //ds.write(0x44, 1);        // start conversion, with parasite power on at the end

  delay(1000);     // maybe 750ms is enough, maybe not
  // we might do a ds.depower() here, but the reset will take care of it.

  present = ds.reset();
  ds.select(addr);
  ds.write(0xBE);         // Read Scratchpad

  for ( i = 0; i < 9; i++) {           // we need 9 bytes
    data[i] = ds.read();
  }

  // Convert the data to actual temperature
  // because the result is a 16 bit signed integer, it should
  // be stored to an "int16_t" type, which is always 16 bits
  // even when compiled on a 32 bit processor.
  int16_t raw = (data[1] << 8) | data[0];

  byte cfg = (data[4] & 0x60);
  // at lower res, the low bits are undefined, so let's zero them
  if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
  else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
  else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
  //// default is 12 bit resolution, 750 ms conversion time

  celsius = (raw*10)/16;
  ds.reset_search();

  return celsius;
}

void sleep() {
  GIMSK = _BV(PCIE); // Включить Pin Change прерывания

  PCMSK |= _BV(WDIE); // PCINT3; включить если нет проблем с освещением или иначе нет шансов проснуться
  ADCSRA &= ~_BV(ADEN); // отключить ADC; уменьшает энергопотребление

  if (SLEEP_PERIOD != WDTO_INFINITE) {
    wdt_enable(SLEEP_PERIOD); // установить таймер
    WDTCR |= _BV(WDIE); // включить прерывания от таймера; фикс для ATtiny85
  }

  // Установить режим сна Power-down; MCUSR &= ~_BV(SM0); MCUSR |= _BV(SM1);
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable(); // разрешить режим сна; MCUSR |= _BV(SE);
  sei(); // включить прерывания
  sleep_cpu(); // заснуть
  cli(); // отключить прерывания; для безопасного отключения PCINT3
  PCMSK &= ~_BV(WDIE); // PCINT3; отключить
  sleep_disable(); // запретить режим сна; MCUSR &= ~_BV(SE);
  ADCSRA |= _BV(ADEN); // включить ADC
  sei(); // включить прерывания; иначе таймеры не будут работать
}
