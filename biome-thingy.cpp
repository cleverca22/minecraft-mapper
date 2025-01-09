#include <arpa/inet.h>
#include <assert.h>
#include <dirent.h>
#include <iostream>
#include <map>
#include <nlohmann/json.hpp>
#include <png.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <unistd.h>
#include <zlib.h>

#include "nbt.h"
#include "utils.h"
#include "chunk.h"
#include "zooms.h"
#include "image.h"
#include "event_loop.h"

using namespace std;
using json = nlohmann::json;

typedef struct {
  uint32_t chunk_offsets[32][32];
  uint32_t chunk_lastmods[32][32];
} region_header;

typedef struct {
  Image *region_image;
  int xoff;
  int zoff;
  uint16_t toplayer[16][16];
  Chunk chunk;
} chunk_parse_state;

typedef struct {
  uint32_t pending_key;
  string pending_value;
  bool has_key;
  bool has_value;
  map<uint32_t,string> blocks;
} level_parse_state;

typedef struct {
  map<uint32_t,string> blocks;
  json color_table;
} level_state;

// 1.7.10 modded level.dat contains a FML/ItemData array
// within that array is K/V pairs
// byte 0 of the K is 1 for blocks, and 2 for items
// the V is the id# for the block/item
// for chunks, /Level/Sections/${n}/Blocks is an uint8_t[16][16][16] for all blocks
// modded 1.7.10 stores bits 11:8 of the block# in /Level/Sections/${n}/Add, a uint8_t[16][16][8]
// the 12bit number from Blocks and Add is then looked up in FML/ItemData

// in 1.16, each setion has its own palette
// /sections/${n}/block_states/palette/${n}/Name has the name for each block in the palette
// /sections/${n}/block_states/palette/${n}/Properties/level has props, for blocks like logs or kelp
// /sections/${n}/block_states/data is a packed n-bit array of indexes into the pallette, inside an int64[], an index isnt split across 64bit
// so if the palette has between 16 and 31 items, it needs 5bit to represent a block
// a 64bit int can hold 12 x 5bit
// so a section has ciel((16*16*16)/12) int64's

void parseAndLoadNBT(string path, const nbt_callbacks *cb) {
  z_stream strm = {
    .total_in = 0,
    .zalloc = NULL,
    .zfree = NULL,
    .opaque = NULL,
  };
  int ret = inflateInit2(&strm, 16 + MAX_WBITS);
  assert(ret == Z_OK);
  FILE *fd = fopen(path.c_str(), "r");

  unsigned char buffer[8192];
  unsigned char *outbuf = (unsigned char*)malloc(1024*1024*16);
  strm.next_out = outbuf;
  strm.avail_out = 1024 * 1024 * 16;
  strm.total_out = 0;
  while (size_t bytes = fread(buffer, 1, 8192, fd)) {
    strm.next_in = buffer;
    strm.avail_in = bytes;
    strm.total_in += bytes;
    int flags = 0;
    if (bytes < 8192) {
      flags |= Z_FINISH;
    }
    printf("%ld %d %ld %d\n", bytes, strm.avail_out, strm.total_out, strm.avail_in);
    ret = inflate(&strm, flags);
    printf("%ld %d %ld %d %d\n", bytes, strm.avail_out, strm.total_out, strm.avail_in, ret);
    if (ret < 0) {
      printf("%s\n", strm.msg);
      break;
    }
    if (bytes < 8192) break;
  }
  fclose(fd);

  parseNBT(outbuf, strm.total_out, cb, false);
  free(outbuf);
  inflateEnd(&strm);
}

int get_item_index(string key) {
  const char *prefix = "/FML/ItemData/";
  const int prefix_len = strlen(prefix);
  const char *input = key.c_str();

  if (strncmp(input, prefix, prefix_len) == 0) {
    input = input + prefix_len;
    return strtol(input, NULL, 10);
  } else return -1;
}

