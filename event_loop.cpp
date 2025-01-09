#include <assert.h>
#include <dirent.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <map>

#include "event_loop.h"
#include "region.h"

using namespace std;

class InotifyWatcher {
public:
  virtual void eventOccured(uint32_t mask) = 0;
};

class RegionWatcher : public InotifyWatcher {
public:
  RegionWatcher(int x, int z);
  virtual void eventOccured(uint32_t mask);
private:
  int x, z;
};

class Inotify {
public:
  Inotify();
  void addWatch(filesystem::path path, uint32_t events, InotifyWatcher *watcher);
  void blockUntilEvent();
private:
  int ifd;
  map<int, InotifyWatcher*> watchers;
};

RegionWatcher::RegionWatcher(int x, int z) : x(x), z(z) {
}

void RegionWatcher::eventOccured(uint32_t mask) {
  if (mask & IN_MODIFY) printf("IN_MODIFY ");
  printf("region %d,%d, event 0x%x occured\n", x, z, mask);
}

Inotify::Inotify() {
  ifd = inotify_init1(IN_CLOEXEC);
  assert(ifd >= 0);
}

void Inotify::addWatch(filesystem::path path, uint32_t events, InotifyWatcher *watcher) {
  int wd = inotify_add_watch(ifd, path.c_str(), events);
  watchers[wd] = watcher;
}

void Inotify::blockUntilEvent() {
  inotify_event *buffer = (inotify_event*)malloc(sizeof(inotify_event) + NAME_MAX);
  int size = read(ifd, buffer, sizeof(inotify_event) + NAME_MAX);

#if 0
  printf("wd: %d, mask: 0x%x, cookie: 0x%x, len: %d\n", buffer->wd, buffer->mask, buffer->cookie, buffer->len);
  if ((buffer->len > 0) && (buffer->len < NAME_MAX)) {
    printf("name: %s\n", buffer->name);
  }
  if (buffer->mask & IN_CLOSE_WRITE) puts("IN_CLOSE_WRITE");
  if (buffer->mask & IN_CREATE) puts("IN_CREATE");
  if (buffer->mask & IN_OPEN) puts("IN_OPEN");
  if (buffer->mask & IN_MODIFY) puts("IN_MODIFY");
#endif

  auto i = watchers.find(buffer->wd);
  if (i == watchers.end()) {
    puts("no watcher registered");
  } else {
    InotifyWatcher *w = i->second;
    if (w) {
      w->eventOccured(buffer->mask);
    } else {
      puts("watcher was NULL!");
    }
  }

  free(buffer);
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
