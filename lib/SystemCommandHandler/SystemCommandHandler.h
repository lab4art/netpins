#ifndef SYSTEM_COMMAND_HANDLER_H
#define SYSTEM_COMMAND_HANDLER_H

#include <ArduinoJson.h>
#include <webadmin.h>
#include <settings.h>
#include <SystemManager.h>
#include <DmxManager.h>

class SystemCommandHandler {
public:
    SystemCommandHandler(
        SettingsManager<Settings>* settingsManager,
        SystemManager* systemManager,
        DmxManager* dmxManager
    );

    WebAdmin::CommandResult handleCommand(JsonVariant &jsonVariant);

private:
    SettingsManager<Settings>* settingsManager;
    SystemManager* systemManager;
    DmxManager* dmxManager;
};

#endif // SYSTEM_COMMAND_HANDLER_H