void check_key(level_parse_state *s) {
  if (s->has_key && s->has_value) {
    uint32_t key = s->pending_key;
    string value = s->pending_value;
    s->has_key = s->has_value = false;
    if (value[0] == 1) { // block
      string v = value.substr(1);
      s->blocks[key] = v;
    } else if (value[0] == 2) { // item
    }
  }
}

void level_tag3(const nbt_callbacks *cb, const string key, uint32_t value) {
  int index = get_item_index(key);
  if (index == -1) return;
  //printf("tag3 %s %d\n", key.c_str(), value);
  level_parse_state *s = (level_parse_state*)cb->state;

  s->has_key = true;
  s->pending_key = value;
  check_key(s);
}

void level_tag8(const nbt_callbacks *cb, const string key, const string value) {
  int index = get_item_index(key);
  if (index == -1) return;
  //printf("tag8 %s %s\n", key.c_str(), value.c_str());
  level_parse_state *s = (level_parse_state*)cb->state;

  s->has_value = true;
  s->pending_value = value;
  check_key(s);
}

void parseLevel(string root, level_state &state_out) {
  string fname = root + "/level.dat";
  level_parse_state s = {
    .has_key = false,
    .has_value = false,
  };
  nbt_callbacks level_cb = {
    .state = &s,
  };
  level_cb.tag3 = level_tag3;
  level_cb.tag8 = level_tag8;
  parseAndLoadNBT(fname, &level_cb);

  for (const auto &n : s.blocks) {
    uint32_t blockid = n.first;
    string block_name = n.second;
    printf("%d %s\n", blockid, block_name.c_str());
  }
  state_out.blocks = s.blocks;
}

static void fill_entire_chunk(Image &img, int xstart, int ystart, uint32_t color) {
  for (int pngy=0; pngy < 16; pngy++) {
    for (int pngx=0; pngx < 16; pngx++) {
      img.setPixel(xstart + pngx, ystart + pngy, color);
    }
  }
}

uint32_t swap_color(uint32_t color) {
  uint32_t r = (color >> 16) & 0xff;
  uint32_t g = (color >> 8) & 0xff;
  uint32_t b = color & 0xff;

  return (b << 16) | (g << 8) | r;
}

void draw_top_layer_to_bitmap(uint16_t toplayer[16][16], int xstart, int zstart, Image &region_image, const level_state &lstate, int regionx, int regionz) {
  for (int z=0; z<16; z++) {
    for (int x=0; x<16; x++) {
      uint16_t block = toplayer[x][z];
      auto i = lstate.blocks.find(block);
      string block_name;
      if (i != lstate.blocks.end()) block_name = i->second;
      else block_name = "unknown";


      auto color = lstate.color_table.find(block_name);
      uint32_t raw_color = 0xffffff;
      if (color != lstate.color_table.end()) raw_color = swap_color(*color);
      else {
        printf("chunk xz %d %d, %d %d, %d, %s\n", x, z, xstart+x + (regionx<<9), zstart+z + (regionz << 9), block, block_name.c_str());
      }

      raw_color = 0xff000000 | raw_color;

      region_image.setPixel(xstart + x, zstart + z, raw_color);
    }
  }
}

void tag7_draw_map(const nbt_callbacks *cb, string key, int size, const uint8_t *data) {
  chunk_parse_state *s = (chunk_parse_state*)cb->state;
  //printf("got map data for %s, %d bytes\n", key.c_str(), size);

  const char *prefix = "/Level/Sections/";
  const int prefix_len = strlen(prefix);

  if (strncmp(key.c_str(), prefix, prefix_len) == 0) {
    const char *num = key.c_str() + prefix_len;
    char *remain;
    uint32_t section = strtol(num, &remain, 10);
    if (strcmp(remain, "/Blocks") == 0) {
      //printf("found blocks for section %d, len %d\n", section, size);
      s->chunk.updateBlocks(section, (const uint8_t(*)[16][16])data);
    } else if (strcmp(remain, "/Add") == 0) {
      //printf("found /Add for section %d, len %d\n", section, size);
      s->chunk.updateAddBlocks(section, (const uint8_t(*)[16][8])data);
    } else {
      //printf("rem %s %d\n", remain, size);
    }
  }
}

