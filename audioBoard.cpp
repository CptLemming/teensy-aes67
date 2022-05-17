#include <QNEthernet.h>
#include "audioBoard.h"

AudioBoard::AudioBoard(AudioPlayQueue &audioReceiverQueue, AudioRecordQueue &audioTransmitterQueue, qindesign::network::EthernetUDP &udp) {
  _audioReceiverQueue = &audioReceiverQueue;
  _audioTransmitterQueue = &audioTransmitterQueue;
  _udp = &udp;
  _startupTimestamp = millis();
}

void AudioBoard::start() {
  _udp->beginMulticast(_multicastIP, _multicastPort);
}

void AudioBoard::timestamp() {
  _packetTimestamp = millis();
}

void AudioBoard::readPackets() {
  int size = _udp->parsePacket();
  if (size < 1) {
    return;
  }
  _udp->read(_packetStreamBuffer, _rtpPacketSize);

  timestamp();
  unsigned short sequenceNumber = (_packetStreamBuffer[2] << 8) | (_packetStreamBuffer[3] & 0xff);

  // Debugging
  // Serial.print("Sequence "); Serial.print(sequenceNumber);
  // Serial.print("    ");
  // Serial.print("Time "); Serial.print(_packetTimestamp - _startupTimestamp);
  // Serial.println();

  for (int sample = 12; sample < _rtpPacketSize; sample++) {
    _incomingAudioBufferIndex += 1;

    if (_incomingAudioBufferIndex >= 256) {
      _incomingAudioBufferIndex = 0;
      int16_t *p = _audioReceiverQueue->getBuffer();
      memcpy(p, _incomingAudioBuffer, 256);
    }
    _incomingAudioBuffer[_incomingAudioBufferIndex] = _packetStreamBuffer[sample];
  }
  _audioReceiverQueue->playBuffer();
}


void AudioBoard::sendRTPData() {
  if (_OutputQueue.size() <= 0) return;

  int bufferIndex = _OutputQueue.front();
  _OutputQueue.pop();

  _outputTimestamp = millis();
  _outputBuffer[bufferIndex][0] = _rtpPayloadType;
  _outputBuffer[bufferIndex][1] = _sequenceNo;
  _outputBuffer[bufferIndex][2] = _outputTimestamp >> 16;
  _outputBuffer[bufferIndex][3] = 0;
  _outputBuffer[bufferIndex][4] = _ssrc >> 16;
  _outputBuffer[bufferIndex][5] = 0;

  uint8_t writeBuffer[100];

  // This uses big endian, which is wrong
  // memcpy(writeBuffer, outputBuffer[bufferIndex], packetSize);
  // So we swap the byte order manually
  int i = 0;
  for (i = 0; i < 50; i++) {
    writeBuffer[(i * 2) + 1] = (_outputBuffer[bufferIndex][i] >> 0) & 0xFF;
    writeBuffer[(i * 2)] = (_outputBuffer[bufferIndex][i] >> 8) & 0xFF;
  }

  // Serial.print("Seq: ");
  // Serial.print(sequenceNo);
  // Serial.print("    Act: ");
  // Serial.print((writeBuffer[2] << 8) | (writeBuffer[3] & 0xff));
  // Serial.print("    Index: ");
  // Serial.print(bufferIndex);
  // Serial.print("    Time: ");
  // Serial.println(_outputTimestamp - _outputLastTimestamp);

  _sequenceNo++;

  _udp->beginPacket(_multicastIP, _multicastPort);
  _udp->write(writeBuffer, _rtpPacketSize);
  _udp->endPacket();

//  outputBufferReaderIndex++;
//
//  if (outputBufferReaderIndex >= numOutputBuffers) outputBufferReaderIndex = 0;
}

void AudioBoard::readAudio() {
  if (_audioTransmitterQueue->available() >= 2) {
    int16_t *recorderBuffer = _audioTransmitterQueue->readBuffer();
//    timestamp = millis();
//    Serial.print("Buffer full - ");
//    Serial.print(recorder.available());
//    Serial.print(" - ");
//    Serial.print(sizeof(recorderBuffer));
//    Serial.print(" - time: ");
//    Serial.println(timestamp);

    int i = 0;
    int inByte = 0;
    for(i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
      _outputBuffer[_outputBufferWriterIndex][_bufferIndex + 6] = recorderBuffer[i];

      _bufferIndex++;
      if (_bufferIndex >= _numSamples) {
        _OutputQueue.push(_outputBufferWriterIndex);
        _bufferIndex = 0;
        _outputBufferWriterIndex++;
      }
      if (_outputBufferWriterIndex >= _numOutputBuffers) {
        _outputBufferWriterIndex = 0;
      }
    }

    _audioTransmitterQueue->freeBuffer();
  }
}
