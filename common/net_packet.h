#pragma once

#include "net.h"
#include <dynamips/macros.h>
#include <array>
#include <expected>
#include <span>

namespace dynamips::packet {
enum class error { truncated, invalid_header, invalid_address, out_of_memory };
using bytes = std::span<const unsigned char>;
fn checksum(bytes data, unsigned int seed = 0) noexcept -> unsigned short;
fn analyze(std::span<unsigned char> frame) noexcept
    -> std::expected<n_pkt_ctx_t, error>;
fn ipv4_prefix(const char *text) noexcept
    -> std::expected<n_ip_network_t, error>;
fn ipv6_prefix(const char *text) noexcept
    -> std::expected<n_ipv6_network_t, error>;
fn ipv4(const char *text) noexcept -> std::expected<n_ip_addr_t, error>;
fn ipv6(const char *text) noexcept -> std::expected<n_ipv6_addr_t, error>;
fn mac(const char *text) noexcept -> std::expected<n_eth_addr_t, error>;
fn board_id(const char *text, int encoding) noexcept
    -> std::expected<std::array<unsigned char, 11>, error>;
} // namespace dynamips::packet
