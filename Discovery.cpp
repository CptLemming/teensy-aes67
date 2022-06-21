#include <QNEthernet.h>
#include <TeensyID.h>
#include "discovery.h"

Discovery::Discovery(qindesign::network::EthernetUDP &udp) {
  _udp = &udp;
}

void Discovery::start() {
  // Advertise as a Calrec device (legacy)
  // MDNS.begin("FB464176-0000-0000-B6C9-24E1ABA93A3F-0-pri");
  // MDNS.addService("UR6500-14E1ABA93A3F-0-pri-5", "_http", "_tcp", 8080);
  // MDNS.addService("UR6500-14E1ABA93A3F-0-pri-5", "_calrec-node", "_tcp", _websocketServerPort, []() {
  //   return std::vector<String>{
  //     "nic=2",
  //     "address=192.168.30.210",
  //     "version=0.1",
  //     "corestatus=ACTIVE"
  //   };
  // });
}

void Discovery::update() {
  if (_advertiseTimer > _advertiseInterval) {
    _advertiseTimer = 0;
    send();
  }
}

void Discovery::send() {
  uint8_t bufferIndex = 0;
  uint8_t writeBuffer[100];
  writeBuffer[bufferIndex++] = 1; // Message type = 1
  writeBuffer[bufferIndex++] = 1; // Message format = 1
  writeBuffer[bufferIndex++] = 3; // CCP Version
  writeBuffer[bufferIndex++] = 3; // Core status (3=ACTIVE)

  // UUIDv4
  uint8_t uuid[16];
  teensyUUID(uuid);
  for (uint8_t i = 0; i < 16; i++) {
    writeBuffer[bufferIndex++] = uuid[i];
  }

  // IPv4 address
  writeBuffer[bufferIndex++] = 192;
  writeBuffer[bufferIndex++] = 168;
  writeBuffer[bufferIndex++] = 30;
  writeBuffer[bufferIndex++] = 210;

  // TCP Port (50002)
  writeBuffer[bufferIndex++] = 0xC3;
  writeBuffer[bufferIndex++] = 0x52;

  // TIme to live?
  writeBuffer[bufferIndex++] = 0x00;
  writeBuffer[bufferIndex++] = 0x24;

  writeBuffer[bufferIndex++] = 2; // Nic index

  const char* deviceName = "FB464176-0000-0000-B6C9-24E1ABA93A3F-0-pri.local";
  writeBuffer[bufferIndex++] = strlen(deviceName);
  for (uint8_t i = 0; i < strlen(deviceName); i++) {
    writeBuffer[bufferIndex++] = deviceName[i];
  }

  _udp->beginPacket(_advertisementIP, _advertisementPort);
  _udp->write(writeBuffer, bufferIndex);
  _udp->endPacket();
}
