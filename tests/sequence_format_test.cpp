#include "../code/POV4.1/SequenceFormat.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#include <fstream>
using namespace SequenceFormat;
void put32(uint8_t* p, uint32_t n) { for (int i=0;i<4;++i) p[i] = uint8_t(n >> (8*i)); }
int main() {
  assert(validName("rg-lines.bmp")); assert(validName("NEW_IMAGE.BMP"));
  for (const char* bad : {"", "../a.bmp", "/a.bmp", "a/b.bmp", "a\\b.bmp", "a b.bmp", ".bmp", "a..bmp", "a.html", "a.bmp.html"}) assert(!validName(bad));
  assert(validName((std::string(44, 'a') + ".bmp").c_str()));
  assert(!validName((std::string(45, 'a') + ".bmp").c_str()));
  uint8_t h[54] = {}; h[0]='B'; h[1]='M'; put32(h+10,54); put32(h+14,40);
  put32(h+18,72); put32(h+22,288); h[26]=1; h[28]=24;
  BMP bmp;
  assert(!parseBMP(h,54,54+216*288,72,64000,bmp));
  assert(bmp.height == 288 && bmp.stride == 216 && !bmp.topDown && bmp.bytes == 62208);
  put32(h+22,uint32_t(-288)); assert(!parseBMP(h,54,54+216*288,72,64000,bmp)); assert(bmp.topDown);
  put32(h+22,288);
  assert(parseBMP(h,53,54+216*288,72,64000,bmp));
  assert(parseBMP(h,54,54+216*288-1,72,64000,bmp));
  h[28]=32; assert(parseBMP(h,54,54+216*288,72,64000,bmp)); h[28]=24;
  h[30]=1; assert(parseBMP(h,54,54+216*288,72,64000,bmp)); h[30]=0;
  put32(h+18,0xffffffff); assert(parseBMP(h,54,54+216*288,72,64000,bmp)); put32(h+18,72);
  for (uint32_t height : {0U, 297U, 65536U, 0x80000000U, 0x7fffffffU}) {
    put32(h+22,height); assert(parseBMP(h,54,68096,72,64000,bmp));
  }
  put32(h+22,296); assert(!parseBMP(h,54,54+216*296,72,64000,bmp));
  put32(h+10,53); assert(parseBMP(h,54,68096,72,64000,bmp));
  put32(h+10,0xffffffff); assert(parseBMP(h,54,68096,72,64000,bmp));
  put32(h+10,54); put32(h+2,100); assert(parseBMP(h,54,68096,72,64000,bmp));
  put32(h+2,100000); assert(parseBMP(h,54,68096,72,64000,bmp));
  // Repository examples exercise actual files, not just synthetic headers.
  for (const char* path : {"images/rg-lines.bmp", "images/by-triangles.bmp"}) {
    std::ifstream in(path, std::ios::binary | std::ios::ate); assert(in);
    auto size = size_t(in.tellg()); in.seekg(0); in.read(reinterpret_cast<char*>(h),54);
    assert(!parseBMP(h,54,size,72,64000,bmp));
  }
  puts("Sequence filename and BMP validation tests passed");
}
