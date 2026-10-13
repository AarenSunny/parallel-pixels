#pragma once

#include <cstddef>

#include "parallel_pixels/image.hpp"

namespace parallel_pixels {

inline constexpr std::size_t kMaximumWorkerCount = 256;

enum class BlurKernel { gaussian, box };

[[nodiscard]] GrayImage blur_scalar(const GrayImage& input, BlurKernel kernel);
[[nodiscard]] GrayImage blur_parallel(const GrayImage& input, BlurKernel kernel,
                                      std::size_t worker_count);
[[nodiscard]] GrayImage gaussian_blur_scalar(const GrayImage& input);
[[nodiscard]] GrayImage gaussian_blur_parallel(const GrayImage& input, std::size_t worker_count);
[[nodiscard]] std::size_t effective_worker_count(const GrayImage& input, std::size_t requested_worker_count);
[[nodiscard]] std::size_t default_worker_count() noexcept;

}  // namespace parallel_pixels
