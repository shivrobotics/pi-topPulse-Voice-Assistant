#pragma once

#include "esphome.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include <HardwareSerial.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cctype>

static const char *const MATRIX_TAG = "pitop_matrix";

// Gamma correction table from pi-topPULSE specification
static const uint8_t MATRIX_GAMMA_LUT[256] = {
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   1,   1,   1,
    1,   1,   1,   1,   1,   1,   1,   1,   1,   2,   2,   2,   2,   2,   2,   2,
    2,   3,   3,   3,   3,   3,   3,   3,   4,   4,   4,   4,   4,   5,   5,   5,
    5,   6,   6,   6,   6,   7,   7,   7,   7,   8,   8,   8,   9,   9,   9,  10,
   10,  10,  11,  11,  11,  12,  12,  13,  13,  13,  14,  14,  15,  15,  16,  16,
   17,  17,  18,  18,  19,  19,  20,  20,  21,  21,  22,  22,  23,  24,  24,  25,
   25,  26,  27,  27,  28,  29,  29,  30,  31,  32,  32,  33,  34,  35,  35,  36,
   37,  38,  39,  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  50,
   51,  52,  54,  55,  56,  57,  58,  59,  60,  61,  62,  63,  64,  66,  67,  68,
   69,  70,  72,  73,  74,  75,  77,  78,  79,  81,  82,  83,  85,  86,  87,  89,
   90,  92,  93,  95,  96,  98,  99, 101, 102, 104, 105, 107, 109, 110, 112, 114,
  115, 117, 119, 120, 122, 124, 126, 127, 129, 131, 133, 135, 137, 138, 140, 142,
  144, 146, 148, 150, 152, 154, 156, 158, 160, 162, 164, 167, 169, 171, 173, 175,
  177, 180, 182, 184, 186, 189, 191, 193, 196, 198, 200, 203, 205, 208, 210, 213,
  215, 218, 220, 223, 225, 228, 231, 233, 236, 239, 241, 244, 247, 249, 252, 255
};

// Digit bitmap 3x5 font table (0-9)
static const uint8_t MATRIX_DIGIT_FONTS[10][15] = {
  {1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 1, 1}, // 0
  {0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0}, // 1
  {1, 1, 1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 1, 1}, // 2
  {1, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1}, // 3
  {1, 0, 0, 1, 0, 1, 1, 1, 1, 0, 0, 1, 0, 0, 1}, // 4
  {1, 1, 1, 1, 0, 0, 1, 1, 1, 0, 0, 1, 1, 1, 1}, // 5
  {1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1}, // 6
  {1, 1, 1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 0}, // 7
  {1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1}, // 8
  {1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 0, 1, 0, 0, 1}  // 9
};

struct MatrixRGB {
  uint8_t r{0};
  uint8_t g{0};
  uint8_t b{0};
};

enum MatrixEffectType : uint8_t {
  EFFECT_OFF = 0,
  EFFECT_SOLID,
  EFFECT_RAINBOW,
  EFFECT_RING_PULSE,
  EFFECT_MATRIX_RAIN,
  EFFECT_FIRE,
  EFFECT_RADAR,
  EFFECT_DIGITS,
  EFFECT_EQUALIZER,
  EFFECT_SPARKLE
};

enum VoiceAnimationState : uint8_t {
  VA_ANIM_NONE = 0,
  VA_ANIM_CONNECTED,
  VA_ANIM_LISTENING,
  VA_ANIM_SPEAKING
};

inline std::string normalize_effect_name(const std::string &input) {
  std::string result = "";
  for (char c : input) {
    if (isalnum(c)) {
      result += static_cast<char>(tolower(c));
    }
  }
  return result;
}

class PiTopMatrix : public esphome::Component {
 public:
  static const uint8_t WIDTH = 7;
  static const uint8_t HEIGHT = 7;

  void setup() override {
    ESP_LOGI(MATRIX_TAG, "Initializing pi-top PULSE 7x7 RGB LED Matrix controller on Serial2 TX (GPIO 6)...");
    this->is_initialised_ = true;
    this->is_power_on_ = true;  // Hardware master power enabled by default
    this->brightness_ = 0.6f;
    this->current_effect_ = EFFECT_OFF; // Default idle effect is OFF (dark screen)
    this->clear();
    this->send_frame_to_hardware_();
    ESP_LOGI(MATRIX_TAG, "pi-top PULSE 7x7 RGB LED Matrix initialized (Power: ON, Effect: OFF).");
  }

