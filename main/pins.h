#pragma once
// All wiring in one place. Buttons: one leg → GPIO, other leg → GND (internal pull-up used).

#define PIN_SDA     8    // OLED SDA
#define PIN_SCL     9    // OLED SCL
#define BTN_LEFT    39   // left big button
#define BTN_FIRE    41   // small button
#define BTN_RIGHT   21   // right big button
#define PIN_BUZZER  14   // passive buzzer (+); (-) → GND
