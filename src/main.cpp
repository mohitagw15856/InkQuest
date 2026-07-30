// InkQuest firmware entry point.
//
// Wires the ecosystem SDK singletons (display, gpio, powerManager) into inkkit's
// thin wrappers and hands them to the InkQuestApp. All the interesting logic
// lives in the host-tested core and the device layer; this file is just the
// Arduino setup/loop shell.
//
// On the host (no ARDUINO) this file compiles to an empty translation unit so a
// native/CI build of the portable core links cleanly.
#ifdef ARDUINO

#include <Arduino.h>
#include <HalDisplay.h>
#include <HalGPIO.h>
#include <HalPowerManager.h>

#include <inkkit/Buttons.h>
#include <inkkit/Display.h>
#include <inkkit/Power.h>

#include "device/InkQuestApp.h"
#include "device/TextRenderer.h"

// The SDK provides these singletons (see the ecosystem HAL).
using inkquest::InkQuestApp;
using inkquest::TextRenderer;

static inkkit::Display g_display(display);
static inkkit::Buttons g_buttons(gpio);
static inkkit::Power g_power(powerManager, display, gpio);
static TextRenderer g_renderer(g_display);
static InkQuestApp g_app(g_renderer, g_buttons, g_power);

void setup() {
  g_display.begin();
  // TODO(hardware-test): confirm whether the SD card / Storage singleton needs an
  // explicit begin()/mount here, or whether the SDK bootstraps it before setup().
  g_app.begin();
}

void loop() {
  g_buttons.update();
  g_app.tick();
  // TODO(hardware-test): tune the poll interval against button responsiveness and
  // e-paper refresh timing; 20 ms is a placeholder.
  delay(20);
}

#endif  // ARDUINO
