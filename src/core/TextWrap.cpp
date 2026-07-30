#include "core/TextWrap.h"

namespace inkquest {
namespace {

// Append `word`, hard-breaking it across lines if it is longer than maxCols.
void emitLongWord(const std::string& word, int maxCols, std::vector<std::string>& lines) {
  size_t i = 0;
  while (i < word.size()) {
    lines.push_back(word.substr(i, static_cast<size_t>(maxCols)));
    i += static_cast<size_t>(maxCols);
  }
}

void wrapParagraph(const std::string& para, int maxCols, std::vector<std::string>& lines) {
  std::string line;
  size_t i = 0;
  const size_t n = para.size();
  while (i < n) {
    // Skip runs of spaces between words.
    while (i < n && para[i] == ' ') ++i;
    if (i >= n) break;
    size_t start = i;
    while (i < n && para[i] != ' ') ++i;
    std::string word = para.substr(start, i - start);

    if (static_cast<int>(word.size()) > maxCols) {
      if (!line.empty()) {
        lines.push_back(line);
        line.clear();
      }
      emitLongWord(word, maxCols, lines);
      continue;
    }
    if (line.empty()) {
      line = word;
    } else if (static_cast<int>(line.size() + 1 + word.size()) <= maxCols) {
      line += ' ';
      line += word;
    } else {
      lines.push_back(line);
      line = word;
    }
  }
  lines.push_back(line);  // may be empty, preserving a blank paragraph line
}

}  // namespace

std::vector<std::string> wrapText(const std::string& text, int maxCols) {
  if (maxCols <= 0) maxCols = 1;
  std::vector<std::string> lines;

  size_t start = 0;
  while (start <= text.size()) {
    size_t nl = text.find('\n', start);
    std::string para = (nl == std::string::npos) ? text.substr(start) : text.substr(start, nl - start);
    // Drop a trailing carriage return so CRLF text wraps like LF text.
    if (!para.empty() && para.back() == '\r') para.pop_back();
    wrapParagraph(para, maxCols, lines);
    if (nl == std::string::npos) break;
    start = nl + 1;
  }
  return lines;
}

}  // namespace inkquest
