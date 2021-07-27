#pragma once

#include <fstream>
#include <iostream>

auto get_file_size(const char *filename) -> size_t;

auto read_file_to_buf(const char *filename, uint8_t *buf, size_t bsize) -> bool;

auto write_buf_to_file(const char *filename, const uint8_t *buf, size_t bsize,
                       long offset) -> bool;