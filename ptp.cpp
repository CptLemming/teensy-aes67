#include <QNEthernet.h>
#include "RTClib.h"
#include "ptp.h"

PTP::PTP(byte *mac, RTC_DS3231 &rtc, qindesign::network::EthernetUDP &ptpEvent, qindesign::network::EthernetUDP &ptpManagement) {
  _mac = mac;
  _rtc = &rtc;
  _ptpEvent = &ptpEvent;
  _ptpManagement = &ptpManagement;
}

void PTP::start() {
  _delayReqSequenceNo = 0;
  _syncSequenceNo = 0;
  _announceSequenceNo = 0;

  // x4 per second
  // announceTimer.begin(sendAnnounceMessage, 200000);
  // x8 per second
  // syncTimer.begin(sendSyncMessage, 100000);
  // delayRequestTimer.begin(sendDelayRequestMessage, 100000);

  _ptpEvent->beginMulticast(_ptpAddress, _ptpEventPort, true);
  _ptpManagement->beginMulticast(_ptpAddress, _ptpGeneralPort, true);
}

void PTP::update() {
  _nanoseconds += _nanosecondTimer * 1000;
  _nanosecondTimer = 0;
  if (_nanoseconds > 1000000000) {
    _nanoseconds = 0;
  }

  processPtpEvent();
  processPtpManagementEvent();
}

void PTP::processPtpEvent() {
  int size = _ptpEvent->parsePacket();

  if (size > 0) {
    Time recv_ts = getTime();
    uint8_t packetStreamBuffer[size];
    _ptpEvent->read(packetStreamBuffer, size);

    int8_t type = packetStreamBuffer[0] & 0x0f;
    int8_t version = packetStreamBuffer[1];
    int8_t domain = packetStreamBuffer[4];
    int16_t flags = (packetStreamBuffer[6] << 8) | (packetStreamBuffer[7] & 0xff);

    // Only process sync messages
    if (version == 2 && domain == 127 && type == 0x00) {
      if ((flags & 0x0200) == 0x0200) {
        // Two step
        // Serial.println("Two step sync message");
        _t2 = recv_ts;
        // Serial.print("T2 :: ");
        // Serial.print(t2.seconds.value);
        // Serial.print(" :: ");
        // Serial.println(t2.nanoseconds);
      } else {
        // TODO
      }
    }
  }
}

