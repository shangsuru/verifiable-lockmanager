#pragma once

#include <string>

auto is_base64(unsigned char c) -> bool;

auto base64_encode(unsigned char const *bytes_to_encode, unsigned int in_len)
    -> std::string;

auto base64_decode(std::string const &encoded_string) -> std::string;