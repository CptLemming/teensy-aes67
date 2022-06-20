/* WebSockets2_Generic2 lib is written in all hpp files, which causes issues with the Arduino IDE when linking.
 * To get around this we're forced to write this file as a .hpp too!
 */
#define WEBSOCKETS_USE_ETHERNET          true
#define USE_QN_ETHERNET                  true

#define LOGO_HEIGHT   32
#define LOGO_WIDTH    32

const unsigned char PROGMEM calrecLogo [] = {
  // calrecLogo, 32x32px
  0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x08, 0x00, 0x00, 0x7f, 0x07, 0x00, 0x01, 0xff, 0x01, 0x80,
  0x03, 0xff, 0x00, 0xc0, 0x07, 0xfe, 0x00, 0x60, 0x0f, 0xf0, 0x10, 0x30, 0x1f, 0xc0, 0x08, 0x38,
  0x3f, 0x80, 0x06, 0x1c, 0x3f, 0x00, 0x03, 0x0c, 0x3e, 0x00, 0x03, 0x0c, 0x7e, 0x00, 0x21, 0x86,
  0x7c, 0x00, 0x10, 0x86, 0x7c, 0x00, 0x08, 0xc6, 0x7c, 0x00, 0x08, 0x46, 0x7c, 0x00, 0x08, 0x42,
  0x7c, 0x00, 0x04, 0x42, 0x7c, 0x00, 0x04, 0x42, 0x7c, 0x00, 0x00, 0x02, 0x7c, 0x00, 0x00, 0x00,
  0x7e, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x00, 0x3f, 0x00, 0x00, 0xe0, 0x1f, 0x80, 0x01, 0xf8,
  0x1f, 0xc0, 0x07, 0xf0, 0x0f, 0xf0, 0x1f, 0xf0, 0x07, 0xff, 0xff, 0xe0, 0x03, 0xff, 0xff, 0xc0,
  0x01, 0xff, 0xff, 0x80, 0x00, 0x7f, 0xfe, 0x00, 0x00, 0x1f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00
};

#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>
#include <QNEthernet.h>
#include <StreamUtils.h>
#include <TeensyID.h>
#include <WebSockets2_Generic.h>
#include <vector>

#include "deviceModel.h"

class CCPWebsocket {
  public:
    void start()  {
      _socketServer->listen(_websocketServerPort);
      _httpServer->begin();
    }

    void update() {
      listenForSocketClients();
      pollSocketClients();
      pollSocketRemoteClients();
      remoteSocketsHeartbeat();
      listenForHttpClients();
    }

    void drawDisplay() {
      _display->clearDisplay();
      _display->drawBitmap(0, 0, calrecLogo, LOGO_WIDTH, LOGO_HEIGHT, SSD1306_WHITE);

      _display->setTextSize(1);
      _display->setTextColor(SSD1306_WHITE);
      _display->cp437(true);
      _display->setCursor(44, 0);
      char* hardwareName = _deviceModel->getDocument()["calrec"]["hardware"]["name"];
      _display->println(hardwareName);

      _display->setCursor(44, 8);
      char* nicAddress = _deviceModel->getDocument()["calrec"]["hardware"]["nics"]["5"]["address"];
      _display->println(nicAddress);

      _display->setCursor(44, 16);
      _display->print(F("Sockets: "));
      _display->println(numFreeSockets());

      _display->display();
    }