void tag8_draw_map(const nbt_callbacks *, const std::string key, const std::string value) {
  const char *prefix = "/sections/";
  const int prefix_len = strlen(prefix);

  if (strncmp(key.c_str(), prefix, prefix_len) == 0) {
    const char *num = key.c_str() + prefix_len;
    char *remain;
    uint32_t section = strtol(num, &remain, 10);
    printf("%s is section %d data, %s\n", key.c_str(), section, remain);
  } else {
    //printf("%s unhandled\n", key.c_str());
  }
}

filesystem::path make_png_name(filesystem::path outpath, int x, int z) {
  char outname[64];
  snprintf(outname, 60, "%d,%d.png", x, z);

  return outpath.append("17").append(outname);
}

void parse_region(const filesystem::path &region_path, signed int x, signed int z, const level_state &lstate, const filesystem::path &outpath) {
  filesystem::path png_out = make_png_name(outpath, x, z);

#if 0
  if ((x != 3) || (z != -7)) {
    puts("skipping region");
    return;
  }
#endif

  bool do_update = need_update(region_path, png_out);

  if (do_update) puts("updating");
  else {
    puts("region older, skipping");
    return;
  }

  Image region_image;

  FILE *fd = fopen(region_path.c_str(), "r");
  region_header *header = (region_header*)malloc(sizeof(*header));
  fread(header, sizeof(*header), 1, fd);

  chunk_parse_state s = {
    .region_image = &region_image,
  };
  nbt_callbacks chunk_callbacks = {
    .state = &s,
  };
  chunk_callbacks.tag7 = &tag7_draw_map;
  chunk_callbacks.tag8 = &tag8_draw_map;

  for (int xoff = 0; xoff < 32; xoff++) {
    for (int zoff = 0; zoff < 32; zoff++) {
      s.xoff = xoff;
      s.zoff = zoff;

      //if (xoff != 28) continue;
      //if (zoff != 16) continue;

      s.chunk.reset();

      //fill_entire_chunk(row_pointers, xoff << 4, zoff << 4, 255, 255, 255);

      uint32_t offset_raw = ntohl(header->chunk_offsets[zoff][xoff]);
      uint32_t offset = (offset_raw >> 8) * 4096;
      uint32_t length = (offset_raw & 0xff) * 4096;

      int xpos = (x << 9) | (xoff << 4);
      int zpos = (z << 9) | (zoff << 4);
      if (offset == 0) {
        int png_xstart = xoff << 4;
        int png_zstart = zoff << 4;
        fill_entire_chunk(region_image, png_xstart, png_zstart, 0);
        continue;
      }

      //snprintf(outname, 60, "chunks/%d,%d.zlib", (x<<5) | xoff, (z<<5) | zoff);

      uint32_t lastmod = ntohl(header->chunk_lastmods[zoff][xoff]);
      string name = region_path.filename().string();
      printf("%s %d %d, x %d<->%d z %d<->%d == off %d sz %d %d\n", name.c_str(), xoff, zoff, xpos, xpos+15, zpos, zpos+15, offset, length, lastmod);

      void *raw_chunk = malloc(length);
      fseek(fd, offset, SEEK_SET);
      int got = fread(raw_chunk, length, 1, fd);
      assert(got == 1);
      //printf("%d %d 0x%x\n", got, length);

      uint32_t actual_length = ntohl(*(uint32_t*)raw_chunk);
      uint8_t comp_type = *(uint8_t*)(raw_chunk+4);
      //printf("length: %d, comp: %d\n", actual_length, comp_type);
      assert(comp_type == 2); // TODO, support other compression types

      //printf("dd if=\"%s\" of=extract bs=1 count=%d skip=%d\n", path.c_str(), actual_length-1, offset+5);

      const int uncomp_size = 1024*1024;
      void *uncompressed = malloc(uncomp_size);
      memset(uncompressed, 0x55, uncomp_size);

      //FILE *chunkout = fopen(outname, "wb");
      //fwrite(raw_chunk+5, actual_length-1, 1, chunkout);
      //fclose(chunkout);

      int uncompressed_size = decompress(raw_chunk + 5, actual_length-1, uncompressed, uncomp_size);
      //printf("true size: %d\n", uncompressed_size);

      if (uncompressed_size > 0) {
        parseNBT((const unsigned char *)uncompressed, uncompressed_size, &chunk_callbacks, false);
        fill_entire_chunk(region_image, xoff << 4, zoff << 4, 0);
        s.chunk.getTopMostBlocks(s.toplayer);
        draw_top_layer_to_bitmap(s.toplayer, xoff << 4, zoff << 4, region_image, lstate, x, z);
        //if ((xoff == 28) && (zoff == 16)) {
        //  s.chunk.printSection(3);
        //  s.chunk.printSection(15);
        //}
        //snprintf(outname, 60, "chunks/%d,%d.good", (x<<5) | xoff, (z<<5) | zoff);
        //to_file(outname, uncompressed, uncompressed_size);
      } else {
        fill_entire_chunk(region_image, xoff << 4, zoff << 4, 0xff0000ff);
        //snprintf(outname, 60, "chunks/%d,%d.bad", (x<<5) | xoff, (z<<5) | zoff);
        //to_file(outname, uncompressed, uncomp_size);
      }

      free(raw_chunk);
      free(uncompressed);
    }
  }

  write_png(png_out, region_image);

  free(header);
  fclose(fd);
}

