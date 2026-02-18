#include "inotify.h"
#include "region.h"

#include <string>
#include <unistd.h>

using namespace std;

class SaveWatcher : public InotifyWatcher {
public:
  SaveWatcher(int x, int z);
  virtual void eventOccured(uint32_t mask);
private:
  int x, z;
};

SaveWatcher::SaveWatcher(int x, int z) : x(x), z(z) {
}

void SaveWatcher::eventOccured(uint32_t mask) {
  printf("event 0x%x, for region %d,%d\n", mask, x, z);
}

int main(int argc, char **argv) {
  filesystem::path savepath;

  int opt;
  while ((opt = getopt(argc, argv, "p:")) != -1) {
    switch (opt) {
    case 'p':
      savepath = optarg;
      break;
    }
  }

  Inotify in;
  in.addWatch(savepath / "region", IN_CLOSE_WRITE | IN_CREATE | IN_OPEN | IN_MODIFY, NULL);
  filesystem::directory_iterator iterator{ savepath / "region" };
  for (auto &ent : iterator) {
    auto &p = ent.path();
    if (!ent.is_directory()) {
      auto coords = parse_region_name(p.stem().string());
      SaveWatcher *w = new SaveWatcher(coords.first, coords.second);
      in.addWatch(p, IN_CLOSE_WRITE | IN_CREATE | IN_OPEN | IN_MODIFY, w);
    }
  }
  while (true) {
    in.blockUntilEvent();
  }
}
