#pragma once

#include <fstream>
#include <iostream>

size_t get_file_size(const char *filename);

bool read_file_to_buf(const char *filename, uint8_t *buf, size_t bsize);

bool write_buf_to_file(const char *filename, const uint8_t *buf, size_t bsize, long offset);