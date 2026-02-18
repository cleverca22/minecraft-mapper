#pragma once

#include "section.h"

#include <map>
#include <set>
#include <string>

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
  void updateBlocks16(int section, const uint16_t data[16][16][16]);
  void printSection(int section_num);
  void getTopMostBlocks(uint16_t toplayer[16][16], const std::set<std::string> &ignoredBlocks, const std::map<uint32_t,std::string> &blockMap);
private:
  void ensureSection(int section);
  Section *sections[16];
};
