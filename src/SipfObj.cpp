#include "SipfClient.h"

uint64_t SipfClient::uploadObjects(uint64_t utime, SipfObjectObject *objs, uint8_t obj_qty) {
  _setup_http_client(data_server, 80);

  puts("upload objects.");
  String contentType = "Content-type: application/octet-stream";

  int packet_size = _build_objects_up(objectBuffer, utime, objs, obj_qty);

  if (packet_size <= OBJ_HEADER_SIZE) {
    puts("Error");
    return 0;
  }

  http_client->beginRequest();
  http_client->post(data_path);
  http_client->sendBasicAuth(user, pass);
  http_client->sendHeader("Content-type: application/octet-stream");
  http_client->sendHeader("Content-Length", packet_size);
  http_client->write(objectBuffer, packet_size);
  http_client->endRequest();

  int statusCode = http_client->responseStatusCode();
  Serial.print("Status code: ");
  Serial.println(statusCode);
  if (statusCode < 200 || 299 < statusCode) {
    return 0;
  }

  packet_size = http_client->read(objectBuffer, http_client->contentLength());
  printf("packet_size=%d\n", packet_size);
  for (uint8_t i = 0; i < packet_size; i++) {
    printf("0x%02x ", objectBuffer[i]);
  }
  printf("\n");

  // COMMAND_TYPE
  if (objectBuffer[0] != OBJID_NOTIFICATION) {
    puts("error type!");
    return 0;
  }

  int payload_length = objectBuffer[10] << 8 | objectBuffer[11];

  if (payload_length != 18) {
    puts("error size!");
    return 0;
  }

  uint64_t otid = 0;
  memcpy(&otid, &objectBuffer[2], sizeof(int64_t));

  http_client->stop();

  return otid;
}

int SipfClient::_build_objects_up(uint8_t *ptr, uint64_t utime, SipfObjectObject *objs, uint8_t obj_qty) {
  int payload_size = _build_objects_up_payload((ptr + OBJ_HEADER_SIZE), MAX_PAYLOAD_SIZE, objs, obj_qty);

  printf("payload_size=%d\n", payload_size);

  // COMMAND_TYPE
  ptr[0] = (uint8_t)OBJECTS_UP;

  // COMMAND_TIME
  ptr[1] = 0xff & (utime >> (8 * 7));
  ptr[2] = 0xff & (utime >> (8 * 6));
  ptr[3] = 0xff & (utime >> (8 * 5));
  ptr[4] = 0xff & (utime >> (8 * 4));
  ptr[5] = 0xff & (utime >> (8 * 3));
  ptr[6] = 0xff & (utime >> (8 * 2));
  ptr[7] = 0xff & (utime >> (8 * 1));
  ptr[8] = 0xff & (utime >> (8 * 0));

  // OPTION_FLAG
  ptr[9] = 0x00;

  // PAYLOAD_SIZE (BigEndian)
  ptr[10] = (uint8_t)(payload_size >> 8);
  ptr[11] = (uint8_t)(payload_size & 0xff);

  if (payload_size > 1) {
    return payload_size + OBJ_HEADER_SIZE;
  }
  return -1;
}

int SipfClient::_build_objects_up_payload(uint8_t *raw_buff, uint16_t sz_raw_buff, SipfObjectObject *objs, uint8_t obj_qty) {
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
  return idx_raw_buff;  // バッファに書いたデータ長を返す
}

//
// 64bitデータのバイトスワップ
//
static uint64_t bswap64(uint64_t value) {
  return ((value >> 56) & 0x00000000000000FF) | ((value >> 40) & 0x000000000000FF00) | ((value >> 24) & 0x0000000000FF0000) | ((value >> 8) & 0x00000000FF000000) | ((value << 8) & 0x000000FF00000000) | ((value << 24) & 0x0000FF0000000000) | ((value << 40) & 0x00FF000000000000) | ((value << 56) & 0xFF00000000000000);
}

