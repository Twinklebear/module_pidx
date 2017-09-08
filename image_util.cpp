#include <string>
#include <iostream>
#include <fstream>
#include <turbojpeg.h>
#include "image_util.h"

void save_jpeg_file(const std::string &fname, const uint32_t *pixels,
    int width, int height)
{
  tjhandle compressor = tjInitCompress();
  const int JPEG_QUALITY = 100;
  unsigned char *jpeg = nullptr;
  unsigned long jpegSize = 0;
  int rc = tjCompress2(compressor, reinterpret_cast<const unsigned char*>(pixels),
      width, width * 4, height, TJPF_RGBA, &jpeg, &jpegSize, TJSAMP_444,
      JPEG_QUALITY, 0);
  if (rc != 0) {
    std::cerr << "Failed to compress jpeg\n";
  }
  tjDestroy(compressor);

  std::ofstream fout(fname.c_str(), std::ios::binary);
  fout.write(reinterpret_cast<const char*>(jpeg), jpegSize);
}


