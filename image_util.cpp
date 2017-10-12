#include <string>
#include <iostream>
#include <fstream>
#include "image_util.h"

JPGCompressor::JPGCompressor(int quality)
  : compressor(tjInitCompress()), buffer(nullptr), bufsize(0), quality(quality)
{}
JPGCompressor::~JPGCompressor() {
  if (buffer) {
    tjFree(buffer);
  }
  tjDestroy(compressor);
}
const std::pair<unsigned char*, unsigned long> JPGCompressor::compress(const uint32_t *pixels,
    int width, int height)
{
  if (buffer) {
    tjFree(buffer);
    buffer = nullptr;
    bufsize = 0;
  }
  const int rc = tjCompress2(compressor, reinterpret_cast<const unsigned char*>(pixels),
      width, width * 4, height, TJPF_RGBA, &buffer, &bufsize, TJSAMP_420,
      quality, TJFLAG_BOTTOMUP);
  if (rc != 0) {
    throw std::runtime_error("Failed to compress JPG!");
  }
  return std::make_pair(buffer, bufsize);
}

JPGDecompressor::JPGDecompressor() : decompressor(tjInitDecompress()) {}
JPGDecompressor::~JPGDecompressor() {
  tjDestroy(decompressor);
}
void JPGDecompressor::decompress(const unsigned char *jpg, const unsigned long jpeg_size,
    const int width, const int height, std::vector<uint32_t> &img)
{
  if (img.size() != width * height * 4) {
    img.resize(width * height * 4, 0);
  }
  const int rc = tjDecompress2(decompressor, jpg, jpeg_size,
      reinterpret_cast<unsigned char*>(img.data()),
      width, width * 4, height, TJPF_RGBA, TJFLAG_BOTTOMUP);
  if (rc != 0) {
    throw std::runtime_error("Failed to decompress JPG!");
  }
}

void save_jpeg_file(const std::string &fname, uint32_t *pixels,
    int width, int height)
{
  tjhandle compressor = tjInitCompress();
  const int JPEG_QUALITY = 100;
  unsigned char *jpeg = nullptr;
  unsigned long jpegSize = 0;
  int rc = tjCompress2(compressor, reinterpret_cast<unsigned char*>(pixels),
      width, width * 4, height, TJPF_RGBA, &jpeg, &jpegSize, TJSAMP_420,
      JPEG_QUALITY, TJFLAG_BOTTOMUP);
  if (rc != 0) {
    std::cerr << "Failed to compress jpeg\n";
  }
  tjDestroy(compressor);

  std::ofstream fout(fname.c_str(), std::ios::binary);
  fout.write(reinterpret_cast<const char*>(jpeg), jpegSize);
  tjFree(jpeg);
}


