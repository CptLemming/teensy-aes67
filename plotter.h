#ifndef plotter_h_
#define plotter_h_

#include "AudioStream.h"

//only used for debugging in combination with the Arduino IDE serial plotter
class Plotter : public AudioStream
{
  public:
    Plotter(uint8_t step) : AudioStream(8, inputQueueArray) {
      _step = step;
    };
    void activate(bool on);
    virtual void update(void);

  private:
    audio_block_t *inputQueueArray[8];
    bool _on = false;
    uint8_t _step = 1;
};

#endif
