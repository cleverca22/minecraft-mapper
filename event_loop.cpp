#include "inotify.h"

#include <assert.h>
#include <dirent.h>
#include <unistd.h>

#include "event_loop.h"
#include "region.h"

using namespace std;

class RegionWatcher : public InotifyWatcher {
public:
  RegionWatcher(int x, int z);
  virtual void eventOccured(uint32_t mask);
private:
  int x, z;
};

RegionWatcher::RegionWatcher(int x, int z) : x(x), z(z) {
}

void RegionWatcher::eventOccured(uint32_t mask) {
  if (mask & IN_MODIFY) printf("IN_MODIFY ");
  printf("region %d,%d, event 0x%x occured\n", x, z, mask);
}

void event_loop(filesystem::path savedir, filesystem::path outdir) {
  Inotify in;

  in.addWatch(savedir / "region", IN_CLOSE_WRITE | IN_CREATE | IN_OPEN, NULL);

  filesystem::directory_iterator iterator{ savedir / "region" };
  for (auto &ent : iterator) {
    auto &p = ent.path();
    if (!ent.is_directory()) {
      auto coords = parse_region_name(p.stem().string());
      RegionWatcher *w = new RegionWatcher(coords.first, coords.second);
      in.addWatch(p, IN_CLOSE_WRITE | IN_CREATE | IN_OPEN | IN_MODIFY, w);
    }
  }

  while (true) {
    in.blockUntilEvent();
  }
}
