#include <arduino.h>
#include "esp32_bt_music_receiver.h"

#define CONFIG_EXAMPLE_I2S_BCK_PIN 26 // (A0 on huzzah32, BCLK2 pin 4 on T4.1)
#define CONFIG_EXAMPLE_I2S_LRCK_PIN 25 // (A1 on huzzah32, LRCK2 pin 3 on T4.1)
#define CONFIG_EXAMPLE_I2S_DATA_PIN 21 // (21 on huzzah32, IN2 pin 5 on T4.1)
// T4.1 GND is connected to huzzah32 GND (down 4 on left side)

BlootoothA2DSink a2d_sink;

void setup() {

  static const i2s_config_t i2s_config = {
    .mode = (i2s_mode_t) (I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 44100,
    .bits_per_sample = (i2s_bits_per_sample_t)16,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = (i2s_comm_format_t) (I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
    .intr_alloc_flags = 0, // default interrupt priority
    .dma_buf_count = 8,
    .dma_buf_len = 128,//64,
    .use_apll = true,
    .tx_desc_auto_clear = true                                              //Auto clear tx descriptor on underflow
  };
    i2s_pin_config_t pin_config = {
      .bck_io_num = CONFIG_EXAMPLE_I2S_BCK_PIN,
      .ws_io_num = CONFIG_EXAMPLE_I2S_LRCK_PIN,
      .data_out_num = CONFIG_EXAMPLE_I2S_DATA_PIN,
      .data_in_num = -1                                                       //Not used
  };


  a2d_sink.set_i2s_config(i2s_config);
  a2d_sink.set_pin_config(pin_config);
  a2d_sink.start("TeensyAudio");
  Serial.begin(115200);
}

unsigned long last = 0;

void loop() {
  if ((millis() - last) > 100) {
    last = millis();
    Serial.println(a2d_sink.get_audio_state());
    //Serial.println(a2d_sink.get_audio_type());
  }
}
