#pragma once

#include <cstddef>
#include <memory>

#include "parallel_pixels/image.hpp"

namespace parallel_pixels {

inline constexpr std::size_t kMaximumWorkerCount = 256;

enum class BlurKernel { gaussian, box };

class BlurWorkerPool {
 public:
  explicit BlurWorkerPool(std::size_t worker_count);
  ~BlurWorkerPool();

  BlurWorkerPool(const BlurWorkerPool&) = delete;
  BlurWorkerPool& operator=(const BlurWorkerPool&) = delete;
  BlurWorkerPool(BlurWorkerPool&&) = delete;
  BlurWorkerPool& operator=(BlurWorkerPool&&) = delete;

  // Concurrent callers are safe, but each blur runs to completion before the
  // next caller dispatches work to the shared worker set.
  [[nodiscard]] GrayImage blur(const GrayImage& input, BlurKernel kernel);
  [[nodiscard]] std::size_t worker_count() const noexcept;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

[[nodiscard]] GrayImage blur_scalar(const GrayImage& input, BlurKernel kernel);
[[nodiscard]] GrayImage blur_parallel(const GrayImage& input, BlurKernel kernel,
                                      std::size_t worker_count);
[[nodiscard]] GrayImage gaussian_blur_scalar(const GrayImage& input);
[[nodiscard]] GrayImage gaussian_blur_parallel(const GrayImage& input, std::size_t worker_count);
[[nodiscard]] std::size_t effective_worker_count(const GrayImage& input, std::size_t requested_worker_count);
[[nodiscard]] std::size_t default_worker_count() noexcept;

}  // namespace parallel_pixels
