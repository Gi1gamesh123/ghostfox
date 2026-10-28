#ifndef RUYIJS_ARGUMENTS_H
#define RUYIJS_ARGUMENTS_H
#include <string>
#include <vector>

namespace ruyijs {
// Only product mode replaces the CRT's lossy ANSI argv with Windows UTF-8 argv.
// This object outlives OptionParser and every borrowed option value.
class Arguments final {
 public:
  bool Prepare(int* count, char*** values);
 private:
  std::vector<std::string> strings_;
  std::vector<char*> pointers_;
};
bool ReadUTF8File(const char* path, std::string* output);
}
#endif
