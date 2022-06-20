#ifndef teensy_aes67_discovery_h_
#define teensy_aes67_discovery_h_

#include <Audio.h>
#include <QNEthernet.h>

class Discovery {
  public:
    void start();
    void update();
    Discovery(qindesign::network::EthernetUDP &Udp);
  private:
    elapsedMillis _advertiseTimer;
    uint16_t _advertiseInterval = 10000; // 10s
    IPAddress _advertisementIP{239, 255, 255, 255};
    uint16_t _advertisementPort = 50004;
    qindesign::network::EthernetUDP *_udp;
    void send();
};

#endif
