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
class ActivateAction : public Action<Ts...>, public Parented<TAS2780> {
 public:
  void play(Ts... x) override { this->parent_->activate(); }
};

template< typename... Ts>
class DeactivateAction : public Action<Ts...>, public Parented<TAS2780> {
 public:
  void play(Ts... x) override { this->parent_->deactivate(); }
};


}
}