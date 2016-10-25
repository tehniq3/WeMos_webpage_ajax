// http://www.esp8266.com/viewtopic.php?f=29&t=2696&start=8v by Jose Angel
#include "Arduino.h" 

/* IoT Garage Door Opener
/ This is a sketch for the ESP8266, ESP-12 in my case, using the Arduino IDE for ESP8266.  This sketch will allow you to see the current
/ state of the garage door using a magnetic switch on the garage door, one side connected to pin 4 of the ESP-12 and the other side
/ connected to ground.  Doing this pulls pin 4 LOW when the Garage Door is closed completely.  The webpage updates (doesn't reload) via AJAX every 1 second
/ in order to update the status of the door.  It is possible to add another magnetic switch to show when the door is fully open by
/ tieing it to another pin the same way.  You would have to add the code for the new pin.
/ Activating the button for the door requires a 4 digit pass code.  If you put in the code and then close the browser or app you are using
/ and then log back on you will have to re-enter the code again.  That is done in case you enter the code and close the browser and someone else logs on.
/ They would be able to activate the door since the ESP would still think that codeOK=1.  Each client connect sets codeOK=0 right off the bat.
/
/ To open the door I am using a High level triggered single 5V relay.  I am pulling pin 5 LOW in setup then high for 1 second to
/ trigger the door.  ESP is on a smd adapter with onboard LDO (5v to 3.3V voltage regultor) from Electrodragon. I am powering the ESP and the relay using a 5V 1A power adapter.
/
/ I am using watchdog to detect any crashes and hopefully restart the software.  I am not completely sure the watchdog works yet.
/ I hope they mplement watchdog interrupt capability in the near future.
/
/ Nicu FLORICA change sketch for control a simple LED from password protect webpage
*/

#include <ESP8266WiFi.h>

extern "C" {
  #include "user_interface.h"
}
 


int openClosePin = 5;
int statusPin = 4;
int serverPort = 80;// server port

String HTTP_req;
String code;

WiFiServer server(serverPort);

//*-- IoT Information
const char* _SSID = "bbk"; // previously [const char* SSID = "xxxxxx";] seems SSID now is an reserved constant, so I added "_"
const char* _PASS = "internet";//not sure but seems similar to anterior I added "_"  to avoid conflict
const char* pass_sent = "1234";
char codeOK='0';//start Code is blank....

void setup() {
    ESP.wdtDisable();
    Serial.begin(115200);
    pinMode(openClosePin, OUTPUT);
    pinMode(statusPin, INPUT);
    digitalWrite(openClosePin, LOW);
    delay(50);
    Serial.print("Connecting to ");
    Serial.println(_SSID);
    WiFi.begin(_SSID, _PASS);
   
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected");
    delay(1000);
    ESP.wdtEnable(WDTO_4S);
    //Start the server
    server.begin();
    Serial.println("Server started");
   
    //Print the IP Address
    Serial.print("Use this URL to connect: ");
    Serial.print("http://");
    Serial.print(WiFi.localIP());
    Serial.print(":");
    Serial.print(serverPort,DEC);
    Serial.println("/");

    delay(1000);
}

