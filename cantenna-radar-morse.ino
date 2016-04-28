
// CONNECTIONS:
// DS3231 SDA --> SDA
// DS3231 SCL --> SCL
// DS3231 VCC --> 3.3v or 5v
// DS3231 GND --> GND

#if defined(ESP8266)
#include <pgmspace.h>
#else
#include <avr/pgmspace.h>
#endif
#include <Wire.h>  // must be incuded here so that Arduino library object file references work
#include <RtcDS3231.h>
#include <morse.h>

#define PIN_STATUS  13
#define PIN_SPEAKER 3

LEDMorseSender radarMorseSender(PIN_STATUS,
                                20.5);  // wpm
SpeakerMorseSender speakerSender(
  PIN_SPEAKER,
  880,  // tone frequency
  110,  // carrier frequency
  20.5);  // wpm


RtcDS3231 Rtc;

// Interrupt Pin Lookup Table
// (copied from Arduino Docs)
//
// CAUTION:  The interrupts are Arduino numbers NOT Atmel numbers
//   and may not match (example, Mega2560 int.4 is actually Atmel Int2)
//   this is only an issue if you plan to use the lower level interupt features
//
// Board           int.0    int.1   int.2   int.3   int.4   int.5
// ---------------------------------------------------------------
// Uno, Ethernet    2       3
// Mega2560         2       3       21      20     [19]      18
// Leonardo         3       2       0       1       7

#define RtcSquareWavePin 3 // Uno/ Nano
#define RtcSquareWaveInterrupt 1 // Uno/ Nano

// marked volatile so interrupt can safely modify them and
// other code can safely read and modify them
volatile uint16_t interuptCount = 0;
volatile bool interuptFlag = false;

void InteruptServiceRoutine()
{
  // since this interupted any other running code,
  // don't do anything that takes long and especially avoid
  // any communications calls within this routine
  interuptCount++;
  interuptFlag = true;
}

void setup ()
{
  Serial.begin(57600);

  // set the interupt pin to input mode
  pinMode(RtcSquareWavePin, INPUT);

  //--------RTC SETUP ------------
  Rtc.Begin();
  // if you are using ESP-01 then uncomment the line below to reset the pins to
  // the available pins for SDA, SCL
  // Wire.begin(0, 2); // due to limited pins, use pin 0 and 2 for SDA, SCL

  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);

  if (!Rtc.IsDateTimeValid())
  {
    Serial.println("RTC lost confidence in the DateTime!");
    Rtc.SetDateTime(compiled);
  }

  if (!Rtc.GetIsRunning())
  {
    Serial.println("RTC was not actively running, starting now");
    Rtc.SetIsRunning(true);
  }

  RtcDateTime now = Rtc.GetDateTime();
  if (now < compiled)
  {
    Serial.println("RTC is older than compile time!  (Updating DateTime)");
    Rtc.SetDateTime(compiled);
  }

  Rtc.Enable32kHzPin(false);
  Rtc.SetSquareWavePin(DS3231SquareWavePin_ModeAlarmBoth);

  // Alarm 1 set to trigger every day when
  // the hours, minutes, and seconds match
  RtcDateTime alarmTime = now + 88; // into the future
  DS3231AlarmOne alarm1(
    alarmTime.Day(),
    alarmTime.Hour(),
    alarmTime.Minute(),
    alarmTime.Second(),
    DS3231AlarmOneControl_HoursMinutesSecondsMatch);
  Rtc.SetAlarmOne(alarm1);

  // Alarm 2 set to trigger at the top of the minute
  DS3231AlarmTwo alarm2(
    0,
    0,
    0,
    DS3231AlarmTwoControl_OncePerMinute);
  Rtc.SetAlarmTwo(alarm2);

  // throw away any old alarm state before we ran
  Rtc.LatchAlarmsTriggeredFlags();

  // setup external interupt
  attachInterrupt(RtcSquareWaveInterrupt, InteruptServiceRoutine, FALLING);

  radarMorseSender.setup();
  radarMorseSender.setMessage(String(PROSIGN_KN) + String(" "));
  speakerSender.setup();
  speakerSender.setMessage(String("    carrier cw    "));
  radarMorseSender.sendBlocking();
  speakerSender.sendBlocking();

  radarMorseSender.setMessage(String("73 de kc1ezp "));
  radarMorseSender.sendBlocking();
}

void loop ()
{
  if (!radarMorseSender.continueSending())
  {
    radarMorseSender.startSending();
  }

  if (!speakerSender.continueSending())
  {
    speakerSender.startSending();
  }
  if (!Rtc.IsDateTimeValid())
  {
    // Common Cuases:
    //    1) the battery on the device is low or even missing and the power line was disconnected
    Serial.println("RTC lost confidence in the DateTime!");
  }

  RtcDateTime now = Rtc.GetDateTime();
  printDateTime(now);
  Serial.println();

  RtcTemperature temp = Rtc.GetTemperature();
  Serial.print(temp.AsFloat());
  Serial.println("C");

  delay(10000); // ten seconds
}

#define countof(a) (sizeof(a) / sizeof(a[0]))

void printDateTime(const RtcDateTime& dt)
{
  char datestring[20];

  snprintf_P(datestring,
             countof(datestring),
             PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
             dt.Month(),
             dt.Day(),
             dt.Year(),
             dt.Hour(),
             dt.Minute(),
             dt.Second() );
  Serial.print(datestring);
}

