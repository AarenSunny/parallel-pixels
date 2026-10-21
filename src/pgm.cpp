#include "parallel_pixels/pgm.hpp"

#include <cstddef>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>

namespace parallel_pixels {
namespace {

class HeaderParser {
 public:
  explicit HeaderParser(std::istream& input) : input_(input) {}

  std::string token() {
    skip_separators();
    std::string value;
    while (true) {
      const auto next = input_.peek();
      if (next == std::char_traits<char>::eof() || is_space(next) || next == '#') break;
      value.push_back(static_cast<char>(input_.get()));
    }
    if (value.empty()) throw std::runtime_error("PGM header ended unexpectedly");
    return value;
  }

  void consume_raster_separator() {
    const auto separator = input_.peek();
    if (separator == std::char_traits<char>::eof() || !is_space(separator)) {
      throw std::runtime_error("PGM header is missing its raster separator");
    }
    input_.get();
    if (separator == '\r' && input_.peek() == '\n') input_.get();
  }

 private:
  static bool is_space(int value) {
    return std::isspace(static_cast<unsigned char>(value)) != 0;
  }

  void skip_separators() {
    while (true) {
      const auto next = input_.peek();
      if (next == std::char_traits<char>::eof()) return;
      if (is_space(next)) {
        input_.get();
      } else if (next == '#') {
        input_.get();
        while (input_.get() != '\n' && input_) {
        }
      } else {
        return;
      }
    }
  }

  std::istream& input_;
};

std::size_t parse_dimension(const std::string& token, const char* label) {
  for (const auto character : token) {
    if (std::isdigit(static_cast<unsigned char>(character)) == 0) {
      throw std::runtime_error(std::string("invalid PGM ") + label);
    }
  }
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
  HeaderParser parser(input);
  if (parser.token() != "P5") throw std::runtime_error("only binary P5 PGM images are supported");
  const auto width = parse_dimension(parser.token(), "width");
  const auto height = parse_dimension(parser.token(), "height");
  if (parser.token() != "255") throw std::runtime_error("only 8-bit PGM images are supported");
  parser.consume_raster_separator();
  if (width > std::numeric_limits<std::size_t>::max() / height) {
    throw std::runtime_error("PGM dimensions overflow addressable memory");
  }
  const auto pixel_count = width * height;
  if (pixel_count > static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max())) {
    throw std::runtime_error("PGM raster is too large to read");
  }
  const auto raster_start = input.tellg();
  input.seekg(0, std::ios::end);
  const auto raster_end = input.tellg();
  if (raster_start == std::streampos(-1) || raster_end < raster_start ||
      static_cast<std::uintmax_t>(raster_end - raster_start) != pixel_count) {
    throw std::runtime_error("PGM raster size is incorrect");
  }
  input.seekg(raster_start);
  if (!input) throw std::runtime_error("failed to seek input PGM: " + path.string());
  GrayImage image(width, height);
  input.read(reinterpret_cast<char*>(image.pixels().data()), static_cast<std::streamsize>(pixel_count));
  if (input.gcount() != static_cast<std::streamsize>(pixel_count)) {
    throw std::runtime_error("PGM raster size is incorrect");
  }
  char trailing_byte = 0;
  if (input.get(trailing_byte)) throw std::runtime_error("PGM raster size is incorrect");
  if (input.bad()) throw std::runtime_error("failed to read input PGM: " + path.string());
  return image;
}

void write_pgm(const std::filesystem::path& path, const GrayImage& image) {
  if (image.pixels().size() >
      static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max())) {
    throw std::runtime_error("PGM raster is too large to write");
  }
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output) throw std::runtime_error("could not open output PGM: " + path.string());
  output << "P5\n" << image.width() << ' ' << image.height() << "\n255\n";
  output.write(reinterpret_cast<const char*>(image.pixels().data()),
               static_cast<std::streamsize>(image.pixels().size()));
  if (!output) throw std::runtime_error("failed to write output PGM: " + path.string());
}

}  // namespace parallel_pixels
