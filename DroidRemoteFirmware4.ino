/*
 * ------------------------------------------------------------------------
 * DroidRemoteFirmware32 (https://github.com/reeltwo/DroidRemoteFirmware32)
 * ------------------------------------------------------------------------
 * Written by Mimir Reynisson (skelmir)
 *
 * DroidRemoteFirmware32 is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * DroidRemoteFirmware32 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with DroidRemoteFirmware32; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

///////////////////////////////////

#if __has_include("build_version.h")
#include "build_version.h"
#endif

#if __has_include("reeltwo_build_version.h")
#include "reeltwo_build_version.h"
#endif

///////////////////////////////////
// CONFIGURABLE OPTIONS
///////////////////////////////////

#define USE_DEBUG                       // Define to enable debug diagnostic
//#define USE_SMQDEBUG
#define SMQ_HOSTNAME                    "Remote"
#define SMQ_SECRET                      "Astromech"
#define USE_WIFI
#define USE_SPIFFS

///////////////////////////////////

#ifdef USE_WIFI
#define USE_MDNS
#define USE_OTA
#define USE_WIFI_WEB
#endif

///////////////////////////////////

#ifdef USE_WIFI
#define WIFI_ENABLED                    false
// Set these to your desired credentials.
#define WIFI_AP_NAME                    "DroidRemote"
#define WIFI_AP_PASSPHRASE              "Astromech"
#define WIFI_ACCESS_POINT               true  /* true if access point: false if joining existing wifi */
#endif

///////////////////////////////////

#include "pin-map.h"

///////////////////////////////////

#include "ReelTwoSMQ32.h"
#include "TFT_eSPI.h"
#include "core/AnimatedEvent.h"
#include "core/PinManager.h"
#include "core/StringUtils.h"
#include "core/PushButton.h"
#include "core/BatteryMonitor.h"
#include "encoder/AnoRotaryEncoder.h"
#include <driver/rtc_io.h>

///////////////////////////////////

#include "lvgl.h"

///////////////////////////////////

#ifdef USE_OTA
#include <ArduinoOTA.h>
#endif
#ifdef USE_SPIFFS
#include <SPIFFS.h>
#define USE_FS SPIFFS
#elif defined(USE_FATFS)
#include <FFat.h>
#define USE_FS FFat
#elif defined(USE_LITTLEFS)
#include <LITTLEFS.h>
#define USE_FS LITTLEFS
#endif
#include <FS.h>

///////////////////////////////////

PinManager sPinManager;

///////////////////////////////////

#define SCREEN_WIDTH            320     // OLED display width, in pixels
#define SCREEN_HEIGHT           170     // OLED display height, in pixels
#define SCREEN_BUFFER_SIZE      (SCREEN_WIDTH * SCREEN_HEIGHT)
#define KEY_REPEAT_RATE_MS      500     // Key repeat rate in milliseconds
#define SCREEN_SLEEP_TIMER      30*1000 // Turn off display if idle for 30 seconds
#define DEEP_SLEEP_TIMER        60*1000 // Enter deep sleep if idle and battery powered for longer than
#define BATTERY_REFRESH_RATE    1000    // Check battery level every second

#define LONGPRESS_DELAY         2000
#define SPLASH_SCREEN_DURATION  2000
#define STATUS_SCREEN_DURATION  1000

////////////////////////////////

TFT_eSPI tft = TFT_eSPI();
static lv_disp_drv_t disp_drv;      // contains callback functions
static lv_disp_draw_buf_t disp_buf; // contains internal graphic buffer(s) called draw buffer(s)
static lv_color_t *lv_disp_buf;
PushButton button1(PIN_BUTTON_1, true);
PushButton button2(PIN_BUTTON_2, true);

////////////////////////////////

LV_FONT_DECLARE(font_Astromech);
LV_FONT_DECLARE(lv_font_montserrat_32);
LV_FONT_DECLARE(lv_font_montserrat_48);

////////////////////////////////

static const lv_font_t* font_large = &lv_font_montserrat_48;
static const lv_font_t* font_normal = &lv_font_montserrat_32;

