#pragma once

#include <map>
#include <WebServer.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <wifiUtils.h>
#include <Log.h>
#include <settings.h>
#include <variant>


class WebAdmin {
    public:
        enum CommandStatus {
            OK,
            OK_REBOOT,
            ERROR
        };
        struct CommandResult {
            CommandStatus status;
            std::string message;
            int reloadDelay; // -1 no reload, 0 reload immediately, >0 reload after delay
        };

    private:
        AsyncWebServer* webServer = new AsyncWebServer(80);
        std::function<void()> onReceivedCallback = [](){};
        
        SettingsManager<Settings>* settingsManager;
        std::function<CommandResult(JsonVariant&)> onSystemCommand = [](JsonVariant&){ return CommandResult{OK, "Success."}; };
        std::function<std::map<std::string, std::string>()> propertiesSupplier = [](){ return std::map<std::string, std::string>(); };
        
        std::string firmwareVersion;
        int factoryResetPin;

        void listFiles() {
            File root = LittleFS.open("/");
            Log::infoln("Files on /");
            File file = root.openNextFile();
            while (file) {
                Log::infoln("- %s", file.name());
                file = root.openNextFile();
            }
        }

    public:
        WebAdmin(
                SettingsManager<Settings>* settingsManager,
                std::function<CommandResult(JsonVariant&)> onSystemCommand,
                const std::string& firmwareVersion,
                int factoryResetPin):
                settingsManager(settingsManager),
                onSystemCommand(onSystemCommand),
                firmwareVersion(firmwareVersion),
                factoryResetPin(factoryResetPin) {

            listFiles();

            webServer->on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
                request->redirect("/admin.html");
            });

            webServer->on("/sys-info", HTTP_GET, [this](AsyncWebServerRequest *request){
                this->onReceivedCallback();

                std::string output;
                JsonDocument doc;
                doc["firmware"] = this->firmwareVersion;
                doc["mac"] = WifiUtils::macAddress;
                doc["ip"] = WiFi.localIP().toString();
                doc["hostname"] = this->settingsManager->getSettings().hostname;
                doc["uptime"] = std::to_string(millis());
                if (this->factoryResetPin == -1) {
                    doc["factoryReset"] = "power cycle";
                } else {
                    doc["factoryReset"] = "pin " + std::to_string(this->factoryResetPin);
                }

                // if query parameter mode equas "details" add more details
                if (request->hasParam("show")) {
                    std::string show = request->getParam("show")->value().c_str();
                    if (show == "all") {
                        for (auto const& pair : propertiesSupplier()) {
                            const auto& key = pair.first;
                            const auto& val = pair.second;
                            doc[key] = val;
                        }
                    }
                }
                serializeJson(doc, output);
                request->send(200, "application/json", output.c_str());
            });

            webServer->serveStatic("/admin.html", LittleFS, "/admin.html");
            webServer->serveStatic("/script.js", LittleFS, "/script.js");
            webServer->serveStatic("/js-yaml-4.1.0.min.js", LittleFS, "/js-yaml-4.1.0.min.js");
            webServer->serveStatic("/style.css", LittleFS, "/style.css");
            webServer->serveStatic("/favicon.svg", LittleFS, "/favicon.svg");
            webServer->serveStatic("/test.txt", LittleFS, "/test.txt");


            webServer->on("/conf/sys", HTTP_GET, [this](AsyncWebServerRequest *request){
                this->onReceivedCallback();
                request->send(200, "application/json", this->settingsManager->getSettings().asJson().c_str());
            });

            AsyncCallbackJsonWebHandler* systemHandler = new AsyncCallbackJsonWebHandler("/system", [this](AsyncWebServerRequest *request, JsonVariant &json) {
                this->onReceivedCallback();

                CommandResult result = this->onSystemCommand(json);

                // serialize CommandResult result to json
                std::string resultJson;
                JsonDocument resultDoc;
                resultDoc["status"] = result.status == OK ? "OK" : result.status == OK_REBOOT ? "OK_REBOOT" : "ERROR";
                resultDoc["message"] = result.message;
                resultDoc["reloadDelay"] = result.reloadDelay;
                serializeJson(resultDoc, resultJson);

                if (result.status == OK || result.status == OK_REBOOT) {
                    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", resultJson.c_str());
                    response->addHeader("Connection", "close");
                    request->send(response);
                    if (result.status == OK_REBOOT) {
                        request->onDisconnect([]() {
                            Log::infoln("Rebooting ...");
                            ESP.restart();
                        });
                    }
                } else {
                    request->send(500, "application/json", resultJson.c_str());
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

        void setPropertiesSupplier(std::function<std::map<std::string, std::string>()> propertiesSupplier) {
            this->propertiesSupplier = propertiesSupplier;
        }

        void end() {
            webServer->end();
        }

};
