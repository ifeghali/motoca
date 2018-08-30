/**
 * Arduino open source project for battery operated electric bike
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * Author: Igor Feghali <ifeghali@php.net>
 *
 */
 
#define DISABLE_SPEAKER2
//#define DEBUG

#include <SD.h>
#include <TMRpcm.h>
#include <SPI.h>

#define PIN_SDCARD A5
#define PIN_SW_GIROFLEX 3       // a
#define PIN_SW_REVERSE 4        // b
#define PIN_SW_SIREN 5          // c
#define PIN_SW_HONK 6           // d
#define PIN_SPEAKER 9           // DON'T TOUCH
#define PIN_SENSOR_ECHO 10
#define PIN_SENSOR_TRIGGER 11
#define PIN_GIROFLEX 12

#ifdef DEBUG
#define DPRINT(x) Serial.print(x)
#define DPRINTLN(x) Serial.println(x)
#else
#define DPRINT(x)
#define DPRINTLN(x)
#endif

TMRpcm audio;   // create an object for use in this sketch
int giroflex_mode = 0;
int last_state = -1;
int count_siren = 0;

void setup() {
  pinMode(PIN_SW_GIROFLEX, INPUT_PULLUP);
  pinMode(PIN_SW_REVERSE, INPUT_PULLUP);
  pinMode(PIN_SW_SIREN, INPUT_PULLUP);
  pinMode(PIN_SW_HONK, INPUT_PULLUP);
  pinMode(PIN_SENSOR_ECHO, INPUT);
  pinMode(PIN_SENSOR_TRIGGER, OUTPUT);
  digitalWrite(PIN_SENSOR_TRIGGER, LOW);
  pinMode(PIN_GIROFLEX, OUTPUT);
  digitalWrite(PIN_GIROFLEX, LOW);
  pinMode(PIN_SPEAKER, OUTPUT);
  pinMode(PIN_SDCARD, OUTPUT);

#ifdef DEBUG
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Arduino Micro
  }
#endif

  audio.speakerPin = PIN_SPEAKER;
  audio.setVolume(4);
  if (!SD.begin(PIN_SDCARD)) {
    DPRINTLN("SD fail");
    tone(PIN_SPEAKER, 392);
    delay(250);
    tone(PIN_SPEAKER, 262);
    delay(500);
    noTone(PIN_SPEAKER);
  } else {
    audio.play("start.wav");
    while (audio.isPlaying());
    audio.disable();
  }
  DPRINTLN("done setup");
}

void loop() {
  int state = 0;
  state += debounce(PIN_SW_GIROFLEX) << 3;
  state += debounce(PIN_SW_REVERSE)  << 2;
  state += debounce(PIN_SW_SIREN)    << 1;
  state += debounce(PIN_SW_HONK);

  if (last_state != state) {
    DPRINT("state: ");
    DPRINTLN(state);
  }
  last_state = state;

  switch (state) {
    case B0000:
      giroflex(0);
      reset_siren();
      audio.disable();
      noTone(PIN_SPEAKER);
      break;
    case B0011:
    case B0010:
    case B1010:
      if (count_siren < 10) {
        count_siren++;
        DPRINT("siren button hold :");
        DPRINTLN(count_siren);
      } else {
        giroflex(2);
        if (!audio.isPlaying())
          audio.play("s2.wav");
      }
      break;
    case B0101:
    case B0100:
    case B0110:
    case B0111:
      giroflex(0);
    case B1100:
    case B1101:
    case B1110:
    case B1111:
      count_siren = 0;
      audio.disable();
      parking();
      break;
    case B1000:
      reset_siren();
      audio.disable();
      noTone(PIN_SPEAKER);
      giroflex(5);
      break;
    case B0001:
      giroflex(0);
      reset_siren();
      if (!audio.isPlaying()) {
        audio.play("honk.wav");
        while (audio.isPlaying());
      }
      break;
    case B1001:
      reset_siren();
    case B1011:
      giroflex(5);
      if (!audio.isPlaying()) {
        audio.play("honk.wav");
        while (audio.isPlaying());
      }
      break;
  }
}

void reset_siren() {
  if (count_siren > 0 and count_siren < 10) {
    giroflex(3);
    audio.disable();
    audio.play("s1.wav");
    while (audio.isPlaying());
    giroflex(0);
  }
  count_siren = 0;
}

void parking() {
  DPRINTLN("parking");
  pinMode(PIN_SPEAKER, OUTPUT);
  noTone(PIN_SPEAKER);
  tone(PIN_SPEAKER, 440, 100);
  delay(500);

  while (digitalRead(PIN_SW_REVERSE) == LOW) {
    digitalWrite(PIN_SENSOR_TRIGGER, HIGH);
    delayMicroseconds(5);
    digitalWrite(PIN_SENSOR_TRIGGER, LOW);
    unsigned long int tt = pulseIn(PIN_SENSOR_ECHO, HIGH);
    if (tt > 2000) {
      noTone(PIN_SPEAKER);
      continue;
    }
    tt = (tt <  500) ? 500  : tt;
    int d = map(tt, 2000, 500, 500, 0);
    tone(PIN_SPEAKER, 440, 100);
    delay(d);
  }

  noTone(PIN_SPEAKER);
  DPRINTLN("no parking");
}

int debounce(int but) {
  int count = 0;
  do {
    delay(1);
    count++;
    if (count >= 50) {
      return 1;
    }
  } while (digitalRead(but) == LOW);
  return 0;
}

void giroflex(int target_mode) {
  /**
     MODE 0: OFF
     MODE 1: LEFT/RIGHT
     MODE 2: LEFT/RIGHT QUICKLY
     MODE 3: ALL FLASH
     MODE 4: BACK/FORTH
     MODE 5: BACK/FORTH QUICKLY
     MODE 6: MIXED
  */

  if (giroflex_mode == target_mode)
    return;

  DPRINT("giroflex mode ");
  DPRINTLN(target_mode);

  /*
   * audio can mess with timers so we wait for it to finish
   */
  while (audio.isPlaying());

  int i;
  for (i = giroflex_mode; i != target_mode; i = (i + 1) % 7) {
    digitalWrite(PIN_GIROFLEX, HIGH);
    delayMicroseconds(900);
    digitalWrite(PIN_GIROFLEX, LOW);
    delay(2);
    digitalWrite(PIN_GIROFLEX, HIGH);
    delayMicroseconds(2000);
    digitalWrite(PIN_GIROFLEX, LOW);
    delay(2);
    digitalWrite(PIN_GIROFLEX, HIGH);
    delayMicroseconds(900);
    digitalWrite(PIN_GIROFLEX, LOW);
    delay(2);
    digitalWrite(PIN_GIROFLEX, HIGH);
    delayMicroseconds(2000);
    digitalWrite(PIN_GIROFLEX, LOW);
    delay(2);
  }
  giroflex_mode = i;
}
