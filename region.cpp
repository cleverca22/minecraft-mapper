#include "region.h"

using namespace std;

pair<int, int> parse_region_name(string name) {
  string no_r_name = name.substr(2);
  size_t split_pos = no_r_name.find(".");
  if (split_pos != string::npos) {
    string x_coord = no_r_name.substr(0, split_pos);
    int x = atoi(x_coord.c_str());
    string z_coord = no_r_name.substr(split_pos + 1);
    int z = atoi(z_coord.c_str());

    //printf("%s == %d %d, x %d<->%d, z %d<->%d\n", name.c_str(), x, z, x << 9, (x+1) << 9, z << 9, (z+1) << 9);

    return {x, z};
  }
  return {0,0};
}
