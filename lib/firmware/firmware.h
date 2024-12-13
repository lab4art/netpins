#pragma once

#include <WiFi.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFiClientSecure.h>
#include <freertos/queue.h>

QueueHandle_t firmwareUpdateResultQueue;

enum FirmwareUpdateStatus {
    STARTED,
    NO_UPDATES,
    SUCCESS,
    FAILED
};

struct FirmwareUpdateResult {
    FirmwareUpdateStatus status;
    String message;
};

void emptyQueue(QueueHandle_t queue) {
    FirmwareUpdateResult* result;
    while (xQueueReceive(firmwareUpdateResultQueue, &result, 0) == pdTRUE) {
        // do nothing
    }
}

FirmwareUpdateResult* lastResult = new FirmwareUpdateResult();

void update_started() {
    Log.noticeln("HTTP update process started");
    lastResult->status = FirmwareUpdateStatus::STARTED;
    lastResult->message = "Update started it takes about 30 sec ...";
    emptyQueue(firmwareUpdateResultQueue);
    // Send the result to the queue, blocking for up to 15 seconds
    if (xQueueSend(firmwareUpdateResultQueue, &lastResult, pdMS_TO_TICKS(15000)) != pdPASS) {
        Log.errorln("Failed to send update result to queue within 15 seconds");
    }
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
    httpUpdate.onProgress(update_progress);
    httpUpdate.onError(update_error);
    httpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    httpUpdate.rebootOnUpdate(false);

    HTTPUpdateResult ret;
    if (spiffs) {
        Log.noticeln("Updating SPIFFS from: %s", url.c_str());
        ret = httpUpdate.updateSpiffs(*client, url);
    } else {
        Log.noticeln("Updating firmware from: %s", url.c_str());
        ret = httpUpdate.update(*client, url);
    }

    switch (ret) {
        case HTTP_UPDATE_FAILED:
            Log.errorln("HTTP_UPDATE_FAILED Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
            lastResult->status = FirmwareUpdateStatus::FAILED;
            lastResult->message = String("Update failed (") + httpUpdate.getLastError() + "): " + httpUpdate.getLastErrorString().c_str();
            break;

        case HTTP_UPDATE_NO_UPDATES:
            Log.noticeln("HTTP_UPDATE_NO_UPDATES");
            lastResult->status = FirmwareUpdateStatus::NO_UPDATES;
            lastResult->message = "No updates available.";
            break;

        case HTTP_UPDATE_OK:
            // When the update start the result is sent early otherwise the ESP crashes is it has an open request during the update process.
            // lastResult->status = FirmwareUpdateStatus::SUCCESS;
            // lastResult->message = "Update successful, rebooting ...";
            Log.noticeln("Firmware updated, rebooting ...");
            ESP.restart();
            break;

        default:
            Log.errorln("Unknown status: %d", ret);
            lastResult->status = FirmwareUpdateStatus::FAILED;
            lastResult->message = "Unknown error.";
            break;
    }

    emptyQueue(firmwareUpdateResultQueue);
    // Send the result to the queue, blocking for up to 15 seconds
    if (xQueueSend(firmwareUpdateResultQueue, &lastResult, pdMS_TO_TICKS(15000)) != pdPASS) {
        Log.errorln("Failed to send update result to queue within 15 seconds");
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