///////////////////////////////////

#ifdef USE_WIFI
#include "wifi/WifiAccess.h"
#endif
#ifdef USE_MDNS
#include <ESPmDNS.h>
#endif
#ifdef USE_WIFI_WEB
#include "wifi/WifiWebServer.h"
#include "web-images.h"
#endif

////////////////////////////////

#ifdef USE_WIFI
WifiAccess wifiAccess;
bool wifiEnabled;
bool wifiActive;
TaskHandle_t eventTask;
#endif

#ifdef USE_OTA
bool otaInProgress;
#endif

///////////////////////////////////////////////////////////////////////////////

#ifdef USE_WIFI
String getHostName()
{
    String mac = wifiAccess.getMacAddress();
    String hostName = mac.substring(mac.length()-5, mac.length());
    hostName.remove(2, 1);
    hostName = WIFI_AP_NAME+String("-")+hostName;
    return hostName;
}
#endif

////////////////////////////////

enum ScreenID
{
    kInvalid = -1,
    kSplashScreen = 0,
    kMainScreen = 1,
    kRemoteScreen,
    kPairingScreen,
    kStatusScreen
};

#include "menus/CommandScreen.h"
CommandScreenHandler* getScreenHandler();

#include "menus/CommandScreenDisplay.h"
#include "CommandDisplayLVGL.h"

CommandDisplayLVGL sDisplayLVGL;
CommandScreenDisplay<CommandDisplayLVGL> sDisplay(sDisplayLVGL, sPinManager,[]() {
    sDisplay.setEnabled(true);
    // Put display to sleep after 30 seconds (default is 15 seconds)
    sDisplay.setScreenBlankDelay(SCREEN_SLEEP_TIMER);
    return true;
});

CommandScreenHandler* getScreenHandler()
{
    return &sDisplay;
}

///////////////////////////////////////////////////////////////////////////////

#include "menus/RemoteScreen.h"
#include "menus/MainScreen.h"
#include "menus/SplashScreen.h"
#include "menus/PairingScreen.h"
#include "menus/StatusScreen.h"

////////////////////////////////

#include <Preferences.h>
Preferences preferences;
#define PREFERENCE_REMOTE_HOSTNAME      "rhost"
#define PREFERENCE_REMOTE_SECRET        "rsecret"
#define PREFERENCE_REMOTE_PAIRED        "rpaired"
#define PREFERENCE_REMOTE_LMK           "rlmk"
#define PREFERENCE_WIFI_ENABLED         "wifi"
#define PREFERENCE_WIFI_SSID            "ssid"
#define PREFERENCE_WIFI_PASS            "pass"
#define PREFERENCE_WIFI_AP              "ap"

////////////////////////////////

static void astro_lvgl_flush_cb(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p)
{
    uint32_t w = ( area->x2 - area->x1 + 1 );
    uint32_t h = ( area->y2 - area->y1 + 1 );

    tft.startWrite();
    tft.setAddrWindow( area->x1, area->y1, w, h );
    tft.pushColors( ( uint16_t * )&color_p->full, w * h, false );
    tft.endWrite();

    lv_disp_flush_ready( disp );
}

////////////////////////////////

BatteryMonitor sBatteryMonitor(PIN_BAT_VOLT);

////////////////////////////////

RTC_DATA_ATTR bool sBooted = false;

void deepSleep()
{
    if (wifiActive)
        WiFi.disconnect();
    digitalWrite(PIN_POWER_ON, LOW);
    digitalWrite(PIN_LCD_BL, LOW);

    gpio_num_t wakeUp = gpio_num_t(PIN_WAKEUP_FROM_SLEEP);
    esp_sleep_enable_ext0_wakeup(wakeUp, 0); // 1 = High, 0 = Low
    rtc_gpio_pullup_en(wakeUp);
    rtc_gpio_pulldown_dis(wakeUp);
    esp_deep_sleep_start();
}

////////////////////////////////

void setupPower()
{
    pinMode(PIN_POWER_ON, OUTPUT);
    digitalWrite(PIN_POWER_ON, HIGH);

    pinMode(PIN_LCD_BL, OUTPUT);
    digitalWrite(PIN_LCD_BL, LOW);
}

