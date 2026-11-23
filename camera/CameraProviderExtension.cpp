/*
 * Copyright (C) 2024 LibreMobileOS Foundation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "CameraProviderExtension.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <android-base/properties.h>
#include <string>

static constexpr int32_t kUiMaxLevel = 100;
static constexpr int32_t kUiDefaultLevel = 65;

static constexpr int32_t kHwStepMa = 5;          // PM8350C qcom,ires-ua = 5000 uA
static constexpr int32_t kMinHwBrightness = 5;
static constexpr int32_t kMaxHwBrightness = 250; // Dual-LED hardware ceiling under all switches
static constexpr double kPerceptualGamma = 1.8;

static std::atomic<int32_t> sCurrentUiLevel{0};
static std::atomic<int32_t> sLastHwLevel{0};
static std::atomic<bool> sTorchEnabled{false};

static int32_t uiLevelToHwBrightness(int32_t uiLevel) {
    int32_t clamped = std::clamp(uiLevel, 1, kUiMaxLevel);
    double norm = static_cast<double>(clamped - 1) / static_cast<double>(kUiMaxLevel - 1);
    double curved = std::pow(norm, kPerceptualGamma);
    double rawMa = kMinHwBrightness + (kMaxHwBrightness - kMinHwBrightness) * curved;
    int32_t quantized = static_cast<int32_t>(std::lround(rawMa / kHwStepMa)) * kHwStepMa;
    return std::clamp(quantized, kMinHwBrightness, kMaxHwBrightness);
}

bool supportsTorchStrengthControlExt() {
    return true;
}

bool supportsSetTorchModeExt() {
    return true;
}

int32_t getTorchMaxStrengthLevelExt() {
    return kUiMaxLevel;
}

int32_t getTorchDefaultStrengthLevelExt() {
    return kUiDefaultLevel;
}

int32_t getTorchStrengthLevelExt() {
    return sCurrentUiLevel.load();
}

void setTorchStrengthLevelExt(int32_t torchStrength, bool enabled) {
    if (!enabled || torchStrength <= 0) {
        sCurrentUiLevel.store(0);
        sLastHwLevel.store(0);
        sTorchEnabled.store(false);
        android::base::SetProperty("camera.torch.enabled", "0");
        android::base::SetProperty("camera.torch.level", "0");
        return;
    }

    int32_t clamped = std::clamp(torchStrength, 1, kUiMaxLevel);
    sCurrentUiLevel.store(clamped);

    int32_t hwLevel = uiLevelToHwBrightness(clamped);
    bool wasEnabled = sTorchEnabled.exchange(true);

    if (!wasEnabled) {
        sLastHwLevel.store(hwLevel);
        android::base::SetProperty("camera.torch.level", std::to_string(hwLevel));
        android::base::SetProperty("camera.torch.enabled", "1");
        return;
    }

    if (sLastHwLevel.exchange(hwLevel) != hwLevel) {
        android::base::SetProperty("camera.torch.level", std::to_string(hwLevel));
    }
}

void setTorchModeExt(bool enabled) {
    setTorchStrengthLevelExt(enabled ? getTorchDefaultStrengthLevelExt() : 0, enabled);
}