  void loop() override {
    uint32_t now = ::millis();
    if (now - this->last_frame_time_ < this->frame_interval_ms_) {
      return;
    }
    this->last_frame_time_ = now;

    // 1. Master Hardware Power Check
    if (!this->is_power_on_) {
      if (this->needs_clear_) {
        this->clear();
        this->send_frame_to_hardware_();
        this->needs_clear_ = false;
      }
      return;
    }

    // 2. High-Priority Voice Assistant Animations
    if (this->va_anim_state_ != VA_ANIM_NONE) {
      this->render_va_animation_(now);
      this->send_frame_to_hardware_();
      return;
    }

    // 3. Standby / Idle State with Effect OFF
    if (this->current_effect_ == EFFECT_OFF) {
      if (this->needs_clear_) {
        this->clear();
        this->send_frame_to_hardware_();
        this->needs_clear_ = false;
      }
      return;
    }

    // 4. Render Active Visual Effect
    this->render_effect_(now);
    this->send_frame_to_hardware_();
  }

  // --- Display Buffer Operations ---

  void clear() {
    for (int y = 0; y < HEIGHT; y++) {
      for (int x = 0; x < WIDTH; x++) {
        this->pixels_[y][x] = {0, 0, 0};
      }
    }
  }

  void set_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
      this->pixels_[y][x] = {r, g, b};
    }
  }

  void set_all(uint8_t r, uint8_t g, uint8_t b) {
    for (int y = 0; y < HEIGHT; y++) {
      for (int x = 0; x < WIDTH; x++) {
        this->pixels_[y][x] = {r, g, b};
      }
    }
  }

  void set_brightness(float brightness) {
    this->brightness_ = std::max(0.0f, std::min(1.0f, brightness));
  }

  float get_brightness() const { return this->brightness_; }

  void set_power(bool on) {
    this->is_power_on_ = on;
    this->needs_clear_ = true;
    if (!on) {
      this->va_anim_state_ = VA_ANIM_NONE;
    }
    ESP_LOGI(MATRIX_TAG, "Global Matrix Power set to: %s", on ? "ON" : "OFF");
  }

  bool is_power_on() const { return this->is_power_on_; }

  void set_solid_color(uint8_t r, uint8_t g, uint8_t b) {
    this->solid_color_ = {r, g, b};
    this->set_effect(EFFECT_SOLID);
  }

  void set_effect(MatrixEffectType effect) {
    this->current_effect_ = effect;
    this->effect_step_ = 0;
    this->va_anim_state_ = VA_ANIM_NONE;
    if (effect == EFFECT_OFF) {
      this->needs_clear_ = true;
    } else if (effect == EFFECT_SOLID) {
      this->set_all(this->solid_color_.r, this->solid_color_.g, this->solid_color_.b);
    }
    ESP_LOGI(MATRIX_TAG, "LED Matrix effect set to: %s (enum %d)", this->get_canonical_effect_name(), static_cast<int>(effect));
  }

  void set_effect_by_name(const std::string &raw_name) {
    std::string norm = normalize_effect_name(raw_name);
    ESP_LOGI(MATRIX_TAG, "Processing set_effect_by_name: '%s' (normalized: '%s')", raw_name.c_str(), norm.c_str());

    // 1. Off / Clear / Disable / Stop / Dark
    if (norm.find("off") != std::string::npos || norm.find("stop") != std::string::npos || 
        norm.find("clear") != std::string::npos || norm.find("disable") != std::string::npos ||
        norm.find("none") != std::string::npos || norm.find("dark") != std::string::npos) {
      this->set_effect(EFFECT_OFF);
    }
    // 2. Rainbow Wave
    else if (norm.find("rainbow") != std::string::npos || norm.find("wave") != std::string::npos) {
      this->set_effect(EFFECT_RAINBOW);
    }
    // 3. Flickering Fire / Flame
    else if (norm.find("fire") != std::string::npos || norm.find("flame") != std::string::npos || 
             norm.find("flicker") != std::string::npos) {
      this->set_effect(EFFECT_FIRE);
    }
    // 4. Ring Pulse
    else if (norm.find("pulse") != std::string::npos || norm.find("ring") != std::string::npos || 
             norm.find("circle") != std::string::npos) {
      this->set_effect(EFFECT_RING_PULSE);
    }
    // 5. Radar Sweep / Sonar
    else if (norm.find("radar") != std::string::npos || norm.find("sonar") != std::string::npos || 
             norm.find("sweep") != std::string::npos) {
      this->set_effect(EFFECT_RADAR);
    }
    // 6. Digit Counter
    else if (norm.find("digit") != std::string::npos || norm.find("count") != std::string::npos || 
             norm.find("number") != std::string::npos) {
      this->set_effect(EFFECT_DIGITS);
    }
    // 7. Audio Equalizer
    else if (norm.find("equalizer") != std::string::npos || norm.find("spectrum") != std::string::npos || 
             norm.find("vumeter") != std::string::npos || norm.find("audio") != std::string::npos) {
      this->set_effect(EFFECT_EQUALIZER);
    }
    // 8. Twinkling Stars / Sparkle
    else if (norm.find("star") != std::string::npos || norm.find("twinkle") != std::string::npos || 
             norm.find("sparkle") != std::string::npos) {
      this->set_effect(EFFECT_SPARKLE);
    }
    // 9. Matrix Rain (Matches 'rain', 'matrixrain', 'digitalrain', or isolated 'matrix')
    else if (norm.find("rain") != std::string::npos || norm.find("matrixrain") != std::string::npos || 
             norm == "matrix") {
      this->set_effect(EFFECT_MATRIX_RAIN);
    }
    // 10. Specific Colors
    else if (norm.find("cyan") != std::string::npos || norm.find("teal") != std::string::npos) {
      this->set_solid_color(0, 200, 255);
    } else if (norm.find("purple") != std::string::npos || norm.find("magenta") != std::string::npos || 
               norm.find("violet") != std::string::npos) {
      this->set_solid_color(180, 0, 255);
    } else if (norm.find("green") != std::string::npos || norm.find("emerald") != std::string::npos) {
      this->set_solid_color(0, 255, 60);
    } else if (norm.find("amber") != std::string::npos || norm.find("gold") != std::string::npos || 
               norm.find("orange") != std::string::npos || norm.find("yellow") != std::string::npos) {
      this->set_solid_color(255, 140, 0);
    } else if (norm.find("red") != std::string::npos || norm.find("coral") != std::string::npos) {
      this->set_solid_color(255, 30, 30);
    } else if (norm.find("blue") != std::string::npos) {
      this->set_solid_color(0, 60, 255);
    } else if (norm.find("white") != std::string::npos) {
      this->set_solid_color(255, 255, 255);
    } else if (norm.find("solid") != std::string::npos || norm.find("color") != std::string::npos) {
      this->set_effect(EFFECT_SOLID);
    } else {
      ESP_LOGW(MATRIX_TAG, "Unrecognized effect name '%s'", raw_name.c_str());
    }
  }

  void set_color_by_name(const std::string &color_name) {
    this->set_effect_by_name(color_name);
  }

  const char *get_canonical_effect_name() const {
    switch (this->current_effect_) {
      case EFFECT_OFF: return "Off";
      case EFFECT_SOLID: return "Solid Color";
      case EFFECT_RAINBOW: return "Rainbow Wave";
      case EFFECT_RING_PULSE: return "Ring Pulse";
      case EFFECT_MATRIX_RAIN: return "Matrix Rain";
      case EFFECT_FIRE: return "Flickering Fire";
      case EFFECT_RADAR: return "Radar Sweep";
      case EFFECT_DIGITS: return "Digit Counter";
      case EFFECT_EQUALIZER: return "Audio Equalizer";
      case EFFECT_SPARKLE: return "Twinkling Stars";
      default: return "Off";
    }
  }

  // --- Voice Assistant Specific Animations ---

  VoiceAnimationState get_va_state() const { return this->va_anim_state_; }

  void trigger_connected_animation() {
    this->va_anim_state_ = VA_ANIM_CONNECTED;
    this->va_anim_start_ = ::millis();
    this->va_step_ = 0;
    ESP_LOGD(MATRIX_TAG, "Triggered HA Connected LED wave animation (2.0s duration)");
  }

  void start_listening_animation() {
    this->va_anim_state_ = VA_ANIM_LISTENING;
    this->va_anim_start_ = ::millis();
    this->va_step_ = 0;
    ESP_LOGD(MATRIX_TAG, "Started Voice Assistant Listening LED halo animation");
  }

  void start_speaking_animation() {
    this->va_anim_state_ = VA_ANIM_SPEAKING;
    this->va_anim_start_ = ::millis();
    this->va_step_ = 0;
    ESP_LOGD(MATRIX_TAG, "Started Voice Assistant Speaking LED equalizer animation");
  }

  void stop_speaking_animation() {
    if (this->va_anim_state_ == VA_ANIM_SPEAKING) {
      this->stop_va_animation(true);
    }
  }

  void stop_va_animation(bool force = true) {
    this->va_anim_state_ = VA_ANIM_NONE;
    this->needs_clear_ = true;
    ESP_LOGD(MATRIX_TAG, "Stopped Voice Assistant LED animation (idle effect: %s)", this->get_canonical_effect_name());
  }

  // --- Show Pattern / Test ---

  void show_digit(uint8_t val, int xd, int yd, uint8_t r, uint8_t g, uint8_t b) {
    if (val > 9) return;
    for (int p = 0; p < 15; p++) {
      int xt = p % 3;
      int yt = p / 3;
      if (MATRIX_DIGIT_FONTS[val][p]) {
        this->set_pixel(xt + xd, 6 - yt - yd, r, g, b);
      }
    }
  }

  void show_number(int val, uint8_t r, uint8_t g, uint8_t b) {
    this->clear();
    int abs_val = std::abs(val);
    int tens = abs_val / 10;
    int units = abs_val % 10;
    if (abs_val > 9) {
      this->show_digit(tens, 0, 1, r, g, b);
    }
    this->show_digit(units, 4, 1, r, g, b);
  }

  // --- Hardware Transmission ---

  void send_frame_to_hardware_() {
    // 1. Send sync frame (17 bytes: 7, 127, 127...)
    static const uint8_t SYNC_FRAME[17] = {
        7, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127
    };
    Serial2.write(SYNC_FRAME, 17);
    Serial2.flush();
    ::delayMicroseconds(2000);

    // 2. Compute average color for the 8th ambient pixel
    uint32_t total_r = 0, total_g = 0, total_b = 0;
    for (int y = 0; y < HEIGHT; y++) {
      for (int x = 0; x < WIDTH; x++) {
        total_r += this->pixels_[y][x].r;
        total_g += this->pixels_[y][x].g;
        total_b += this->pixels_[y][x].b;
      }
    }
    uint8_t avg_r = this->correct_color_(total_r / 49);
    uint8_t avg_g = this->correct_color_(total_g / 49);
    uint8_t avg_b = this->correct_color_(total_b / 49);

    uint8_t ambient_b0 = 0, ambient_b1 = 0;
    this->rgb_to_bytes_(avg_r, avg_g, avg_b, ambient_b0, ambient_b1);

    // 3. Send 7 columns (each column = 1 col byte + 16 RGB bytes = 17 bytes)
    uint8_t col_buf[17];
    for (int col = 0; col < WIDTH; col++) {
      col_buf[0] = static_cast<uint8_t>(col);
      int buf_idx = 1;
      for (int row = 0; row < HEIGHT; row++) {
        // Apply pi-top PULSE default rotation mapping: (6 - col, row)
        MatrixRGB p = this->pixels_[6 - col][row];
        uint8_t r = this->correct_color_(p.r);
        uint8_t g = this->correct_color_(p.g);
        uint8_t b = this->correct_color_(p.b);
        uint8_t b0 = 0, b1 = 0;
        this->rgb_to_bytes_(r, g, b, b0, b1);
        col_buf[buf_idx++] = b0;
        col_buf[buf_idx++] = b1;
      }
      // 8th ambient pixel
      col_buf[buf_idx++] = ambient_b0;
      col_buf[buf_idx++] = ambient_b1;

      Serial2.write(col_buf, 17);
      Serial2.flush();
      ::delayMicroseconds(2000);
    }
  }

 private:
  bool is_initialised_{false};
  bool is_power_on_{true};
  bool needs_clear_{false};
  float brightness_{0.6f};
  uint32_t last_frame_time_{0};
  uint32_t frame_interval_ms_{35}; // ~28 FPS smooth animation
  uint32_t effect_step_{0};

  MatrixRGB pixels_[HEIGHT][WIDTH];
  MatrixRGB solid_color_{0, 180, 255}; // Default Cyan
  MatrixEffectType current_effect_{EFFECT_OFF};

  VoiceAnimationState va_anim_state_{VA_ANIM_NONE};
  uint32_t va_anim_start_{0};
  uint32_t va_step_{0};

  uint8_t correct_color_(uint32_t val) const {
    uint32_t scaled = static_cast<uint32_t>(roundf(val * this->brightness_));
    if (scaled > 255) scaled = 255;
    return MATRIX_GAMMA_LUT[scaled];
  }

  inline void rgb_to_bytes_(uint8_t r, uint8_t g, uint8_t b, uint8_t &b0, uint8_t &b1) const {
    b0 = ((r >> 3) & 0x1F) | ((g >> 1) & 0x60);
    b1 = ((b >> 3) & 0x1F) | ((g << 2) & 0xE0);
  }

  // --- Effect Renderers ---

  void render_va_animation_(uint32_t now) {
    this->clear();
    this->va_step_++;

    if (this->va_anim_state_ == VA_ANIM_CONNECTED) {
      uint32_t elapsed = now - this->va_anim_start_;
      if (elapsed > 2000) {
        this->va_anim_state_ = VA_ANIM_NONE;
        this->needs_clear_ = true;
        ESP_LOGD(MATRIX_TAG, "Connected animation completed after 2.0s.");
        return;
      }

      float progress = (elapsed % 1000) / 1000.0f;
      float radius = progress * 5.0f;
      float fade = 1.0f - (elapsed / 2000.0f);

      for (int y = 0; y < 7; y++) {
        for (int x = 0; x < 7; x++) {
          float dist = sqrtf((x - 3) * (x - 3) + (y - 3) * (y - 3));
          float diff = fabsf(dist - radius);
          if (diff < 1.4f) {
            float intensity = 1.0f - (diff / 1.4f);
            uint8_t val = static_cast<uint8_t>(255 * intensity * fade);
            this->set_pixel(x, y, 0, val, val); // Cyan wave
          }
        }
      }
    } else if (this->va_anim_state_ == VA_ANIM_LISTENING) {
      // Rotating amber/cyan glowing perimeter ring
      float angle = (this->va_step_ * 0.2f);
      int center_pulse = static_cast<int>(127 + 127 * sinf(this->va_step_ * 0.25f));

      // Center glowing core
      this->set_pixel(3, 3, 0, center_pulse, center_pulse);

      // Perimeter orbit
      for (int i = 0; i < 24; i++) {
        int px = 0, py = 0;
        if (i < 7) { px = i; py = 0; }
        else if (i < 13) { px = 6; py = i - 6; }
        else if (i < 19) { px = 18 - i; py = 6; }
        else { px = 0; py = 24 - i; }

        float pixel_angle = (i / 24.0f) * 2.0f * M_PI;
        float brightness = 0.5f + 0.5f * cosf(pixel_angle - angle);
        uint8_t r = static_cast<uint8_t>(255 * brightness);
        uint8_t g = static_cast<uint8_t>(160 * brightness);
        this->set_pixel(px, py, r, g, 0); // Amber listening halo
      }
    } else if (this->va_anim_state_ == VA_ANIM_SPEAKING) {
      // Dynamic audio wave / equalizer response
      for (int x = 0; x < 7; x++) {
        float h = 1.0f + 5.0f * (0.5f + 0.5f * sinf(this->va_step_ * 0.35f + x * 0.9f));
        int height = static_cast<int>(roundf(h));
        for (int y = 0; y < height; y++) {
          uint8_t g = 255 - (y * 30);
          uint8_t b = 100 + (y * 25);
          this->set_pixel(x, 6 - y, 0, g, b); // Emerald to cyan bounce
        }
      }
    }
  }

  void render_effect_(uint32_t now) {
    this->effect_step_++;

    switch (this->current_effect_) {
      case EFFECT_SOLID:
        this->set_all(this->solid_color_.r, this->solid_color_.g, this->solid_color_.b);
        break;

      case EFFECT_RAINBOW: {
        for (int y = 0; y < 7; y++) {
          for (int x = 0; x < 7; x++) {
            float hue = fmodf((this->effect_step_ * 4.0f) + (x + y) * 25.0f, 360.0f);
            MatrixRGB c = this->hsv_to_rgb_(hue, 1.0f, 1.0f);
            this->set_pixel(x, y, c.r, c.g, c.b);
          }
        }
        break;
      }

      case EFFECT_RING_PULSE: {
        this->clear();
        float radius = fmodf(this->effect_step_ * 0.15f, 4.5f);
        for (int y = 0; y < 7; y++) {
          for (int x = 0; x < 7; x++) {
            float dist = sqrtf((x - 3) * (x - 3) + (y - 3) * (y - 3));
            float diff = fabsf(dist - radius);
            if (diff < 1.0f) {
              float intensity = 1.0f - diff;
              MatrixRGB c = this->hsv_to_rgb_(fmodf(this->effect_step_ * 3.0f, 360.0f), 1.0f, intensity);
              this->set_pixel(x, y, c.r, c.g, c.b);
            }
          }
        }
        break;
      }

      case EFFECT_MATRIX_RAIN: {
        static uint8_t drops[7] = {0, 3, 5, 1, 6, 2, 4};
        this->clear();
        for (int x = 0; x < 7; x++) {
          if ((this->effect_step_ % 2) == 0) {
            drops[x] = (drops[x] + 1) % 10;
          }
          int head = drops[x];
          if (head < 7) {
            this->set_pixel(x, head, 180, 255, 180); // Bright white-green head
          }
          if (head - 1 >= 0 && head - 1 < 7) {
            this->set_pixel(x, head - 1, 0, 200, 20); // Darker green trail
          }
          if (head - 2 >= 0 && head - 2 < 7) {
            this->set_pixel(x, head - 2, 0, 80, 0); // Dim tail
          }
        }
        break;
      }

      case EFFECT_FIRE: {
        static uint8_t heat[7][7] = {0};
        // Base heat source
        for (int x = 0; x < 7; x++) {
          heat[6][x] = (rand() % 120) + 135;
        }
        // Propagate upward with cooling
        for (int y = 0; y < 6; y++) {
          for (int x = 0; x < 7; x++) {
            int left = (x > 0) ? heat[y + 1][x - 1] : heat[y + 1][x];
            int mid = heat[y + 1][x];
            int right = (x < 6) ? heat[y + 1][x + 1] : heat[y + 1][x];
            int val = ((left + mid * 2 + right) / 4) - (rand() % 25);
            heat[y][x] = std::max(0, val);
          }
        }
        for (int y = 0; y < 7; y++) {
          for (int x = 0; x < 7; x++) {
            uint8_t h = heat[y][x];
            // Heat to fire color mapping
            uint8_t r = h;
            uint8_t g = (h > 100) ? (h - 100) * 1.6f : 0;
            uint8_t b = (h > 220) ? (h - 220) * 4 : 0;
            this->set_pixel(x, y, r, g, b);
          }
        }
        break;
      }

      case EFFECT_RADAR: {
        this->clear();
        float angle = this->effect_step_ * 0.12f;
        for (int r = 1; r <= 3; r++) {
          int x = 3 + static_cast<int>(roundf(r * cosf(angle)));
          int y = 3 + static_cast<int>(roundf(r * sinf(angle)));
          this->set_pixel(x, y, 0, 255, 120);
        }
        // Center blip
        this->set_pixel(3, 3, 200, 255, 200);
        break;
      }

      case EFFECT_DIGITS: {
        int current_digit = (this->effect_step_ / 25) % 10;
        this->clear();
        this->show_digit(current_digit, 2, 1, 0, 220, 255); // Centered Cyan digit
        break;
      }

      case EFFECT_EQUALIZER: {
        this->clear();
        for (int x = 0; x < 7; x++) {
          int h = 1 + (rand() % 7);
          for (int y = 0; y < h; y++) {
            uint8_t r = (y > 4) ? 255 : (y * 40);
            uint8_t g = (y > 4) ? (255 - (y - 4) * 100) : 220;
            this->set_pixel(x, 6 - y, r, g, 30);
          }
        }
        break;
      }

      case EFFECT_SPARKLE: {
        if ((this->effect_step_ % 3) == 0) {
          int rx = rand() % 7;
          int ry = rand() % 7;
          MatrixRGB c = this->hsv_to_rgb_(rand() % 360, 0.9f, 1.0f);
          this->pixels_[ry][rx] = c;
        }
        // Fade existing pixels
        for (int y = 0; y < 7; y++) {
          for (int x = 0; x < 7; x++) {
            this->pixels_[y][x].r = (this->pixels_[y][x].r * 85) / 100;
            this->pixels_[y][x].g = (this->pixels_[y][x].g * 85) / 100;
            this->pixels_[y][x].b = (this->pixels_[y][x].b * 85) / 100;
          }
        }
        break;
      }

      default:
        break;
    }
  }

  MatrixRGB hsv_to_rgb_(float h, float s, float v) const {
    float c = v * s;
    float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;
    float r = 0, g = 0, b = 0;
    if (h < 60) { r = c; g = x; }
    else if (h < 120) { r = x; g = c; }
    else if (h < 180) { g = c; b = x; }
    else if (h < 240) { g = x; b = c; }
    else if (h < 300) { r = x; b = c; }
    else { r = c; b = x; }
    return {
      static_cast<uint8_t>((r + m) * 255),
      static_cast<uint8_t>((g + m) * 255),
      static_cast<uint8_t>((b + m) * 255)
    };
  }
};
