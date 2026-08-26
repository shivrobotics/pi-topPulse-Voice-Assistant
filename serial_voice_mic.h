#pragma once

#include "esphome.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/microphone/microphone.h"
#include "esphome/components/audio/audio.h"
#include "esphome/components/speaker/speaker.h"
#include "configure.h"
#include <HardwareSerial.h>
#include <vector>
#include <atomic>
#include <cmath>

static const char *const TAG = "serial_voice_mic";

inline void play_chime_tone(esphome::speaker::Speaker *spk, float freq, uint32_t duration_ms, float volume = 0.30f) {
  if (spk == nullptr) return;
  const uint32_t sample_rate = 16000;
  size_t num_samples = (sample_rate * duration_ms) / 1000;
  std::vector<int16_t> buffer(num_samples);
  const float phase_inc = (2.0f * (float)M_PI * freq) / static_cast<float>(sample_rate);
  float phase = 0.0f;
  float max_amp = 32767.0f * std::max(0.05f, std::min(1.0f, volume));

  for (size_t i = 0; i < num_samples; i++) {
    float env = 1.0f;
    if (i < 80) env = static_cast<float>(i) / 80.0f;
    else if (i > num_samples - 80) env = static_cast<float>(num_samples - i) / 80.0f;

    buffer[i] = static_cast<int16_t>(sinf(phase) * max_amp * env);
    phase += phase_inc;
    if (phase >= 2.0f * (float)M_PI) phase -= 2.0f * (float)M_PI;
  }

  spk->start();

  uint32_t start_t = esphome::millis();
  while (!spk->is_running() && (esphome::millis() - start_t < 150)) {
    vTaskDelay(pdMS_TO_TICKS(5));
  }

  const uint8_t *byte_ptr = reinterpret_cast<const uint8_t *>(buffer.data());
  size_t total_bytes = buffer.size() * sizeof(int16_t);
  size_t written = 0;
  start_t = esphome::millis();
  while (written < total_bytes && (esphome::millis() - start_t < (duration_ms + 250))) {
    size_t w = spk->play(byte_ptr + written, total_bytes - written, pdMS_TO_TICKS(10));
    written += w;
    if (w == 0) {
      vTaskDelay(pdMS_TO_TICKS(5));
    }
  }
}

inline void play_chime_connected(esphome::speaker::Speaker *spk, float volume = 0.30f) {
  play_chime_tone(spk, 523.25f, 80, volume * 0.85f);
  vTaskDelay(pdMS_TO_TICKS(20));
  play_chime_tone(spk, 659.25f, 80, volume * 0.92f);
  vTaskDelay(pdMS_TO_TICKS(20));
  play_chime_tone(spk, 783.99f, 140, volume);
  vTaskDelay(pdMS_TO_TICKS(40));
}

inline void play_chime_wake_word(esphome::speaker::Speaker *spk, float volume = 0.30f) {
  play_chime_tone(spk, 880.0f, 50, volume * 0.90f);
  vTaskDelay(pdMS_TO_TICKS(15));
  play_chime_tone(spk, 1174.66f, 80, volume);
  vTaskDelay(pdMS_TO_TICKS(30));
}

class SerialVoiceMic : public esphome::Component, public esphome::microphone::Microphone {
 public:
  static const size_t SAMPLES_PER_CHUNK = 128;
  static const size_t MAX_BYTES_PER_LOOP = 512;

  uint32_t total_raw_bytes_received{0};
  uint32_t window_bytes_received{0};
  uint8_t min_raw_val{255};
  uint8_t max_raw_val{0};
  uint32_t last_debug_time{0};

