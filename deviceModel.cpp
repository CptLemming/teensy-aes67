#include <ArduinoJson.h>
#include <SD.h>
#include "deviceModel.h"

DeviceModel::DeviceModel() {
}

void DeviceModel::loadConfig() {
  File file = SD.open(_filename);

  DeserializationError error = deserializeJson(_doc, file);
  if (error) {
    Serial.println("Failed to read file, using default configuration");
  }
  file.close();
}

void DeviceModel::saveConfig() {
  SD.remove(_filename);

  File file = SD.open(_filename, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to create file");
    return;
  }

  if (serializeJson(_doc, file) == 0) {
    Serial.println("Failed to write to file");
  }

  file.close();
}

void DeviceModel::createDefaultConfig() {
  _doc.clear();
  JsonObject calrec = _doc.createNestedObject("calrec");

  JsonObject calrec_hardware = calrec.createNestedObject("hardware");
  calrec_hardware["manufacturer"] = "PJRC";
  calrec_hardware["model"] = "Teensy";
  calrec_hardware["modelversion"] = "4.1";
  calrec_hardware["hid"] = "FB464176-0000-0000-B6C9-24E1ABA93A3F";
  calrec_hardware["slot"] = 0;
  calrec_hardware["slotname"] = "Teensy";
  calrec_hardware["primary"] = true;
  calrec_hardware["name"] = "Prototype";
  calrec_hardware["description"] = "Microcontroller";
  calrec_hardware["locked"] = false;
  calrec_hardware["reboot"] = false;
  calrec_hardware["deviceidmode"] = false;
  calrec_hardware["hastwinassigned"] = false;
  calrec_hardware["samplerate"] = 44100;
  calrec_hardware["sampleratetracking"] = nullptr;
  calrec_hardware["packettime"] = 1000;
  calrec_hardware["operatinglevel"] = 150;
  calrec_hardware["micheadroom"] = 200;
  calrec_hardware["micimpedanceswitch"] = 10;

  JsonObject calrec_hardware_corestatus = calrec_hardware.createNestedObject("corestatus");
  calrec_hardware_corestatus["local"] = 3;
  calrec_hardware_corestatus["neighbour"] = 2;

  JsonObject calrec_hardware_limits = calrec_hardware.createNestedObject("limits");
  calrec_hardware_limits["maxstreams"] = 2;
  calrec_hardware_limits["maxsenders"] = 1;
  calrec_hardware_limits["maxreceivers"] = 1;
  calrec_hardware_limits["maxchannels"] = 4;
  calrec_hardware_limits["maxchannelsperstream"] = 2;

  JsonArray calrec_hardware_limits_supportedpackettimes = calrec_hardware_limits.createNestedArray("supportedpackettimes");
  calrec_hardware_limits_supportedpackettimes.add(1000);

  JsonArray calrec_hardware_limits_linkoffsetrange = calrec_hardware_limits.createNestedArray("linkoffsetrange");
  calrec_hardware_limits_linkoffsetrange.add(250);
  calrec_hardware_limits_linkoffsetrange.add(20000);

  JsonArray calrec_hardware_limits_supportedsamplerates = calrec_hardware_limits.createNestedArray("supportedsamplerates");
  calrec_hardware_limits_supportedsamplerates.add(44100);

  JsonArray calrec_hardware_limits_supportedcodecs = calrec_hardware_limits.createNestedArray("supportedcodecs");
  calrec_hardware_limits_supportedcodecs.add("L16");
  calrec_hardware_limits["supportsunicast"] = true;
  calrec_hardware_limits["supportsstreamptime"] = true;
  calrec_hardware_limits["supportsstreamsamplerate"] = true;
  calrec_hardware_limits["supportschannelrouting"] = true;
  calrec_hardware_limits["supportsdifferenttransportips"] = true;
  calrec_hardware_limits["perslotptpclock"] = true;
  calrec_hardware_limits["supportssampleratechange"] = false;
  calrec_hardware_limits["supportsdelreqinterval"] = false;
  calrec_hardware_limits["supportsarchive"] = false;
  calrec_hardware_limits["supportsbackuprestore"] = false;
  calrec_hardware_limits["supportsupdates"] = false;

  // Must create a pair of nics, even though we only have 1x
  JsonObject calrec_hardware_nics_5 = calrec_hardware["nics"].createNestedObject("5");
  calrec_hardware_nics_5["name"] = "Pri";
  calrec_hardware_nics_5["nicpairindex"] = 0;
  calrec_hardware_nics_5["primary"] = true;
  calrec_hardware_nics_5["up"] = true;
  calrec_hardware_nics_5["ipallocation"] = "static";
  calrec_hardware_nics_5["mac"] = "70:B3:D5:04:2A:9C";
  calrec_hardware_nics_5["address"] = "192.168.30.210";
  calrec_hardware_nics_5["netmask"] = "255.255.255.0";
  calrec_hardware_nics_5["gateway"] = "192.168.30.100";
  calrec_hardware_nics_5["linkspeed"] = 1000;
  calrec_hardware_nics_5["mtu"] = 100;

  JsonObject calrec_hardware_nics_5_bandwidthutilisation = calrec_hardware_nics_5.createNestedObject("bandwidthutilisation");
  calrec_hardware_nics_5_bandwidthutilisation["rx"] = 1;
  calrec_hardware_nics_5_bandwidthutilisation["tx"] = 1;

  JsonObject calrec_hardware_nics_6 = calrec_hardware["nics"].createNestedObject("6");
  calrec_hardware_nics_6["name"] = "Sec";
  calrec_hardware_nics_6["nicpairindex"] = 0;
  calrec_hardware_nics_6["primary"] = false;
  calrec_hardware_nics_6["up"] = true;
  calrec_hardware_nics_6["ipallocation"] = "static";
  calrec_hardware_nics_6["mac"] = "70:B3:D5:04:2A:9D";
  calrec_hardware_nics_6["address"] = "192.168.30.211";
  calrec_hardware_nics_6["netmask"] = "255.255.255.0";
  calrec_hardware_nics_6["gateway"] = "192.168.30.100";
  calrec_hardware_nics_6["linkspeed"] = 1000;
  calrec_hardware_nics_6["mtu"] = 100;

  JsonObject calrec_hardware_nics_6_bandwidthutilisation = calrec_hardware_nics_6.createNestedObject("bandwidthutilisation");
  calrec_hardware_nics_6_bandwidthutilisation["rx"] = 2;
  calrec_hardware_nics_6_bandwidthutilisation["tx"] = 2;

  JsonArray calrec_hardware_errors = calrec_hardware.createNestedArray("errors");

  JsonObject calrec_hardware_protocols = calrec_hardware.createNestedObject("protocols");
  calrec_hardware_protocols["aes70cm2"] = false;
  calrec_hardware_protocols["ravenna"] = false;
  calrec_hardware_protocols["nmos"] = false;
  calrec_hardware_protocols["sap"] = false;

  // Input
  JsonObject calrec_input = calrec.createNestedObject("input");
  JsonObject calrec_input_0 = calrec_input.createNestedObject("0");
  JsonObject calrec_input_0_ioport = calrec_input_0.createNestedObject("ioport");
  calrec_input_0_ioport["type"] = 1;

  JsonArray calrec_input_0_ioport_owners = calrec_input_0_ioport.createNestedArray("owners");
  calrec_input_0_ioport["source"] = nullptr;

  JsonArray calrec_input_0_ioport_destinations = calrec_input_0_ioport.createNestedArray("destinations");
  calrec_input_0_ioport["defaultname"] = "Input mic 1";
  calrec_input_0_ioport["username"] = "";
  calrec_input_0_ioport["description"] = "";
  calrec_input_0_ioport["offline"] = false;
  calrec_input_0_ioport["portid"] = "";
  calrec_input_0["portassoc"] = 0;
  calrec_input_0["otherid"] = 0;
  calrec_input_0["src"] = false;
  calrec_input_0["phpwr"] = false;
  calrec_input_0["gain"] = 100;
  JsonArray calrec_input_0_gainrange = calrec_input_0.createNestedArray("gainrange");
  calrec_input_0_gainrange.add(-180);
  calrec_input_0_gainrange.add(780);
  calrec_input_0["locked"] = false;
  calrec_input_0["virtual"] = false;

  // Output
  JsonObject calrec_output = calrec.createNestedObject("output");

  // GPI
  JsonObject calrec_gpi = calrec.createNestedObject("gpi");

  int numGpi = 4;
  for (int i = 0; i < numGpi; i++) {
    JsonObject calrec_gpi_port = calrec_gpi.createNestedObject(i);
    calrec_gpi_port["state"] = false;
    calrec_gpi_port["inverted"] = false;
    calrec_gpi_port["pulsetime"] = 100;
    calrec_gpi_port["opmode"] = 0;

    JsonObject calrec_gpi_ioport = calrec_gpi_port.createNestedObject("ioport");
    calrec_gpi_ioport["type"] = 6;
    calrec_gpi_ioport["source"] = nullptr;
    calrec_gpi_ioport["defaultname"] = "GPI " + String(i + 1);
    calrec_gpi_ioport["username"] = "";
    calrec_gpi_ioport["description"] = "";
    calrec_gpi_ioport["offline"] = false;
    calrec_gpi_ioport["portid"] = "";
    JsonArray calrec_gpi_ioport_owners = calrec_gpi_ioport.createNestedArray("owners");
    JsonArray calrec_gpi_ioport_destinations = calrec_gpi_ioport.createNestedArray("destinations");
  }

  // GPO
  JsonObject calrec_gpo = calrec.createNestedObject("gpo");

  int numGpo = 2;
  for (int i = 0; i < numGpo; i++) {
    JsonObject calrec_gpo_port = calrec_gpo.createNestedObject(i);
    calrec_gpo_port["state"] = false;
    calrec_gpo_port["inverted"] = false;
    calrec_gpo_port["pulsetime"] = 100;
    calrec_gpo_port["opmode"] = 0;

    JsonObject calrec_gpo_ioport = calrec_gpo_port.createNestedObject("ioport");
    calrec_gpo_ioport["type"] = 7;
    calrec_gpo_ioport["source"] = nullptr;
    calrec_gpo_ioport["defaultname"] = "GPO " + String(i + 1);
    calrec_gpo_ioport["username"] = "";
    calrec_gpo_ioport["description"] = "";
    calrec_gpo_ioport["offline"] = false;
    calrec_gpo_ioport["portid"] = "";
    JsonArray calrec_gpo_ioport_owners = calrec_gpo_ioport.createNestedArray("owners");
    JsonArray calrec_gpo_ioport_destinations = calrec_gpo_ioport.createNestedArray("destinations");
  }

  JsonObject calrec_gpiogroups = calrec.createNestedObject("gpiogroups");

  JsonObject calrec_senders = calrec.createNestedObject("senders");

  JsonObject calrec_receivers = calrec.createNestedObject("receivers");
  calrec["sync"]["ptp"] = nullptr;

  JsonObject calrec_protocols = calrec.createNestedObject("protocols");
}

StaticJsonDocument<4000> DeviceModel::getDocument() {
  return _doc;
}

void DeviceModel::updateGpiState(int index, bool value) {
  _doc["calrec"]["gpi"][String(index)]["state"].set(value);
}

void DeviceModel::updateGpiInvert(int index, bool value) {
  _doc["calrec"]["gpi"][String(index)]["inverted"].set(value);
}

void DeviceModel::updateGpoState(int index, bool value) {
  _doc["calrec"]["gpo"][String(index)]["state"].set(value);
}

void DeviceModel::updateHardwareName(char* value) {
  _doc["calrec"]["hardware"]["name"].set(value);
}
