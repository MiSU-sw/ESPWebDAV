#include "config.h"
#include "network.h"
#include "serial.h"
#include "pins.h"
#include "ESP8266WiFi.h"
#include "time.h"
#include "ESPWebDAV.h"
#include "sdControl.h"

String IpAddress2String(const IPAddress& ipAddress)
{
  return String(ipAddress[0]) + String(".") +\
  String(ipAddress[1]) + String(".") +\
  String(ipAddress[2]) + String(".") +\
  String(ipAddress[3])  ;
}

bool Network::start() {
  wifiConnected = false;
  wifiConnecting = true;
  
  // Set hostname first
  WiFi.hostname(HOSTNAME);
  // Reduce startup surge current
  WiFi.setAutoConnect(false);
  WiFi.mode(WIFI_STA);
  WiFi.setPhyMode(WIFI_PHY_MODE_11N);
  WiFi.begin(config.ssid(), config.password());

  // Wait for connection
  unsigned int timeout = 0;
  while(WiFi.status() != WL_CONNECTED) {
    //blink();
    SERIAL_ECHO(".");
    timeout++;
    if(timeout++ > WIFI_CONNECT_TIMEOUT/100) {
      SERIAL_ECHOLN("");
      wifiConnecting = false;
      return false;
    }
    else
      delay(100);
  }

  SERIAL_ECHOLN("");
  SERIAL_ECHO("Connected to "); SERIAL_ECHOLN(config.ssid());
  SERIAL_ECHO("IP address: "); SERIAL_ECHOLN(WiFi.localIP());
  SERIAL_ECHO("RSSI: "); SERIAL_ECHOLN(WiFi.RSSI());
  SERIAL_ECHO("Mode: "); SERIAL_ECHOLN(WiFi.getPhyMode());
  SERIAL_ECHO("Access to SD at the Run prompt : \\\\"); SERIAL_ECHO(WiFi.localIP());SERIAL_ECHO("\\");SERIAL_ECHOLN(SHARE_LOCATION_NAME);

  wifiConnected = true;

  config.save();
  String sIp = IpAddress2String(WiFi.localIP());
  config.save_ip(sIp.c_str());

  SERIAL_ECHOLN("Going to start DAV server");
  if(startDAVServer() < 0) return false;
  wifiConnecting = false;

  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  SERIAL_ECHO("\nWaiting for time\n");
  while (!time(nullptr)) {
    SERIAL_ECHO(".");
    delay(100);
  }
  SERIAL_ECHOLN(" ");

  time_t rawtime;
  struct tm * timeinfo;

  time (&rawtime);
  timeinfo = localtime (&rawtime);

  delay(1000);
  SERIAL_PRINTF("Current local time and date: %s", asctime(timeinfo));

  return true;
}

int Network::startDAVServer() {
  if(!sdcontrol.canWeTakeBus()) {
    return -1;
  }
  sdcontrol.takeBusControl();
  
  // start the SD DAV server
  if(!dav.init(SD_CS, SPI_FULL_SPEED, SERVER_PORT))   {
    DBG_PRINT("ERROR: "); DBG_PRINTLN("Failed to initialize SD Card");
    // indicate error on LED
    //errorBlink();
    initFailed = true;
  }
  else {
    //blink();
  }
  
  sdcontrol.relinquishBusControl();
  DBG_PRINT(HOSTNAME);DBG_PRINTLN(" WebDAV server started");
  return 0;
}

bool Network::isConnected() {
  return wifiConnected;
}

bool Network::isConnecting() {
  return wifiConnecting;
}

// a client is waiting and FS is ready and other SPI master is not using the bus
bool Network::ready() {
  if(!isConnected()) return false;
  
  // do it only if there is a need to read FS
	if(!dav.isClientWaiting())	return false;
	
	if(initFailed) {
	  dav.rejectClient("Failed to initialize SD Card");
	  return false;
	}
	
	// has other master been using the bus in last few seconds
	if(!sdcontrol.canWeTakeBus()) {
		dav.rejectClient("Marlin is reading from SD card");
		return false;
	}

	return true;
}

void Network::handle() {
  if(network.ready()) {
	  sdcontrol.takeBusControl();
	  dav.handleClient();
	  sdcontrol.relinquishBusControl();
	}
}

Network network;
