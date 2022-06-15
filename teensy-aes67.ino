#define WEBSOCKETS_USE_ETHERNET       true
#define USE_QN_ETHERNET               true
#define _WEBSOCKETS_LOGLEVEL_         1

#include "RTClib.h"
#include "USBHost_t36.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoTrellis.h>
#include <ArduinoJson.h>
#include <Audio.h>
#include <QNEthernet.h>
#include <SPI.h>
#include <SD.h>
#include <TeensyID.h>
#include <Wire.h>

#include "input_i2s2_16bit.h"
#include "audioBoard.h"
#include "deviceModel.h"
#include "ptp.h"
#include "websocket.hpp"
#include "plotter.h"

#define SDCARD_CS_PIN    BUILTIN_SDCARD
#define SDCARD_MOSI_PIN  11
#define SDCARD_SCK_PIN   13
#define USBBAUD          115200
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define OLED_RESET     4 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32

// ====== AUDIO
// AudioInputUSB            usb1;
Plotter                   plotter(8);
AudioPlayQueue            audioReceiverQueue;
AudioRecordQueue          audioTransmitterQueue;
AudioOutputI2S            i2s;
// Bluetooth audio receiver via esp32
AudioInputI2S2_16bitslave i2sSlaveInput;
// AudioOutputUSB           usb2;
AudioSynthWaveform        waveform1;
AudioConnection           patchCord1(waveform1, 0, plotter, 0);
AudioConnection           patchCord2(waveform1, 0, plotter, 1);
AudioConnection           patchCord3(waveform1, 0, audioTransmitterQueue, 0);
AudioConnection           patchCord4(waveform1, 0, i2s, 0);
AudioConnection           patchCord5(waveform1, 0, i2s, 1);
// Receive network audio, play to USB & headphone jack
// AudioConnection          patchCord1(audioReceiverQueue, 0, plotter, 0);
// AudioConnection          patchCord2(audioReceiverQueue, 0, plotter, 1);
// AudioConnection          patchCord3(audioReceiverQueue, 0, i2s, 0);
// AudioConnection          patchCord4(audioReceiverQueue, 0, i2s, 1);
// AudioConnection          patchCord5(audioReceiverQueue, 0, usb2, 0);
// AudioConnection          patchCord6(audioReceiverQueue, 0, usb2, 1);
// Receive USB audio, play to network & headphone jack
// AudioConnection          patchCord1(usb1, 0, plotter, 0);
// AudioConnection          patchCord2(usb1, 1, plotter, 1);
// AudioConnection          patchCord3(usb1, 0, audioTransmitterQueue, 0);
// AudioConnection          patchCord4(usb1, 0, i2s, 0);
// AudioConnection          patchCord5(usb1, 1, i2s, 1);
AudioControlSGTL5000     sgtl5000_1;

// ====== ETHERNET
IPAddress staticIP{192, 168, 30, 210};
IPAddress subnetMask{255, 255, 255, 0};
IPAddress gateway{192, 168, 30, 1};

// ====== VARIABLES
byte mac[6];
qindesign::network::EthernetUDP udp;
qindesign::network::EthernetUDP ptpEvent;
qindesign::network::EthernetUDP ptpManagement;
websockets2_generic::WebsocketsServer socketServer;
USBHost esp32Serial;
RTC_DS3231 rtc;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

// ====== CLASSES
IntervalTimer rtpOutputTimer;
Adafruit_NeoTrellis trellis;
PTP ptp(mac, rtc, ptpEvent, ptpManagement);
AudioBoard audioBoard(audioReceiverQueue, audioTransmitterQueue, udp, ptp);
DeviceModel deviceModel;
CCPWebsocket websocket(socketServer, deviceModel, trellis, display);

