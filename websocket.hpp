/* WebSockets2_Generic2 lib is written in all hpp files, which causes issues with the Arduino IDE when linking.
 * To get around this we're forced to write this file as a .hpp too!
 */
#define WEBSOCKETS_USE_ETHERNET          true
#define USE_QN_ETHERNET                  true

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
      MDNS.setServiceName("UR6500-14E1ABA93A3F-0-pri-5");
      MDNS.addService("_http", "_tcp", 8080);
      MDNS.addService("_calrec-node", "_tcp", _websocketServerPort, []() {
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
      listenForHttpClients();
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

    CCPWebsocket(websockets2_generic::WebsocketsServer &socketServer, DeviceModel &deviceModel, Adafruit_NeoTrellis &trellis) {
      _socketServer = &socketServer;
      _deviceModel = &deviceModel;
      _trellis = &trellis;
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
        }
      }
    }
    void pollSocketClients() {
      for (byte i = 0; i < _maxSocketClients; i++)
      {
        _socketClients[i].poll();
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
    websockets2_generic::WebsocketsClient _socketClients[_maxSocketClients];
    websockets2_generic::WebsocketsServer *_socketServer;
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

          if (strcmp(operation, "subscribe") == 0) {
            Serial.println("Recv subscription");

            // Doc is too big to be sent in one go, needs to be chunked / streamed
            StaticJsonDocument<4000> outgoing;
            JsonObject outgoingResponse = outgoing.createNestedObject("response");
            outgoingResponse["code"] = 200;
            JsonObject outgoingData = outgoing.createNestedObject("data");
            outgoingData.set(_deviceModel->getDocument().as<JsonObject>());
            StringStream stream;
            serializeJson(outgoing, stream);

            client.stream("");

            while (stream.available() > 0) {
              client.send((char)stream.read());
            }

            client.end("");

            Serial.println("Sent model data");
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
            } else if (strcmp(path, "/calrec/input/0/gain") == 0) {
              Serial.println("Update Input 0 gain");
              long value = message["value"];
              _deviceModel->getDocument()["input"][0]["gain"] = value;
              // processEncoderChange();
            } else {
              if (strcmp(path, "/calrec/input/0/phpwr") == 0) {
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
            if (strncmp(path, calrecSendersPath, strlen(calrecSendersPath)) == 0) {
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
