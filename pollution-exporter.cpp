#include <arpa/inet.h>
#include <assert.h>
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <string>
#include <unistd.h>

#include "utils.h"
#include "inotify.h"

using namespace std;
using namespace filesystem;

template <typename F> void for_each_file_matching_pattern(const path dir, const regex pattern, F func) {
  filesystem::directory_iterator i{dir};
  smatch matches;
  for (auto &ent : i) {
    if (ent.is_regular_file()) {
      const auto &name = ent.path().stem().string();
      if (regex_search(name, matches, pattern)) {
        func(ent.path(), matches);
      }
    }
  }
}

// GTNH stores pollution in the `dim/gregtech/Pollution.X.Z.dat` files
// the first 3 bytes are a header, containing the magic# (always 0), the version (0), and if the first element is default or not
// it then stores alternating sets of "default value" and "non default value"
// a "default value" is just a 16bit number, saying the next N entries are the default (0 pollution)
// a "non default value" is a 16bit count, and then N 32bit numbers, for the pollution ammounts
// the file contains a total of 4096 entries, which map to a 64x64 chunk grid
void parse_pollution_file(int dimension, int x, int z, const path p, FILE *fh) {
  //cout << dimension << " " << x << " " << z << " " << p << endl;

  const string data = readFile(p);
  const uint8_t *raw = (const uint8_t*)data.data();
  uint8_t magic = raw[0];
  uint8_t version = raw[1];
  uint8_t nullRange = raw[2];
  //printf("magic %d version %d nullRange %d\n", magic, version, nullRange);
  unsigned int ptr = 3;
  uint32_t map[4096];
  int ptr2 = 0;
  while (ptr < data.size()) {
    if (nullRange) {
      uint16_t length = ntohs(*(uint16_t*)(raw + ptr));
      ptr += 2;
      //printf("%d defaults\n", length);
      for (int i=0; i<length; i++) {
        map[ptr2++] = 0;
      }
      nullRange = !nullRange;
    } else {
      uint16_t length = ntohs(*(uint16_t*)(raw + ptr));
      ptr += 2;
      //printf("%d non-default: ", length);
      for (int i=0; i<length; i++) {
        uint32_t data = ntohl(*(uint32_t*)(raw + ptr));
        ptr += 4;
        //printf("%d ", data);
        map[ptr2++] = data;
      }
      //puts("");
      nullRange = !nullRange;
    }
  }
  for (int i=0; i<4096; i++) {
    if (map[i] != 0) {
      int x2 = (i / 64) + (x * 64);
      int z2 = (i % 64) + (z * 64);
      //printf("%d %d %d\n", x2, z2, map[i]);
      if (fh) {
        fprintf(fh, "chunk_pollution{dim=\"%d\",x=\"%d\",z=\"%d\"} %d\n", dimension, x2, z2, map[i]);
      }
    }
  }
  assert(ptr2 == 4096);
}

class PollutionRegion : public InotifyWatcher {
public:
  PollutionRegion(const PollutionRegion&) = delete;
  PollutionRegion &operator=(const PollutionRegion&) = delete;

  PollutionRegion(const path p, int dim, int x, int z) : region_x(x), region_z(z), p(p), dim(dim) {
    puts("on load check");
    parse_pollution_file(dim, region_x, region_z, p, NULL);
  }
  void eventOccured(uint32_t mask) {
    printf("%d %d event %d!\n", region_x, region_z, mask);
    parse_pollution_file(dim, region_x, region_z, p, NULL);
  }
private:
  int region_x, region_z;
  int dim;
  const path p;
};

int main(int argc, char **argv) {
  char *savepath = NULL;
  char *prom_out = NULL;
  bool loop = true;
  int opt;
  while ((opt = getopt(argc, argv, "p:o:")) != -1) {
    switch (opt) {
    case 'p':
      savepath = optarg;
      break;
    case 'o':
      prom_out = optarg;
      break;
    }
  }
  if (!savepath) {
    puts("error, specify a save path with `-p <path>`");
    return 0;
  }
  bool oneshot = false;
  if (prom_out) oneshot = true;
  if (oneshot) {
    do {
      FILE *fh = fopen(prom_out, "w");
      for_each_dimension(savepath, [fh](path directory, int dimension) -> void {
        directory /= "gregtech";
        if (exists(directory)) {
          for_each_file_matching_pattern(directory, regex("^Pollution\\.([-0-9]+)\\.([-0-9]+)$"), [dimension, fh](path p, smatch matches) -> void {
            int x = atoi(matches[1].str().c_str());
            int z = atoi(matches[2].str().c_str());
            parse_pollution_file(dimension, x, z, p, fh);
          });
        }
      });
      fclose(fh);
      if (loop) sleep(10);
    } while (loop);
  } else {
    Inotify inotif;
    for_each_dimension(savepath, [&inotif](path directory, int dimension) -> void {
      directory /= "gregtech";
      if (exists(directory)) {
        for_each_file_matching_pattern(directory, regex("^Pollution\\.([-0-9]+)\\.([-0-9]+)$"), [dimension, &inotif](path p, smatch matches) -> void {
          int x = atoi(matches[1].str().c_str());
          int z = atoi(matches[2].str().c_str());
          PollutionRegion *pr = new PollutionRegion(p, dimension, x, z);
          inotif.addWatch(p, IN_CLOSE_WRITE, pr);
        });
      }
    });
    while (true) {
      inotif.blockUntilEvent();
    }
  }
  return 0;
}
