#include <vector>
#include <algorithm>

int main(int argc, char *argv[]) {
  std::vector<int> vi(1000*1000*100);
  std::random_shuffle(vi.begin(), vi.end());
  return 0;
}