    // Trigger an update to remote device
    void updateGpiPin(int index) {
      Serial.printf("GPI pin changed %d\n", index);
      StaticJsonDocument<2000> outgoing;
      outgoing["seq"] = _sequenceNo++;
      outgoing["type"] = "p";
      JsonArray patch = outgoing.createNestedArray("patch");

      uint32_t colours[] = {
        _trellis->pixels.Color(255, 0, 0),
        _trellis->pixels.Color(0, 255, 0),
        _trellis->pixels.Color(0, 0, 255),
        _trellis->pixels.Color(255, 255, 0)
      };

      bool isGpiActive = _deviceModel->getDocument()["calrec"]["gpi"][String(index)]["state"];
      bool isGpiInverted = _deviceModel->getDocument()["calrec"]["gpi"][String(index)]["inverted"];
      if (isGpiInverted) isGpiActive = !isGpiActive;

      if (isGpiActive) {
        _trellis->pixels.setPixelColor(index, colours[index]);
      } else {
        _trellis->pixels.setPixelColor(index, 0);
      }
      _trellis->pixels.show();

      JsonObject pinUpdateDoc = patch.createNestedObject();
      pinUpdateDoc["op"] = "replace";
      pinUpdateDoc["path"] = "/calrec/gpi/" + String(index) + "/state";
      pinUpdateDoc["value"] = isGpiActive;

      Serial.println("Send GPI active state");
      serializeJson(outgoing, Serial);
      Serial.println();

      for (byte i = 0; i < _maxSocketClients; i++) {
        if (_socketClients[i].available()) {
          // TODO Can we serialize once and copy multiple times
          StringStream pinOutput;
          serializeJson(outgoing, pinOutput);
          send(_socketClients[i], pinOutput);
        }
      }
    }

    // Responds to remote updates
    void updateGpoPin(int index, websockets2_generic::WebsocketsClient &client) {
      Serial.printf("GPO pin changed %d\n", index);
      StaticJsonDocument<4000> outgoing;
      JsonObject outgoingResponse = outgoing.createNestedObject("response");
      outgoingResponse["code"] = 200;

      uint32_t buttonColour = _trellis->pixels.Color(255, 0, 0);

      bool isGpoActive = _deviceModel->getDocument()["calrec"]["gpo"][String(index)]["state"];
      bool isGpoInverted = _deviceModel->getDocument()["calrec"]["gpo"][String(index)]["inverted"];
      if (isGpoInverted) isGpoActive = !isGpoActive;

      if (isGpoActive) {
        _trellis->pixels.setPixelColor(index, buttonColour);
      } else {
        _trellis->pixels.setPixelColor(index, 0);
      }
      _trellis->pixels.show();

      JsonArray outgoingData = outgoing.createNestedArray("data");
      JsonObject pinUpdateDoc = outgoingData.createNestedObject();
      pinUpdateDoc["op"] = "replace";
      pinUpdateDoc["path"] = "/calrec/gpo/" + String(index) + "/state";
      pinUpdateDoc["value"] = isGpoActive;

      StringStream pinOutput;
      serializeJson(outgoing, pinOutput);

      Serial.println("Send GPO active state");
      serializeJson(outgoing, Serial);
      Serial.println();

      send(client, pinOutput);
    }

    void subscribeToRemoteGpo(int index, const char* hostname, const char* path) {
      Serial.printf("GPO start subscription %s %s\n", hostname, path);
      StaticJsonDocument<200> outgoing;
      JsonObject patchDoc = outgoing.createNestedObject();
      patchDoc["op"] = "add";
      patchDoc["path"] = "/calrec/gpiogroups/c1a60653-2d63-4403-8c9b-82af38c789e2/gpos/0/ioport/destinations/-";
      JsonObject patchDocValue = patchDoc.createNestedObject("value");
      patchDocValue["hostname"] = "FB464176-0000-0000-B6C9-24E1ABA93A3F-0-pri.local";
      patchDocValue["protocol"] = "calrec_hca";
      patchDocValue["url"] = "/calrec/gpo/" + String(index);
      StaticJsonDocument<200> outgoing2;
      JsonObject subDoc = outgoing2.createNestedObject();
      subDoc["op"] = "subscribe";
      subDoc["path"] = "/calrec/gpiogroups/c1a60653-2d63-4403-8c9b-82af38c789e2/gpos/0";

      StringStream sub;
      serializeJson(outgoing, sub);
      StringStream sub2;
      serializeJson(outgoing2, sub2);

      Serial.println("Make remote subscription");
      serializeJson(outgoing, Serial);
      Serial.println();

      // TODO Check for an existing ws to the same host and re-use
      int freeIndex = getFreeRemoteSocketClientIndex();
      websockets2_generic::WebsocketsClient remoteSocketClient;
      _remoteSocketClients[freeIndex] = remoteSocketClient;

      String buildUrl = "ws://";
      buildUrl += hostname;
      buildUrl += ":50002";
      // const char* url = buildUrl.c_str();
      const char* url  = "ws://UR6500-70B3D5042D33-1-pri.local:50002";
      String localHostname = "FB464176-0000-0000-B6C9-24E1ABA93A3F-0-pri";
      websockets2_generic::WSString base64Authorization = websockets2_generic::crypto2_generic::base64Encode((uint8_t *)localHostname.c_str(), localHostname.length());
      remoteSocketClient.addHeader("Authorization", base64Authorization.c_str());
      if (remoteSocketClient.connect(url)) {
        Serial.printf("Connected to remote server %s\n", url);

        delay(1000);
        send(remoteSocketClient, sub);
      } else {
        Serial.printf("Couldn't connect to remote server %s!\n", url);
      }

      remoteSocketClient.onMessage([&](websockets2_generic::WebsocketsMessage message) {
        Serial.print("Got remote WS Message: ");
        Serial.println(message.data());
      });
    }

