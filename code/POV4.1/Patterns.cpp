#include "Patterns.h"
#include "EngineMath.h"
#include <math.h>
#include <string.h>
namespace Patterns {
namespace {
constexpr float PI_F=3.14159265359f;
using EngineMath::clamp;
RGB multiply(RGB c,float v) { return {c.r*v,c.g*v,c.b*v}; }
RGB rgb(uint32_t c) { return {float((c>>16)&255)/255,float((c>>8)&255)/255,float(c&255)/255}; }
float wave(float phase) { return 0.5f+0.5f*sinf(phase*2*PI_F); }
void rainbow(RGB* p,size_t n,const Context& c) {
  for(size_t i=0;i<n;++i)p[i]=hsv(float(i)/n+c.seconds*0.07f,0.95f,0.72f+0.18f*wave(c.seconds*0.13f));
}
void pixelWalk(RGB* p,size_t n,const Context& c) {
  uint32_t step=uint32_t(c.seconds*12);unsigned channel=(step/n)%4;
  const RGB channels[]={{1,0,0},{0,1,0},{0,0,1},{0.5f,0.5f,0.5f}};
  p[step%n]=channels[channel];
}
void channels(RGB* p,size_t n,const Context& c) {
  unsigned channel=unsigned(c.seconds/3)%4;float phase=fmodf(c.seconds,3)/3;
  const RGB colors[]={{1,0,0},{0,1,0},{0,0,1},{0.6f,0.6f,0.6f}};
  RGB color=multiply(colors[channel],0.55f*sinf(PI_F*phase));
  for(size_t i=0;i<n;++i)p[i]=color;
}
void fade(RGB* p,size_t n,const Context& c) {
  for(size_t i=0;i<n;++i)p[i]=multiply(rgb(c.color),wave(c.seconds*0.16f-float(i)/n*0.3f));
}
void walking(RGB* p,size_t n,const Context& c) {
  // Motion changes the pulse depth, not absolute-time frequency: multiplying
  // uptime by a changing speed would cause ever-larger phase jumps.
  const float pulseDepth=0.12f+0.12f*clamp(c.speed/500,0,1);
  for(size_t i=0;i<n;++i) {
    float x=float(i)/n;
    p[i]=hsv(0.48f+x*0.15f+0.035f*sinf(c.seconds*0.3f),0.85f,
      0.26f+pulseDepth*wave(x-c.seconds*0.12f));
  }
}
void sword(RGB* p,size_t n,const Context& c) {
  RGB base=rgb(c.color);
  for(size_t i=0;i<n;++i) {
    float shimmer=0.80f+0.06f*sinf(i*0.71f+c.seconds*13)+0.04f*sinf(i*1.31f-c.seconds*9);
    float accent=clamp((c.speed-120)/900,0,0.35f)*(0.45f+0.55f*wave(float(i)/n-c.seconds*2));
    p[i]={base.r*shimmer+accent,base.g*shimmer+accent,base.b*shimmer+accent};
  }
}
void flashlight(RGB* p,size_t n,const Context&) {for(size_t i=0;i<n;++i)p[i]={1,1,0.92f};}
void solid(RGB* p,size_t n,const Context& c) {for(size_t i=0;i<n;++i)p[i]=rgb(c.color);}
void breathing(RGB* p,size_t n,const Context& c) {
  RGB color=multiply(rgb(c.color),0.04f+0.96f*wave(c.seconds/3));
  for(size_t i=0;i<n;++i)p[i]=color;
}
void chase(RGB* p,size_t n,const Context& c) {
  for(size_t i=0;i<n;++i) {
    float phase=fmodf(float(i)/n-c.seconds*0.18f+1000,1);
    p[i]=multiply(rgb(c.color),phase<0.18f?(1-phase/0.18f):0);
  }
}
const Definition registry[]={
  {"rainbow",rainbow},{"pixel_walk",pixelWalk},{"channels",channels},{"fade",fade},
  {"walking_stick",walking},{"sword",sword},{"flashlight",flashlight},
  {"solid",solid},{"breathing",breathing},{"chase",chase}
};
}
RGB hsv(float hue,float sat,float value) {
  hue-=floorf(hue);float h=hue*6;int sector=int(h);float f=h-sector;
  float p=value*(1-sat),q=value*(1-sat*f),t=value*(1-sat*(1-f));
  switch(sector) {case 0:return {value,t,p};case 1:return {q,value,p};case 2:return {p,value,t};
    case 3:return {p,q,value};case 4:return {t,p,value};default:return {value,p,q};}
}
const Definition* find(const char* name) {
  for(const auto& pattern:registry)if(strcmp(name,pattern.name)==0)return &pattern;
  return nullptr;
}
}