////////////////////////////////


lv_obj_t* lv_display_gif_load(fs::File fd)
{
    if (!fd || fd.isDirectory())
    {
        DEBUG_PRINTLN("File not found");
        return nullptr;
    }
    size_t imageSize = fd.available();
    printf("imageSize: %d\n", imageSize);
    char* imageData = (char*)ps_malloc(imageSize);
    printf("imageData: %p\n", imageData);
    if (imageData != nullptr)
    {
        size_t readBytes = fd.readBytes(imageData, imageSize);
        printf("readBytes: %d\n", readBytes);
        fd.close();
        if (readBytes == imageSize)
        {
            lv_img_dsc_t image_desc;
            image_desc.data_size = imageSize;
            image_desc.data = (const uint8_t*)imageData;
            lv_obj_t* img = lv_img_create(lv_scr_act());
            lv_img_set_src(img, &image_desc);
            return img;

            // static lv_img_dsc_t splash_gif;
            // memset(&splash_gif, '\0', sizeof(splash_gif));
            // // splash_gif.header.cf = LV_IMG_CF_RAW_CHROMA_KEYED,
            // splash_gif.data_size = imageSize;
            // splash_gif.data = (const uint8_t*)imageData;
            // lv_obj_t* splash_img = lv_gif_create(lv_scr_act());
            // // lv_obj_center(splash_img);
            // lv_gif_set_src(splash_img, &splash_gif);
            // printf("splash_img: %p\n", splash_img);
            // return splash_img;
        }
        free(imageData);
    }
    else
    {
        DEBUG_PRINTLN("Failed to allocate space for image");
    }
    return nullptr;
}

////////////////////////////////

bool lv_display_gif_until_complete(fs::File fd)
{
    if (!fd || fd.isDirectory())
    {
        DEBUG_PRINTLN("File not found");
        return false;
    }
    size_t imageSize = fd.available();
    char* imageData = (char*)ps_malloc(imageSize);
    if (imageData != nullptr)
    {
        size_t readBytes = fd.readBytes(imageData, imageSize);
        fd.close();
        if (readBytes == imageSize)
        {
            static bool sSplashFinished;
            lv_img_dsc_t splash_gif;
            memset(&splash_gif, '\0', sizeof(splash_gif));
            splash_gif.header.cf = LV_IMG_CF_RAW_CHROMA_KEYED,
            splash_gif.data_size = imageSize;
            splash_gif.data = (const uint8_t*)imageData;
            lv_obj_t* splash_img = lv_gif_create(lv_scr_act());
            lv_obj_center(splash_img);
            lv_gif_set_src(splash_img, &splash_gif);
            ((lv_gif_t*)splash_img)->gif->loop_count = 1;
            lv_obj_add_event_cb(splash_img, [](lv_event_t* evt){
                sSplashFinished = true;
            }, LV_EVENT_READY, NULL);
            // Spin until gif is finished
            while (!sSplashFinished)
            {
                lv_timer_handler();
                delay(1);
            }
            lv_obj_del(splash_img);
            return true;
        }
        free(imageData);
    }
    else
    {
        DEBUG_PRINTLN("Failed to allocate space for image");
    }
    return false;
}


