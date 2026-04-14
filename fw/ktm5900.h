// Copyright 2025 Robotic Systems
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <optional>

#include "mbed.h"
#include "hal/spi_api.h"

#include "fw/ccm.h"
#include "fw/stm32_digital_output.h"
#include "fw/stm32_spi.h"
#include "fw/aux_common.h"

namespace moteus {

class Ktm5900 {
 public:
  using Options = Stm32Spi::Options;

  Ktm5900(const Options& options)
      : spi_([&]() {
               auto options_copy = options;
               options_copy.width = 8;
               options_copy.mode = 0;  // VESC bit-bang is mode 0
               return options_copy;
             }()),
        cs_(std::in_place_t(), options.cs, 1) {
    BuildReadAngle(read_angle_tx_);
    BuildWriteReg(write_angle_bits_tx_, 0xe1, 0x18);
    BuildWriteReg(write_revolution_bits_tx_, 0xe3, 0x00);
  }

  void ISR_StartSample() MOTEUS_CCM_ATTRIBUTE {
    if (transaction_active_) { return; }

    switch (stage_) {
      case Stage::kInitWriteAngleBits1: {
        StartTransaction(write_angle_bits_tx_, 4);
        break;
      }
      case Stage::kInitWriteAngleBits2: {
        StartTransaction(write_angle_bits_tx_, 4);
        break;
      }
      case Stage::kInitWriteRevolutionBits1: {
        StartTransaction(write_revolution_bits_tx_, 4);
        break;
      }
      case Stage::kInitWriteRevolutionBits2: {
        StartTransaction(write_revolution_bits_tx_, 4);
        break;
      }
      case Stage::kReadDiscard:
      case Stage::kReadSample: {
        StartTransaction(read_angle_tx_, 8);
        break;
      }
    }
  }

  void ISR_MaybeFinishSample(aux::Spi::Status* status) MOTEUS_CCM_ATTRIBUTE {
    if (!transaction_active_) { return; }

    rx_buffer_[transaction_index_] = spi_.finish_byte();
    transaction_index_++;

    if (transaction_index_ < transaction_size_) {
      spi_.start_byte(tx_buffer_[transaction_index_]);
      return;
    }

    Delay();
    cs_->set();
    Delay();
    transaction_active_ = false;

    switch (stage_) {
      case Stage::kInitWriteAngleBits1: {
        stage_ = Stage::kInitWriteAngleBits2;
        return;
      }
      case Stage::kInitWriteAngleBits2: {
        stage_ = Stage::kInitWriteRevolutionBits1;
        return;
      }
      case Stage::kInitWriteRevolutionBits1: {
        stage_ = Stage::kInitWriteRevolutionBits2;
        return;
      }
      case Stage::kInitWriteRevolutionBits2: {
        stage_ = Stage::kReadDiscard;
        return;
      }
      case Stage::kReadDiscard: {
        stage_ = Stage::kReadSample;
        return;
      }
      case Stage::kReadSample: {
        uint64_t data = 0;
        for (int i = 0; i < 8; i++) {
          data <<= 8;
          data |= rx_buffer_[i];
        }

        if (IsCheckCRC8OK(data, 26)) {
          status->value = static_cast<uint32_t>(data >> (64 - 24));
          status->active = true;
          status->nonce++;
        } else {
          status->checksum_errors++;
        }

        stage_ = Stage::kReadDiscard;
        return;
      }
    }
  }

 private:
  static void Delay() {
    for (volatile int i = 0; i < 12; i++) {
      __NOP();
    }
  }

  static void BuildReadAngle(uint8_t* buffer) {
    buffer[0] = 0x23;
    buffer[1] = 0x00;
    buffer[2] = 0x00;
    buffer[3] = 0x00;
    buffer[4] = 0x00;
    buffer[5] = 0x00;
    buffer[6] = 0x00;
    buffer[7] = 0x00;
  }

  static void BuildWriteReg(uint8_t* buffer,
                            uint16_t address,
                            uint8_t value) {
    const uint32_t command =
        0x5b000000u |
        (static_cast<uint32_t>(address) << 8) |
        static_cast<uint32_t>(value);
    buffer[0] = (command >> 24) & 0xff;
    buffer[1] = (command >> 16) & 0xff;
    buffer[2] = (command >> 8) & 0xff;
    buffer[3] = command & 0xff;
  }

  void StartTransaction(const uint8_t* source, uint8_t size) {
    transaction_size_ = size;
    transaction_index_ = 0;
    for (int i = 0; i < size; i++) {
      tx_buffer_[i] = source[i];
      rx_buffer_[i] = 0;
    }

    Delay();
    cs_->clear();
    Delay();
    spi_.start_byte(tx_buffer_[0]);
    transaction_active_ = true;
  }

  static uint8_t IsCheckCRC8OK(uint64_t input, int32_t len) {
    input >>= 56 - len;
    uint64_t data = input;
    input &= ~0xFFull;
    uint64_t inputTemp = input;
    uint64_t mark = 0x8000000000000000ull >> (56 - len);
    uint64_t poly = 0x8380000000000000ull >> (56 - len);

    for (int32_t i = 0; i < len; i++) {
      if ((input & mark) != 0) {
        input ^= poly;
      }
      mark >>= 1;
      poly >>= 1;
    }
    
    inputTemp += (input ^ 0xFF) & 0xFF;
    return (inputTemp == data) ? 1 : 0;
  }

  Stm32Spi spi_;
  std::optional<Stm32DigitalOutput> cs_;

  enum class Stage {
    kInitWriteAngleBits1,
    kInitWriteAngleBits2,
    kInitWriteRevolutionBits1,
    kInitWriteRevolutionBits2,
    kReadDiscard,
    kReadSample,
  };

  Stage stage_ = Stage::kInitWriteAngleBits1;
  bool transaction_active_ = false;
  uint8_t transaction_size_ = 0;
  uint8_t transaction_index_ = 0;
  uint8_t write_angle_bits_tx_[4] = {};
  uint8_t write_revolution_bits_tx_[4] = {};
  uint8_t read_angle_tx_[8] = {};
  uint8_t tx_buffer_[8] = {};
  uint8_t rx_buffer_[8] = {};
};

}
