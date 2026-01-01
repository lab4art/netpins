#include "firmware.h"

QueueHandle_t firmwareUpdateResultQueue;
FirmwareUpdateResult* lastResult = new FirmwareUpdateResult();
