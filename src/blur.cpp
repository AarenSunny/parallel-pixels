#include "parallel_pixels/blur.hpp"

#include <algorithm>
#include <array>
#include <condition_variable>
#include <cstdint>
#include <exception>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

namespace parallel_pixels {
namespace {

struct KernelSpec {
  std::array<std::array<unsigned, 3>, 3> weights;
  unsigned divisor;
};

constexpr KernelSpec kGaussianKernel{
    .weights = {{{1, 2, 1}, {2, 4, 2}, {1, 2, 1}}},
    .divisor = 16,
};
constexpr KernelSpec kBoxKernel{
    .weights = {{{1, 1, 1}, {1, 1, 1}, {1, 1, 1}}},
    .divisor = 9,
};

const KernelSpec& kernel_spec(BlurKernel kernel) {
  switch (kernel) {
    case BlurKernel::gaussian:
      return kGaussianKernel;
    case BlurKernel::box:
      return kBoxKernel;
  }
  throw std::invalid_argument("unknown blur kernel");
}

std::size_t clamped_offset(std::size_t value, int offset, std::size_t upper_bound) {
  if (offset < 0 && value == 0) return 0;
  if (offset > 0 && value + 1 >= upper_bound) return upper_bound - 1;
  if (offset < 0) return value - 1;
  if (offset > 0) return value + 1;
  return value;
}

void blur_rows(const GrayImage& input, GrayImage& output, const KernelSpec& kernel,
               std::size_t begin_row, std::size_t end_row) {
  for (std::size_t y = begin_row; y < end_row; ++y) {
    for (std::size_t x = 0; x < input.width(); ++x) {
      unsigned weighted_sum = 0;
      for (int kernel_y = -1; kernel_y <= 1; ++kernel_y) {
        const auto source_y = clamped_offset(y, kernel_y, input.height());
        for (int kernel_x = -1; kernel_x <= 1; ++kernel_x) {
          const auto source_x = clamped_offset(x, kernel_x, input.width());
          const auto weight = kernel.weights[static_cast<std::size_t>(kernel_y + 1)]
                                            [static_cast<std::size_t>(kernel_x + 1)];
          weighted_sum += weight * input.at(source_x, source_y);
        }
      }
      output.at(x, y) = static_cast<std::uint8_t>(weighted_sum / kernel.divisor);
    }
  }
}

}  // namespace

struct BlurWorkerPool::Impl {
  explicit Impl(std::size_t requested_workers) {
    if (requested_workers == 0) throw std::invalid_argument("worker count must be positive");
    if (requested_workers > kMaximumWorkerCount) {
      throw std::invalid_argument("worker count exceeds the safety cap");
    }
    try {
      workers.reserve(requested_workers);
      for (std::size_t index = 0; index < requested_workers; ++index) {
        workers.emplace_back([this, index] { worker_loop(index); });
      }
    } catch (...) {
      {
        std::lock_guard lock(state_mutex);
        stopping = true;
      }
      work_ready.notify_all();
      // Join any threads created before construction failed while the shared
      // state they observe is still alive.
      workers.clear();
      throw;
    }
  }

  ~Impl() {
    {
      std::lock_guard lock(state_mutex);
      stopping = true;
    }
    work_ready.notify_all();
    // The synchronization state is declared after workers and would otherwise
    // be destroyed first. Join here, in the destructor body, so no worker can
    // observe partially destroyed state.
    workers.clear();
  }

  GrayImage apply(const GrayImage& source, BlurKernel kernel) {
    std::lock_guard operation_lock(operation_mutex);
    GrayImage result(source.width(), source.height());
    {
      std::lock_guard state_lock(state_mutex);
      input = &source;
      output = &result;
      spec = &kernel_spec(kernel);
      active_workers = std::min(workers.size(), source.height());
      completed_workers = 0;
      worker_error = nullptr;
      ++generation;
    }
    work_ready.notify_all();

    std::unique_lock state_lock(state_mutex);
    work_complete.wait(state_lock, [this] { return completed_workers == workers.size(); });
    if (worker_error) std::rethrow_exception(worker_error);
    return result;
  }

