#include "Arduino.h"
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

//
// ファイル受信
//
uint64_t SipfClient::downloadFile(String filename, uint8_t buf[], size_t bufSize) {

  // 1. Get download URL
  String downloadURL = requestFileDownloadURL(filename);
  Serial.println(downloadURL);
  if (downloadURL == "") {
    return 0;
  }

  // 2. Download contet
  uint64_t downLoadfileSize = downloadFileContent(buf, bufSize, downloadURL);

  return downLoadfileSize;
}

uint64_t SipfClient::downloadFileContent(uint8_t buf[], size_t bufSize, String url) {

  int host_index = url.indexOf("://");
  String host = url.substring(host_index + 3);
  int path_index = host.indexOf("/");
  String path = host.substring(path_index);
  host = host.substring(0, path_index);

  bool readComplete = false;
  uint64_t lastReadTime = 0;
  int readByte = 0;

  _setup_http_client(host, 80);

  int err = http_client->get(path);
  if (err != 0) {
    printf("%s http_client->get error, %d\n", __func__, err);
    goto QUIT;
  }
  if (http_client->responseStatusCode() != 200) {
    printf("%s http_client->responseStatusCode error, statuCode %d\n", __func__, http_client->responseStatusCode());
    goto QUIT;
  }
  if (bufSize < http_client->contentLength()) {
    printf("%s error, buffer size is too small\n", __func__);
    goto QUIT;
  }

  lastReadTime = millis();
  while (true) {
    if (!http_client->connected()) {
      printf("%s error, !http_client->connected\n", __func__);
      break;
    }
    if (!http_client->available()) {
      if ((millis() - lastReadTime) > (30 * 1000)) {
        printf("%s error, !http_client->available, wait timeout\n", __func__);
        break;
      }
      delay(1000);  // wait for retry
      continue;
    }
    readByte += http_client->read(&buf[readByte], (bufSize - readByte));
    if (http_client->endOfBodyReached()) {
      readComplete = true;
      break;
    }
    if ((bufSize - readByte) <= 0) {
      printf("%s error, buffer size is too small\n", __func__);
      break;
    }
    lastReadTime = millis();
  }

QUIT:
  http_client->stop();
  if (!readComplete) {
    return 0;
  }
  return readByte;
}

String SipfClient::requestFileDownloadURL(String filename) {
  String url_name = String(file_path) + filename + String("/");

  String downloadUrl = "";
  int err = 0;
  int statusCode = 0;

  _setup_http_client(file_server, 80);

  http_client->beginRequest();
  http_client->get(url_name);
  http_client->sendBasicAuth(user, pass);
  http_client->endRequest();

  statusCode = http_client->responseStatusCode();
  if (statusCode != 200) {
    printf("%s http_client->responseStatusCode error, statuCode %d\n", __func__, statusCode);
    goto QUIT;
  }

  downloadUrl = http_client->responseBody();
  downloadUrl.trim();

QUIT:
  http_client->stop();
  return downloadUrl;
}
