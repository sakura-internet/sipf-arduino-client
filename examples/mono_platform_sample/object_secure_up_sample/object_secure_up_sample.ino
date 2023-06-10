/*
 *  object_up_sample.ino
 *  Author T.Hayakawa
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *  This sketch connects to a website via LTE. Specifically,
 *  this example downloads the URL "http://arduino.tips/asciilogo.txt" and
 *  prints it to the Serial monitor.
 */

// libraries
#include "SpifClient.h"
#include <String>
#include <LTE.h>
#include <SDHCI.h>

LTE lteAccess;
LTETLSClient client;

SDClass theSD;

SpifClient theSpifClient;


int port = 443; // port 443 is the default for HTTPS

#define ROOTCA_FILE "cert/SAKURAIoTRootCA.crt"   // Define the path to a file containing CA
                                       // certificates that are trusted.
#define CERT_FILE   "path/to/certfile" // Define the path to a file containing certificate
                                       // for this client, if required by the server.
#define KEY_FILE    "path/to/keyfile"  // Define the path to a file containing private key
 
String readFromSerial() {
  /* Read String from serial monitor */
  String str;
  int  read_byte = 0;
  while (true) {
    if (Serial.available() > 0) {
      read_byte = Serial.read();
      if (read_byte == '\n' || read_byte == '\r') {
        Serial.println("");
        break;
      }
      Serial.print((char)read_byte);
      str += (char)read_byte;
    }
  }
  return str;
}

void readApnInformation(char apn[], LTENetworkAuthType *authtype,
                       char user_name[], char password[]) {
  /* Set APN parameter to arguments from readFromSerial() */

  String read_buf;

  while (strlen(apn) == 0) {
    Serial.print("Enter Access Point Name:");
    readFromSerial().toCharArray(apn, LTE_NET_APN_MAXLEN);
  }

  while (true) {
    Serial.print("Enter APN authentication type(CHAP, PAP, NONE):");
    read_buf = readFromSerial();
    if (read_buf.equals("NONE") == true) {
      *authtype = LTE_NET_AUTHTYPE_NONE;
    } else if (read_buf.equals("PAP") == true) {
      *authtype = LTE_NET_AUTHTYPE_PAP;
    } else if (read_buf.equals("CHAP") == true) {
      *authtype = LTE_NET_AUTHTYPE_CHAP;
    } else {
      /* No match authtype */
      Serial.println("No match authtype. type at CHAP, PAP, NONE.");
      continue;
    }
    break;
  }

  if (*authtype != LTE_NET_AUTHTYPE_NONE) {
    while (strlen(user_name)== 0) {
      Serial.print("Enter username:");
      readFromSerial().toCharArray(user_name, LTE_NET_USER_MAXLEN);
    }
    while (strlen(password) == 0) {
      Serial.print("Enter password:");
      readFromSerial().toCharArray(password, LTE_NET_PASSWORD_MAXLEN);
    }
  }

  return;
}

void setup()
{
  char apn[LTE_NET_APN_MAXLEN] = APP_LTE_APN;
  LTENetworkAuthType authtype = APP_LTE_AUTH_TYPE;
  char user_name[LTE_NET_USER_MAXLEN] = APP_LTE_USER_NAME;
  char password[LTE_NET_PASSWORD_MAXLEN] = APP_LTE_PASSWORD;

  // initialize serial communications and wait for port to open:
  Serial.begin(115200);
  while (!Serial) {
      ; // wait for serial port to connect. Needed for native USB port only
  }

  /* Initialize SD */
  while (!theSD.begin()) {
    /* wait until SD card is mounted. */
    Serial.println("Insert SD card.");
  }

  Serial.println("Starting web client.");

  /* Set if Access Point Name is empty */
  if (strlen(APP_LTE_APN) == 0) {
    Serial.println("This sketch doesn't have a APN information.");
    readApnInformation(apn, &authtype, user_name, password);
  }
  Serial.println("=========== APN information ===========");
  Serial.print("Access Point Name  : ");
  Serial.println(apn);
  Serial.print("Authentication Type: ");
  Serial.println(authtype == LTE_NET_AUTHTYPE_CHAP ? "CHAP" :
                 authtype == LTE_NET_AUTHTYPE_NONE ? "NONE" : "PAP");
  if (authtype != LTE_NET_AUTHTYPE_NONE) {
    Serial.print("User Name          : ");
    Serial.println(user_name);
    Serial.print("Password           : ");
    Serial.println(password);
  }

  while (true) {

    /* Power on the modem and Enable the radio function. */

    if (lteAccess.begin() != LTE_SEARCHING) {
      Serial.println("Could not transition to LTE_SEARCHING.");
      Serial.println("Please check the status of the LTE board.");
      for (;;) {
        sleep(1);
      }
    }

    /* The connection process to the APN will start.
     * If the synchronous parameter is false,
     * the return value will be returned when the connection process is started.
     */
    if (lteAccess.attach(APP_LTE_RAT,
                         apn,
                         user_name,
                         password,
                         authtype,
                         APP_LTE_IP_TYPE) == LTE_READY) {
      Serial.println("attach succeeded.");
      break;
    }

    /* If the following logs occur frequently, one of the following might be a cause:
     * - APN settings are incorrect
     * - SIM is not inserted correctly
     * - If you have specified LTE_NET_RAT_NBIOT for APP_LTE_RAT,
     *   your LTE board may not support it.
     * - Rejected from LTE network
     */
    Serial.println("An error has occurred. Shutdown and retry the network attach process after 1 second.");
    lteAccess.shutdown();
    sleep(1);
  }
  
  // Set certifications via a file on the SD card before connecting to the server
  File rootCertsFile = theSD.open(ROOTCA_FILE, FILE_READ);
  client.setCACert(rootCertsFile, rootCertsFile.available());
  rootCertsFile.close();

  theSpifClient.begin(&client, port);

  if(!theSpifClient.authorization()){
    puts("Authorization error!");
  }

}

void loop()
{  
  static uint64_t utime = 0;

  static uint8_t  dmy_data0 = 0;
  static uint16_t dmy_data1 = 0x1111;
  static float    dmy_data2 = 0.25;

  SipfObjectObject objs[] = {
    {OBJ_TYPE_UINT8,   0x01, OBJ_SIZE_UINT8,   &dmy_data0},
    {OBJ_TYPE_UINT16,  0x02, OBJ_SIZE_UINT16,  (uint8_t*)&dmy_data1},
    {OBJ_TYPE_FLOAT32, 0x03, OBJ_SIZE_FLOAT32, (uint8_t*)&dmy_data2}
  };

  dmy_data0++;
  dmy_data1+=4;
  dmy_data2+=0.33;

  theSpifClient.upload(utime,objs,sizeof(objs)/sizeof(SipfObjectObject));

  int64_t otid;
  if(theSpifClient.receiveResult(&otid)){
    printf("Result:%08x\n",otid);
  }

  sleep(50);

  return;
/*
stop_client:
  // if the server's disconnected, stop the client:
  if (!client.available() && !client.connected()) {
    Serial.println();
    Serial.println("disconnecting.");
    client.stop();
    theSpifClient.end();
  }

  exit(1);
*/
}