    void processPinChange() {
      Serial.println("Pins changed");

      StaticJsonDocument<2000> outgoing;
      JsonObject outgoingResponse = outgoing.createNestedObject("response");
      outgoingResponse["code"] = 200;
      JsonArray outgoingData = outgoing.createNestedArray("data");

      uint32_t colours[] = {
        _trellis->pixels.Color(255, 0, 0),
        _trellis->pixels.Color(0, 255, 0),
        _trellis->pixels.Color(0, 0, 255),
        _trellis->pixels.Color(255, 255, 0)
      };

      int numGpi = 4;
      for (int i = 0; i < numGpi; i++) {
        bool isGpiActive = _deviceModel->getDocument()["calrec"]["gpi"][String(i)]["state"];
        bool isGpiInverted = _deviceModel->getDocument()["calrec"]["gpi"][String(i)]["inverted"];
        if (isGpiInverted) isGpiActive = !isGpiActive;

        Serial.print("Set GPI "); Serial.println(i + 1); Serial.println(isGpiActive);
        if (isGpiActive) {
          _trellis->pixels.setPixelColor(i, colours[i]);
        } else {
          _trellis->pixels.setPixelColor(i, 0);
        }

        JsonObject pinUpdateDoc = outgoingData.createNestedObject();
        pinUpdateDoc["op"] = "replace";
        pinUpdateDoc["path"] = "/calrec/gpi/" + String(i) + "/inverted";
        pinUpdateDoc["value"] = isGpiInverted;
      }

      _trellis->pixels.show();

      StringStream pinOutput;
      serializeJson(outgoing, pinOutput);

      Serial.println("Send reply");
      serializeJson(outgoing, Serial);

      for (byte i = 0; i < _maxSocketClients; i++)
      {
        if (_socketClients[i].available()) {
          send(_socketClients[i], pinOutput);
        }
      }
    }

    CCPWebsocket(websockets2_generic::WebsocketsServer &socketServer, DeviceModel &deviceModel, Adafruit_NeoTrellis &trellis, Adafruit_SSD1306 &display) {
      _socketServer = &socketServer;
      _deviceModel = &deviceModel;
      _trellis = &trellis;
      _display = &display;
      _httpServer = new EthernetServer(_httpServerPort);
    }
  protected:
    void listenForSocketClients() {
      if (_socketServer->poll())
      {
        int8_t freeIndex = getFreeSocketClientIndex();

        if (freeIndex >= 0)
        {
          websockets2_generic::WebsocketsClient newClient = _socketServer->accept();

          Serial.printf("Accepted new websockets client at index %d\n", freeIndex);
          newClient.onMessage([&](websockets2_generic::WebsocketsClient &client, websockets2_generic::WebsocketsMessage message) {
            return handleMessage(client, message);
          });
          newClient.onEvent([&](websockets2_generic::WebsocketsClient &client, websockets2_generic::WebsocketsEvent event, String data) {
            return handleEvent(client, event, data);
          });
          _socketClients[freeIndex] = newClient;
          drawDisplay();
        }
      }
    }

