/*
 * Copyright (C) 2024 LibreMobileOS Foundation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "CameraProviderExtension.h"

#include <fstream>
#include <string>

static const std::string kTorchBrightness = "brightness";
static const std::string kTorchMaxBrightness = "max_brightness";
static const std::string kToggleSwitchPath = "/sys/class/leds/led:switch_1/brightness";

static std::string kTorchLedPaths[] = {
    "/sys/class/leds/led:torch_0",
    "/sys/class/leds/led:torch_1",
    "/sys/class/leds/led:torch_2",
    "/sys/class/leds/led:torch_3",
};

/**
 * Write value to path, explicitly flush, and close file.
 */
template <typename T>
static void set(const std::string& path, const T& value) {
    std::ofstream file(path);
    if (file.is_open()) {
        file << value;
        file.flush();
        file.close();
    }
}

/**
 * Read value from the path and close file.
 */
template <typename T>
static T get(const std::string& path, const T& def) {
    std::ifstream file(path);
    T result;

    file >> result;
    if (file.is_open()) {
        file.close();
    }
    return file.fail() ? def : result;
}

bool supportsTorchStrengthControlExt() {
    return true;
}

bool supportsSetTorchModeExt() {
    // Return true so the framework bypasses the Oplus HAL and relies solely on this extension
    return true;
}

int32_t getTorchMaxStrengthLevelExt() {
    // Read the actual safe maximum from the generic kernel node.
    auto node = kTorchLedPaths[0] + "/" + kTorchMaxBrightness;
    return get(node, 200);
}

int32_t getTorchDefaultStrengthLevelExt() {
    // Dynamically set default to whatever the current max is.
    return getTorchMaxStrengthLevelExt();
}

int32_t getTorchStrengthLevelExt() {
    auto node = kTorchLedPaths[0] + "/" + kTorchBrightness;
    return get(node, 0);
}

void setTorchStrengthLevelExt(int32_t torchStrength) {
    if (torchStrength == 0) {
        // Latch off first, then zero out LEDs
        set(kToggleSwitchPath, 0);
        for (const auto& path : kTorchLedPaths) {
            set(path + "/" + kTorchBrightness, 0);
        }
        return;
    }

    // PM8350c Latch Sequence: Must drop master switch to 0 to accept new PWM values
    set(kToggleSwitchPath, 0);

    for (const auto& path : kTorchLedPaths) {
        set(path + "/" + kTorchBrightness, torchStrength);
    }

    // Commit the new brightness targets
    set(kToggleSwitchPath, 1);
}

void setTorchModeExt(bool enabled) {
    int32_t strength = getTorchDefaultStrengthLevelExt();
    setTorchStrengthLevelExt(enabled ? strength : 0);
}
