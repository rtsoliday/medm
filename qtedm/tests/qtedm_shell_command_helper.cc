#include <fstream>

int main(int argc, char **argv)
{
  if (argc != 3) {
    return 2;
  }
  std::ofstream output(argv[1], std::ios::binary | std::ios::trunc);
  if (!output) {
    return 3;
  }
  output << argv[2];
  return output ? 0 : 4;
}
