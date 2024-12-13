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
    public:
        enum CommandStatus {
            OK,
            OK_REBOOT,
            ERROR
        };
        struct CommandResult {
            CommandStatus status;
            String message;
            int reloadDelay; // -1 no reload, 0 reload immediately, >0 reload after delay
        };

    private:
        AsyncWebServer* webServer = new AsyncWebServer(80);
        std::function<void()> onReceivedCallback = [](){};
        
        SettingsManager<Settings>* settingsManager;
        SettingsManager<DmxSettings>* dmxSettingsManager;
        std::function<CommandResult(JsonDocument&)> onSystemCommand = [](JsonDocument&){ return CommandResult{OK, "Success."}; };

        void serveFile(AsyncWebServerRequest *request, String path, String contentType) {
            File file = LittleFS.open(path, "r");
            if (!file) {
                request->send(500, "text/plain", "Failed to open file");
                return;
            }
            String htmlContent = file.readString();
            file.close();
            AsyncWebServerResponse *response = request->beginResponse(200, contentType, htmlContent);
            response->addHeader("Content-encoding", "gzip");
            request->send(response);

        }

    public:
        WebAdmin(
                SettingsManager<Settings>* settingsManager,
                SettingsManager<DmxSettings>* dmxSettingsManager,
                std::function<CommandResult(JsonDocument&)> onSystemCommand):
                settingsManager(settingsManager),
                dmxSettingsManager(dmxSettingsManager),
                onSystemCommand(onSystemCommand) {

            webServer->on("/sys-info", HTTP_GET, [this](AsyncWebServerRequest *request){
                this->onReceivedCallback();

                String output;
                JsonDocument doc;
                doc["firmware"] = FIRMWARE_VERSION;
                doc["mac"] = WifiUtils::macAddress;
                doc["ip"] = WiFi.localIP().toString();
                doc["hostname"] = this->settingsManager->getSettings().hostname;
                doc["uptime"] = std::to_string(millis());
                serializeJson(doc, output);

                request->send(200, "application/json", output);
            });

            webServer->on("/", HTTP_GET, [this](AsyncWebServerRequest *request){
                serveFile(request, "/admin.html.gz", "text/html");
            });
            webServer->on("/script.js", HTTP_GET, [this](AsyncWebServerRequest *request){
                serveFile(request, "/script.js.gz", "application/javascript");
            });
            webServer->on("/js-yaml-4.1.0.min.js", HTTP_GET, [this](AsyncWebServerRequest *request){
                serveFile(request, "/js-yaml-4.1.0.min.js.gz", "application/javascript");
            });
            webServer->on("/style.css", HTTP_GET, [this](AsyncWebServerRequest *request){
                serveFile(request, "/style.css.gz", "text/css");
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
                String* requestBody = (String*)request->_tempObject;
                Log.noticeln("Received POST request. Body: %s", requestBody->c_str());
                JsonDocument doc;
                deserializeJson(doc, *requestBody);
                CommandResult result = this->onSystemCommand(doc);
                delete requestBody;
                request->_tempObject = NULL;

                // serialize CommandResult result to json
                String resultJson;
                JsonDocument resultDoc;
                resultDoc["status"] = result.status == OK ? "OK" : result.status == OK_REBOOT ? "OK_REBOOT" : "ERROR";
                resultDoc["message"] = result.message;
                resultDoc["reloadDelay"] = result.reloadDelay;
                serializeJson(resultDoc, resultJson);

                if (result.status == OK || result.status == OK_REBOOT) {
                    request->send(200, "application/json", resultJson);
                    if (result.status == OK_REBOOT) {
                        delay(1000);
                        Log.noticeln("Rebooting ...");
                        ESP.restart();
                    }
                } else {
                    request->send(500, "application/json", resultJson);
                }
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
