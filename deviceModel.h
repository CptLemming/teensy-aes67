#ifndef teensy_aes67_device_model_h_
#define teensy_aes67_device_model_h_

#include <ArduinoJson.h>

class DeviceModel {
  public:
    DeviceModel();
    StaticJsonDocument<4000> getDocument();
    void loadConfig();
    void saveConfig();
    void createDefaultConfig();
    void updateGpiInvert(int index, bool value);
    void updateGpiState(int index, bool value);
    void updateHardwareName(char* value);
  private:
    StaticJsonDocument<4000> _doc;
    const char *_filename = "/config.json";
};

#endif
