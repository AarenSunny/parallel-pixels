#include "parallel_pixels/pgm.hpp"

#include <cstddef>
#include <cctype>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace parallel_pixels {
namespace {

class HeaderParser {
 public:
  explicit HeaderParser(const std::vector<std::uint8_t>& bytes) : bytes_(bytes) {}

  std::string token() {
    skip_separators();
    const auto begin = position_;
    while (position_ < bytes_.size() && !is_space(bytes_[position_]) && bytes_[position_] != '#') {
      ++position_;
    }
    if (begin == position_) throw std::runtime_error("PGM header ended unexpectedly");
    return std::string(bytes_.begin() + static_cast<std::ptrdiff_t>(begin),
                       bytes_.begin() + static_cast<std::ptrdiff_t>(position_));
  }

  std::size_t consume_raster_separator() {
    if (position_ >= bytes_.size() || !is_space(bytes_[position_])) {
      throw std::runtime_error("PGM header is missing its raster separator");
    }
    if (bytes_[position_] == '\r' && position_ + 1 < bytes_.size() && bytes_[position_ + 1] == '\n') {
      position_ += 2;
    } else {
      ++position_;
    }
    return position_;
  }

 private:
  static bool is_space(std::uint8_t value) {
    return std::isspace(static_cast<unsigned char>(value)) != 0;
  }

  void skip_separators() {
    while (position_ < bytes_.size()) {
      if (is_space(bytes_[position_])) {
        ++position_;
      } else if (bytes_[position_] == '#') {
        while (position_ < bytes_.size() && bytes_[position_] != '\n') ++position_;
      } else {
        return;
      }
    }
  }

  const std::vector<std::uint8_t>& bytes_;
  std::size_t position_ = 0;
};

std::size_t parse_dimension(const std::string& token, const char* label) {
  std::size_t consumed = 0;
  unsigned long long value = 0;
  try {
    value = std::stoull(token, &consumed);
  } catch (const std::exception&) {
    throw std::runtime_error(std::string("invalid PGM ") + label);
  }
  if (consumed != token.size() || value == 0 || value > std::numeric_limits<std::size_t>::max()) {
    throw std::runtime_error(std::string("invalid PGM ") + label);
  }
  return static_cast<std::size_t>(value);
}

}  // namespace

GrayImage read_pgm(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) throw std::runtime_error("could not open input PGM: " + path.string());
  const std::vector<std::uint8_t> bytes(std::istreambuf_iterator<char>(input), {});
  HeaderParser parser(bytes);
  if (parser.token() != "P5") throw std::runtime_error("only binary P5 PGM images are supported");
  const auto width = parse_dimension(parser.token(), "width");
  const auto height = parse_dimension(parser.token(), "height");
  if (parser.token() != "255") throw std::runtime_error("only 8-bit PGM images are supported");
  const auto raster_start = parser.consume_raster_separator();
  if (width > std::numeric_limits<std::size_t>::max() / height) {
    throw std::runtime_error("PGM dimensions overflow addressable memory");
  }
  const auto pixel_count = width * height;
  if (bytes.size() - raster_start != pixel_count) throw std::runtime_error("PGM raster size is incorrect");
  return GrayImage(width, height, {bytes.begin() + static_cast<std::ptrdiff_t>(raster_start), bytes.end()});
}

void write_pgm(const std::filesystem::path& path, const GrayImage& image) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output) throw std::runtime_error("could not open output PGM: " + path.string());
  output << "P5\n" << image.width() << ' ' << image.height() << "\n255\n";
  output.write(reinterpret_cast<const char*>(image.pixels().data()),
               static_cast<std::streamsize>(image.pixels().size()));
  if (!output) throw std::runtime_error("failed to write output PGM: " + path.string());
}

}  // namespace parallel_pixels
