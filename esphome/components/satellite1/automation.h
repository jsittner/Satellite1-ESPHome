#pragma once
#include "xmos_flashing.h"

#include "esphome/core/automation.h"

namespace esphome {
namespace satellite1 {

template<typename... Ts> class XMOSFlashAction : public Action<Ts...> {
 public:
  XMOSFlashAction(XMOSFlasher *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, md5_url)
  TEMPLATABLE_VALUE(std::string, md5)
  TEMPLATABLE_VALUE(std::string, url)

  void play(Ts... x) override {
    if (this->md5_url_.has_value()) {
      this->parent_->set_md5_url(this->md5_url_.value(x...));
    }
    if (this->md5_.has_value()) {
      this->parent_->set_md5(this->md5_.value(x...));
    }
    this->parent_->set_url(this->url_.value(x...));

    this->parent_->flash_remote_image();
  }

 protected:
  XMOSFlasher *parent_;
};

template<typename... Ts> 
class XMOSFlashEmbeddedAction : public Action<Ts...>, public Parented<XMOSFlasher> {
 public:
  void play(Ts... x) override {
    this->parent_->flash_embedded_image();
  }
};


template<XMOSFlasherState State> class XMOSFlasherStateTrigger : public Trigger<> {
 public:
  explicit XMOSFlasherStateTrigger(XMOSFlasher *xflash) {
    xflash->add_on_state_callback([this, xflash]() {
      if (xflash->state == State)
        this->trigger();
    });
  }
};

class XMOSFlashingProgressUpdateTrigger : public Trigger<>{
public:
  explicit XMOSFlashingProgressUpdateTrigger(XMOSFlasher *xflash) {
    xflash->add_on_state_callback([this, xflash]() {
      if(  xflash->state == XMOS_FLASHER_FLASHING && xflash->flashing_progress != this->last_reported_ )
        this->last_reported_ = xflash->flashing_progress;
        this->trigger();
    });
  }
protected:
  uint8_t last_reported_{0};
};


using XMOSFlasherSuccessTrigger = XMOSFlasherStateTrigger<XMOS_FLASHER_SUCCESS_STATE>;
using XMOSFlasherFailedTrigger = XMOSFlasherStateTrigger<XMOS_FLASHER_ERROR_STATE>;


}
}