    uint8_t numFreeSockets() {
      uint8_t result = 0;

      for (byte i = 0; i < _maxSocketClients; i++) {
        if (_socketClients[i].available()) result++;
      }

      return result;
    }

    void pollSocketClients() {
      for (byte i = 0; i < _maxSocketClients; i++) {
        _socketClients[i].poll();
      }
    }

    void pollSocketRemoteClients() {
      for (byte i = 0; i < _maxRemoteSocketClients; i++) {
        _remoteSocketClients[i].poll();
      }
    }

    void remoteSocketsHeartbeat() {
      if (websocketHeartbeatTimer > 1000) {
        websocketHeartbeatTimer = 0;

        for (byte i = 0; i < _maxRemoteSocketClients; i++) {
          if (_remoteSocketClients[i].available()) {
            _remoteSocketClients[i].send("{\"hb\":\"hb\"}");
          }
        }
      }
    }

    void listenForHttpClients() {
      // Listen for incoming http clients.
      EthernetClient client = _httpServer->available();

      if (client)
      {
        Serial.println("Http client connected!");

        // An http request ends with a blank line.
        bool currentLineIsBlank = true;

        while (client.connected())
        {
          if (client.available())
          {
            char c = client.read();

            if (c == '\n' && currentLineIsBlank)
            {
              // If we've gotten to the end of the line (received a newline
              // character) and the line is blank, the http request has ended,
              // so we can send a reply.
              sendHttpReply(client);
              break;
            }
            else if (c == '\n')
            {
              // Starting a new line.
              currentLineIsBlank = true;
            }
            else if (c != '\r')
            {
              // Read a character on the current line.
              currentLineIsBlank = false;
            }
          }
        }

        // The NativeEthernet's WebServer example adds a small delay here. For me it
        // seems to work without the delay. Uncomment to following line if you have
        // issues connecting to the website in the browser.
        delay(1);

        // Close the connection.
        client.stop();
      }
    }
  private:
    static const byte _maxSocketClients = 4;
    static const byte _maxRemoteSocketClients = 4;
    static const uint16_t _websocketServerPort = 50002;
    static const uint16_t _httpServerPort = 8080;
    uint16_t _sequenceNo = 0;
    DeviceModel *_deviceModel;
    Adafruit_NeoTrellis *_trellis;
    Adafruit_SSD1306 *_display;
    elapsedMillis websocketHeartbeatTimer;
    websockets2_generic::WebsocketsClient _socketClients[_maxSocketClients];
    websockets2_generic::WebsocketsServer *_socketServer;
    websockets2_generic::WebsocketsClient _remoteSocketClients[_maxRemoteSocketClients];
    EthernetServer *_httpServer;

    int8_t getFreeSocketClientIndex() {
      // If a client in our list is not available, it's connection is closed and we
      // can use it for a new client.
      for (byte i = 0; i < _maxSocketClients; i++)
      {
        if (!_socketClients[i].available())
          return i;
      }

      return -1;
    }

    int8_t getFreeRemoteSocketClientIndex() {
      for (byte i = 0; i < _maxRemoteSocketClients; i++) {
        if (!_remoteSocketClients[i].available())
          return i;
      }

      return -1;
    }

