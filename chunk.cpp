#include <string.h>
#include <stdio.h>

#include "chunk.h"

Chunk::Chunk() {
  for (int i=0; i<16; i++) {
    sections[i] = NULL;
  }
}

Chunk::~Chunk() {
  reset();
}

void Chunk::ensureSection(int section) {
  if (sections[section] == NULL) {
    sections[section] = new Section();
  }
}

void Chunk::updateBlocks(int section_num, const uint8_t data[16][16][16]) {
  ensureSection(section_num);
  Section *section = sections[section_num];
  for (int x=0; x<16; x++) {
    for (int y=0; y<16; y++) {
      for (int z=0; z<16; z++) {
        uint16_t old = section->blocks[x][y][z];
        old &= 0xff00;
        old |= data[y][z][x];
        section->blocks[x][y][z] = old;
        //if ((x==6) && (z==9)) printf("%d %d %d, %d/%x\n", x, (section_num*16)+y, z, data[y][x][z], data[y][x][z]);
      }
    }
  }
}

void Chunk::updateAddBlocks(int section_num, const uint8_t data[16][16][8]) {
  ensureSection(section_num);
  Section *section = sections[section_num];
  for (int x=0; x<16; x++) {
    for (int y=0; y<16; y++) {
      for (int z=0; z<16; z++) {
        uint16_t old = section->blocks[x][y][z];
        old &= 0xff;
        uint8_t byte = data[y][z][x/2];
        if ((x%2) == 0) byte &= 0xf;
        else byte = byte >> 4;
        old |= byte << 8;
        section->blocks[x][y][z] = old;
      }
    }
  }
}

void Chunk::printSection(int section_num) {
  ensureSection(section_num);
  Section *section = sections[section_num];
  for (int y=0; y<16; y++) {
    printf("section-y %d, y %d\n", y, (section_num*16)+y);
    for (int z=0; z<16; z++) {
      printf("z %2d  ", z);
      for (int x=0; x<16; x++) {
        printf("%3d ", section->blocks[x][y][z]);
      }
      puts("");
    }
  }
}

void Chunk::getTopMostBlocks(uint16_t toplayer[16][16]) {
  //puts("checking for top-most");
  for (int section_num=0; section_num<16; section_num++) {
    //printf("in section %d\n", section_num);
    Section *section = sections[section_num];
    if (!section) continue;

    for (int z=0; z<16; z++) {
      for (int x=0; x<16; x++) {
        for (int y=0; y<16; y++) {
          if (section->blocks[x][y][z] == 0) continue; // ignore air

          toplayer[x][z] = section->blocks[x][y][z];
        }
      }
    }
  }
  //puts("done finding top-most");
}

void Chunk::reset() {
  for (int i=0; i<16; i++) {
    if (sections[i]) {
      delete sections[i];
      sections[i] = NULL;
    }
  }
}
