#pragma once

#include <WebServer.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
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
        std::function<CommandResult(JsonVariant&)> onSystemCommand = [](JsonVariant&){ return CommandResult{OK, "Success."}; };

        void listFiles() {
            File root = LittleFS.open("/");
            Log.noticeln("Files on /");
            File file = root.openNextFile();
            while (file) {
                Log.noticeln("- %s", file.name());
                file = root.openNextFile();
            }
        }

    public:
        WebAdmin(
                SettingsManager<Settings>* settingsManager,
                SettingsManager<DmxSettings>* dmxSettingsManager,
                std::function<CommandResult(JsonVariant&)> onSystemCommand):
                settingsManager(settingsManager),
                dmxSettingsManager(dmxSettingsManager),
                onSystemCommand(onSystemCommand) {

            listFiles();

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

            webServer->on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
                request->redirect("/index.html");
            });
            webServer->serveStatic("/index.html", LittleFS, "/index.html");
            webServer->serveStatic("/script.js", LittleFS, "/script.js");
            webServer->serveStatic("/js-yaml-4.1.0.min.js", LittleFS, "/js-yaml-4.1.0.min.js");
            webServer->serveStatic("/style.css", LittleFS, "/style.css");
            webServer->serveStatic("/favicon.svg", LittleFS, "/favicon.svg");
            webServer->serveStatic("/test.txt", LittleFS, "/test.txt");


            webServer->on("/conf/sys", HTTP_GET, [this](AsyncWebServerRequest *request){
                this->onReceivedCallback();
                request->send(200, "application/json", this->settingsManager->getSettings().asJson().c_str());
            });

            webServer->on("/conf/dmx", HTTP_GET, [this](AsyncWebServerRequest *request){
                this->onReceivedCallback();
                request->send(200, "application/json", this->dmxSettingsManager->getSettings().asJson().c_str());
            });

            AsyncCallbackJsonWebHandler* systemHandler = new AsyncCallbackJsonWebHandler("/system", [this](AsyncWebServerRequest *request, JsonVariant &json) {
                this->onReceivedCallback();

                CommandResult result = this->onSystemCommand(json);

                // serialize CommandResult result to json
                String resultJson;
                JsonDocument resultDoc;
                resultDoc["status"] = result.status == OK ? "OK" : result.status == OK_REBOOT ? "OK_REBOOT" : "ERROR";
                resultDoc["message"] = result.message;
                resultDoc["reloadDelay"] = result.reloadDelay;
                serializeJson(resultDoc, resultJson);

                if (result.status == OK || result.status == OK_REBOOT) {
                    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", resultJson);
                    response->addHeader("Connection", "close");
                    request->send(response);
                    if (result.status == OK_REBOOT) {
                        request->onDisconnect([]() {
                            Log.noticeln("Rebooting ...");
                            ESP.restart();
                        });
                    }
                } else {
                    request->send(500, "application/json", resultJson);
                }
            });
            webServer->addHandler(systemHandler);

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