    void handleMessage(websockets2_generic::WebsocketsClient &client, websockets2_generic::WebsocketsMessage message) {
      websockets2_generic::WSInterfaceString data = message.data();
      data.remove(data.length() - 6);

      // Log message
      StaticJsonDocument<4000> incoming;
      deserializeJson(incoming, data);

      if (incoming.containsKey("hb")) {
        // Ping Pong!
        send(client, data);
      } else {
        Serial.print("Got Message: ");
        Serial.println(data);
        // Should be a JSON array
        Serial.print("Arr size: ");
        Serial.println(incoming["patch"].size());
        if (incoming["patch"].size() < 1) {
          StaticJsonDocument<200> outgoing;
          outgoing.to<JsonArray>();
          Serial.println("Send blank array");
          String blankOutput;
          serializeJson(outgoing, blankOutput);
          send(client, blankOutput);
        } else {
          JsonObject message = incoming["patch"][0];
          const char* operation = message["op"];
          const char* path = message["path"];
          Serial.print("Operation ");
          Serial.println(operation);

          // TODO
          // Got Message: [{"op":"replace","path":"/calrec/hardware/locked","value":true}]
          // Got Message: [{"op":"replace","path":"/calrec/gpo/0/inverted","value":true}]
          // Got Message: [{"op":"replace","path":"/calrec/input/0/phpwr","value":true}]
          // Got Message: [{"op":"replace","path":"/calrec/input/0/gain","value":20}]
          // Got Message: [{"op":"replace","path":"/calrec/senders/e8afe401-baa7-4acf-a4b6-61d5225565c2/username","value":"Tx"}]
          // Got Message: [{"op":"add","path":"/calrec/receivers/30398680-7cd4-40c3-b94a-6c649153ff39","value":{"enabled":true,"linkoffset":2000,"locked":false,"nics":[5,6],"ports":[[],[]],"sender":null,"username":"New Receiver"}}]
          // Core Remote GPO -> Local GPO
          // Got Message: [{"op":"replace","path":"/calrec/gpo/0/ioport/source","value":{"hostname":"UR6500-70B3D5042D33-0-pri.local","protocol":"calrec_hca","url":"/calrec/gpiogroups/c1a60653-2d63-4403-8c9b-82af38c789e2/gpos/0"}}]
          // Got Message: [{"op":"add","path":"/calrec/gpi/0/ioport/destinations/-","value":{"hostname":"UR6500-70B3D5042D33-0-pri.local","protocol":"calrec_hca","url":"/calrec/gpiogroups/c1a60653-2d63-4403-8c9b-82af38c789e2/gpis/0"}}]
          // Got Message: [{"op":"remove","path":"/calrec/gpi/0/ioport/destinations","value":{"hostname":"UR6500-70B3D5042D33-1-pri.local","protocol":"calrec_hca","url":"/calrec/gpiogroups/c1a60653-2d63-4403-8c9b-82af38c789e2/gpis/0"}}]

          if (strcmp(operation, "subscribe") == 0) {
            Serial.printf("Recv subscription :: %s\n", path);

            // Doc is too big to be sent in one go, needs to be chunked / streamed
            StaticJsonDocument<4000> outgoing;
            outgoing["seq"] = incoming["seq"];
            JsonObject outgoingResponse = outgoing.createNestedObject("response");
            outgoingResponse["code"] = 200;

            if (strcmp(path, "/calrec/hardware/hid") == 0) {
              outgoing["data"] = _deviceModel->getDocument()["calrec"]["hardware"]["hid"];
            } else if (strcmp(path, "/calrec/hardware/name") == 0) {
              outgoing["data"] = _deviceModel->getDocument()["calrec"]["hardware"]["name"];
            } else if (strcmp(path, "/calrec/hardware/model") == 0) {
              outgoing["data"] = _deviceModel->getDocument()["calrec"]["hardware"]["model"];
            } else if (strcmp(path, "/calrec/gpi/0") == 0) {
              JsonObject outgoingData = outgoing.createNestedObject("data");
              outgoingData.set(_deviceModel->getDocument()["calrec"]["gpi"]["0"]);
            } else if (strcmp(path, "/calrec/gpo/0") == 0) {
              JsonObject outgoingData = outgoing.createNestedObject("data");
              outgoingData.set(_deviceModel->getDocument()["calrec"]["gpo"]["0"]);
            } else {
              // Assume full model
              JsonObject outgoingData = outgoing.createNestedObject("data");
              outgoingData.set(_deviceModel->getDocument().as<JsonObject>());
            }

            StringStream stream;
            serializeJson(outgoing, stream);
            send(client, stream);

            Serial.println("Sent model data >> ");
            serializeJson(outgoing, Serial);
            Serial.println();
          } else if (strcmp(operation, "unsubscribe") == 0) {
            Serial.printf("unsubscription :: %s\n", path);
            StaticJsonDocument<200> outgoing;
            outgoing["seq"] = incoming["seq"];
            JsonObject outgoingResponse = outgoing.createNestedObject("response");
            outgoingResponse["code"] = 200;
            outgoing["data"] = nullptr;

            StringStream stream;
            serializeJson(outgoing, stream);
            send(client, stream);
          } else if (strcmp(operation, "replace") == 0 || strcmp(operation, "remove") == 0) {
            if (strcmp(path, "/calrec/gpi/0/inverted") == 0) {
              Serial.println("Update GPI 1 invert");
              bool value = message["value"];
              _deviceModel->updateGpiInvert(0, value);
              processPinChange();
            } else if (strcmp(path, "/calrec/gpi/1/inverted") == 0) {
              Serial.println("Update GPI 2 invert");
              bool value = message["value"];
              _deviceModel->updateGpiInvert(1, value);
              processPinChange();
            } else if (strcmp(path, "/calrec/gpi/2/inverted") == 0) {
              Serial.println("Update GPI 3 invert");
              bool value = message["value"];
              _deviceModel->updateGpiInvert(2, value);
              processPinChange();
            } else if (strcmp(path, "/calrec/gpi/3/inverted") == 0) {
              Serial.println("Update GPI 4 invert");
              bool value = message["value"];
              _deviceModel->updateGpiInvert(3, value);
              processPinChange();
            } else if (strcmp(path, "/calrec/gpi/4/inverted") == 0) {
              Serial.println("Update GPI 5 invert");
              bool value = message["value"];
              _deviceModel->updateGpiInvert(4, value);
              processPinChange();
            } else if (strcmp(path, "/calrec/gpi/5/inverted") == 0) {
              Serial.println("Update GPI 6 invert");
              bool value = message["value"];
              _deviceModel->updateGpiInvert(1, value);
              processPinChange();
            } else if (strcmp(path, "/calrec/gpi/6/inverted") == 0) {
              Serial.println("Update GPI 7 invert");
              bool value = message["value"];
              _deviceModel->updateGpiInvert(6, value);
              processPinChange();
            } else if (strcmp(path, "/calrec/gpi/7/inverted") == 0) {
              Serial.println("Update GPI 8 invert");
              bool value = message["value"];
              _deviceModel->updateGpiInvert(7, value);
              processPinChange();
            } else if (strcmp(path, "/calrec/gpo/0/state") == 0) {
              Serial.println("Update GPO 1 state");
              bool value = message["value"];
              _deviceModel->updateGpoState(0, value);
              updateGpoPin(0, client);
            } else if (strcmp(path, "/calrec/input/0/gain") == 0) {
              Serial.println("Update Input 0 gain");
              long value = message["value"];
              _deviceModel->getDocument()["input"][0]["gain"] = value;
              // processEncoderChange();
            } else {
              if (strcmp(path, "/calrec/hardware/name") == 0) {
                Serial.println("Update Name");
                char* value = message["value"];
                _deviceModel->updateHardwareName(value);
                drawDisplay();
              } else if (strcmp(path, "/calrec/input/0/phpwr") == 0) {
                Serial.println("Update Input 0 phpwr");
                bool value = message["value"];
                _deviceModel->getDocument()["input"][0]["phpwr"] = value;
              } else if (strcmp(path, "/calrec/gpo/0/ioport/source") == 0) {
                const char* gpoHostname = message["value"]["hostname"];
                const char* gpoPath = message["value"]["url"];
                if (gpoHostname != nullptr) {
                  subscribeToRemoteGpo(0, gpoHostname, gpoPath);
                }
              }
              StaticJsonDocument<4000> outgoing;
              JsonObject outgoingResponse = outgoing.createNestedObject("response");
              outgoingResponse["code"] = 200;
              JsonArray outgoingData = outgoing.createNestedArray("data");
              outgoingData.add(message);
              StringStream stream;
              serializeJson(outgoing, stream);
              send(client, stream);

              Serial.println("Echo back patch data");
            }
          } else if (strcmp(operation, "add") == 0) {
            const char* calrecSendersPath = "/calrec/senders";
            if (strcmp(path, "/calrec/gpi/0/ioport/destinations/-") == 0) {
              Serial.println("Patch GPI");
              int destinationIndex = _deviceModel->getDocument()["calrec"]["gpi"]["0"]["ioport"]["destinations"].size() + 1;
              JsonObject destination = _deviceModel->getDocument()["calrec"]["gpi"]["0"]["ioport"]["destinations"].createNestedObject(destinationIndex);
              destination.set(message["value"]);

              StaticJsonDocument<4000> outgoing;
              JsonObject outgoingResponse = outgoing.createNestedObject("response");
              outgoingResponse["code"] = 200;
              JsonObject outgoingData = outgoing["data"].createNestedObject();
              outgoingData["op"] = operation;
              outgoingData["path"] = "/calrec/gpi/0/ioport/destinations/-";// + String(destinationIndex);
              outgoingData["value"] = destination;

              StringStream stream;
              serializeJson(outgoing, stream);
              send(client, stream);

              Serial.println("Echo back destination data");
            } else if (strncmp(path, calrecSendersPath, strlen(calrecSendersPath)) == 0) {
              Serial.println("Add sender");
              int senderIndex = _deviceModel->getDocument()["senders"].size() + 1;
              JsonObject sender = _deviceModel->getDocument()["senders"].createNestedObject(senderIndex);
              sender.set(message["value"]);
              sender["status"] = true;
              sender["sessionid"] = teensyUUID();
              JsonArray senderSdps = sender.createNestedArray("sdps");
              senderSdps.add("");
              senderSdps.add("");

              StaticJsonDocument<4000> outgoing;
              JsonObject outgoingResponse = outgoing.createNestedObject("response");
              outgoingResponse["code"] = 200;
              JsonObject outgoingData = outgoing["data"].createNestedObject();
              outgoingData["op"] = operation;
              outgoingData["path"] = path;
              outgoingData["value"] = sender;

              StringStream stream;
              serializeJson(outgoing, stream);
              send(client, stream);

              Serial.println("Echo back sender data");
            }
          }
        }
      }
    }

