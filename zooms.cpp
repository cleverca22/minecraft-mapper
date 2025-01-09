#include <assert.h>
#include <iostream>
#include <stdint.h>
#include <string.h>

#include "zooms.h"
#include "utils.h"

using namespace std;

uint32_t get_pixel_color(uint8_t *buffer, int x, int y, int color) {
  int row_off = y * 512 * 3;
  int off = row_off + (x * 3);
  return buffer[off + color];
}

uint32_t shrink_pixel(const Image &input, int x, int y) {
  uint32_t a = input.getPixel(x+0, y+0);
  uint32_t b = input.getPixel(x+0, y+1);
  uint32_t c = input.getPixel(x+1, y+0);
  uint32_t d = input.getPixel(x+1, y+1);

  uint32_t out = 0x0; // alpha
  for (int i=0; i<4; i++) {
    uint32_t accumulator = 0;
    accumulator += (a >> (i*8)) & 0xff;
    accumulator += (b >> (i*8)) & 0xff;
    accumulator += (c >> (i*8)) & 0xff;
    accumulator += (d >> (i*8)) & 0xff;
    accumulator = accumulator>>2;

    out |= (accumulator << (i*8));
  }

  return out;
}

void set_pixel(uint8_t *buffer, int x, int y, uint8_t r, uint8_t g, uint8_t b) {
  int row_off = y * 512 * 3;
  int off = row_off + (x * 3);
  buffer[off+0] = r;
  buffer[off+1] = g;
  buffer[off+2] = b;
}

int shrink_tile(string path, int xin, int yin, int tilex, int tiley, Image &input_buffer, Image &output_buffer, int zoom) {
  char buffer[64];
  snprintf(buffer, 60, "%d/%d,%d.png", zoom, xin + tilex, yin + tiley);
  string input = path + buffer;
  //cout << input << endl;

  int ret = load_png(input, input_buffer);

  //printf("shrinking %s into tile %d,%d, found %d\n", input.c_str(), tilex, tiley, ret);

  if (ret == 0) {
    // TODO, blank out sub-tile
    for (int i=0; i<256; i++) {
      int row_off = ((tiley * 256) + i) * 512;
      int col_off = tilex * 256;
      memset(output_buffer.bitmap + row_off + col_off, 0, 256*4);
    }
    return ret;
  }

  for (int x=0; x<512; x += 2) {
    for (int y=0; y<512; y += 2) {
      uint32_t pix = shrink_pixel(input_buffer, x, y);
      output_buffer.setPixel((tilex*256) + (x>>1), (tiley*256) + (y>>1), pix);
    }
  }

  return ret;
}

void regen_zooms(string path) {
  const int maxzoom = 17;
  char buffer[64];

  Image input_buffer;
  Image output_buffer;

  for (int zoom=16; zoom > 7; zoom--) {
    const int steps = maxzoom - zoom;
    printf("generating %d from %d, steps %d\n", zoom, zoom+1, steps);
    for (int x=(-512>>steps); x<(512>>steps); x++) {
      for (int y=(-512>>steps); y<(512>>steps); y++) {
        int xin = x<<1;
        int yin = y<<1;
        printf("%d,%d  %d,%d\n", x, y, xin, yin);

        int parts = 0;

        parts += shrink_tile(path, xin, yin, 0, 0, input_buffer, output_buffer, zoom+1);
        parts += shrink_tile(path, xin, yin, 0, 1, input_buffer, output_buffer, zoom+1);
        parts += shrink_tile(path, xin, yin, 1, 0, input_buffer, output_buffer, zoom+1);
        parts += shrink_tile(path, xin, yin, 1, 1, input_buffer, output_buffer, zoom+1);

        //printf("found %d sub-tiles\n", parts);

        if (parts > 0) {
          snprintf(buffer, 60, "%d/%d,%d.png", zoom, x, y);
          string outpath = path + buffer;
          write_png(outpath, output_buffer);
        }
      }
    }
  }
}
