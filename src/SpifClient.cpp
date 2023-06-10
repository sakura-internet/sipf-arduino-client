/*
 *  SpifClient.cpp
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

#include "SpifClient.h"

void SpifClient::begin(LTEClient* _client, int _port)
{
  client = _client;
  port = _port;
  
}

void SpifClient::begin(LTETLSClient* _client, int _port)
{
  tlsclient = _client;
  port = _port;
  
}

void SpifClient::end()
{
//  delete http_client;
}

bool SpifClient::authorization()
{
  if(port == 80){
    http_client = new HttpClient(*client, auth_server, port);
  }else{
    http_client = new HttpClient(*tlsclient, auth_server, port);  	
  }

  http_client->post(auth_path);

  // read the status code and body of the response
  int statusCode = http_client->responseStatusCode();
  String response = http_client->responseBody();

  if(response == "") return false;

  response.trim();
  int index = response.indexOf('\n');
  user = response.substring(0, index);
  pass = response.substring(index+1);

  Serial.print("Status code: ");
  Serial.println(statusCode);
  Serial.print("User: ");
  Serial.println(user);
  Serial.print("Password: ");
  Serial.println(pass);

//  delete http_client;

  if(port == 80){
    http_client = new HttpClient(*client, data_server, port);
  }else{
    http_client = new HttpClient(*tlsclient, data_server, port);
  }

  if(port == 80){
    http_file_client = new HttpClient(*client, file_server, port);
  }else{
    http_file_client = new HttpClient(*tlsclient, file_server, port);
  }

  return true;
};

bool SpifClient::SipfFileRequestUploadURL(File& file, String filename)
{

  puts("upload a file.");
  String url_name = String(file_path) + filename + String("/");

  Serial.print("Url: ");
  Serial.println(url_name);

  http_file_client->beginRequest();
  http_file_client->put(url_name);  
  http_file_client->sendBasicAuth(user, pass);
  http_file_client->endRequest();

  // read the status code and body of the response
  int statusCode = http_file_client->responseStatusCode();
  String response = http_file_client->responseBody();

  Serial.print("Status code: ");
  Serial.println(statusCode);

  http_file_client->stop();
  delete http_file_client;
	
  if(statusCode != 200){
    puts("error responce.");
    return false;
  }

  Serial.print("Response: ");
  Serial.println(response);

  http_file_client = new HttpClient(*client, "content.sipf.iot.sakura.ad.jp", port);

  int index = response.indexOf(".jp/");
  String path = response.substring(index+4);
  Serial.print("Path: ");
  Serial.println(path);

  int file_size = file.size();
  Serial.print("size: ");
  Serial.println(file_size);

//  while(file_size > 0){

    const int buffer_size = 1024*2;

    uint8_t buf[buffer_size];
    file.read(buf,buffer_size);

    http_file_client->beginRequest();
    http_file_client->put(path);
    http_file_client->sendBasicAuth(user, pass);
    http_file_client->sendHeader("Content-type: application/octet-stream");
    http_file_client->sendHeader("Content-Length", buffer_size);

    if(file_size > buffer_size){
      file_size -= buffer_size;
      http_file_client->sendHeader("Content-Length", buffer_size);
      http_file_client->write(buf,buffer_size);
    } else {
      file_size = 0;
      http_file_client->sendHeader("Content-Length", file_size);
      http_file_client->write(buf,file_size);
    }
//  }

  http_file_client->endRequest();

  http_file_client->stop();
  delete http_file_client;

  http_file_client = new HttpClient(*client, "file.sipf.iot.sakura.ad.jp", port);

  http_client->beginRequest();
  url_name = String(file_path) + filename + String("/complete/");
  http_client->endRequest();

   // read the status code and body of the response
  statusCode = http_file_client->responseStatusCode();
  response = http_file_client->responseBody();

  Serial.print("Status code: ");
  Serial.println(statusCode);

  if(statusCode != 200){
    puts("error responce.");
    http_file_client->stop();
    delete http_file_client;
    return false;
  }

  Serial.print("Response: ");
  Serial.println(response);

  return true;

}

bool SpifClient::upload(uint64_t utime, SipfObjectObject *objs, uint8_t obj_qty)
{
  if(port == 80){
    http_client = new HttpClient(*client, data_server, port);
  }else{
    http_client = new HttpClient(*tlsclient, data_server, port);
  }

  puts("upload objects.");
  String contentType = "Content-type: application/octet-stream";

  int packet_size = SipfObjectCreateObjUp(objectBuffer,utime,objs,obj_qty);

  if(packet_size<=OBJ_HEADER_SIZE){  
    puts("Error");
    return false;
  }

#if 0
  for(int i=0;i<packet_size;i++){
    printf("pac[%d] = %02x\n",i,objectBuffer[i]);
  }
  printf("\n");
#endif

  http_client->beginRequest();
  http_client->post(data_path);
  http_client->sendBasicAuth(user, pass);
  http_client->sendHeader("Content-type: application/octet-stream");
  http_client->sendHeader("Content-Length", packet_size);
  http_client->write(objectBuffer,packet_size);
  http_client->endRequest();

  return true;

}

bool SpifClient::receiveResult(int64_t* otid)
{
  int statusCode = http_client->responseStatusCode();
//  String response = http_client->responseBody();
  int packet_size = http_client->read(objectBuffer, http_client->contentLength());

  printf("packet_size=%d\n",packet_size);

  Serial.print("Status code: ");
  Serial.println(statusCode);
  Serial.println("Response: ");
//  Serial.println(response);
//  SipfObjectsParse(objectBuffer, (SipfObjectObject*)0, packet_size);

   printf("objectBuffer=%08x\n",objectBuffer);

  // COMMAND_TYPE
  if(objectBuffer[0] != OBJID_NOTIFICATION){
    puts("error type!");
    return false;
  }
  
  int payload_length = objectBuffer[10] << 8 | objectBuffer[11];

  if(objectBuffer[0] != OBJID_NOTIFICATION){
    puts("error type!");
    return false;
  }

  if(payload_length != 18){
    puts("error size!");
    return false;
  }

  for(int i=0;i<sizeof(int64_t);i++){
    *otid |= objectBuffer[i+2] << (8*(7-i));
  }

  http_client->stop();
  delete http_client;

  return true;
}

int SpifClient::SipfObjectCreateObjUp(uint8_t* ptr, uint64_t utime, SipfObjectObject *objs, uint8_t obj_qty)
{
  int payload_size = SipfObjectCreateObjUpPayload((ptr+OBJ_HEADER_SIZE), MAX_PAYLOAD_SIZE, objs, obj_qty);

printf("payload_size=%d\n",payload_size);

  // COMMAND_TYPE
  ptr[0] = (uint8_t)OBJECTS_UP;
    
  // COMMAND_TIME
  ptr[1] = 0xff & (utime >> (8*7));
  ptr[2] = 0xff & (utime >> (8*6));
  ptr[3] = 0xff & (utime >> (8*5));
  ptr[4] = 0xff & (utime >> (8*4));
  ptr[5] = 0xff & (utime >> (8*3));
  ptr[6] = 0xff & (utime >> (8*2));
  ptr[7] = 0xff & (utime >> (8*1));
  ptr[8] = 0xff & (utime >> (8*0));

  // OPTION_FLAG
  ptr[9] = 0x00;
    
  // PAYLOAD_SIZE(BigEndian)
  ptr[10] = (uint8_t)payload_size >> 8;
  ptr[11] = (uint8_t)payload_size & 0xff;

  if(payload_size>1){
    return payload_size + OBJ_HEADER_SIZE;
  }
  return -1; 
}

int SpifClient::SipfObjectCreateObjUpPayload(uint8_t *raw_buff, uint16_t sz_raw_buff, SipfObjectObject *objs, uint8_t obj_qty)
{
    static uint8_t work[220];
    if (raw_buff == NULL) {
      return -1;
    }

    memset(raw_buff, 0, sz_raw_buff);
    int idx_raw_buff = 0;
    for (int i = 0; i < obj_qty; i++) {
//        LOG_DBG("objs[%d]", i);
        if (objs[i].value_len > 220) {
            return -1;
        }
        raw_buff[idx_raw_buff++] = objs[i].obj_type;
        raw_buff[idx_raw_buff++] = objs[i].obj_tagid;
        raw_buff[idx_raw_buff++] = objs[i].value_len;

        switch (objs[i].obj_type) {
        case OBJ_TYPE_UINT8:
        case OBJ_TYPE_INT8:
        case OBJ_TYPE_BIN:
        case OBJ_TYPE_STR_UTF8:
            // バイトスワップ必要なし
            memcpy(&raw_buff[idx_raw_buff], objs[i].value, objs[i].value_len);
            idx_raw_buff += objs[i].value_len;
            break;
        default:
            // バイトスワップする(リトリエンディアン->ビッグエンディアン)
            if (objs[i].value_len % 2) {
                // データ長が偶数じゃない
                return -1;
            }
            for (int j = 0; j < objs[i].value_len; j++) {
                work[j] = objs[i].value[objs[i].value_len - j - 1];
            }
            memcpy(&raw_buff[idx_raw_buff], work, objs[i].value_len);
            idx_raw_buff += objs[i].value_len;
            break;
        }
    }
    return idx_raw_buff; // バッファに書いたデータ長を返す
}