    void handleEvent(websockets2_generic::WebsocketsClient &client, websockets2_generic::WebsocketsEvent event, String data) {
      if (event == websockets2_generic::WebsocketsEvent::ConnectionClosed)
      {
        Serial.println("Connection closed");
        drawDisplay();
      }
    }

    void sendHttpReply(EthernetClient &client) {
      Serial.print("Data size:");
      Serial.println(measureJsonPretty(_deviceModel->getDocument()));

      Serial.println("Prepare headers");

      client.println(F("HTTP/1.0 200 OK"));
      client.println(F("Content-Type: application/json"));
    //  client.println(F("Connection: close"));
    //  client.print(F("Content-Length: "));
    //  client.println(measureJsonPretty(doc));
      client.println();

      Serial.println("send doc");

      // Write to buffer for better performance
      WriteBufferingStream bufferedWifiClient(client, 64);
      serializeJson(_deviceModel->getDocument(), bufferedWifiClient);
      bufferedWifiClient.flush();

      Serial.println("request complete");
    }

    void send(websockets2_generic::WebsocketsClient &client, const websockets2_generic::WSInterfaceString &data) {
      client.send(data + "^&*%£");
    }

    void send(websockets2_generic::WebsocketsClient &client, StringStream &stream) {
      StringStream delim = StringStream{"^&*%£"};

      client.stream("");

      while (stream.available() > 0) {
        client.send((char)stream.read());
      }

      while (delim.available() > 0) {
        client.send((char)delim.read());
      }

      client.end("");
    }
};
