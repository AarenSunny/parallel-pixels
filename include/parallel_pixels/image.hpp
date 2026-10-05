#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace parallel_pixels {

class GrayImage {
 public:
  GrayImage(std::size_t width, std::size_t height)
      : width_(width), height_(height), pixels_(pixel_count(width, height)) {}

  GrayImage(std::size_t width, std::size_t height, std::vector<std::uint8_t> pixels)
      : width_(width), height_(height), pixels_(std::move(pixels)) {
    if (pixels_.size() != pixel_count(width, height)) {
      throw std::invalid_argument("pixel count does not match image dimensions");
    }
  }

  [[nodiscard]] std::size_t width() const noexcept { return width_; }
  [[nodiscard]] std::size_t height() const noexcept { return height_; }
  [[nodiscard]] const std::vector<std::uint8_t>& pixels() const noexcept { return pixels_; }
  [[nodiscard]] std::vector<std::uint8_t>& pixels() noexcept { return pixels_; }

  [[nodiscard]] std::uint8_t at(std::size_t x, std::size_t y) const {
    if (x >= width_ || y >= height_) throw std::out_of_range("pixel coordinate is outside the image");
    return pixels_[y * width_ + x];
  }

  std::uint8_t& at(std::size_t x, std::size_t y) {
    if (x >= width_ || y >= height_) throw std::out_of_range("pixel coordinate is outside the image");
    return pixels_[y * width_ + x];
  }

  friend bool operator==(const GrayImage&, const GrayImage&) = default;

 private:
  static std::size_t pixel_count(std::size_t width, std::size_t height) {
    if (width == 0 || height == 0) throw std::invalid_argument("image dimensions must be positive");
    if (width > std::numeric_limits<std::size_t>::max() / height) {
      throw std::overflow_error("image dimensions overflow addressable memory");
    }
    return width * height;
  }

  std::size_t width_;
  std::size_t height_;
  std::vector<std::uint8_t> pixels_;
};

}  // namespace parallel_pixels
