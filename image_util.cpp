#include <string>
#include <iostream>
#include <stdexcept>
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
const std::pair<unsigned char*, unsigned long> JPGCompressor::compress(uint32_t *pixels,
    int width, int height)
{
  const int rc = tjCompress2(compressor, reinterpret_cast<unsigned char*>(pixels),
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
void JPGDecompressor::decompress(unsigned char *jpg, const unsigned long jpeg_size,
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
  JPGCompressor compressor(100);
  auto jpg = compressor.compress(pixels, width, height);
  std::ofstream fout(fname.c_str(), std::ios::binary);
  fout.write(reinterpret_cast<const char*>(jpg.first), jpg.second);
}


