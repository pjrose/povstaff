#include "../code/POV4.1/EngineMath.h"
#include "../code/POV4.1/Patterns.h"
#include <assert.h>
#include <stdio.h>
#include <math.h>
int main() {
  using namespace EngineMath;
  const float volts[]={3.2f,3.4f,3.7f,4.0f,4.2f};
  const float pct[]={0,7,40,88,100};
  for(unsigned i=0;i<5;++i)assert(fabsf(stateOfCharge(volts[i])-pct[i])<0.01f);
  assert(batteryPixels(stateOfCharge(3.4f),72)==5);
  assert(stateOfCharge(2)==0 && stateOfCharge(5)==100);
  assert(stateOfCharge(NAN)==0);
  assert(classifySupply(3.2f,false)==PowerSource::Battery);
  assert(classifySupply(4.2f,true)==PowerSource::Battery); // host does not turn cell voltage into USB rail
  assert(classifySupply(4.71f,true)==PowerSource::USB);
  assert(classifySupply(4.72f,false)==PowerSource::Invalid); // voltage alone is not USB evidence
  assert(classifySupply(4.4f,true)==PowerSource::Invalid);
  assert(classifySupply(4.45f,true)==PowerSource::USB);
  assert(classifySupply(5.50f,true)==PowerSource::USB);
  assert(classifySupply(5.60f,true)==PowerSource::Invalid);
  assert(classifySupply(0,true)==PowerSource::Invalid);
  assert(classifySupply(NAN,true)==PowerSource::Invalid);
  for(unsigned mv=3100;mv<4300;++mv)assert(stateOfCharge(mv/1000.0f)<=stateOfCharge((mv+1)/1000.0f));
  float demand=72*4*3*20*(64.0f/255),budget=1800-72*4;
  float scale=limiter(demand,budget,1,0.01f);
  assert(scale<1 && demand*scale<=budget+0.01f);
  float released=limiter(100,budget,scale,0.01f);
  assert(released>scale && released<1);
  assert(limiter(demand*2,budget,released,0.01f)<released);
  float usbDemand=72*4*3*20*(16.0f/255);
  float usbScale=limiter(usbDemand,500-72*4,1,0.01f);
  assert(usbDemand*usbScale+72*4<=500.01f); // first external-power white frame respects reduced budget
  float angle=0;
  assert(advanceLines(360,10000,360,angle)==3);
  assert(fabsf(angle-0.6f)<0.001f);
  assert(advanceLines(360,10000,360,angle)==4);
  angle=0;assert(advanceLines(360,10000000,360,angle)==18); // stalled loop bounded at 50ms
  assert(advanceLines(360,10000,0,angle)==0);
  assert(advanceLines(NAN,10000,360,angle)==0);
  const char* patterns[]={"rainbow","pixel_walk","channels","fade","walking_stick","sword","flashlight","solid","breathing","chase"};
  for(const char* name:patterns) {
    auto effect=Patterns::find(name);assert(effect);
    for(unsigned frame=0;frame<1000;++frame) {
      Patterns::RGB pixels[72]={};
      effect->render(pixels,72,{frame*0.01f,400,0x0088ff});
      for(auto p:pixels)assert(isfinite(p.r)&&isfinite(p.g)&&isfinite(p.b)&&p.r>=0&&p.g>=0&&p.b>=0);
    }
  }
  SwingPulse swing;
  assert(swing.update(100,true,0)==0); // no stale motion immediately after reset
  assert(swing.update(0,true,10)==0);
  for(unsigned t=20;t<100;t+=10)assert(swing.update(float(17+(t%3)*10),true,t)==0);
  assert(swing.update(50,true,100)==0); // threshold starts a continuous attack
  const float rising=swing.update(50,true,120);
  assert(rising>0 && rising<1);
  assert(fabsf(swing.update(50,true,140)-1)<0.001f);
  const float fading=swing.update(50,true,340);
  assert(fading>0 && fading<1);
  assert(swing.update(50,true,940)==0);
  assert(swing.update(100,true,1940)==0); // sustained motion cannot retrigger
  assert(swing.update(10,true,1950)==0); // a slower/reversing swing rearms
  assert(swing.update(50,true,1960)==0);
  assert(swing.update(50,true,2000)==1);
  const float beforeRetrigger=swing.update(10,true,2200);
  const float afterRetrigger=swing.update(50,true,2200);
  assert(beforeRetrigger==afterRetrigger); // no brightness jump on retrigger during fade
  assert(swing.update(50,true,2220)>afterRetrigger);
  assert(swing.update(50,false,2230)==0); // IMU outage always dark
  assert(swing.update(50,true,2240)==0); // reconnection requires a new rest-to-swing edge
  assert(swing.update(NAN,true,2250)==0);
  swing.reset();
  assert(swing.update(0,true,0xfffffff0U)==0);
  assert(swing.update(50,true,0xfffffff1U)==0);
  assert(swing.update(50,true,25U)==1); // 40ms attack crosses millis wrap
  assert(swing.update(50,true,825U)==0);
  auto walking=Patterns::find("walking_stick");
  Patterns::RGB dark[72]={},lit[72]={};
  walking->render(dark,72,{600,300,0xba66ff});
  walking->render(lit,72,{600,300,0xba66ff,1});
  for(unsigned i=0;i<72;++i) {
    assert(dark[i].r==0 && dark[i].g==0 && dark[i].b==0);
    assert(lit[i].r>0 && lit[i].g>0 && lit[i].b>0);
    assert(lit[i].b>lit[i].r && lit[i].r>lit[i].g); // selected purple retained
  }
  assert(!Patterns::find("invalid"));
  puts("Engine math and all 10 procedural patterns: PASS");
}
