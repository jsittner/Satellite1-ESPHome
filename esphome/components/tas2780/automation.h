#pragma once

#include "esphome/core/automation.h"
#include "tas2780.h"

namespace esphome {

namespace tas2780 {

template< typename... Ts>
class ResetAction : public Action<Ts...>, public Parented<TAS2780> {
 public:
  void play(Ts... x) override { this->parent_->reset(); }
};

template< typename... Ts>
class ActivateAction : public Action<Ts...> {
 public:
  ActivateAction(TAS2780 *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(uint8_t, mode)
  
  void play(Ts... x) override { 
    if( this->mode_.has_value() ){
      this->parent_->activate(this->mode_.value(x...));
    }
    else{
      this->parent_->activate(); 
    }
  }

protected:
 TAS2780 *parent_;  
};

template< typename... Ts>
class DeactivateAction : public Action<Ts...>, public Parented<TAS2780> {
 public:
  void play(Ts... x) override { this->parent_->deactivate(); }
};


}
}