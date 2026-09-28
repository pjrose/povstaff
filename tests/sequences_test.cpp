#include "../code/POV4.1/Sequences.h"
#include "support/LittleFS.h"
#include <cassert>
#include <cstdio>
#include <cstring>
HostLittleFS LittleFS;
uint32_t testMillis = 0;
std::vector<uint8_t> fixture(bool topDown) {
  std::vector<uint8_t> data(54+216*2, 0);
  auto put = [&](size_t at, uint32_t n) { for(int i=0;i<4;++i) data[at+i] = uint8_t(n>>(i*8)); };
  data[0]='B'; data[1]='M'; put(2,uint32_t(data.size())); put(10,54); put(14,40);
  put(18,72); put(22,topDown ? uint32_t(-2) : 2); data[26]=1; data[28]=24;
  for(int i=0;i<72;++i) {
    data[54+i*3] = 255; // first stored row blue
    data[54+216+i*3+2] = 255; // second stored row red
  }
  return data;
}
int main() {
  LittleFS.files["/z.bmp"] = fixture(false);
  LittleFS.files["/a.bmp"] = fixture(true);
  LittleFS.files["/broken.bmp"] = {1,2,3};
  LittleFS.files["/.upload.tmp"] = {4,5,6};
  LittleFS.files["/imagelist.txt"] = {7,8,9};
  LittleFS.mountFailure = true;
  assert(!Sequences::begin()); assert(LittleFS.formats == 0); assert(LittleFS.files.size() == 5);
  LittleFS.mountFailure = false;
  assert(Sequences::begin()); assert(LittleFS.files.count("/.upload.tmp") == 0);
  assert(Sequences::count() == 2); assert(std::strcmp(Sequences::currentName(), "a.bmp") == 0);
  assert(std::strstr(Sequences::error(), "skipped"));
  const std::string catalogueBeforeSelect = Sequences::filesJSON().c_str();
  assert(catalogueBeforeSelect.find("\"revision\":1") != std::string::npos);
  uint8_t rgb[216];
  assert(Sequences::readLine(0,rgb)); assert(rgb[0]==0 && rgb[2]==255);
  assert(Sequences::readLine(1,rgb)); assert(rgb[0]==255 && rgb[2]==0);
  assert(Sequences::readLine(2,rgb)); assert(rgb[2]==255); // wraps
  assert(Sequences::select(1)); assert(Sequences::readLine(0,rgb)); assert(rgb[0]==255); // bottom-up reversed
  assert(std::strstr(Sequences::filesJSON().c_str(), "\"revision\":1")); // playback does not invalidate browser previews
  assert(!Sequences::select(99)); assert(Sequences::currentIndex()==1);
  assert(Sequences::next()); assert(Sequences::currentIndex()==0);
  assert(Sequences::next(-1)); assert(Sequences::currentIndex()==1);
  assert(Sequences::reload()); assert(Sequences::currentIndex()==1); // preserve selection by name
  assert(std::strstr(Sequences::filesJSON().c_str(), "\"revision\":2")); // content reload does invalidate previews
  Sequences::setDuration(15);
  testMillis += 60000; // selected image waited while a procedural mode ran
  const uint32_t revisionBeforeResume = Sequences::revision();
  Sequences::restartPlaybackClock();
  Sequences::tick(testMillis); assert(Sequences::currentIndex()==1);
  assert(Sequences::revision()==revisionBeforeResume); // restarting does not reload image data
  testMillis += 14999; Sequences::tick(testMillis); assert(Sequences::currentIndex()==1);
  testMillis += 1; Sequences::tick(testMillis); assert(Sequences::currentIndex()==0);
  assert(Sequences::select(1));
  Sequences::setDuration(0); testMillis += 60000; Sequences::tick(testMillis); assert(Sequences::currentIndex()==1);
  testMillis = 0xffffff00U; Sequences::setDuration(1);
  testMillis += 999; Sequences::tick(testMillis); assert(Sequences::currentIndex()==1);
  testMillis += 1; Sequences::tick(testMillis); assert(Sequences::currentIndex()==0); // wraps millis correctly
  assert(std::strstr(Sequences::filesJSON().c_str(), "\"active\":true"));
  LittleFS.files.erase("/a.bmp"); assert(Sequences::reload()); assert(Sequences::count()==1);
  assert(std::strcmp(Sequences::currentName(), "z.bmp") == 0);
  assert(Sequences::format()); assert(Sequences::count()==0); assert(LittleFS.formats==1);
  assert(!Sequences::readLine(0,rgb)); for(auto c : rgb) assert(c==0);
  assert(!Sequences::next());
  puts("Sequence storage, recovery, row order, selection and timing tests passed");
}
