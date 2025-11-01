#pragma once

#include <string>
#include <NeoPixelBus.h>

class ColorUtils {
public:
    /**
     * Parse a hex color string (with or without #) to RgbColor
     * @param hex Hex color string (e.g., "#FF0000" or "FF0000")
     * @return RgbColor object
     * @throws std::invalid_argument if hex string is invalid
     */
    static RgbColor parseHexColor(const std::string& hex);
    
    /**
     * Convert RgbColor to hex color string
     * @param color RgbColor object
     * @return Hex color string with # prefix (e.g., "#FF0000")
     */
    static std::string toHexColor(const RgbColor& color);
};