  void setup() override {
    ESP_LOGI(TAG, "Initializing Hardware Serial2 for pi-top PULSE serial microphone (RX=7, TX=6 @ 250000 baud)...");

    Wire.begin(4, 5);

    __update_device_state_bit(0, 0);  // Enable speaker
    __update_device_state_bit(1, 0);  // Enable MCU
    __update_device_state_bit(2, 0);  // Enable EEPROM
    __update_device_state_bit(3, 1);  // Set microphone sample rate to 16,000Hz

    // Expand RX buffer to 4096 bytes to guarantee zero lost audio packets
    Serial2.setRxBufferSize(4096);
    Serial2.begin(250000, SERIAL_8N1, 7, 6);

    this->audio_stream_info_ = esphome::audio::AudioStreamInfo(16, 1, 16000);
    this->sample_buffer_.reserve(SAMPLES_PER_CHUNK * sizeof(int16_t));
    this->last_debug_time = esphome::millis();

    ESP_LOGI(TAG, "SerialVoiceMic initialized (16000Hz, 16-bit mono PCM). Ready for Voice Assistant.");

#if defined(USE_ESP32)
    xTaskCreatePinnedToCore(
        [](void *param) {
          auto *mic = static_cast<SerialVoiceMic *>(param);
          while (true) {
            mic->read_serial_audio_();
            vTaskDelay(pdMS_TO_TICKS(2));
          }
        },
        "serial_mic_task",
        4096,
        this,
        2, // Priority 2 for low jitter audio streaming
        nullptr,
        1
    );
#endif
  }

  void loop() override {
    // Background audio reading is managed by dedicated FreeRTOS task
  }

  void start() override {
    int prev = this->start_count_.fetch_add(1);
    ESP_LOGI(TAG, "SerialVoiceMic start requested (active references: %d)", prev + 1);
    this->state_ = esphome::microphone::STATE_RUNNING;
  }

  void stop() override {
    int count = this->start_count_.load();
    while (count > 0) {
      if (this->start_count_.compare_exchange_weak(count, count - 1)) {
        count--;
        break;
      }
    }
    ESP_LOGI(TAG, "SerialVoiceMic stop requested (active references: %d)", count);
    if (count == 0) {
      this->state_ = esphome::microphone::STATE_STOPPED;
      this->sample_buffer_.clear();
      ESP_LOGI(TAG, "All subscribers released. SerialVoiceMic state set to STOPPED.");
    }
  }

  void read_serial_audio_() {
    size_t available = Serial2.available();
    if (available == 0) {
      return;
    }

    size_t bytes_to_read = std::min(available, MAX_BYTES_PER_LOOP);
    uint8_t raw_bytes[MAX_BYTES_PER_LOOP];
    size_t read_bytes = Serial2.readBytes(raw_bytes, bytes_to_read);

    this->total_raw_bytes_received += read_bytes;
    this->window_bytes_received += read_bytes;

    bool is_streaming = (this->state_ == esphome::microphone::STATE_RUNNING);

    for (size_t i = 0; i < read_bytes; i++) {
      uint8_t raw = raw_bytes[i];
      if (raw < this->min_raw_val) this->min_raw_val = raw;
      if (raw > this->max_raw_val) this->max_raw_val = raw;

      if (is_streaming) {
        int16_t pcm_val = static_cast<int16_t>((static_cast<int32_t>(raw) - 128) << 8);
        this->sample_buffer_.push_back(static_cast<uint8_t>(pcm_val & 0xFF));
        this->sample_buffer_.push_back(static_cast<uint8_t>((pcm_val >> 8) & 0xFF));

        if (this->sample_buffer_.size() >= SAMPLES_PER_CHUNK * sizeof(int16_t)) {
          this->data_callbacks_.call(this->sample_buffer_);
          this->sample_buffer_.clear();
        }
      }
    }

    uint32_t now = esphome::millis();
    if (now - this->last_debug_time >= 2000) {
      int16_t pcm_min = static_cast<int16_t>((static_cast<int32_t>(this->min_raw_val) - 128) << 8);
      int16_t pcm_max = static_cast<int16_t>((static_cast<int32_t>(this->max_raw_val) - 128) << 8);

      ESP_LOGD(TAG, "[2s Telemetry] State: %s (refs: %d) | Total Rx: %lu B | Window Rx: %lu B | Raw [Min: %u, Max: %u] | PCM [Min: %d, Max: %d]",
               is_streaming ? "RUNNING" : "STOPPED",
               this->start_count_.load(),
               (unsigned long)this->total_raw_bytes_received,
               (unsigned long)this->window_bytes_received,
               this->min_raw_val,
               this->max_raw_val,
               pcm_min,
               pcm_max);

      this->window_bytes_received = 0;
      this->min_raw_val = 255;
      this->max_raw_val = 0;
      this->last_debug_time = now;
    }
  }

 protected:
  std::vector<uint8_t> sample_buffer_;
  std::atomic<int> start_count_{0};
};
