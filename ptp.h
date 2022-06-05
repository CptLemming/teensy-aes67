#ifndef teensy_aes67_ptp_h_
#define teensy_aes67_ptp_h_

#include "RTClib.h"
#include <QNEthernet.h>

struct TimeInSeconds {
   uint64_t value : 48;
};

struct Time {
  TimeInSeconds seconds;
  uint32_t microseconds;
};

class PTP {
  public:
    PTP(byte *mac, RTC_DS3231 &rtc, qindesign::network::EthernetUDP &ptpEvent, qindesign::network::EthernetUDP &ptpManagement);
    void start();
    void update();
    Time getTime();
    uint64_t getSeconds();
    uint32_t getNanoSeconds();
  private:
    void processPtpEvent();
    void processPtpManagementEvent();
    void writePTPHeader(uint8_t *buffer, uint8_t type, uint16_t flags, uint8_t control, uint16_t sequenceNo, uint16_t length);
    void sendAnnounceMessage();
    void sendSyncMessage();
    void sendFollowupMessage(uint16_t sequenceNo);
    void sendDelayRequestMessage();
    void sendDelayResponseMessage(uint16_t sequenceNo, uint8_t *senderClock, uint8_t *senderPort);
    byte *_mac;
    RTC_DS3231 *_rtc;
    qindesign::network::EthernetUDP *_ptpEvent;
    qindesign::network::EthernetUDP *_ptpManagement;
    elapsedMicros _nanosecondTimer;
    IPAddress _ptpAddress{224, 0, 1, 129};
    unsigned int _ptpEventPort = 319;
    unsigned int _ptpGeneralPort = 320;
    uint32_t _seconds = 0;
    uint32_t _nanoseconds = 0;

    const uint16_t announce_packet_size = 64;
    const uint16_t sync_packet_size = 44;
    const uint16_t follow_up_packet_size = 44;
    const uint16_t delay_req_packet_size = 44;
    const uint16_t delay_res_packet_size = 54;

    uint16_t _delayReqSequenceNo = 0;
    uint16_t _syncSequenceNo = 0;
    uint16_t _announceSequenceNo = 0;

    Time _t1, _t2, _t3, _t4 = Time{0, 0};
};

#endif
