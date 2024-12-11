#pragma once

#include <WiFi.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFiClientSecure.h>

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
    // do not include ssl if compiler flag is not set


    std::unique_ptr<WiFiClient> client;
    if (ENABLE_HTTPS_OTA) {
        if (url.startsWith("https://")) {
            std::unique_ptr<WiFiClientSecure> secureClient(new WiFiClientSecure);
            // secureClient->setCACert(github_root_ca);
            secureClient->setInsecure();
            client = std::move(secureClient);
        } else {
            client.reset(new WiFiClient);
        }
    } else {
        client.reset(new WiFiClient);
    }

    httpUpdate.onStart(update_started);
    httpUpdate.onEnd(update_finished);
    // httpUpdate.onProgress(update_progress);
    httpUpdate.onError(update_error);
    httpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    t_httpUpdate_return ret;
    if (spiffs) {
        Log.noticeln("Updating SPIFFS from: %s", url.c_str());
        ret = httpUpdate.updateSpiffs(*client, url);
        ESP.restart();
    } else {
        Log.noticeln("Updating firmware from: %s", url.c_str());
        ret = httpUpdate.update(*client, url);
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