void loop() {
  // Check if a client has connected
  WiFiClient client = server.available();
 
  if (!client) {
    return;
  }

  Serial.println("found client");
 
  // Return the response
  boolean currentLineIsBlank = true;
  codeOK='0';
  while (client.connected()) {
    if (client.available()) {   // client data available to read
      char c = client.read(); // read 1 byte (character) from client
      HTTP_req += c;  // save the HTTP request 1 char at a time
      // last line of client request is blank and ends with \n
      // respond to client only after last line received
      if (c == '\n' && currentLineIsBlank) {
        client.println("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: keep-alive");
        client.println();
        if (HTTP_req.indexOf("ajax_switch") > -1) {
          // read switch state and send appropriate paragraph text
          GetSwitchState(client);
          delay(0);
        }
        else {  // HTTP request for web page
          // send web page - contains JavaScript with AJAX calls
          client.print("<!DOCTYPE html>\r\n<html>\r\n<head>\r\n<title>ESP8266 Ajax Switch</title>\r\n<meta name='viewport' content='width=device-width', initial-scale='1'>");
          client.print("<script>\r\nfunction GetSwitchState() {\r\nnocache = \"&nocache=\" + Math.random() * 1000000;\r\nvar request = new XMLHttpRequest();\r\nrequest.onreadystatechange = function() {\r\nif (this.readyState == 4) {\r\nif (this.status == 200) {\r\nif (this.responseText != null) {\r\ndocument.getElementById(\"switch_txt\").innerHTML = this.responseText;\r\n}}}}\r\nrequest.open(\"GET\", \"ajax_switch\" + nocache, true);\r\nrequest.send(null);\r\nsetTimeout('GetSwitchState()', 1000);\r\n}\r\n</script>\n");
          client.print("<script>\r\nfunction DoorActivate() {\r\nvar request = new XMLHttpRequest();\r\nrequest.open(\"GET\", \"door_activate\" + nocache, true);\r\nrequest.send(null);\r\n}\r\n</script>\n");
          client.print("</head>\r\n<body onload=\"GetSwitchState()\">\r\n<center><h1>ESP8266 Ajax Switch</h1><hr>\r\n<div id=\"switch_txt\">\r\n</div>\r\n<br>\n");
          client.print("Input password to control Garage Door.\r\n<br><br><form name=\"passcode\" onSubmit=\"GetCode()\"><input type=\"password\" name=\"password\" size='8' maxlength='4'>&nbsp;<input type=submit name=\"Submit\" value=\"Submit\" onClick=\"GetCode()\" style='height:22px; width:80px'></form><br><br>\n");
          if (HTTP_req.indexOf(pass_sent) > 0) {
            GetCode();
          }
          if (codeOK == '0') {
            client.print("<button type=\"button\" disabled style='height:50px; width:225px'>Push the Switch</button><br><br>\n");
          }
          if (codeOK == '1') {
            client.print("<button type=\"button\" onclick=\"DoorActivate()\" style='height:50px; width:225px'>Push the Switch</button><br><br>\n");
          }
          //client.println(system_get_free_heap_size());
          if (HTTP_req.indexOf("door_activate") > -1) {
            // read switch state and send appropriate paragraph text
            DoorActivate();
          }
          //}
          client.print("</body>\r\n</html>\n");
          delay(0);
        }
 
        // display received HTTP request on serial port
        Serial.println(HTTP_req);
        HTTP_req = "";            // finished with request, empty string
        break;
      }
      // every line of text received from the client ends with \r\n
      if (c == '\n') {
          // last character on line of received text
          // starting new line with next character read
          currentLineIsBlank = true;
        }
        else if (c != '\r') {
          // a text character was received from client
          currentLineIsBlank = false;
        }
      } // end if (client.available())
  } // end while (client.connected())
  delay(1);      // give the web browser time to receive the data
  client.stop(); // close the connection
  delay(0);
}

// send the state of the switch to the web browser
void GetSwitchState(WiFiClient cl) {
    if (digitalRead(openClosePin)) {
      cl.println("<p>Switch is currently: <span style='background-color:#FF0000; font-size:18pt'>ON</span></p>");
    }
    else {
      cl.println("<p>Switch is currently: <span style='background-color:#00FF00; font-size:18pt'>OFF</span></p>");
    }
  }
 
  void GetCode() {
      codeOK='1';
  }
 
  void DoorActivate() {
    if (digitalRead(openClosePin)) digitalWrite(openClosePin, LOW);
    else digitalWrite(openClosePin, HIGH);
    
/*    
   digitalWrite(openClosePin, HIGH);
    delay(1000);
    digitalWrite(openClosePin, LOW);
 */   
    codeOK='0';
  }
