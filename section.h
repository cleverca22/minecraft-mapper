#pragma once

#include <stdint.h>

class Section {
public:
  Section() {
    memset(blocks, 0, sizeof(blocks));
  }

  uint16_t blocks[16][16][16]; // y x z
};
