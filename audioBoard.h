#ifndef teensy_aes67_audio_h_
#define teensy_aes67_audio_h_

#include <Audio.h>
#include <QNEthernet.h>
#include <queue>

class AudioBoard {
  public:
    void start();
    void readPackets();
    void readAudio();
    void sendRTPData();
    AudioBoard(AudioPlayQueue &audioReceiverQueue, AudioRecordQueue &audioTransmitterQueue, qindesign::network::EthernetUDP &Udp);
  private:
    AudioPlayQueue *_audioReceiverQueue;
    AudioRecordQueue *_audioTransmitterQueue;
    qindesign::network::EthernetUDP *_udp;
    unsigned long _packetTimestamp = 0;
    unsigned long _startupTimestamp = 0;
    uint8_t _incomingAudioBuffer[256];
    uint16_t _incomingAudioBufferIndex = 0;
    static const unsigned short _rtpPacketSize = 100;
    uint8_t _packetStreamBuffer[_rtpPacketSize];
    std::queue <int> _OutputQueue;
    uint16_t _outputBuffer[48][50];
    IPAddress _multicastIP{239, 4, 212, 163}; // Core
    //IPAddress multicastIP{239, 34, 13, 86}; // RPI
    //IPAddress multicastIP{239, 219, 130, 34}; // PC
    int _multicastPort = 5004;
    int _numOutputBuffers = 48;
    int _numSamples = 44;
    int _outputBufferReaderIndex = 0;
    int _outputBufferWriterIndex = 0;
    int _bufferIndex = 0;
    unsigned long _outputTimestamp = 0;
    unsigned long _outputLastTimestamp = 0;
    unsigned long _ssrc = 1649937450;
    unsigned short _rtpPayloadType = 32864;
    unsigned short _sequenceNo = 0;

    void timestamp();
};

#endif
