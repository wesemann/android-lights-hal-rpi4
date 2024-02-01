/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "Lights.h"

#include <android-base/logging.h>
#include <dirent.h>
#include <fstream>

namespace aidl {
namespace android {
namespace hardware {
namespace light {

static const std::string LEDS_DIR = "/sys/class/leds";

Light::Light(HwLight hwLight, std::string path) : hwLight(hwLight), path(path)
{
}

Led::Led(HwLight hwLight, std::string path, uint32_t maxBrightness)
  : Light(hwLight, path), maxBrightness(maxBrightness)
{
}

Led *Led::createLed(HwLight hwLight, std::string path)
{
  uint32_t maxBrightness;
  std::ifstream stream(path + "/max_brightness");

  if (auto stream = std::ifstream(path + "/max_brightness")) {
      stream >> maxBrightness;
  } else {
      LOG(ERROR) << "Failed to read `max_brightness` for " << path;
      return nullptr;
  }

  LOG(INFO) << "Lights: Create led " << path << " with max brightness " << maxBrightness;

  return new Led(hwLight, path, maxBrightness);
}

ndk::ScopedAStatus Led::setLightStateInternal(const HwLightState &state) const
{
  LOG(INFO) << "Lights: Led setting light state";
  
  ndk::ScopedAStatus status = ndk::ScopedAStatus::ok();

  if (auto stream = std::ofstream(path + "/trigger")) {
      switch (state.flashMode) {
      case FlashMode::NONE:
        stream << "none";
        break;
      case FlashMode::HARDWARE:
        LOG(ERROR) << "Lights: Hardware flash mode not yet supported - what trigger to set?";
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
      case FlashMode::TIMED:
        stream << "timer";
        break;
      }
  } else {
      LOG(ERROR) << "Lights Failed to write `trigger` to " << path;
      return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
  }
  
  if (state.flashMode == FlashMode::TIMED) {
	if (auto stream = std::ofstream(path + "/delay_on")) {
	  stream << state.flashOnMs;
    } else {
      LOG(ERROR) << "Lights Failed to write `flashOnMs` to " << path;
      return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }
  
    if (auto stream = std::ofstream(path + "/delay_off")) {
	  stream << state.flashOffMs;
    } else {
      LOG(ERROR) << "Lights Failed to write `flashOffMs` to " << path;
      return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }
  }
  
  unsigned int brightness = 1;

  if (state.color == 0) {
    brightness = 0;
  }

  LOG(INFO) << "Lights: Setting global led to brightness " << std::hex << brightness;
  if (auto stream = std::ofstream(path + "/brightness")) {
      stream << brightness;
  } else {
      LOG(ERROR) << "Lights: Failed to write `brightness` to " << path;
      return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
  }

  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Lights::setLightState(int id, const HwLightState &state)
{
  LOG(INFO) << "Lights setting state for id=" << id << " to color " << std::hex << state.color;

  if (id >= lights.size())
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);

  const auto &light = lights[id];
  return light->setLightStateInternal(state);
}

ndk::ScopedAStatus Lights::getLights(std::vector<HwLight>* hwLights) {

    LOG(INFO) << "Lights reporting supported lights";

    for (const auto &light: lights)
      hwLights->emplace_back(light->hwLight);

    return ndk::ScopedAStatus::ok();
}

Lights::Lights()
{
  int id = 0;
  int ordinal = 0;

  if (auto leds = opendir(LEDS_DIR.c_str())) {
      while (dirent *ent = readdir(leds)) {
          LOG(INFO) << "Lights: ent->d_name=" << ent->d_name;
          if ((ent->d_type == DT_DIR && ent->d_name[0] != '.') ||
              (ent->d_type == DT_LNK && !strncmp("led", ent->d_name, 3))) {
              LOG(INFO) << "Lights: open '" << ent->d_name << "'";
              std::string ledPath = LEDS_DIR + "/" + ent->d_name;
              LOG(INFO) << "Lights: ledPath=" << ledPath;
              if (auto led = Led::createLed(HwLight { .id = id++,
                                                      .ordinal = ordinal++,
                                                      .type = LightType::MICROPHONE
                                                    },
                                            ledPath))
                lights.emplace_back(led);
          }
      }

      closedir(leds);

  } else {
      LOG(ERROR) << "Lights: Failed to open " << LEDS_DIR;
  }

  LOG(INFO) << "Lights: Found " << ordinal << " leds";
}

}  // namespace light
}  // namespace hardware
}  // namespace android
}  // namespace aidl
