#include "device/BmpCover.h"

#ifdef ARDUINO

#include <inkkit/SdStream.h>
#include <inkkit/Storage.h>

#include "core/Bmp.h"
#include "device/Config.h"

namespace inkquest {

bool drawCover(TextRenderer& renderer, const std::string& path, int x, int y, int maxW, int maxH) {
  if (path.empty() || !inkkit::sd::exists(path.c_str())) return false;
  HalFile file;
  if (!inkkit::sd::openRead(kTag, path.c_str(), file)) return false;
  inkkit::SdFileReader reader(file);

  BmpInfo info;
  const bool ok = decodeBmp(reader, info, [&](int px, int py, bool black) {
    if (!black) return;              // leave white as page background
    if (px >= maxW || py >= maxH) return;  // clip to the box
    renderer.pixel(x + px, y + py);
  });
  file.close();
  return ok;
}

}  // namespace inkquest

#endif  // ARDUINO
