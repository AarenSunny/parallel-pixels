#include "parallel_pixels/blur.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <thread>
#include <vector>

namespace parallel_pixels {
namespace {

constexpr std::array<std::array<unsigned, 3>, 3> kGaussianKernel{{
    {{1, 2, 1}},
    {{2, 4, 2}},
    {{1, 2, 1}},
}};

std::size_t clamped_offset(std::size_t value, int offset, std::size_t upper_bound) {
  if (offset < 0 && value == 0) return 0;
  if (offset > 0 && value + 1 >= upper_bound) return upper_bound - 1;
  if (offset < 0) return value - 1;
  if (offset > 0) return value + 1;
  return value;
}

void blur_rows(const GrayImage& input, GrayImage& output, std::size_t begin_row, std::size_t end_row) {
  for (std::size_t y = begin_row; y < end_row; ++y) {
    for (std::size_t x = 0; x < input.width(); ++x) {
      unsigned weighted_sum = 0;
      for (int kernel_y = -1; kernel_y <= 1; ++kernel_y) {
        const auto source_y = clamped_offset(y, kernel_y, input.height());
        for (int kernel_x = -1; kernel_x <= 1; ++kernel_x) {
          const auto source_x = clamped_offset(x, kernel_x, input.width());
          const auto weight = kGaussianKernel[static_cast<std::size_t>(kernel_y + 1)]
                                                     [static_cast<std::size_t>(kernel_x + 1)];
          weighted_sum += weight * input.at(source_x, source_y);
        }
      }
      output.at(x, y) = static_cast<std::uint8_t>(weighted_sum / 16U);
    }
  }
}

}  // namespace

GrayImage gaussian_blur_scalar(const GrayImage& input) {
  GrayImage output(input.width(), input.height());
  blur_rows(input, output, 0, input.height());
  return output;
}

std::size_t effective_worker_count(const GrayImage& input, std::size_t requested_worker_count) {
  if (requested_worker_count == 0) throw std::invalid_argument("worker count must be positive");
  if (requested_worker_count > kMaximumWorkerCount) {
    throw std::invalid_argument("worker count exceeds the safety cap");
  }
  return std::min(requested_worker_count, input.height());
}

GrayImage gaussian_blur_parallel(const GrayImage& input, std::size_t worker_count) {
  GrayImage output(input.width(), input.height());
  const auto active_workers = effective_worker_count(input, worker_count);
  const auto base_rows = input.height() / active_workers;
  const auto extra_rows = input.height() % active_workers;
  std::vector<std::jthread> workers;
  workers.reserve(active_workers);
  std::size_t begin_row = 0;
  for (std::size_t worker = 0; worker < active_workers; ++worker) {
    const auto row_count = base_rows + (worker < extra_rows ? 1U : 0U);
    const auto end_row = begin_row + row_count;
    workers.emplace_back(blur_rows, std::cref(input), std::ref(output), begin_row, end_row);
    begin_row = end_row;
  }
  workers.clear();
  return output;
}

std::size_t default_worker_count() noexcept {
  const auto detected = std::thread::hardware_concurrency();
  return detected == 0 ? 1 : std::min(static_cast<std::size_t>(detected), kMaximumWorkerCount);
}

}  // namespace parallel_pixels