void setupLCD()
{
    digitalWrite(PIN_POWER_ON, HIGH);

    lv_init();
    lv_disp_buf = (lv_color_t *)heap_caps_malloc(SCREEN_BUFFER_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    memset(lv_disp_buf, '\0', SCREEN_BUFFER_SIZE * sizeof(lv_color_t));

    tft.begin();          /* TFT init */
    tft.setRotation( 3 ); /* Landscape orientation, flipped */

    lv_disp_draw_buf_init(&disp_buf, lv_disp_buf, NULL, SCREEN_BUFFER_SIZE);

    /*Initialize the display*/
    lv_disp_drv_init(&disp_drv);
    /*Change the following line to your display resolution*/
    disp_drv.hor_res = SCREEN_WIDTH;
    disp_drv.ver_res = SCREEN_HEIGHT;
    disp_drv.flush_cb = astro_lvgl_flush_cb;
    disp_drv.draw_buf = &disp_buf;
    lv_disp_drv_register(&disp_drv);
    digitalWrite(PIN_LCD_BL, HIGH);

    if (!sBooted)
    {
        // Only draw splash screen on first boot
        lv_display_gif_until_complete(SPIFFS.open("/splash.gif"));
    }

    button1.attachClick([]() {
        preferences.putBool(PREFERENCE_WIFI_ENABLED, !wifiEnabled);
        sStatusScreen.showStatus(
            ((wifiEnabled) ? "WiFi Disabled" : "WiFi Enabled"),
            []() {
                reboot();
            }
        );
    });
    button1.setPressTicks(LONGPRESS_DELAY);
    button1.attachLongPressStart([]() {
        sStatusScreen.showStatus("Sleeping", []() {
            tft.fillScreen(0x0000);
            DEBUG_PRINTLN("Entering deep sleep");
            deepSleep();
        });
    });
    button2.attachClick([]() {
        if (SMQ::isPairing())
        {
            DEBUG_PRINTLN("Pairing Stopped ...");
            sStatusScreen.showStatus("Pairing Stopped");
            SMQ::stopPairing();
        }
        else
        {
            DEBUG_PRINTLN("Pairing Started ...");
            SMQ::startPairing();
            sDisplay.switchToScreen(kPairingScreen);
        }
    });
    button2.setPressTicks(LONGPRESS_DELAY);
    button2.attachLongPressStart([]() {
        DEBUG_PRINTLN("Unpairing");
        if (preferences.remove(PREFERENCE_REMOTE_PAIRED))
        {
            DEBUG_PRINTLN("Unpairing Success...");
            sStatusScreen.showStatus("Unpaired", []() {
                reboot();
            });
        }
        else
        {
            DEBUG_PRINTLN("Not Paired...");
        }
    });

    if (sDisplay.begin())
    {
        sDisplay.clearDisplay();
    }
}

void print_wakeup_reason()
{
    esp_sleep_wakeup_cause_t wakeup_reason =
        esp_sleep_get_wakeup_cause();
    switch(wakeup_reason)
    {
        case ESP_SLEEP_WAKEUP_EXT0:
            DEBUG_PRINTLN("Wakeup caused by external signal using RTC_IO");
            break;
        case ESP_SLEEP_WAKEUP_EXT1:
            DEBUG_PRINTLN("Wakeup caused by external signal using RTC_CNTL");
            break;
        case ESP_SLEEP_WAKEUP_TIMER:
            DEBUG_PRINTLN("Wakeup caused by timer");
            break;
        case ESP_SLEEP_WAKEUP_TOUCHPAD:
            DEBUG_PRINTLN("Wakeup caused by touchpad");
            break;
        case ESP_SLEEP_WAKEUP_ULP:
            DEBUG_PRINTLN("Wakeup caused by ULP program");
            break;
        default:
            DEBUG_PRINTF("Wakeup was not caused by deep sleep: %d\n", wakeup_reason);
            break;
    }
}

///////////////////////////////////

bool mountReadOnlyFileSystem()
{
#ifdef USE_SPIFFS
    return (SPIFFS.begin(true));
#endif
    return false;
}

void unmountFileSystems()
{
#ifdef USE_SPIFFS
    SPIFFS.end();
#endif
}

///////////////////////////////////////////////////////////////////////////////

#ifdef USE_WIFI_WEB
#include "WebPages.h"
#endif

////////////////////////////////

void setup()
{
    setupPower();

    REELTWO_READY();

#ifdef USE_SPIFFS
    if (!mountReadOnlyFileSystem())
    {
        DEBUG_PRINTLN("Failed to mount filesystem");
    }
#endif
    if (!sBooted)
    {
        // Print git info on startup only
        PrintReelTwoInfo(Serial, "Droid Remote");
    }
    else
    {
        // Delay to allow USB uart to reconnect
        delay(1000);
    }

    if (!preferences.begin("droidremote", false))
    {
        DEBUG_PRINTLN("Failed to init prefs");
    }
#ifdef USE_WIFI
    wifiEnabled = wifiActive = preferences.getBool(PREFERENCE_WIFI_ENABLED, WIFI_ENABLED);
#endif

    SetupEvent::ready();

    //Puts ESP in STATION MODE
    WiFi.mode(WIFI_STA);
    if (SMQ::init(preferences.getString(PREFERENCE_REMOTE_HOSTNAME, SMQ_HOSTNAME),
                    preferences.getString(PREFERENCE_REMOTE_SECRET, SMQ_SECRET)))
    {
        SMQLMK key;
        if (preferences.getBytes(PREFERENCE_REMOTE_LMK, &key, sizeof(SMQLMK)) == sizeof(SMQLMK))
        {
            SMQ::setLocalMasterKey(&key);
        }

        SMQAddressKey pairedHosts[SMQ_MAX_PAIRED_HOSTS];
        size_t pairedHostsSize = preferences.getBytesLength(PREFERENCE_REMOTE_PAIRED);
        unsigned numHosts = pairedHostsSize / sizeof(pairedHosts[0]);
        printf("numHosts: %d\n", numHosts);
        Serial.print("WiFi.macAddress() : "); Serial.println(WiFi.macAddress());
        if (numHosts != 0)
        {
            if (preferences.getBytes(PREFERENCE_REMOTE_PAIRED, pairedHosts, pairedHostsSize) == pairedHostsSize)
            {
                SMQ::addPairedHosts(numHosts, pairedHosts);
            }
        }
        printf("Droid Gizmo %s:%s\n",
            preferences.getString(PREFERENCE_REMOTE_HOSTNAME, SMQ_HOSTNAME).c_str(),
                preferences.getString(PREFERENCE_REMOTE_SECRET, SMQ_SECRET).c_str());
        SMQ::setHostPairingCallback([](SMQHost* host) {
            if (host == nullptr)
            {
                // Pairing cancelled or timed out
                printf("Pairing timed out\n");
            }
            else
            {
                switch (SMQ::masterKeyExchange(&host->fLMK))
                {
                    case -1:
                        printf("Pairing Stopped\n");
                        SMQ::stopPairing();
                        return;
                    case 1:
                        // Save new master key
                        SMQLMK lmk;
                        SMQ::getLocalMasterKey(&lmk);
                        printf("Saved new master key\n");
                        preferences.putBytes(PREFERENCE_REMOTE_LMK, &lmk, sizeof(lmk));
                        break;
                    case 0:
                        // We had the master key
                        break;
                }
                printf("Pairing: %s [%s]\n", host->getHostName().c_str(), host->fLMK.toString().c_str());
                if (SMQ::addPairedHost(&host->fAddr, &host->fLMK))
                {
                    SMQAddressKey pairedHosts[SMQ_MAX_PAIRED_HOSTS];
                    unsigned numHosts = SMQ::getPairedHostCount();
                    if (SMQ::getPairedHosts(pairedHosts, numHosts) == numHosts)
                    {
                        preferences.putBytes(PREFERENCE_REMOTE_PAIRED,
                            pairedHosts, numHosts*sizeof(pairedHosts[0]));
                        printf("Pairing Success\n");
                    }
                }
                printf("Pairing Stopped\n");
                SMQ::stopPairing();
            }
        });
        SMQ::setHostDiscoveryCallback([](SMQHost* host) {
            if (host->hasTopic("BUTTON"))
            {
                printf("Discovered: %s\n", host->getHostName().c_str());
                sMainScreen.addHost(host->getHostName(), host->getHostAddress());
            }
        });

        SMQ::setHostLostCallback([](SMQHost* host) {
            printf("Lost: %s\n", host->getHostName().c_str());
            sMainScreen.removeHost(host->getHostAddress());
        });
    }
    setupLCD();
    Serial.println("READY");
    if (sBooted)
    {
        print_wakeup_reason();
        // If waking up from sleep skip splash screen
        sDisplay.switchToScreen(kMainScreen);
    }
    // Set the refresh callback and frequency for the battery monitor
    sBatteryMonitor.setRefreshCallback(BATTERY_REFRESH_RATE, []() {
        unsigned milliVolt = sBatteryMonitor.voltage();
        sDisplayLVGL.refreshBatteryVoltage(sBatteryMonitor.level(milliVolt), milliVolt);
        sDisplayLVGL.setWiFiStatus(wifiEnabled);

        if (sDisplay.getScreenSleepDuration() > DEEP_SLEEP_TIMER &&
            sBatteryMonitor.isBatteryPowered(milliVolt))
        {
            // Sleep duration time out and we are running on battery so enter deep sleep
            printf("DEEP SLEEP\n");
            deepSleep();
        }
    });
#ifdef USE_WIFI
    if (wifiEnabled)
    {
    #ifdef USE_WIFI_WEB
        // In preparation for adding WiFi settings web page
        wifiAccess.setNetworkCredentials(
            preferences.getString(PREFERENCE_WIFI_SSID, getHostName()),
            preferences.getString(PREFERENCE_WIFI_PASS, WIFI_AP_PASSPHRASE),
            preferences.getBool(PREFERENCE_WIFI_AP, WIFI_ACCESS_POINT),
            preferences.getBool(PREFERENCE_WIFI_ENABLED, WIFI_ENABLED));
        wifiAccess.notifyWifiConnected([](WifiAccess &wifi) {
        #ifdef STATUSLED_PIN
            statusLED.setMode(sCurrentMode = kWifiMode);
        #endif
            Serial.print("Connect to http://"); Serial.println(wifi.getIPAddress());
        #ifdef USE_MDNS
            // No point in setting up mDNS if R2 is the access point
            if (!wifi.isSoftAP() && webServer.enabled())
            {
                String hostName = getHostName();
                Serial.print("Host name: "); Serial.println(hostName);
                if (!MDNS.begin(hostName.c_str()))
                {
                    DEBUG_PRINTLN("Error setting up MDNS responder!");
                }
            }
        #endif
        });
    #endif
    #ifdef USE_OTA
        ArduinoOTA.onStart([]()
        {
            String type;
            if (ArduinoOTA.getCommand() == U_FLASH)
            {
                type = "sketch";
            }
            else // U_SPIFFS
            {
                type = "filesystem";
            }
            DEBUG_PRINTLN("OTA START");
        })
        .onEnd([]()
        {
            DEBUG_PRINTLN("OTA END");
        })
        .onProgress([](unsigned int progress, unsigned int total)
        {
            // float range = (float)progress / (float)total;
        })
        .onError([](ota_error_t error)
        {
            String desc;
            if (error == OTA_AUTH_ERROR) desc = "Auth Failed";
            else if (error == OTA_BEGIN_ERROR) desc = "Begin Failed";
            else if (error == OTA_CONNECT_ERROR) desc = "Connect Failed";
            else if (error == OTA_RECEIVE_ERROR) desc = "Receive Failed";
            else if (error == OTA_END_ERROR) desc = "End Failed";
            else desc = "Error: "+String(error);
            DEBUG_PRINTLN(desc);
        });
    #endif
    }
#endif
#ifdef USE_WIFI_WEB
    // For safety we will stop the motors if the web client is connected
    webServer.setConnect([]() {
        // Callback for each connected web client
        // DEBUG_PRINTLN("Hello");
    });
#endif

#if defined(USE_WIFI) || defined(USE_DROID_REMOTE) || defined(USE_LVGL_DISPLAY)
    xTaskCreatePinnedToCore(
          eventLoopTask,
          "Events",
          5000,    // shrink stack size?
          NULL,
          1,
          &eventTask,
          0);
#endif
    sBooted = true;
}

///////////////////////////////////////////////////////////////////////////////

SMQMESSAGE(LCD, {
    char buffer[200];
    uint8_t x = msg.get_uint8("x");
    uint8_t y = msg.get_uint8("y");
    bool invert = msg.get_boolean("invert");
    bool centered = msg.get_boolean("centered");
    uint8_t siz = msg.get_uint8("size");
    if (msg.get_string("text", buffer, sizeof(buffer)) != nullptr)
    {
        sRemoteScreen.drawCommand(x, y, invert, centered, siz, buffer);
    }
    else
    {
        printf("RECEIVED LCD (%d,%d) [i:%d,c:%d,s:%d] <null>\n", x, y, invert, centered, siz);
    }
})

///////////////////////////////////////////////////////////////////////////////

SMQMESSAGE(EXIT, {
    String addr = msg.getString("addr");
    sDisplay.switchToScreen(kMainScreen);
})

///////////////////////////////////////////////////////////////////////////////

#define CONSOLE_BUFFER_SIZE                 300
#define COMMAND_BUFFER_SIZE                 256

static bool sNextCommand;
static bool sProcessing;
static unsigned sPos;
static uint32_t sWaitNextSerialCommand;
static char sBuffer[CONSOLE_BUFFER_SIZE];
static bool sCmdNextCommand;
static char sCmdBuffer[COMMAND_BUFFER_SIZE];

static void runSerialCommand()
{
    sWaitNextSerialCommand = 0;
    sProcessing = true;
}

static void resetSerialCommand()
{
    sWaitNextSerialCommand = 0;
    sNextCommand = false;
    sProcessing = (sCmdBuffer[0] == ':');
    sPos = 0;
}

static void abortSerialCommand()
{
    sBuffer[0] = '\0';
    sCmdBuffer[0] = '\0';
    sCmdNextCommand = false;
    resetSerialCommand();
}

void reboot()
{
    printf("=================== REBOOTING\n");
    delay(1000);
    tft.fillScreen(0x0000);
    Serial.println("Restarting...");
    preferences.end();
    ESP.restart();
}

void processConfigureCommand(const char* cmd)
{
    if (startswith(cmd, "#DPFACTORY"))
    {
        preferences.clear();
        reboot();
    }
    else if (startswith(cmd, "#DRRESTART"))
    {
        reboot();
    }
    else if (startswith(cmd, "#DRNAME"))
    {
        String newName = String(cmd);
        if (preferences.getString(PREFERENCE_REMOTE_HOSTNAME, SMQ_HOSTNAME) != newName)
        {
            preferences.putString(PREFERENCE_REMOTE_HOSTNAME, cmd);
            printf("Changed.\n");
            reboot();
        }
    }
    else if (startswith(cmd, "#DRSECRET"))
    {
        String newSecret = String(cmd);
        if (preferences.getString(PREFERENCE_REMOTE_SECRET, SMQ_HOSTNAME) != newSecret)
        {
            preferences.putString(PREFERENCE_REMOTE_SECRET, newSecret);
            printf("Changed.\n");
            reboot();
        }
    }
    else if (startswith(cmd, "#DRCONFIG"))
    {
        printf("Name=%s\n", preferences.getString(PREFERENCE_REMOTE_HOSTNAME, SMQ_HOSTNAME).c_str());
        printf("Secret=%s\n", preferences.getString(PREFERENCE_REMOTE_HOSTNAME, SMQ_SECRET).c_str());
    }
    else if (startswith(cmd, "#DRMASTERKEY0"))
    {
        if (preferences.remove(PREFERENCE_REMOTE_LMK))
        {
            printf("Master Key Deleted.\n");
        }
        else
        {
            printf("No master key.\n");
        }
    }
    else if (startswith(cmd, "#DRMASTERKEY"))
    {
        SMQLMK lmk;
        printf("New Master Key Generated. All devices must be paired again\n");
        SMQ::createLocalMasterKey(&lmk);
        preferences.putBytes(PREFERENCE_REMOTE_LMK, &lmk, sizeof(lmk));
        SMQ::setLocalMasterKey(&lmk);
    }
    else if (startswith(cmd, "#DRPAIR"))
    {
        printf("Pairing Started ...\n");
        SMQ::startPairing();
    }
    else if (startswith(cmd, "#DRUNPAIR"))
    {
        if (preferences.remove(PREFERENCE_REMOTE_PAIRED))
        {
            printf("Unpairing Success...\n");
            reboot();
        }
        else
        {
            printf("Not Paired...\n");
        }
    }
    else
    {
        Serial.println(F("Invalid"));
    }
}

bool processDroidRemoteCommand(const char* cmd)
{
    switch (*cmd++)
    {
        default:
            // INVALID
            return false;
    }
    return true;
}

bool processCommand(const char* cmd, bool firstCommand)
{
    sWaitNextSerialCommand = 0;
    if (*cmd == '\0')
        return true;
    if (!firstCommand)
    {
        if (cmd[0] != ':')
        {
            Serial.println(F("Invalid"));
            return false;
        }
        return processDroidRemoteCommand(cmd+1);
    }
    switch (cmd[0])
    {
        case ':':
            if (cmd[1] == 'D' && cmd[2] == 'R')
                return processDroidRemoteCommand(cmd+3);
            break;
        case '#':
            processConfigureCommand(cmd);
            return true;
        default:
            Serial.println(F("Invalid"));
            break;
    }
    return false;
}

///////////////////////////////////////////////////////////////////////////////

#if defined(USE_WIFI)
void eventLoopTask(void* )
{
    for (;;)
    {
        if (wifiActive)
        {
        #ifdef USE_OTA
            ArduinoOTA.handle();
        #endif
        #ifdef USE_WIFI_WEB
            webServer.handle();
        #endif
        }
        vTaskDelay(1);
    }
}
#endif

void loop()
{
    lv_timer_handler();
    AnimatedEvent::process();

    sDisplay.process();
    // if (sNextBatteryCheck < millis())
    // {
    //     sDisplayLVGL.refreshBatteryVoltage((analogRead(PIN_BAT_VOLT) * 2 * 3.3 * 1000) / 4096);
    //     sNextBatteryCheck = millis() + 1000;
    // }

    SMQ::process();

    if (Serial.available())
    {
        int ch = Serial.read();
        if (ch == 0x0A || ch == 0x0D)
        {
            runSerialCommand();
        }
        else if (sPos < SizeOfArray(sBuffer)-1)
        {
            sBuffer[sPos++] = ch;
            sBuffer[sPos] = '\0';
        }
    }
    if (sProcessing && millis() > sWaitNextSerialCommand)
    {
        if (sCmdBuffer[0] == ':')
        {
            char* end = strchr(sCmdBuffer+1, ':');
            if (end != nullptr)
                *end = '\0';
            if (!processCommand(sCmdBuffer, !sCmdNextCommand))
            {
                // command invalid abort buffer
                DEBUG_PRINT(F("Unrecognized: ")); DEBUG_PRINTLN(sCmdBuffer);
                abortSerialCommand();
                end = nullptr;
            }
            if (end != nullptr)
            {
                *end = ':';
                strcpy(sCmdBuffer, end);
                DEBUG_PRINT(F("REMAINS: "));
                DEBUG_PRINTLN(sCmdBuffer);
                sCmdNextCommand = true;
            }
            else
            {
                sCmdBuffer[0] = '\0';
                sCmdNextCommand = false;
            }
        }
        else if (sBuffer[0] == ':')
        {
            char* end = strchr(sBuffer+1, ':');
            if (end != nullptr)
                *end = '\0';
            if (!processCommand(sBuffer, !sNextCommand))
            {
                // command invalid abort buffer
                DEBUG_PRINT(F("Unrecognized: ")); DEBUG_PRINTLN(sBuffer);
                abortSerialCommand();
                end = nullptr;
            }
            if (end != nullptr)
            {
                *end = ':';
                strcpy(sBuffer, end);
                sPos = strlen(sBuffer);
                DEBUG_PRINT(F("REMAINS: "));
                DEBUG_PRINTLN(sBuffer);
                sNextCommand = true;
            }
            else
            {
                resetSerialCommand();
                sBuffer[0] = '\0';
            }
        }
        else
        {
            processCommand(sBuffer, true);
            resetSerialCommand();
        }
    }
    vTaskDelay(1);
}
#pragma GCC diagnostic pop
