#define WEBSOCKETS_USE_ETHERNET       true
#define USE_QN_ETHERNET               true
#define _WEBSOCKETS_LOGLEVEL_         1

#include <Adafruit_NeoTrellis.h>
#include <ArduinoJson.h>
#include <Audio.h>
#include <QNEthernet.h>
#include <SPI.h>
#include <SD.h>
#include <StreamUtils.h>
#include <TeensyID.h>
#include <Wire.h>

#include "audioBoard.h"
#include "deviceModel.h"
#include "websocket.hpp"
#include "plotter.h"

#define SDCARD_CS_PIN    BUILTIN_SDCARD
#define SDCARD_MOSI_PIN  11
#define SDCARD_SCK_PIN   13

// ====== AUDIO
// AudioInputUSB            usb1;
Plotter                  plotter(8);
AudioPlayQueue           audioReceiverQueue;
AudioRecordQueue         audioTransmitterQueue;
AudioOutputI2S           i2s2;
// AudioOutputUSB           usb2;
AudioSynthWaveform       waveform1;
// Receive network audio, play to USB & headphone jack
// AudioConnection          patchCord1(audioReceiverQueue, 0, plotter, 0);
// AudioConnection          patchCord2(audioReceiverQueue, 0, plotter, 1);
AudioConnection          patchCord3(audioReceiverQueue, 0, i2s2, 0);
AudioConnection          patchCord4(audioReceiverQueue, 0, i2s2, 1);
// AudioConnection          patchCord5(audioReceiverQueue, 0, usb2, 0);
// AudioConnection          patchCord6(audioReceiverQueue, 0, usb2, 1);
// Receive USB audio, play to network & headphone jack
// AudioConnection          patchCord1(usb1, 0, plotter, 0);
// AudioConnection          patchCord2(usb1, 1, plotter, 1);
// AudioConnection          patchCord3(usb1, 0, audioTransmitterQueue, 0);
// AudioConnection          patchCord4(usb1, 0, i2s2, 0);
// AudioConnection          patchCord5(usb1, 1, i2s2, 1);
AudioControlSGTL5000     sgtl5000_1;

// ====== ETHERNET
IPAddress staticIP{192, 168, 30, 210};
IPAddress subnetMask{255, 255, 255, 0};
IPAddress gateway{192, 168, 30, 1};

// ====== VARIABLES
byte mac[6];
qindesign::network::EthernetUDP udp;
websockets2_generic::WebsocketsServer socketServer;

// ====== CLASSES
IntervalTimer rtpOutputTimer;
Adafruit_NeoTrellis trellis;
AudioBoard audioBoard(audioReceiverQueue, audioTransmitterQueue, udp);
DeviceModel deviceModel;
CCPWebsocket websocket(socketServer, deviceModel, trellis);

void setup() {
  Serial.begin(115200);

  // Set the MAC address.
  teensyMAC(mac);

  // Setup audio board
  Serial.println("Setup audio");
  AudioMemory(128);
  sgtl5000_1.enable();
  sgtl5000_1.volume(0.5);
  audioReceiverQueue.setBehaviour(AudioPlayQueue::NON_STALLING);
  audioReceiverQueue.setMaxBuffers(32);
  audioTransmitterQueue.begin();
  audioBoard.start();

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

  // Setup networking
  Serial.println("Setup network");
  qindesign::network::Ethernet.setHostname("FB464176-0000-0000-B6C9-24E1ABA93A3F-0-pri");
  qindesign::network::Ethernet.macAddress(mac);
  qindesign::network::Ethernet.begin(staticIP, subnetMask, gateway);
  qindesign::network::Ethernet.onLinkState([](bool state) {
    Serial.printf("[Ethernet] Link %s\n", state ? "ON" : "OFF");
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
  // plotter.activate(true);
}

// TODO Can this be inlined in the timer call
void sendRTPData() {
  audioBoard.sendRTPData();
}

void loop() {
  websocket.update();
  audioBoard.readPackets();
  audioBoard.readAudio();
  trellis.read();
}

TrellisCallback onButtonAction(keyEvent evt){
  Serial.print("Press btn ::");
  Serial.println(evt.bit.NUM);

  // 8x GPI buttons
  if (evt.bit.NUM >= 0 && evt.bit.NUM < 8) {
    deviceModel.updateGpiState(evt.bit.NUM, evt.bit.EDGE == SEESAW_KEYPAD_EDGE_RISING);
    websocket.processPinChange();
  }

  return 0;
}
