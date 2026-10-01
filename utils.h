#pragma once

#include <string>
#include <filesystem>
#include <regex>
#include <filesystem>

#include "image.h"

extern uint64_t own_age;

void to_file(const std::filesystem::path &path, const void *buffer, int size);
std::string readFile(const std::filesystem::path &path);
int decompress(const void *input, int insize, void *output, int outsize);

int load_png(const std::filesystem::path &path, Image &img);
void write_png(const std::filesystem::path &path, const Image &img);

uint64_t get_file_age(const std::filesystem::path &path);
uint64_t get_own_age(void);
bool need_update(const std::filesystem::path &input, const std::filesystem::path &output);

template <typename F> void for_each_dimension(const std::filesystem::path savepath, F func) {
  std::filesystem::directory_iterator i{savepath};
  std::regex pattern("^(DIM_SPACESTATION|DIM|PERSONAL_DIM_)([-0-9]+)$");
  std::smatch matches;

  func(savepath, 0);

  for (auto &ent : i) {
    auto &p = ent.path();
    if (ent.is_directory()) {
      const std::string filename = p.stem().string();
      if (regex_search(filename, matches, pattern)) {
        func(p, atoi(matches[2].str().c_str()));
      }
    }
  }
}