//
// COMMAND_TYPE_OBJECTS_DOWN_REQUESTを送信し、COMMAND_TYPE_OBJECTS_DOWNを受信する。戻り値は、受信したオブジェクト群のデータのbyte数を返す。
//
uint64_t SipfClient::downloadObjects(uint64_t utime, SipfObjectDown *objDown) {
  _setup_http_client(data_server, 80);

  // 要求(COMMAND_TYPE_DOWN_REQUEST)を送信
  int packet_size = _build_objects_down_request(objectBuffer, utime);

  http_client->beginRequest();
  http_client->post(data_path);
  http_client->sendBasicAuth(user, pass);
  http_client->sendHeader("Content-type: application/octet-stream");
  http_client->sendHeader("Content-Length", packet_size);
  http_client->write(objectBuffer, packet_size);
  http_client->endRequest();

  int statusCode = http_client->responseStatusCode();
  if (statusCode < 200 || 299 < statusCode) {
    printf("error, responseStatusCode (%d)", statusCode);
    return 0;
  }

  // 応答(COMMAND_TYPE_OBJECTS_DOWN)の解析
  packet_size = http_client->read(objectBuffer, http_client->contentLength());
  /*
    printf("packet_size=%d\n", packet_size);
    for (uint8_t i = 0; i < packet_size; i++)
    {
        printf("0x%02x ", objectBuffer[i]);
    }
    printf("\n");
*/
  if (objectBuffer[0] != OBJECTS_DOWN) {
    printf("error, COMMAND_TYPE (0x%02X)", objectBuffer[0]);
    return 0;
  }

  int payload_length = (objectBuffer[10] << 8 | objectBuffer[11]);
  if (payload_length < OBJ_DOWN_PAYLOAD_SIZE_MIN) {
    printf("error, payload_length (%d)\n", payload_length);
    return 0;
  }

  int i = OBJ_HEADER_SIZE;

  objDown->down_request_result = objectBuffer[i];
  i += sizeof(objDown->down_request_result);

  memcpy(&objDown->otid, &objectBuffer[i], sizeof(objDown->otid));
  i += sizeof(objDown->otid);

  memcpy(&objDown->timestamp_src, &objectBuffer[i], sizeof(objDown->timestamp_src));
  objDown->timestamp_src = bswap64(objDown->timestamp_src);
  i += sizeof(objDown->timestamp_src);

  memcpy(&objDown->timestamp_platfrom_from_src, &objectBuffer[i], sizeof(objDown->timestamp_platfrom_from_src));
  objDown->timestamp_platfrom_from_src = bswap64(objDown->timestamp_platfrom_from_src);
  i += sizeof(objDown->timestamp_platfrom_from_src);

  objDown->remains = objectBuffer[i];
  i += sizeof(objDown->remains);

  i += 1;  // RESERVED 1byte

  int objDataLength = packet_size - (OBJ_HEADER_SIZE + OBJ_DOWN_PAYLOAD_SIZE_MIN);
  objDown->objects_data = &objectBuffer[i];

  http_client->stop();

  return objDataLength;
}

//
// COMMAND_TYPE_OBJECTS_DOWN_REQUESTのデータを組立
//
int SipfClient::_build_objects_down_request(uint8_t *ptr, uint64_t utime) {
  // COMMAND_TYPE
  ptr[0] = (uint8_t)OBJECTS_DOWN_REQUEST;

  // COMMAND_TIME
  ptr[1] = 0xff & (utime >> (8 * 7));
  ptr[2] = 0xff & (utime >> (8 * 6));
  ptr[3] = 0xff & (utime >> (8 * 5));
  ptr[4] = 0xff & (utime >> (8 * 4));
  ptr[5] = 0xff & (utime >> (8 * 3));
  ptr[6] = 0xff & (utime >> (8 * 2));
  ptr[7] = 0xff & (utime >> (8 * 1));
  ptr[8] = 0xff & (utime >> (8 * 0));

  // OPTION_FLAG
  ptr[9] = 0x00;

  // PAYLOAD_LENGTH
  ptr[10] = 0x00;
  ptr[11] = 0x01;

  // PAYLOAD
  ptr[12] = 0x00;  // RESERVED 1byte

  return (OBJ_HEADER_SIZE + 1);
}
