/*
 *  SipfClient.cpp
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

#include "SipfClient.h"

void SipfClient::begin(LTEClient *_client, int _port) {
  client = _client;
  port = _port;
}

void SipfClient::begin(LTETLSClient *_client, int _port) {
  tlsclient = _client;
  port = _port;
}

void SipfClient::end() {
}

void SipfClient::_setup_http_client(const String &aServerName, uint16_t aServerPort) {
  if (http_client != NULL) {
    delete http_client;
    http_client = NULL;
  }

  if (client != NULL) {
    http_client = new HttpClient(*client, aServerName, aServerPort);
  } else {
    http_client = new HttpClient(*tlsclient, aServerName, aServerPort);
  }
}

bool SipfClient::authenticate() {
  _setup_http_client(auth_server, port);

  http_client->post(auth_path);

  // read the status code and body of the response
  int statusCode = http_client->responseStatusCode();
  String response = http_client->responseBody();

  Serial.print("Status code: ");
  Serial.println(statusCode);
  if (statusCode < 200 || 299 < statusCode) {
    return false;
  }

  if (response == "") {
    return false;
  }

  response.trim();
  int index = response.indexOf('\n');
  user = response.substring(0, index);
  pass = response.substring(index + 1);

  Serial.print("User: ");
  Serial.println(user);
  Serial.print("Password: ");
  Serial.println(pass);

  return true;
};
