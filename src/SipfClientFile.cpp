#include "SipfClient.h"

bool SipfClient::uploadFile(String filename, uint8_t buf[], size_t size) {
  // 1. Get uplaod URL
  String uploadURL = requestFileUploadURL(filename);
  if (uploadURL == "") {
    return false;
  }

  // 2. Uplaod content
  bool ret = uploadFileContent(buf, size, uploadURL);
  if (!ret) {
    return false;
  }

  // 3. Complete upload
  ret = finalizeFileUpload(filename);
  if (!ret) {
    return false;
  }

  return true;
}

String SipfClient::requestFileUploadURL(String filename) {
  puts("request upload a file");
  String url_name = String(file_path) + filename + String("/");

  Serial.print("Url: ");
  Serial.println(url_name);

  _setup_http_client(file_server, 80);

  http_client->beginRequest();
  http_client->put(url_name);
  http_client->sendBasicAuth(user, pass);
  http_client->endRequest();

  // read the status code and body of the response
  int statusCode = http_client->responseStatusCode();
  String response = http_client->responseBody();

  Serial.print("Status code: ");
  Serial.println(statusCode);

  http_client->stop();

  if (statusCode != 200) {
    puts("error responce.");
    return "";
  }

  response.trim();
  return response;
}

bool SipfClient::finalizeFileUpload(String filename) {
  puts("request complete upload");
  String url_name = String(file_path) + filename + String("/complete/");

  Serial.print("Url: ");
  Serial.println(url_name);

  _setup_http_client(file_server, 80);

  http_client->beginRequest();
  http_client->put(url_name);
  http_client->sendBasicAuth(user, pass);
  http_client->endRequest();

  // read the status code and body of the response
  int statusCode = http_client->responseStatusCode();
  String response = http_client->responseBody();

  Serial.print("Status code: ");
  Serial.println(statusCode);

  http_client->stop();

  if (statusCode != 200) {
    puts("error responce.");
    return false;
  }

  return true;
}

bool SipfClient::uploadFileContent(uint8_t buf[], size_t size, String url) {

  int host_index = url.indexOf("://");
  String host = url.substring(host_index + 3);
  int path_index = host.indexOf("/");
  String path = host.substring(path_index);
  host = host.substring(0, path_index);

  Serial.print("Host: ");
  Serial.println(host);
  Serial.print("Path: ");
  Serial.println(path);

  _setup_http_client(host, 80);

  http_client->beginRequest();
  http_client->put(path);
  http_client->sendHeader("Content-type", "application/octet-stream");
  http_client->sendHeader("Content-Length", size);

  uint8_t *sp = buf;
  size_t sent = 0;
  while (sent < size) {
    size_t sending = size - sent;
    if (sending > 1024) {
      sending = 1024;
    }
    http_client->write(sp, sending);
    sp += sending;
    sent += sending;
  }

  http_client->endRequest();

  // read the status code and body of the response
  int statusCode = http_client->responseStatusCode();
  String response = http_client->responseBody();

  Serial.print("Status code: ");
  Serial.println(statusCode);

  http_client->stop();

  return true;
}
