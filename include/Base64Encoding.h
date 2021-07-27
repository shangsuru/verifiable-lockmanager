#pragma once

#include <string>

bool is_base64(unsigned char c);

std::string base64_encode(unsigned char const *bytes_to_encode, unsigned int in_len);

std::string base64_decode(std::string const &encoded_string);