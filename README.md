# Teensy AES67

Features:

- Send & receive of audio over network via AES67 compatible UDP streams
- Receive audio via USB input
- Receive bluetooth audio via ESP32 A2DP sink via I2S2 connection
- Play audio locally with Teensy audio board
- GPIO network devices, controlled & displayed with Adafruit Neotrellis
- Calrec (CCP) control of device over the network

Todo:

- Audio streams are hard coded, should be dynamic
- Send Bluetooth AVRC data to Teensy to use in labels
- Send device name from Teensy to ESP32 to customise broadcast name
- Working PTPv2
- NMOS control

Bill of materials:

- [Teensy 4.1](https://www.pjrc.com/store/teensy41.html)
- [Teensy audio shield](https://www.pjrc.com/store/teensy3_audio.html)
- [Teensy ethernet kit](https://www.pjrc.com/store/ethernet_kit.html)
- [Adafruit Neotrellis](https://www.adafruit.com/product/4352)
- [ESP32 Pico D4](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/hw-reference/esp32/get-started-pico-kit.html)
- [POE splitter](https://www.digikey.co.uk/en/products/detail/adafruit-industries-llc/4552/13561753)

Resources and projects used to get the project this far:

- https://github.com/JayShoe/esp32_T4_bt_music_receiver
- https://github.com/philhartung/aes67-sender
- https://github.com/bondagit/aes67-linux-daemon
- https://hartung.io/
- https://www.researchgate.net/figure/Header-fields-for-RTP-UDP-IP-packets-Version-4-with-the-appropriate-dynamics_fig2_226190399
- https://github.com/ssilverman/QNEthernet
