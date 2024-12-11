#pragma once

#include <ArduinoJson.h>
#include <WebServer.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include "wifiUtils.cpp"
#include <ArduinoLog.h>
#include <constants.h>
#include <settings.h>


class WebAdmin {
    private:
        AsyncWebServer* webServer = new AsyncWebServer(80);
        std::function<void()> onReceivedCallback = [](){};
        
        SettingsManager<Settings>* settingsManager;
        SettingsManager<DmxSettings>* dmxSettingsManager;
        std::function<void(JsonDocument&)> onSystemCommand = [](JsonDocument doc){};


    public:
        WebAdmin(
                SettingsManager<Settings>* settingsManager,
                SettingsManager<DmxSettings>* dmxSettingsManager,
                std::function<void(JsonDocument&)> onSystemCommand):
                settingsManager(settingsManager),
                dmxSettingsManager(dmxSettingsManager),
                onSystemCommand(onSystemCommand) {

            webServer->on("/sys-info", HTTP_GET, [this](AsyncWebServerRequest *request){
                this->onReceivedCallback();

                // std::string output;
                String output;
                JsonDocument doc;
                doc["firmware"] = FIRMWARE_VERSION;
                doc["mac"] = WifiUtils::macAddress;
                doc["ip"] = WiFi.localIP().toString();
                doc["hostname"] = this->settingsManager->getSettings().hostname;
                doc["uptime"] = std::to_string(millis());
                serializeJson(doc, output);

                // request->send(200, "application/json", String(output.c_str()));
                request->send(200, "application/json", output);
                // request->send(200, "text/html", response.c_str());
            });

            webServer->on("/", HTTP_GET, [](AsyncWebServerRequest *request){
                String path = "/admin.html.gz";
                File file = LittleFS.open(path, "r");
                if (!file) {
                    request->send(500, "text/plain", "Failed to open file");
                    return;
                }
                String htmlContent = file.readString();
                file.close();

                // request->send(200, "text/html", htmlContent);
                AsyncWebServerResponse *response = request->beginResponse(200, "text/html", htmlContent);
                response->addHeader("Content-encoding", "gzip");
                request->send(response);
            });
            // webServer->serveStatic("/script.js", LittleFS, "/script.js"); // TODO improvement
            webServer->on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request){
                String path = "/script.js.gz";
                File file = LittleFS.open(path, "r");
                if (!file) {
                    request->send(500, "text/plain", "Failed to open file");
                    return;
                }
                String htmlContent = file.readString();
                file.close();
                // request->send(200, "application/javascript", htmlContent.c_str());
                AsyncWebServerResponse *response = request->beginResponse(200, "application/javascript", htmlContent);
                // response->addHeader("Content-Type", "text/html");
                response->addHeader("Content-encoding", "gzip");
                request->send(response);
            });

            webServer->on("/js-yaml-4.1.0.min.js", HTTP_GET, [](AsyncWebServerRequest *request){
                String path = "/js-yaml-4.1.0.min.js.gz";
                File file = LittleFS.open(path, "r");
                if (!file) {
                    request->send(500, "text/plain", "Failed to open file");
                    return;
                }
                String htmlContent = file.readString();
                file.close();
                AsyncWebServerResponse *response = request->beginResponse(200, "application/javascript", htmlContent);
                response->addHeader("Content-encoding", "gzip");
                request->send(response);
            });


            // HTTP_GET is used for convinience that users can use a browser without any plugin or form
            webServer->on("^\\/conf\\/(.+)$", HTTP_GET, [this](AsyncWebServerRequest *request){
                this->onReceivedCallback();
                String configName = request->pathArg(0);

                if (configName == "sys") {
                    request->send(200, "application/json", this->settingsManager->getSettings().asJson().c_str());
                } else if (configName == "dmx") {
                    request->send(200, "application/json", this->dmxSettingsManager->getSettings().asJson().c_str());
                } else {
                    request->send(404);
                }
            });

            webServer->on("^\\/system$", HTTP_POST, [this](AsyncWebServerRequest *request) {
                this->onReceivedCallback();
                // This callback after the last chunk of body data has been received
                request->send(204);
                String* requestBody = (String*)request->_tempObject;
                Log.noticeln("Received POST request. Body: %s", requestBody->c_str());
                JsonDocument doc;
                deserializeJson(doc, *requestBody);
                this->onSystemCommand(doc);
                delete requestBody;
                request->_tempObject = NULL;
                // if the URL of the request is http://example.com/path/to/page?query=string, request->url() returns /path/to/page?query=string.

            }, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
                // This callback is called for each chunk of body data received
                Log.noticeln("Received chunk, Len: %d, Index: %d, Total: %d.", len, index, total);

                if (request->_tempObject == NULL) {
                request->_tempObject = new String();
                }
                
                // Append the current chunk to _tempObject
                String* requestBody = (String*)request->_tempObject;
                for (size_t i = 0; i < len; i++) {
                // Serial.print((char)data[i]);
                *requestBody += (char)data[i];
                }
            });

            webServer->onNotFound([](AsyncWebServerRequest *request){
                // this->onReceivedCallback(); ignore, not a valid request
                request->send(404, "text/plain", "Not found");
            });

            webServer->begin();
        }

        void setOnReceivedCallback(std::function<void()> onReceivedCallback) {
            this->onReceivedCallback = onReceivedCallback;
        }

        void end() {
            webServer->end();
        }

};
