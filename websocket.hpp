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

#include "deviceModel.h"

class CCPWebsocket {
  public:
    void start()  {
      _socketServer->listen(_websocketServerPort);
      _httpServer->begin();

      // Advertise as a Calrec device
      MDNS.begin("FB464176-0000-0000-B6C9-24E1ABA93A3F-0-pri");
      MDNS.addService("UR6500-14E1ABA93A3F-0-pri-5", "_http", "_tcp", 8080);
      MDNS.addService("UR6500-14E1ABA93A3F-0-pri-5", "_calrec-node", "_tcp", _websocketServerPort, []() {
        return std::vector<String>{
          "nic=2",
          "address=192.168.30.210",
          "version=0.1",
          "corestatus=ACTIVE"
        };
      });
    }

    void update() {
      listenForSocketClients();
      pollSocketClients();
      pollSocketRemoteClients();
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
      _display->println(F("192.168.30.210"));

      _display->setCursor(44, 16);
      _display->print(F("Sockets: "));
      _display->println(numFreeSockets());

      _display->display();
    }

    // Must trigger an update to remote device
    void updateGpiPin(int index) {
      Serial.printf("GPI pin changed %d\n", index);
      StaticJsonDocument<2000> outgoing;
      // JsonArray outgoingData = outgoing.createNestedArray("data");

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

      JsonObject pinUpdateDoc = outgoing.createNestedObject();
      pinUpdateDoc["op"] = "replace";
      // FIXME Cheat here, hard code a specific remote GPI
      pinUpdateDoc["path"] = "/calrec/gpiogroups/c1a60653-2d63-4403-8c9b-82af38c789e2/gpis/" + String(index) + "/state";
      pinUpdateDoc["value"] = _deviceModel->getDocument()["calrec"]["gpi"][String(index)]["state"];

      StringStream pinOutput;
      serializeJson(outgoing, pinOutput);

      Serial.println("Send GPI active state");
      serializeJson(outgoing, Serial);
      Serial.println();

      if (_remoteSocketClient->available()) {
        _remoteSocketClient->stream("");
        while (pinOutput.available() > 0) {
          _remoteSocketClient->send((char)pinOutput.read());
        }
        _remoteSocketClient->end("");
      }
    }

    // Responds to remote updates
    void updateGpoPin(int index) {
      Serial.printf("GPO pin changed %d\n", index);
      StaticJsonDocument<2000> outgoing;
      // JsonArray outgoingData = outgoing.createNestedArray("data");

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

      JsonObject pinUpdateDoc = outgoing.createNestedObject();
      pinUpdateDoc["op"] = "replace";
      pinUpdateDoc["path"] = "/calrec/gpo/" + String(index) + "/state";
      pinUpdateDoc["value"] = _deviceModel->getDocument()["calrec"]["gpo"][String(index)]["state"];

      StringStream pinOutput;
      serializeJson(outgoing, pinOutput);

      Serial.println("Send active state");
      serializeJson(outgoing, Serial);
      Serial.println();

      for (byte i = 0; i < _maxSocketClients; i++)
      {
        if (_socketClients[i].available()) {

          _socketClients[i].stream("");
          while (pinOutput.available() > 0) {
            _socketClients[i].send((char)pinOutput.read());
          }
          _socketClients[i].end("");
        }
      }
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
          // _socketClients[i].send(pinOutput);

          _socketClients[i].stream("");
          while (pinOutput.available() > 0) {
            _socketClients[i].send((char)pinOutput.read());
          }
          _socketClients[i].end("");
        }
      }
    }

    CCPWebsocket(websockets2_generic::WebsocketsServer &socketServer, websockets2_generic::WebsocketsClient &socketClient, DeviceModel &deviceModel, Adafruit_NeoTrellis &trellis, Adafruit_SSD1306 &display) {
      _remoteSocketClient = &socketClient;
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

      for (byte i = 0; i < _maxSocketClients; i++)
      {
        if (_socketClients[i].available()) result++;
      }

      return result;
    }

    void pollSocketClients() {
      for (byte i = 0; i < _maxSocketClients; i++)
      {
        _socketClients[i].poll();
      }
    }

    void pollSocketRemoteClients() {
      if (_remoteSocketClient->available()) {
        _remoteSocketClient->poll();
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
    static const uint16_t _websocketServerPort = 50002;
    static const uint16_t _httpServerPort = 8080;
    DeviceModel *_deviceModel;
    Adafruit_NeoTrellis *_trellis;
    Adafruit_SSD1306 *_display;
    websockets2_generic::WebsocketsClient _socketClients[_maxSocketClients];
    websockets2_generic::WebsocketsServer *_socketServer;
    websockets2_generic::WebsocketsClient *_remoteSocketClient;
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

    void handleMessage(websockets2_generic::WebsocketsClient &client, websockets2_generic::WebsocketsMessage message) {
      auto data = message.data();

      // Log message
      StaticJsonDocument<4000> incoming;
      deserializeJson(incoming, data);

      if (incoming.containsKey("hb")) {
        // Ping Pong!
        client.send(data);
      } else {
        Serial.print("Got Message: ");
        Serial.println(data);
        // Should be a JSON array
        Serial.print("Arr size: ");
        Serial.println(incoming.size());
        if (incoming.size() < 1) {
          StaticJsonDocument<200> outgoing;
          outgoing.to<JsonArray>();
          Serial.println("Send blank array");
          String blankOutput;
          serializeJson(outgoing, blankOutput);
          client.send(blankOutput);
        } else {
          JsonObject message = incoming[0];
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

          if (strcmp(operation, "subscribe") == 0) {
            Serial.printf("Recv subscription :: %s\n", path);

            // Doc is too big to be sent in one go, needs to be chunked / streamed
            StaticJsonDocument<4000> outgoing;
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

            client.stream("");

            while (stream.available() > 0) {
              client.send((char)stream.read());
            }

            client.end("");

            Serial.println("Sent model data");
          } else if (strcmp(operation, "unsubscribe") == 0) {
            Serial.printf("unsubscription :: %s\n", path);
            StaticJsonDocument<200> outgoing;
            JsonObject outgoingResponse = outgoing.createNestedObject("response");
            outgoingResponse["code"] = 200;
            outgoing["data"] = nullptr;

            StringStream stream;
            serializeJson(outgoing, stream);

            client.stream("");

            while (stream.available() > 0) {
              client.send((char)stream.read());
            }

            client.end("");
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
              updateGpoPin(0);
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
              }
              StaticJsonDocument<4000> outgoing;
              JsonObject outgoingResponse = outgoing.createNestedObject("response");
              outgoingResponse["code"] = 200;
              JsonArray outgoingData = outgoing.createNestedArray("data");
              outgoingData.add(message);
              StringStream stream;
              serializeJson(outgoing, stream);

              client.stream("");

              while (stream.available() > 0) {
                client.send((char)stream.read());
              }

              client.end("");

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

              client.stream("");

              while (stream.available() > 0) {
                client.send((char)stream.read());
              }

              client.end("");

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

              client.stream("");

              while (stream.available() > 0) {
                client.send((char)stream.read());
              }

              client.end("");

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
};
