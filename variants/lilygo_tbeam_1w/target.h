#pragma once

#define RADIOLIB_STATIC_ONLY 1
#include <RadioLib.h>
#include <SPI.h>
#include <helpers/radiolib/RadioLibWrappers.h>
#include <helpers/radiolib/CustomSX1262Wrapper.h>
#include <helpers/AutoDiscoverRTCClock.h>
#include <helpers/sensors/EnvironmentSensorManager.h>
#include "TBeam1WBoard.h"

#ifdef DISPLAY_CLASS
  #include <helpers/ui/SH1106Display.h>
  #include <helpers/ui/MomentaryButton.h>
  extern DISPLAY_CLASS display;
  extern MomentaryButton user_btn;
#endif

extern TBeam1WBoard board;
extern WRAPPER_CLASS radio_driver;
extern AutoDiscoverRTCClock rtc_clock;
extern EnvironmentSensorManager sensors;

// Shared SPI bus used by the SX1262 and the on-board microSD slot.
// Exposing the exact bus instance lets the Room Server mount SD without
// creating a second SPI controller on the same pins.
extern SPIClass tbeam_shared_spi;

bool radio_init();
mesh::LocalIdentity radio_new_identity();

