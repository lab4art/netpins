#include "SystemCommandHandler.h"
#include <Log.h>
#include <factoryReset.h>
#include <firmware.h>

SystemCommandHandler::SystemCommandHandler(
    SettingsManager<Settings>* settingsManager,
    SystemManager* systemManager,
    DmxListener* dmxListener
) : settingsManager(settingsManager),
    systemManager(systemManager),
    dmxListener(dmxListener) {
}

WebAdmin::CommandResult SystemCommandHandler::handleCommand(JsonVariant &jsonVariant) {
    systemManager->markCommandReceived();
    FactoryReset::getInstance().resetCounter(true);

    std::string command = jsonVariant["command"].as<std::string>();

    if (command == "sys-config") {
        settingsManager->fromJson(jsonVariant["data"].as<std::string>());
        if (settingsManager->isDirty()) {
            settingsManager->save();
            return WebAdmin::CommandResult{WebAdmin::CommandStatus::OK_REBOOT, "Saved, rebooting ...", 3000};
        } else {
            return WebAdmin::CommandResult{WebAdmin::CommandStatus::OK, "No updates.", -1};
        }
    } else if (command == "sys-config-merge") {
        settingsManager->mergeJson(jsonVariant["data"].as<std::string>());
        if (settingsManager->isDirty()) {
            settingsManager->save();
            return WebAdmin::CommandResult{WebAdmin::CommandStatus::OK_REBOOT, "Saved, rebooting ...", 3000};
        } else {
            return WebAdmin::CommandResult{WebAdmin::CommandStatus::OK, "No updates.", -1};
        }
    } else if (command == "firmware-update" || command == "spiffs-update") {
        FirmwareUpdateParams* params;

        if (command == "firmware-update") {
            params = new FirmwareUpdateParams{jsonVariant["data"]["url"].as<std::string>(), false};
        } else {
            params = new FirmwareUpdateParams{jsonVariant["data"]["url"].as<std::string>(), true};
        }

        xTaskCreate(
            firmwareUpdateTask,   // Task function
            "FirmwareUpdateTask", // Name of the task
            10000,                // Stack size (in words)
            params,               // Task input parameters
            1,                    // Priority of the task
            NULL                  // Task handle
        );
        // Wait for the result from the firmware update task
        FirmwareUpdateResult* updateResult;
        if (xQueueReceive(firmwareUpdateResultQueue, &updateResult, pdMS_TO_TICKS(90000)) != pdTRUE) {
            Log::errorln("Failed to receive update result within 90 seconds.");
            return WebAdmin::CommandResult{WebAdmin::CommandStatus::ERROR, "Update timed-out after 90 seconds.", 3000};
        } else {
            if (updateResult->status == FirmwareUpdateStatus::NO_UPDATES) {
                return WebAdmin::CommandResult{WebAdmin::CommandStatus::OK, updateResult->message, -1};
            } else if (updateResult->status == FirmwareUpdateStatus::STARTED) {
                return WebAdmin::CommandResult{WebAdmin::CommandStatus::OK, updateResult->message, 30000};  // update takes ~20s
            } else if (updateResult->status == FirmwareUpdateStatus::SUCCESS) {
                return WebAdmin::CommandResult{WebAdmin::CommandStatus::OK_REBOOT, updateResult->message, 3000};
            } else {
                return WebAdmin::CommandResult{WebAdmin::CommandStatus::ERROR, updateResult->message, -1};
            }
        }
    } else if (command == "reboot") {
        return WebAdmin::CommandResult{WebAdmin::CommandStatus::OK_REBOOT, "Rebooting ...", 3000};
    } else if (command == "save-dmx") {
        auto updated = dmxListener->storeDmxData(dmxListener->getDmxData());
        return WebAdmin::CommandResult{WebAdmin::CommandStatus::OK, updated ? "Saved." : "No updates.", -1};
    } else if (command == "reset-dmx") {
        dmxListener->clearDmxData();
        return WebAdmin::CommandResult{WebAdmin::CommandStatus::OK, "DMX data cleared.", -1};
    }
    return WebAdmin::CommandResult{WebAdmin::CommandStatus::ERROR, "Unknown command.", -1};
}
