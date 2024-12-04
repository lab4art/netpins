#pragma once

#include <WiFi.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>


void update_started() {
    Log.noticeln("HTTP update process started");
}

void update_finished() {
    Log.noticeln("HTTP update process finished.");
}

void update_progress(int cur, int total) {
    Log.noticeln("HTTP update process at %d of %d bytes...", cur, total);
}

void update_error(int err) {
    Log.noticeln("HTTP update fatal error code %d", err);
}

void firmwareUpdate(String url, bool spiffs = false) {
    // HTTPClient http;
    WiFiClient client;
    t_httpUpdate_return ret;
    if (spiffs) {
        Log.noticeln("Updating SPIFFS from: %s", url.c_str());
        ret = httpUpdate.updateSpiffs(client, url);
        ESP.restart();
    } else {
        Log.noticeln("Updating firmware from: %s", url.c_str());
        ret = httpUpdate.update(client, url);
        // restart called internally
    }
    switch (ret) {
        case HTTP_UPDATE_FAILED:
        Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
        break;

        case HTTP_UPDATE_NO_UPDATES:
        Serial.println("HTTP_UPDATE_NO_UPDATES");
        break;

        case HTTP_UPDATE_OK:
        Serial.println("HTTP_UPDATE_OK");
        break;
    }
}

struct FirmwareUpdateParams {
    String url;
    bool spiffs;
};

void firmwareUpdateTask(void * pvParameters) {
  // Cast the parameters
  FirmwareUpdateParams* params = static_cast<FirmwareUpdateParams*>(pvParameters);
  firmwareUpdate(params->url, params->spiffs);
  delete params;
  vTaskDelete(NULL);
}