void PTP::processPtpManagementEvent() {
  int size = _ptpManagement->parsePacket();

  if (size > 0) {
    uint8_t packetStreamBuffer[size];
    _ptpManagement->read(packetStreamBuffer, size);

    int8_t type = packetStreamBuffer[0] & 0x0f;
    int8_t version = packetStreamBuffer[1];
    int8_t domain = packetStreamBuffer[4];
    int16_t flags = (packetStreamBuffer[6] << 8) | (packetStreamBuffer[7] & 0xff);

    if (version == 2 && domain == 127) {
      if (type == 0x08) {
        // Serial.println("followup message");
        uint64_t followUp_seconds = ((uint64_t) packetStreamBuffer[34]) << 40;
        followUp_seconds += ((uint64_t) packetStreamBuffer[35]) << 32;
        followUp_seconds += ((uint64_t) packetStreamBuffer[36]) << 24;
        followUp_seconds += ((uint64_t) packetStreamBuffer[37]) << 16;
        followUp_seconds += ((uint64_t) packetStreamBuffer[38]) << 8;
        followUp_seconds += packetStreamBuffer[39];
        uint32_t followUp_nanoSeconds = ((uint32_t) packetStreamBuffer[40]) << 24;
        followUp_nanoSeconds += ((uint32_t) packetStreamBuffer[41]) << 16;
        followUp_nanoSeconds += ((uint32_t) packetStreamBuffer[42]) << 8;
        followUp_nanoSeconds += packetStreamBuffer[43];
        // Follow up
        _t1 = Time{
          followUp_seconds,
          followUp_nanoSeconds / 1000
        };
        // Serial.print("Time diff :: ");
        // Serial.print(nanoseconds);
        // Serial.print(" - ");
        // Serial.print(followUp_nanoSeconds);
        // Serial.print(" = ");
        // Serial.println(abs(followUp_nanoSeconds - nanoseconds));
        _seconds = followUp_seconds;
        _nanoseconds = followUp_nanoSeconds;

        // Serial.print("T1 :: ");
        // Serial.print(t1.seconds.value);
        // Serial.print(" :: ");
        // Serial.println(t1.microseconds);

        // Send a delay request every time we receive a follow up message
        sendDelayRequestMessage();
      } else if (
        type == 0x09 &&
        _mac[0] == packetStreamBuffer[46] &&
        _mac[1] == packetStreamBuffer[47] &&
        _mac[2] == packetStreamBuffer[48] &&
        _mac[3] == packetStreamBuffer[49] &&
        _mac[4] == packetStreamBuffer[50] &&
        _mac[5] == packetStreamBuffer[51]
      ) {
        // Serial.println("delay response message");
        uint64_t delayResponse_seconds = ((uint64_t) packetStreamBuffer[34]) << 40;
        delayResponse_seconds += ((uint64_t) packetStreamBuffer[35]) << 32;
        delayResponse_seconds += ((uint64_t) packetStreamBuffer[36]) << 24;
        delayResponse_seconds += ((uint64_t) packetStreamBuffer[37]) << 16;
        delayResponse_seconds += ((uint64_t) packetStreamBuffer[38]) << 8;
        delayResponse_seconds += packetStreamBuffer[39];
        uint32_t delayResponse_nanoSeconds = ((uint32_t) packetStreamBuffer[40]) << 24;
        delayResponse_nanoSeconds += ((uint32_t) packetStreamBuffer[41]) << 16;
        delayResponse_nanoSeconds += ((uint32_t) packetStreamBuffer[42]) << 8;
        delayResponse_nanoSeconds += packetStreamBuffer[43];
        // Delay response
        _t4 = Time{
          delayResponse_seconds,
          delayResponse_nanoSeconds / 1000
        };

        // Serial.print("REM:");
        // Serial.print(t4.microseconds - t1.microseconds);
        // Serial.print(",");
        // Serial.print("LOCAL:");
        // Serial.print(t3.microseconds - t2.microseconds);
        // Serial.println();

        // Serial.print("T4 :: ");
        // Serial.print(t4.seconds.value);
        // Serial.print(" :: ");
        // Serial.println(t4.nanoseconds);

        Time delta = Time{0, 0};
        delta.seconds.value = ((_t4.seconds.value - _t1.seconds.value) - (_t3.seconds.value - _t2.seconds.value)) / 2;
        delta.microseconds = ((_t4.microseconds - _t1.microseconds) - (_t3.microseconds - _t2.microseconds)) / 2;

        // Serial.print("SEC:");
        // Serial.print(delta.seconds.value);
        // Serial.print(",");
        // Serial.print("MS:");
        // Serial.println(delta.microseconds);

        // offset.seconds.value = delta.seconds.value;
		    // offset.microseconds = delta.microseconds;

        // Serial.print(offset.seconds.value);
        // Serial.print(",");
        // Serial.println(offset.microseconds);

        // Serial.print("Offset :: ");
        // Serial.print(offset.seconds.value);
        // Serial.print(" :: ");
        // Serial.println(offset.nanoseconds);
      }
    }
  }
}

uint32_t PTP::getNanoSeconds() {
  return _nanoseconds;
}

Time PTP::getTime() {
  // return Time{_rtc->now().unixtime(), _nanoseconds};
  return Time{_seconds, _nanoseconds};
}