void setup() {
  Serial.begin(115200);

  if ( Serial && CrashReport ) {
    // Output a crash report on reboot
    Serial.print(CrashReport);
  }

  // Set the MAC address.
  teensyMAC(mac);

  // Start USB host (power ESP32)
  Serial.println("Setup ESP32");
  esp32Serial.begin();

  // Start RTC clock
  Serial.println("Setup clock");
  rtc.begin();
  if (rtc.lostPower()) {
  Serial.println("Clock: Power lost setting time");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  Serial.print("Time: ");
  DateTime now = rtc.now();
  Serial.print(now.year(), DEC);
  Serial.print('/');
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.day(), DEC);
  Serial.print(" (");
  Serial.print(daysOfTheWeek[now.dayOfTheWeek()]);
  Serial.print(") ");
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.print(now.second(), DEC);
  Serial.println();
  Serial.print("Temperature: ");
  Serial.print(rtc.getTemperature());
  Serial.println(" C");

  // Setup audio board
  Serial.println("Setup audio");
  AudioMemory(128);
  sgtl5000_1.enable();
  sgtl5000_1.volume(0.8);
  audioReceiverQueue.setBehaviour(AudioPlayQueue::NON_STALLING);
  audioReceiverQueue.setMaxBuffers(8);
  audioTransmitterQueue.begin();

  waveform1.pulseWidth(0.5);
  waveform1.begin(0.4, 220, WAVEFORM_PULSE);

  // Start a timer to send RTP data every 1ms
  Serial.println("Start timer");
  rtpOutputTimer.begin(sendRTPData, 1000);

  // Setup SD card
  Serial.println("Setup SD card");
  SPI.setMOSI(SDCARD_MOSI_PIN);
  SPI.setSCK(SDCARD_SCK_PIN);
  while (!SD.begin(SDCARD_CS_PIN)) {
    Serial.println("Failed to initialize SD library");
    delay(1000);
  }
  Serial.println("Load default config");
  // deviceModel.loadConfig();
  deviceModel.createDefaultConfig();
  deviceModel.saveConfig();

  // Start OLED display
  Serial.println("Setup OLED");
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  display.display();
  delay(10);
  websocket.drawDisplay();

  // Setup networking
  Serial.println("Setup network");
  qindesign::network::Ethernet.setHostname("FB464176-0000-0000-B6C9-24E1ABA93A3F-0-pri");
  qindesign::network::Ethernet.macAddress(mac);
  qindesign::network::Ethernet.begin(staticIP, subnetMask, gateway);
  qindesign::network::Ethernet.onLinkState([](bool state) {
    Serial.printf("[Ethernet] Link %s\n", state ? "ON" : "OFF");
    if (state) {
      audioBoard.start();
      ptp.start();
    }
  });

  // Start websocket server.
  Serial.println("Start webserver");
  websocket.start();

  Serial.println("Started teensy audio board");
  Serial.printf("Mac address:   %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.printf("Audio samples: %d\n", AUDIO_BLOCK_SAMPLES);
  Serial.print( "IP:            "); Serial.println(Ethernet.localIP());
  serializeJson(deviceModel.getDocument(), Serial);
  Serial.println();

  // Setup neotrellis 4x4 button board
  Serial.println("Setup neotrellis");
  if (!trellis.begin()) {
    Serial.println("Could not start trellis, check wiring?");
    while(1) delay(1);
  } else {
    Serial.println("NeoPixel Trellis started");
  }
  for(int i=0; i<NEO_TRELLIS_NUM_KEYS; i++){
    trellis.activateKey(i, SEESAW_KEYPAD_EDGE_RISING);
    trellis.activateKey(i, SEESAW_KEYPAD_EDGE_FALLING);
    trellis.registerCallback(i, onButtonAction);
    trellis.pixels.setPixelColor(i, 0);
  }

  // [DEBUG] Setup serial plotter
  plotter.activate(true);
}

void loop() {
  ptp.update();
  websocket.update();
  audioBoard.readPackets();
  audioBoard.readAudio();
  trellis.read();
  esp32Serial.Task();

  // TODO Move this to the socket manager
}

TrellisCallback onButtonAction(keyEvent evt){
  Serial.printf("Press btn :: %d\n", evt.bit.NUM);

  // 4x GPI buttons
  if (evt.bit.NUM >= 0 && evt.bit.NUM < 4) {
    deviceModel.updateGpiState(evt.bit.NUM, evt.bit.EDGE == SEESAW_KEYPAD_EDGE_RISING);
    websocket.updateGpiPin(evt.bit.NUM);
  }

  return 0;
}
