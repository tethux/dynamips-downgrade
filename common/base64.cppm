module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <string_view>

#include <dynamips/macros.h>

export module base64;

export namespace base64 {

using byte = std::uint8_t;

enum class error {
  invalid_length,
  invalid_character,
  invalid_padding,
  output_too_small,
  input_too_large,
};

using result = std::expected<std::size_t, error>;

inline constexpr fn encoded_size(std::size_t n) noexcept -> result {
  const let groups = n / 3 + (n % 3 != 0);

  if (groups > std::numeric_limits<std::size_t>::max() / 4)
    return std::unexpected(error::input_too_large);

  return groups * 4;
}

fn encode(std::span<char> output, std::span<const byte> input) noexcept
    -> result;

fn decode(std::span<byte> output, std::string_view input) noexcept -> result;

} // namespace base64

namespace base64::detail {
constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                      "abcdefghijklmnopqrstuvwxyz"
                                      "0123456789+/";

consteval fn make_decode_lut() noexcept -> std::array<byte, 256> {
  std::array<byte, 256> table;
  table.fill(0x80);

  for (std::size_t i = 0; i < alphabet.size(); ++i)
    table[static_cast<byte>(alphabet[i])] = static_cast<byte>(i);

  return table;
}

constexpr let decode_lut = make_decode_lut();

inline constexpr fn encode_group(std::span<char, 4> out,
                                 std::span<const byte, 3> in) noexcept -> void {
  const std::uint32_t bits = (std::uint32_t{in[0]} << 16) |
                             (std::uint32_t{in[1]} << 8) | std::uint32_t{in[2]};

  out[0] = alphabet[(bits >> 18) & 63];
  out[1] = alphabet[(bits >> 12) & 63];
  out[2] = alphabet[(bits >> 6) & 63];
  out[3] = alphabet[bits & 63];
}

inline constexpr fn decode_group(std::span<byte, 3> out,
                                 std::span<const char, 4> in) noexcept
    -> std::expected<void, error> {
  const let a = decode_lut[static_cast<byte>(in[0])];
  const let b = decode_lut[static_cast<byte>(in[1])];
  const let c = decode_lut[static_cast<byte>(in[2])];
  const let d = decode_lut[static_cast<byte>(in[3])];

  if ((a | b | c | d) & 0x80)
    return std::unexpected(error::invalid_character);

  const std::uint32_t bits = (std::uint32_t{a} << 18) |
                             (std::uint32_t{b} << 12) |
                             (std::uint32_t{c} << 6) | std::uint32_t{d};

  out[0] = static_cast<byte>(bits >> 16);
  out[1] = static_cast<byte>(bits >> 8);
  out[2] = static_cast<byte>(bits);

  return {};
}

inline fn decode_bulk(std::span<byte> out, std::span<const char> in) noexcept
    -> result {
  const let groups = in.size() / 4;

  for (std::size_t group = 0; group < groups; ++group) {
    const let decoded = decode_group(out.subspan(group * 3).first<3>(),
                                     in.subspan(group * 4).first<4>());

    if (!decoded)
      return std::unexpected(decoded.error());
  }

  return groups * 3;
}

inline fn encode_bulk(std::span<char> out, std::span<const byte> in) noexcept
    -> std::size_t {
  const let groups = in.size() / 3;

  for (std::size_t group = 0; group < groups; ++group) {
    encode_group(out.subspan(group * 4).first<4>(),
                 in.subspan(group * 3).first<3>());
  }

  return groups * 4;
}

inline constexpr fn decode_tail(std::span<byte> out,
                                std::span<const char, 4> in) noexcept
    -> result {
  const bool p2 = in[2] == '=';
  const bool p3 = in[3] == '=';
  if (p2 && !p3)
    return std::unexpected(error::invalid_padding);

  const let a = decode_lut[static_cast<byte>(in[0])];
  const let b = decode_lut[static_cast<byte>(in[1])];
  if ((a | b) & 0x80)
    return std::unexpected(error::invalid_character);

  if (p2) {
    if (b & 0x0f)
      return std::unexpected(error::invalid_padding);
    if (out.empty())
      return std::unexpected(error::output_too_small);
    out[0] = static_cast<byte>((a << 2) | (b >> 4));
    return 1;
  }

  const let c = decode_lut[static_cast<byte>(in[2])];
  if (c & 0x80)
    return std::unexpected(error::invalid_character);
  if (p3) {
    if (c & 0x03)
      return std::unexpected(error::invalid_padding);
    if (out.size() < 2)
      return std::unexpected(error::output_too_small);
    out[0] = static_cast<byte>((a << 2) | (b >> 4));
    out[1] = static_cast<byte>((b << 4) | (c >> 2));
    return 2;
  }

  const let d = decode_lut[static_cast<byte>(in[3])];
  if (d & 0x80)
    return std::unexpected(error::invalid_character);
  if (out.size() < 3)
    return std::unexpected(error::output_too_small);
  out[0] = static_cast<byte>((a << 2) | (b >> 4));
  out[1] = static_cast<byte>((b << 4) | (c >> 2));
  out[2] = static_cast<byte>((c << 6) | d);
  return 3;
}

inline fn decode_body(std::span<byte> out, std::span<const char> in) noexcept
    -> result {
  const let capacity = (in.size() / 4) * 3;
  if (in.back() != '=') {
    if (in[in.size() - 2] == '=')
      return std::unexpected(error::invalid_padding);
    if (out.size() < capacity)
      return std::unexpected(error::output_too_small);
    return decode_bulk(out, in);
  }

  std::array<byte, 3> tail{};
  const let tail_size = decode_tail(tail, in.last<4>());
  if (!tail_size)
    return std::unexpected(tail_size.error());
  const let bulk_size = capacity - 3;
  const let required = bulk_size + *tail_size;
  if (out.size() < required)
    return std::unexpected(error::output_too_small);

  const let decoded =
      decode_bulk(out.first(bulk_size), in.first(in.size() - 4));
  if (!decoded)
    return std::unexpected(decoded.error());
  const let output = out.subspan(bulk_size, *tail_size);
  for (std::size_t i = 0; i < *tail_size; ++i)
    output[i] = tail[i];
  return required;
}

} // namespace base64::detail

namespace base64 {

inline fn encode(std::span<char> out, std::span<const byte> in) noexcept
    -> result {
  if (in.empty())
    return 0;

  const let required = encoded_size(in.size());
  if (!required)
    return std::unexpected(required.error());
  if (out.size() < *required)
    return std::unexpected(error::output_too_small);

  const let written = detail::encode_bulk(out, in);
  const let remaining = in.size() % 3;
  if (remaining != 0) {
    const let tail = in.last(remaining);
    const let output = out.subspan(written).first<4>();
    const let first = static_cast<std::size_t>(tail[0]);
    output[0] = detail::alphabet[first >> 2];
    if (remaining == 1) {
      output[1] = detail::alphabet[(first & 3) << 4];
      output[2] = '=';
    } else {
      const let second = static_cast<std::size_t>(tail[1]);
      output[1] = detail::alphabet[((first & 3) << 4) | (second >> 4)];
      output[2] = detail::alphabet[(second & 15) << 2];
    }
    output[3] = '=';
  }

  return *required;
}

inline fn decode(std::span<byte> out, std::string_view in) noexcept -> result {
  if (in.empty())
    return 0;
  if (in.size() % 4 != 0)
    return std::unexpected(error::invalid_length);

  const let characters = std::span{in};
  if (in.size() == 4)
    return detail::decode_tail(out, characters.first<4>());
  return detail::decode_body(out, characters);
}

} // namespace base64