void PTP::writePTPHeader(uint8_t *buffer, uint8_t type, uint16_t flags, uint8_t control, uint16_t sequenceNo, uint16_t length) {
  // Transport & type
  buffer[0] = type;
  // PTP version
  buffer[1] = 2;
  // Length
  buffer[2] = length >> 8;
  buffer[3] = length & 0x00ff;
  // Domain
  buffer[4] = 127;
  // <-- reserved -->
  buffer[5] = 0;
  // Flags
  buffer[6] = flags >> 8;
  buffer[7] = flags & 0x00ff;
  // Correction
  buffer[8] = 0;
  buffer[9] = 0;
  buffer[10] = 0;
  buffer[11] = 0;
  buffer[12] = 0;
  buffer[13] = 0;
  buffer[14] = 0;
  buffer[15] = 0;
  // <-- reserved -->
  buffer[16] = 0;
  buffer[17] = 0;
  buffer[18] = 0;
  buffer[19] = 0;
  // Clock ID = 0x70B3D5FFFE042A9C
  buffer[20] = 0xFF;
  buffer[21] = 0xFF;
  buffer[22] = _mac[0];
  buffer[23] = _mac[1];
  buffer[24] = _mac[2];
  buffer[25] = _mac[3];
  buffer[26] = _mac[4];
  buffer[27] = _mac[5];
  // Source port ID
  buffer[28] = 0;
  buffer[29] = 1;
  // Sequence ID
  buffer[30] = sequenceNo >> 8;
  buffer[31] = sequenceNo & 0x00ff;
  // Control type
  buffer[32] = control;
  // Log message period
  buffer[33] = 3;
}

void PTP::sendAnnounceMessage() {
  uint8_t writeBuffer[announce_packet_size];
  writePTPHeader(writeBuffer, 0x0b, 0x0008, 5, _announceSequenceNo, announce_packet_size);
  uint16_t gmClockVariance = 17258;
  // Timestamp (seconds)
  writeBuffer[34] = 0;
  writeBuffer[35] = 0;
  writeBuffer[36] = 0;
  writeBuffer[37] = 0;
  writeBuffer[38] = 0;
  writeBuffer[39] = 0;
  // Timestamp (nanoseconds)
  writeBuffer[40] = 0;
  writeBuffer[41] = 0;
  writeBuffer[42] = 0;
  writeBuffer[43] = 0;
  // Current UTC offset
  writeBuffer[44] = 0;
  writeBuffer[45] = 37;
  // Unknown??
  writeBuffer[46] = 0;
  // Priority 1
  writeBuffer[47] = 99;
  // GM clock class
  writeBuffer[48] = 248;
  // GM clock accuracy
  writeBuffer[49] = 0x21;
  // GM clock variance
  writeBuffer[50] = gmClockVariance >> 8;
  writeBuffer[51] = gmClockVariance & 0x00ff;
  // Priority 2
  writeBuffer[52] = 99;
  // GM Clock ID = 0x70 B3 D5 FF FE 04 2A 9C
  writeBuffer[53] = 0x70;
  writeBuffer[54] = 0xB3;
  writeBuffer[55] = 0xD5;
  writeBuffer[56] = 0xFF;
  writeBuffer[57] = 0xFE;
  writeBuffer[58] = 0x04;
  writeBuffer[59] = 0x2A;
  writeBuffer[60] = 0x9C;
  // Steps removed
  writeBuffer[61] = 0;
  writeBuffer[62] = 0;
  // Time source
  writeBuffer[63] = 0xa0;

  _ptpManagement->beginPacket(_ptpAddress, _ptpGeneralPort);
  _ptpManagement->write(writeBuffer, announce_packet_size);
  _ptpManagement->endPacket();

  _announceSequenceNo++;
}

void PTP::sendSyncMessage() {
  uint8_t writeBuffer[sync_packet_size];
  writePTPHeader(writeBuffer, 0x00, 0x0200, 0, _syncSequenceNo, sync_packet_size);
  // Timestamp (seconds)
  writeBuffer[34] = 0;
  writeBuffer[35] = 0;
  writeBuffer[36] = 0;
  writeBuffer[37] = 0;
  writeBuffer[38] = 0;
  writeBuffer[39] = 0;
  // Timestamp (nanoseconds)
  writeBuffer[40] = 0;
  writeBuffer[41] = 0;
  writeBuffer[42] = 0;
  writeBuffer[43] = 0;

  _ptpEvent->beginPacket(_ptpAddress, _ptpEventPort);
  _ptpEvent->write(writeBuffer, sync_packet_size);
  _ptpEvent->endPacket();

  _syncSequenceNo++;
  // Serial.println("Send sync msg");
}

