#pragma once

#include "section.h"

class Chunk {
public:
  // X neg=west, pos=east
  // Y neg=down, pos=up
  // Z neg=north, pos=south
  Chunk();
  ~Chunk();
  void reset();
  void updateAddBlocks(int section, const uint8_t data[16][16][8]);
  void updateBlocks(int section, const uint8_t data[16][16][16]);
  void printSection(int section_num);
  void getTopMostBlocks(uint16_t toplayer[16][16]);
private:
  void ensureSection(int section);
  Section *sections[16];
};
