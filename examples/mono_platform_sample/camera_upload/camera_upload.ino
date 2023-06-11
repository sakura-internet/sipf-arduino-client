// libraries
#include "SipfClient.h"
#include <String>
#include <LTE.h>
#include <Camera.h>

// APN nameard

#define APP_LTE_APN "sakura" // replace your APN

/* APN authentication settings
 * Ignore these parameters when setting LTE_NET_AUTHTYPE_NONE.
 */

#define APP_LTE_USER_NAME "" // replace with your username
#define APP_LTE_PASSWORD ""  // replace with your password

// APN IP type
// #define APP_LTE_IP_TYPE (LTE_NET_IPTYPE_V4V6) // IP : IPv4v6
#define APP_LTE_IP_TYPE (LTE_NET_IPTYPE_V4) // IP : IPv4
// #define APP_LTE_IP_TYPE (LTE_NET_IPTYPE_V6) // IP : IPv6

// APN authentication type
// #define APP_LTE_AUTH_TYPE (LTE_NET_AUTHTYPE_CHAP) // Authentication : CHAP
// #define APP_LTE_AUTH_TYPE (LTE_NET_AUTHTYPE_PAP) // Authentication : PAP
#define APP_LTE_AUTH_TYPE (LTE_NET_AUTHTYPE_NONE) // Authentication : NONE

/* RAT to use
 * Refer to the cellular carriers information
 * to find out which RAT your SIM supports.
 * The RAT set on the modem can be checked with LTEModemVerification::getRAT().
 */

#define APP_LTE_RAT (LTE_NET_RAT_CATM) // RAT : LTE-M (LTE Cat-M1)
// #define APP_LTE_RAT (LTE_NET_RAT_NBIOT) // RAT : NB-IoT

LTE lteAccess;
LTEClient client;
SipfClient sipfClient;

/**
 * Print camera error message
 */

void printCameraError(enum CamErr err)
{
    Serial.print("Error: ");
    switch (err)
    {
    case CAM_ERR_NO_DEVICE:
        Serial.println("No Device");
        break;
    case CAM_ERR_ILLEGAL_DEVERR:
        Serial.println("Illegal device error");
        break;
    case CAM_ERR_ALREADY_INITIALIZED:
        Serial.println("Already initialized");
        break;
    case CAM_ERR_NOT_INITIALIZED:
        Serial.println("Not initialized");
        break;
    case CAM_ERR_NOT_STILL_INITIALIZED:
        Serial.println("Still picture not initialized");
        break;
    case CAM_ERR_CANT_CREATE_THREAD:
        Serial.println("Failed to create thread");
        break;
    case CAM_ERR_INVALID_PARAM:
        Serial.println("Invalid parameter");
        break;
    case CAM_ERR_NO_MEMORY:
        Serial.println("No memory");
        break;
    case CAM_ERR_USR_INUSED:
        Serial.println("Buffer already in use");
        break;
    case CAM_ERR_NOT_PERMITTED:
        Serial.println("Operation not permitted");
        break;
    default:
        break;
    }
}

bool initializeCamera()
{
    /* begin() without parameters means that
     * number of buffers = 1, 30FPS, QVGA, YUV 4:2:2 format */
    int err = theCamera.begin();
    if (err != CAM_ERR_SUCCESS)
    {
        printCameraError(err);
        return false;
    }

    /* Auto white balance configuration */

    Serial.println("Set Auto white balance parameter");
    err = theCamera.setAutoWhiteBalanceMode(CAM_WHITE_BALANCE_AUTO);
    if (err != CAM_ERR_SUCCESS)
    {
        printCameraError(err);
        return false;
    }

    /* Set parameters about still picture.
     * In the following case, QUADVGA and JPEG.
     */
    Serial.println("Set still picture format");
    err = theCamera.setStillPictureImageFormat(
        CAM_IMGSIZE_QUADVGA_H,
        CAM_IMGSIZE_QUADVGA_V,
        CAM_IMAGE_PIX_FMT_JPG,
        5);
    if (err != CAM_ERR_SUCCESS)
    {
        printCameraError(err);
        return false;
    }

    err = theCamera.setJPEGQuality(90);
    if (err != CAM_ERR_SUCCESS)
    {
        printCameraError(err);
        return false;
    }
    return true;
}

void setup()
{
    char apn[LTE_NET_APN_MAXLEN] = APP_LTE_APN;
    LTENetworkAuthType authtype = APP_LTE_AUTH_TYPE;
    char user_name[LTE_NET_USER_MAXLEN] = APP_LTE_USER_NAME;
    char password[LTE_NET_PASSWORD_MAXLEN] = APP_LTE_PASSWORD;

    // initialize serial communications and wait for port to open:
    Serial.begin(115200);
    while (!Serial)
    {
        ; // wait for serial port to connect. Needed for native USB port only
    }

    // initialize camera
    if (!initializeCamera())
    {
        Serial.println("Camera initialize error");
    }

    // Chech Access Point Name is empty
    if (strlen(APP_LTE_APN) == 0)
    {
        Serial.println("ERROR: This sketch doesn't have a APN information.");
        while (1)
        {
            sleep(1);
        }
    }
    Serial.println("=========== APN information ===========");
    Serial.print("Access Point Name  : ");
    Serial.println(apn);
    Serial.print("Authentication Type: ");
    Serial.println(authtype == LTE_NET_AUTHTYPE_CHAP ? "CHAP" : authtype == LTE_NET_AUTHTYPE_NONE ? "NONE"
                                                                                                  : "PAP");
    if (authtype != LTE_NET_AUTHTYPE_NONE)
    {
        Serial.print("User Name          : ");
        Serial.println(user_name);
        Serial.print("Password           : ");
        Serial.println(password);
    }

    while (true)
    {

        /* Power on the modem and Enable the radio function. */

        if (lteAccess.begin() != LTE_SEARCHING)
        {
            Serial.println("Could not transition to LTE_SEARCHING.");
            Serial.println("Please check the status of the LTE board.");
            for (;;)
            {
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
                             APP_LTE_IP_TYPE) == LTE_READY)
        {
            Serial.println("Attach succeeded.");
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

    sipfClient.begin(&client, 80);

    if (!sipfClient.authenticate())
    {
        Serial.println("SIFP Authentication error");
    }
}

void loop()
{
    static uint64_t utime = 0;
    static uint32_t counter = 0;

    bool res;

    String filename = "image.jpg";

    // Capture Image
    CamImage img = theCamera.takePicture();
    if (img.isAvailable())
    {
        Serial.print("Image size: ");
        Serial.println(img.getImgSize());

        res = sipfClient.uploadFile(filename, img.getImgBuff(), img.getImgSize());
        if (res)
        {
            Serial.println("Upload OK");
        }
        else
        {
            Serial.println("Upload Error");
        }
    }
    else
    {
        Serial.println("Camera error");
    }

    uint8_t buf[32];
    filename.toCharArray(buf, sizeof(buf) - 1);
    SipfObjectObject objs[] = {
        {OBJ_TYPE_UINT32, 0x01, OBJ_SIZE_UINT32, (uint8_t *)&counter},
        {OBJ_TYPE_STR_UTF8, 0x02, filename.length(), (uint8_t *)buf},

    };
    counter++;

    uint64_t otid = sipfClient.uploadObjects(uint64_t(0), objs, sizeof(objs) / sizeof(SipfObjectObject));
    if (otid != 0)
    {
        Serial.print("OTID: 0x");
        Serial.println(otid, HEX);
    }

    sleep(60);
}