  void worker_loop(std::size_t index) {
    std::size_t observed_generation = 0;
    while (true) {
      const GrayImage* source = nullptr;
      GrayImage* result = nullptr;
      const KernelSpec* selected_kernel = nullptr;
      std::size_t begin_row = 0;
      std::size_t end_row = 0;
      bool has_rows = false;
      {
        std::unique_lock state_lock(state_mutex);
        work_ready.wait(state_lock,
                        [this, observed_generation] { return stopping || generation != observed_generation; });
        if (stopping) return;
        observed_generation = generation;
        source = input;
        result = output;
        selected_kernel = spec;
        has_rows = index < active_workers;
        if (has_rows) {
          const auto base_rows = source->height() / active_workers;
          const auto extra_rows = source->height() % active_workers;
          begin_row = index * base_rows + std::min(index, extra_rows);
          end_row = begin_row + base_rows + (index < extra_rows ? 1U : 0U);
        }
      }

      try {
        if (has_rows) blur_rows(*source, *result, *selected_kernel, begin_row, end_row);
      } catch (...) {
        std::lock_guard state_lock(state_mutex);
        if (!worker_error) worker_error = std::current_exception();
      }

      {
        std::lock_guard state_lock(state_mutex);
        ++completed_workers;
        if (completed_workers == workers.size()) work_complete.notify_one();
      }
    }
  }

  std::mutex operation_mutex;
  std::mutex state_mutex;
  std::condition_variable work_ready;
  std::condition_variable work_complete;
  std::vector<std::jthread> workers;
  const GrayImage* input = nullptr;
  GrayImage* output = nullptr;
  const KernelSpec* spec = nullptr;
  std::exception_ptr worker_error;
  std::size_t active_workers = 0;
  std::size_t completed_workers = 0;
  std::size_t generation = 0;
  bool stopping = false;
};

BlurWorkerPool::BlurWorkerPool(std::size_t worker_count) : impl_(std::make_unique<Impl>(worker_count)) {}

BlurWorkerPool::~BlurWorkerPool() = default;

GrayImage BlurWorkerPool::blur(const GrayImage& input, BlurKernel kernel) {
  return impl_->apply(input, kernel);
}

std::size_t BlurWorkerPool::worker_count() const noexcept {
  return impl_->workers.size();
}

GrayImage blur_scalar(const GrayImage& input, BlurKernel kernel) {
  GrayImage output(input.width(), input.height());
  blur_rows(input, output, kernel_spec(kernel), 0, input.height());
  return output;
}

GrayImage gaussian_blur_scalar(const GrayImage& input) {
  return blur_scalar(input, BlurKernel::gaussian);
}

std::size_t effective_worker_count(const GrayImage& input, std::size_t requested_worker_count) {
  if (requested_worker_count == 0) throw std::invalid_argument("worker count must be positive");
  if (requested_worker_count > kMaximumWorkerCount) {
    throw std::invalid_argument("worker count exceeds the safety cap");
  }
  return std::min(requested_worker_count, input.height());
}

GrayImage blur_parallel(const GrayImage& input, BlurKernel kernel, std::size_t worker_count) {
  GrayImage output(input.width(), input.height());
  const auto active_workers = effective_worker_count(input, worker_count);
  const auto& spec = kernel_spec(kernel);
  const auto base_rows = input.height() / active_workers;
  const auto extra_rows = input.height() % active_workers;
  std::vector<std::jthread> workers;
  workers.reserve(active_workers);
  std::size_t begin_row = 0;
  for (std::size_t worker = 0; worker < active_workers; ++worker) {
    const auto row_count = base_rows + (worker < extra_rows ? 1U : 0U);
    const auto end_row = begin_row + row_count;
    workers.emplace_back(blur_rows, std::cref(input), std::ref(output), std::cref(spec), begin_row,
                         end_row);
    begin_row = end_row;
  }
  workers.clear();
  return output;
}

GrayImage gaussian_blur_parallel(const GrayImage& input, std::size_t worker_count) {
  return blur_parallel(input, BlurKernel::gaussian, worker_count);
}

std::size_t default_worker_count() noexcept {
  const auto detected = std::thread::hardware_concurrency();
  return detected == 0 ? 1 : std::min(static_cast<std::size_t>(detected), kMaximumWorkerCount);
}

}  // namespace parallel_pixels
