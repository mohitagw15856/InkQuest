// Host unit tests for the word-wrapping used by the reader.
#include <cstdio>
#include <string>
#include <vector>

#include "core/TextWrap.h"

using inkquest::wrapText;

static int g_failures = 0;
#define CHECK(cond)                                                 \
  do {                                                              \
    if (!(cond)) {                                                  \
      ++g_failures;                                                 \
      std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
    }                                                               \
  } while (0)

int main() {
  {
    auto lines = wrapText("the quick brown fox", 9);
    CHECK(lines.size() == 2);
    CHECK(lines[0] == "the quick");
    CHECK(lines[1] == "brown fox");
  }
  {
    // Explicit newlines force breaks and blank lines survive.
    auto lines = wrapText("one\n\ntwo", 20);
    CHECK(lines.size() == 3);
    CHECK(lines[0] == "one");
    CHECK(lines[1] == "");
    CHECK(lines[2] == "two");
  }
  {
    // A word longer than the column count is hard-broken.
    auto lines = wrapText("supercalifragilistic", 5);
    CHECK(lines.front() == "super");
    for (const auto& l : lines) CHECK(static_cast<int>(l.size()) <= 5);
  }
  {
    auto lines = wrapText("hi", 40);
    CHECK(lines.size() == 1);
    CHECK(lines[0] == "hi");
  }
  {
    // CRLF is normalised.
    auto lines = wrapText("a\r\nb", 10);
    CHECK(lines.size() == 2);
    CHECK(lines[0] == "a");
    CHECK(lines[1] == "b");
  }

  std::printf("textwrap: %s\n", g_failures == 0 ? "OK" : "FAILED");
  return g_failures == 0 ? 0 : 1;
}
