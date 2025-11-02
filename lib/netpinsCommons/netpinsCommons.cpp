#include "netpinsCommons.h"
#include <stdexcept>

RgbColor NetpinsCommons::parseHexColor(const std::string& hex) {
    // if string starts with # remove it
    std::string hexColor = hex;
    if (hexColor[0] == '#') {
        hexColor = hexColor.substr(1);
    }
    if (hexColor.length() != 6) {
        throw std::invalid_argument("Hex color must be 6 characters long");
    }
    uint8_t r = std::stoi(hexColor.substr(0, 2), nullptr, 16);
    uint8_t g = std::stoi(hexColor.substr(2, 2), nullptr, 16);
    uint8_t b = std::stoi(hexColor.substr(4, 2), nullptr, 16);
    return RgbColor(r, g, b);
}

std::string NetpinsCommons::toHexColor(const RgbColor& color) {
    char buffer[8];
    snprintf(buffer, sizeof(buffer), "#%02X%02X%02X", color.R, color.G, color.B);
    return std::string(buffer);
}

uint8_t NetpinsCommons::linearBlend(uint8_t left, uint8_t right, float progress) {
    return left + (right - left) * progress;
}