void PTP::sendFollowupMessage(uint16_t sequenceNo) {
  uint8_t writeBuffer[follow_up_packet_size];
  writePTPHeader(writeBuffer, 0x08, 0x0000, 2, sequenceNo, follow_up_packet_size);
  // Timestamp (seconds)
  writeBuffer[34] = 0;
  writeBuffer[35] = 0;
  writeBuffer[36] = 0;
  writeBuffer[37] = 0;
  writeBuffer[38] = 0;
  writeBuffer[39] = 0;
  // Timestamp (nanoseconds)
  writeBuffer[40] = 0;
  writeBuffer[41] = 0;
  writeBuffer[42] = 0;
  writeBuffer[43] = 0;

  _ptpEvent->beginPacket(_ptpAddress, _ptpGeneralPort);
  _ptpEvent->write(writeBuffer, follow_up_packet_size);
  _ptpEvent->endPacket();
}

void PTP::sendDelayRequestMessage() {
  _t3 = getTime();

  // Serial.print("T3 :: ");
  // Serial.print(t3.seconds.value);
  // Serial.print(" :: ");
  // Serial.println(t3.nanoseconds);

  uint8_t writeBuffer[delay_req_packet_size];
  writePTPHeader(writeBuffer, 0x01, 0x0000, 1, _delayReqSequenceNo, delay_req_packet_size);
  // Timestamp (seconds)
  writeBuffer[34] = (_t3.seconds.value >> 40) & 0xff;
  writeBuffer[35] = (_t3.seconds.value >> 32) & 0xff;
  writeBuffer[36] = (_t3.seconds.value >> 24) & 0xff;
  writeBuffer[37] = (_t3.seconds.value >> 16) & 0xff;
  writeBuffer[38] = (_t3.seconds.value >> 8) & 0xff;
  writeBuffer[39] = _t3.seconds.value & 0xff;
  // Timestamp (nanoseconds)
  writeBuffer[40] = (_t3.microseconds >> 24) & 0xff;
  writeBuffer[41] = (_t3.microseconds >> 16) & 0xff;
  writeBuffer[42] = (_t3.microseconds >> 8) & 0xff;
  writeBuffer[43] = _t3.microseconds & 0xff;

  _ptpEvent->beginPacket(_ptpAddress, _ptpEventPort);
  _ptpEvent->write(writeBuffer, delay_req_packet_size);
  _ptpEvent->endPacket();

  _delayReqSequenceNo++;
}

void PTP::sendDelayResponseMessage(uint16_t sequenceNo, uint8_t *senderClock, uint8_t *senderPort) {
  uint8_t writeBuffer[delay_res_packet_size];
  writePTPHeader(writeBuffer, 0x09, 0x0000, 3, sequenceNo, delay_res_packet_size);
  // Timestamp (seconds)
  writeBuffer[34] = 0;
  writeBuffer[35] = 0;
  writeBuffer[36] = 0;
  writeBuffer[37] = 0;
  writeBuffer[38] = 0;
  writeBuffer[39] = 0;
  // Timestamp (nanoseconds)
  writeBuffer[40] = 0;
  writeBuffer[41] = 0;
  writeBuffer[42] = 0;
  writeBuffer[43] = 0;
  // Req Clock ID
  writeBuffer[44] = senderClock[0];
  writeBuffer[45] = senderClock[1];
  writeBuffer[46] = senderClock[2];
  writeBuffer[47] = senderClock[3];
  writeBuffer[48] = senderClock[4];
  writeBuffer[49] = senderClock[5];
  writeBuffer[50] = senderClock[6];
  writeBuffer[51] = senderClock[7];
  // Req Clock port ID
  writeBuffer[52] = senderPort[0];
  writeBuffer[53] = senderPort[1];

  _ptpManagement->beginPacket(_ptpAddress, _ptpEventPort);
  _ptpManagement->write(writeBuffer, delay_res_packet_size);
  _ptpManagement->endPacket();
}
