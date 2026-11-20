/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/parseint.h>
#include <android-base/properties.h>
#include <android-base/strings.h>

#include <cstdlib>
#include <cstring>
#include <fs_mgr.h>
#include <string>

#define _REALLY_INCLUDE_SYS__SYSTEM_PROPERTIES_H_
#include <sys/_system_properties.h>

using android::base::GetProperty;
using android::base::ParseInt;
using android::base::ReadFileToString;
using android::base::Trim;

namespace {

    constexpr const char* kPartitionPrefixes[] = {
        "", "system.", "system_ext.", "product.", "vendor.", "odm.", "vendor_dlkm.", "system_dlkm."
    };

    constexpr const char* kBaseFingerprint =
    "OnePlus/NE2213/OP516FL1:16/BP2A.250605.015/S.3c11fd8-181a3a9-17c6449:user/release-keys";

        void OverrideProperty(const char* name, const char* value) {
            size_t valuelen = strlen(value);

            prop_info* pi = (prop_info*)__system_property_find(name);
            if (pi != nullptr) {
                __system_property_update(pi, value, valuelen);
            } else {
                __system_property_add(name, strlen(name), value, valuelen);
            }
        }

        void SetModelAndFingerprint(const std::string& model, const std::string& region) {
            // 1. Set ro.boot.hardware.revision in case any .rc script checks it
            OverrideProperty("ro.boot.hardware.revision", region.c_str());

            // 2. Set ro.product.<partition>.model and ro.product.<partition>.name
            for (const char* prefix : kPartitionPrefixes) {
                std::string model_prop = std::string("ro.product.") + prefix + "model";
                std::string name_prop = std::string("ro.product.") + prefix + "name";
                OverrideProperty(model_prop.c_str(), model.c_str());
                OverrideProperty(name_prop.c_str(), model.c_str());
            }

            // 3. Build and set the dynamic fingerprint (OnePlus/<model>/...)
            std::string fp = GetProperty("ro.build.fingerprint", "");
            if (fp.empty()) {
                fp = kBaseFingerprint;
            }
            size_t pos = fp.find("NE221");
            if (pos != std::string::npos && pos + 6 <= fp.size()) {
                fp.replace(pos, 6, model);
            }
            for (const char* prefix : kPartitionPrefixes) {
                std::string fp_prop = std::string("ro.") + prefix + "build.fingerprint";
                OverrideProperty(fp_prop.c_str(), fp.c_str());
            }
        }

        int GetOperatorOrRfVersion() {
            int val = 0;
            std::string buf;

            if (ParseInt(Trim(GetProperty("ro.boot.rf_version", "")), &val) && val > 0) {
                return val;
            }
            if (ReadFileToString("/proc/oplusVersion/operatorName", &buf) &&
                ParseInt(Trim(buf), &val) && val > 0) {
                return val;
                }
                if (ReadFileToString("/proc/oplusVersion/modemType", &buf) &&
                    ParseInt(Trim(buf), &val) && val > 0) {
                    return val;
                    }
                    return 0;
        }

}  // anonymous namespace

void vendor_process_bootenv() {
    std::string oplus_bootmode;
    if ((android::fs_mgr::GetKernelCmdline("oplusboot.mode", &oplus_bootmode) && oplus_bootmode == "charger") ||
        GetProperty("ro.boot.mode", "") == "charger") {
        OverrideProperty("ro.boot.mode", "charger");
    OverrideProperty("ro.bootmode", "charger");
        }

        std::string prj_str = Trim(GetProperty("ro.boot.prjname", ""));
        if (prj_str.empty()) {
            ReadFileToString("/proc/oplusVersion/prjName", &prj_str);
            prj_str = Trim(prj_str);
        }

        int prj_hex = static_cast<int>(strtol(prj_str.c_str(), nullptr, 16));
        int op_ver = GetOperatorOrRfVersion();
        std::string multisim = Trim(GetProperty("ro.boot.radio.multisim.config", ""));

        switch (prj_hex) {
            case 0x20846:  // China
                SetModelAndFingerprint("NE2210", "CN");
                break;
            case 0x2084A:  // T-Mobile NA
                SetModelAndFingerprint("NE2217", "NA");
                break;
            case 0x20847:  // Shared by IN (NE2211), EU (NE2213), and NA (NE2215)
                if (op_ver == 3 || op_ver == 22 || op_ver == 32 || multisim == "ssss") {
                    SetModelAndFingerprint("NE2215", "NA");
                } else if (op_ver == 4 || op_ver == 13) {
                    SetModelAndFingerprint("NE2211", "IN");
                } else {
                    SetModelAndFingerprint("NE2213", "EU");
                }
                break;
            default:
                // Fallback if ro.boot.prjname wasn't populated yet on wly
                if (multisim == "ssss") {
                    SetModelAndFingerprint("NE2215", "NA");
                } else {
                    LOG(ERROR) << "Unexpected project: " << prj_str << " (op_ver=" << op_ver << ")";
                }
                break;
        }
}

void vendor_load_properties() {
    vendor_process_bootenv();
}
