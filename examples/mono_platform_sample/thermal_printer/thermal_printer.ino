/*
 *    Copyright (C) 2023 SAKURA internet Inc.
 */
#include <SoftwareSerial.h>
#include <LTE.h>
#include "SipfClient.h"
#include <Time.h>
#include <Adafruit_Thermal.h>
#include <JPEGDEC.h>
#include "sakura_logo.h"

LTE lteAccess;
LTEClient lteClient;
LTEScanner lteScanner;
SipfClient sipfClient;

SoftwareSerial PrinterSerial(PIN_D30, PIN_D31);
Adafruit_Thermal Printer(&PrinterSerial);

JPEGDEC jpeg;

static uint8_t imageBuffer[(1024 * 64)];   // 画像ファイルデータのバッファ
static uint8_t demodeBuffer[(1024 * 32)];  // 画像ファイルデータのディザリングに使用するバッファ

/*******************************************************************************
 *
 * setup
 *
 *******************************************************************************/
void setup() {
  // initialize LED
  pinMode(LED0, OUTPUT);
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);

  // initialize serial communications and wait for port to open:
  Serial.begin(115200);
  while (!Serial) {
    ;  // wait for serial port to connect. Needed for native USB port only
  }

  // サーマルプリンター初期化
  PrinterSerial.begin(19200);
  Printer.begin();

  // ロゴを印刷
  Printer.printBitmap(SAKURA_LOGO_WIDTH, SAKURA_LOGO_HEIGHT, sakura_logo_data);
  Printer.feed(4);

  // LTE通信初期化
  setupLte();

  // SIPF通信初期化
  sipfClient.begin(&lteClient, 80);
  if (!sipfClient.authenticate()) {
    printf("%s error, sipfClient.authenticate\n", __func__);
  }
}

//
// LTE通信初期化
//
void setupLte(void) {
  while (true) {
    // Power on the modem and Enable the radio function.
    printf("lteAccess.begin\n", __func__);
    if (lteAccess.begin() != LTE_SEARCHING) {
      printf("%s error, Could not transition to LTE_SEARCHING.\n", __func__);
      while (true) {
        sleep(1);
      }
    }

    // The connection process to the APN will start.
    // If the synchronous parameter is false, the return value will be returned when the connection process is started.
    if (lteAccess.attach(APP_LTE_RAT,
                         APP_LTE_APN,
                         APP_LTE_USER_NAME,
                         APP_LTE_PASSWORD,
                         APP_LTE_AUTH_TYPE,
                         APP_LTE_IP_TYPE)
        == LTE_READY) {
      printf("%s Attach succeeded.\n", __func__);
      break;
    }

    // If the following logs occur frequently, one of the following might be a cause:
    // - APN settings are incorrect
    // - SIM is not inserted correctly
    // - If you have specified LTE_NET_RAT_NBIOT for APP_LTE_RAT, your LTE board may not support it.
    // - Rejected from LTE network
    printf("%s lteAccess An error has occurred. Shutdown and retry the network attach process after 1 second.\n", __func__);
    lteAccess.shutdown();
    sleep(1);
  }
}

/*******************************************************************************
 *
 * loop
 *
 *******************************************************************************/
void loop() {
  static int lastFileSize = 0;

  // 画像ファイルをダウンロード
  digitalWrite(LED2, HIGH);
  printf("downloadFile\n");
  int fileSize = sipfClient.downloadFile("image.jpg", imageBuffer, sizeof(imageBuffer));
  printf("  fileSize %d byte (%.2f KB)\n", fileSize, ((float)fileSize / 1024));
  digitalWrite(LED2, LOW);

  // 画像ファイルのサイズが前回と異なる場合は、印刷
  if (fileSize != lastFileSize) {
    printf("printJpeg\n");
    Printer.printf("-- %s --\n", getTimeStr());
    printJpeg(imageBuffer, fileSize);
    Printer.feed(4);
    printf("  OK\n");

    lastFileSize = fileSize;
  }

  // メッセージ(オブジェクト)をダウンロード
  uint64_t objDataLength = 0;
  SipfObjectDown objDown;

  memset(&objDown, 0, sizeof(objDown));
  printf("downloadObjects\n");
  objDataLength = sipfClient.downloadObjects(0, &objDown);

  printf("  objDataLength %llu\n", objDataLength);
  for (int i = 0; i < objDataLength; i++) {  // オブジェクトのデータをダンプ
    printf("    %03d 0x%02X\n", i, objDown.objects_data[i]);
  }
  if (objDataLength >= 3) {
    if (objDown.objects_data[0] == OBJ_TYPE_STR_UTF8) {  // オブジェクトのタイプが可変長UTF-8文字列の場合は、印刷
      Printer.printf("%s\n", &objDown.objects_data[3]);
    }
  }

  sleep(60);
}

//
// 現在時刻の文字列を取得
//
char *getTimeStr(void) {
  static char timestr[20];

  time_t unixTime = lteAccess.getTime();
  struct tm *timeinfo;

  timeinfo = localtime(&unixTime);
  strftime(timestr, sizeof(timestr), "%Y/%m/%d %H:%M:%S", timeinfo);
  return timestr;
}

//
// JPEGデータを印刷 ... ライブラリ(JPEGDEC)を利用してディザリングし、そのコールバック関数内で印刷を実行
//
void printJpeg(uint8_t *jpeg_data, int jpeg_data_len) {
  int ret;

  jpeg.openRAM(jpeg_data, jpeg_data_len, JPEGDraw);

  printf("jpeg %d * %d\n", jpeg.getWidth(), jpeg.getHeight());

  jpeg.setPixelType(ONE_BIT_DITHERED);

  printf("jpeg.decodeDither...\n");
  ret = jpeg.decodeDither(demodeBuffer, 0);
  printf("  ret %d\n", ret);

  jpeg.close();
}

//
// ディザリング(decodeDither関数)実行時のコールバック関数
//
int JPEGDraw(JPEGDRAW *pDraw) {
  digitalWrite(LED3, HIGH);

  printf("JPEGDraw Callback ");
  printf("x %3d, ", pDraw->x);
  printf("y %3d, ", pDraw->y);
  printf("iWidth %03d, ", pDraw->iWidth);
  printf("iHeight %03d, ", pDraw->iHeight);
  printf("iBpp %d\n", pDraw->iBpp);

  for (int i = 0; i < (pDraw->iWidth * pDraw->iHeight); i++) {
    demodeBuffer[i] = ~demodeBuffer[i];  // 白黒反転
  }
  Printer.printBitmap(pDraw->iWidth, pDraw->iHeight, demodeBuffer);  // 印刷

  digitalWrite(LED3, LOW);

  return 1;
}
