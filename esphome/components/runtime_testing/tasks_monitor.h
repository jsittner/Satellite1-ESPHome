#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace runtime_testing {



class TaskMonitor : public Component {
public:
    void setup() override;
    void loop() override;

};

}
}