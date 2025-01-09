#pragma once

#include <string>
#include <filesystem>

#include "image.h"

extern uint64_t own_age;

void to_file(const std::filesystem::path &path, void *buffer, int size);
std::string readFile(const std::filesystem::path &path);
int decompress(const void *input, int insize, void *output, int outsize);

int load_png(const std::filesystem::path &path, Image &img);
void write_png(const std::filesystem::path &path, const Image &img);

uint64_t get_file_age(const std::filesystem::path &path);
uint64_t get_own_age(void);
bool need_update(const std::filesystem::path &input, const std::filesystem::path &output);