void region_loop(const filesystem::path &path, const string &name, const level_state &lstate, const filesystem::path &outpath) {
  filesystem::directory_iterator iterator{path};

  for (auto &ent : iterator) {
    auto &p = ent.path();
    if (!ent.is_directory()) {
      string filename = p.stem().string();
      string no_r_name = filename.substr(2);
      size_t split_pos = no_r_name.find(".");
      if (split_pos != string::npos) {
        string x_coord = no_r_name.substr(0, split_pos);
        int x = atoi(x_coord.c_str());
        string z_coord = no_r_name.substr(split_pos + 1);
        int z = atoi(z_coord.c_str());

        printf("%s == %d %d, x %d<->%d, z %d<->%d\n", filename.c_str(), x, z, x << 9, (x+1) << 9, z << 9, (z+1) << 9);
        parse_region(p, x, z, lstate, outpath);
      }
    }
  }

#if 0
  DIR *fd = opendir(path.c_str());
  struct dirent *dir;
  while ((dir = readdir(fd))) {
    int x, z;
    char ext[32];
    int res = sscanf(dir->d_name, "r.%d.%d.%s", &x, &z, ext);
    if (res != 3) {
      printf("ignoring %s\n", dir->d_name);
      continue;
    }
    printf("%s == %d %d %s, x %d<->%d, z %d<->%d\n", dir->d_name, x, z, ext, x << 9, (x+1) << 9, z << 9, (z+1) << 9);
    parse_region(path, dir->d_name, x, z, lstate, outpath);
  }
  closedir(fd);
#endif
}

int main(int argc, char **argv) {
  char *savepath = NULL;
  const char *outpath = ".";
  bool watch_dir = false;

  level_state lstate;
  own_age = get_own_age();

  int opt;
  while ((opt = getopt(argc, argv, "p:o:w")) != -1) {
    switch (opt) {
    case 'p':
      savepath = optarg;
      break;
    case 'o':
      outpath = optarg;
      break;
    case 'w':
      watch_dir = true;
      break;
    }
  }

  if (!savepath) {
    puts("error, specify a save path with `-p <path>`");
    return 0;
  }

  lstate.color_table = json::parse(readFile("colors.json"));

  parseLevel(savepath, lstate);

  if (watch_dir) {
    event_loop(savepath, outpath);
  } else {
    region_loop(string(savepath) + "/region/", "Overworld", lstate, outpath);
    regen_zooms(outpath);
  }


  return 0;
}
