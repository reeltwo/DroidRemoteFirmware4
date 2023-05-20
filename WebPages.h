///////////////////////////////////////////////////////////////////////////////
//
// Web Configuration Interface for RDH
// 
///////////////////////////////////////////////////////////////////////////////

#ifdef USE_WIFI_WEB

////////////////////////////////

String wifiSSID;
String wifiPass;
bool wifiAP;

WElement wifiContents[] = {
    W1("WiFi Setup"),
    WCheckbox("WiFi Enabled", "enabled",
        []() { return wifiEnabled; },
        [](bool val) { wifiEnabled = val; } ),
    WHR(),
    WCheckbox("Access Point", "apmode",
        []() { return (wifiAP = preferences.getBool(PREFERENCE_WIFI_AP, WIFI_ACCESS_POINT)); },
        [](bool val) { wifiAP = val; } ),
    WTextField("WiFi:", "wifi",
        []()->String { return (wifiSSID = preferences.getString(PREFERENCE_WIFI_SSID, getHostName())); },
        [](String val) { wifiSSID = val; } ),
    WPassword("Password:", "password",
        []()->String { return (wifiPass = preferences.getString(PREFERENCE_WIFI_PASS, WIFI_AP_PASSPHRASE)); },
        [](String val) { wifiPass = val; } ),
    WHR(),
    WButton("Save", "save", []() {
        DEBUG_PRINTLN("WiFi Changed");
        preferences.putBool(PREFERENCE_WIFI_ENABLED, wifiEnabled);
        preferences.putBool(PREFERENCE_WIFI_AP, wifiAP);
        preferences.putString(PREFERENCE_WIFI_SSID, wifiSSID);
        preferences.putString(PREFERENCE_WIFI_PASS, wifiPass);
        reboot();
    }),
    WHorizontalAlign(),
    WButton("Home", "home", "/"),
    WVerticalAlign(),
    rseriesSVG
};

////////////////////////////////

String remoteHostName;
String remoteSecret;

WElement remoteContents[] = {
    W1("Droid Remote Setup"),
    WTextField("Device Name:", "hostname",
        []()->String { return (remoteHostName = preferences.getString(PREFERENCE_REMOTE_HOSTNAME, SMQ_HOSTNAME)); },
        [](String val) { remoteHostName = val; } ),
    WPassword("Secret:", "secret",
        []()->String { return (remoteSecret = preferences.getString(PREFERENCE_REMOTE_SECRET, SMQ_SECRET)); },
        [](String val) { remoteSecret = val; } ),
    WButton("Save", "save", []() {
        DEBUG_PRINTLN("Remote Changed");
        preferences.putString(PREFERENCE_REMOTE_HOSTNAME, remoteHostName);
        preferences.putString(PREFERENCE_REMOTE_SECRET, remoteSecret);
        reboot();
    }),
    WHorizontalAlign(),
    WButton("Home", "home", "/"),
    WVerticalAlign(),
    rseriesSVG
};

////////////////////////////////

WElement firmwareContents[] = {
    W1("Firmware Setup"),
    WFirmwareFile("Firmware:", "firmware"),
    WFirmwareUpload("Reflash", "firmware"),
    WLabel("Current Firmware Build Date:", "label"),
    WLabel(__DATE__, "date"),
#ifdef BUILD_VERSION
    WHRef(BUILD_VERSION, "Sources"),
#endif
    WButton("Clear Prefs", "clear", []() {
        DEBUG_PRINTLN("Clear all preference settings");
        preferences.clear();
    }),
    WHorizontalAlign(),
    WButton("Reboot", "reboot", []() {
        reboot();
    }),
    WHorizontalAlign(),
    WButton("Home", "home", "/"),
    WVerticalAlign(),
    rseriesSVG
};

////////////////////////////////

WMenuData mainMenu[] = {
    { "WiFi", "/wifi" },
    { "Remote", "/remote" },
    { "Firmware", "/firmware" },
    { "Back", "/" }
};

WElement mainContents[] = {
    WVerticalMenu("menu", mainMenu, SizeOfArray(mainMenu)),
    rseriesSVG
};


WPage pages[] = {
    WPage("/", mainContents, SizeOfArray(mainContents)),
      WPage("/wifi", wifiContents, SizeOfArray(wifiContents)),
      WPage("/remote", remoteContents, SizeOfArray(remoteContents)),
      WPage("/firmware", firmwareContents, SizeOfArray(firmwareContents)),
        WUpload("/upload/firmware",
            [](Client& client)
            {
                if (Update.hasError())
                    client.println("HTTP/1.0 200 FAIL");
                else
                    client.println("HTTP/1.0 200 OK");
                client.println("Content-type:text/html");
                client.println("Vary: Accept-Encoding");
                client.println();
                client.println();
                client.stop();
                if (!Update.hasError())
                {
                    reboot();
                    delay(1000);
                    preferences.end();
                    ESP.restart();
                }
            #ifdef USE_OTA
                otaInProgress = false;
            #endif
            },
            [](WUploader& upload)
            {
                if (upload.status == UPLOAD_FILE_START)
                {
                    otaInProgress = true;
                    unmountFileSystems();
                    Serial.printf("Update: %s\n", upload.filename.c_str());
                    if (!Update.begin(upload.fileSize))
                    {
                        //start with max available size
                        Update.printError(Serial);
                    }
                }
                else if (upload.status == UPLOAD_FILE_WRITE)
                {
                    float range = (float)upload.receivedSize / (float)upload.fileSize;
                    DEBUG_PRINTLN("Received: "+String(range*100)+"%");
                   /* flashing firmware to ESP*/
                    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
                    {
                        Update.printError(Serial);
                    }
                }
                else if (upload.status == UPLOAD_FILE_END)
                {
                    DEBUG_PRINTLN("GAME OVER");
                    if (Update.end(true))
                    {
                        //true to set the size to the current progress
                        Serial.printf("Update Success: %u\nRebooting...\n", upload.receivedSize);
                    }
                    else
                    {
                        Update.printError(Serial);
                    }
                }
            })
};

WifiWebServer<10,SizeOfArray(pages)> webServer(pages, wifiAccess);
#endif
