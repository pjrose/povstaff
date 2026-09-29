#include "Engine.h"
#include "Config.h"
#include "EngineMath.h"
#include "Sequences.h"
#include "LSM6.h"
#include "Patterns.h"
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <HWCDC.h>
#include <SPI.h>
#include <math.h>

namespace {
using EngineMath::clamp;
constexpr uint8_t DATA_PIN=35, CLOCK_PIN=36; // QT Py ESP32-S3 hardware MOSI/SCK
constexpr uint32_t BATTERY_PERIOD_MS=1000, BATTERY_DISPLAY_MS=400;
constexpr uint8_t BATTERY_SAMPLES=16;
constexpr float BATTERY_RECOVERY_V=3.35f, GAMMA=2.2f;
constexpr float SPIN_ENTER_DPS=100, SPIN_EXIT_DPS=65, STILL_DPS=25;
constexpr uint32_t ORIENTATION_DEBOUNCE_MS=1200, SPIN_DEBOUNCE_MS=180;
constexpr float LED_CHANNEL_MA=20, LED_IDLE_MA=1;
constexpr float LED_BASE_MA=Config::PIXELS*Config::STRIPS*LED_IDLE_MA;
constexpr float PI_F=3.14159265359f;
static_assert(Config::MAX_LED_CURRENT_MA>LED_BASE_MA,"LED current budget must exceed quiescent estimate");
static_assert(Config::USB_LED_CURRENT_MA>LED_BASE_MA,"USB LED budget must exceed quiescent estimate");
enum class Mode : uint8_t { Auto, Pov, Test, Idle, Walking, Sword, Flashlight, Solid, Breathing, Rainbow, Chase, Battery, Off };
constexpr const char* MODE_NAMES[]={"auto","pov","led_test","idle","walking_stick","sword","flashlight","solid","breathing","rainbow","chase","battery","off"};
using Patterns::RGB;
Adafruit_NeoPixel heartbeat(1,PIN_NEOPIXEL,NEO_GRB+NEO_KHZ800);
LSM6 imu;
EngineMath::SwingPulse swingPulse;
RGB frame[Config::PIXELS];
float gammaTable[257];
Mode requested=Mode::Auto, active=Mode::Idle, candidate=Mode::Idle;
const Patterns::Definition* pattern=Patterns::find("rainbow");
bool locked=false, servicing=false, imuOK=false, critical=true;
EngineMath::PowerSource powerSource=EngineMath::PowerSource::Invalid;
bool usbPresent=false, sawUSB=false;
uint32_t lastUSBCheck=0,lastUSBSeen=0;
uint8_t brightness=Config::DEFAULT_BRIGHTNESS, bootStage=0;
uint32_t color=0x0088ff, bootTime=0, meterTime=0, lastFrame=0, lastOutput=0;
uint32_t lastIMU=0, lastRetry=0, candidateSince=0, lastHeartbeat=0;
uint32_t lastBattery=0, sampleTime=0, adcSum=0, mvSum=0;
uint8_t sampleCount=0;
bool sampling=true, batteryOverlay=false, forceFrame=true;
float batteryV=0, batteryPercent=0, rawMv=0, rawAdc=0;
float speed=0, accelX=0, accelY=0, accelZ=0;
float powerScale=1, currentMa=LED_BASE_MA, appliedBrightness=0;
float angle=0;
uint32_t povLine=0, povRevision=0, lastAngle=0;
const char* orientation="unknown";
bool targetReady=false;
float targetChannels[Config::PIXELS*3]={};
EngineMath::TemporalDither<Config::PIXELS*3> quantizer;
uint32_t channelBudget=0,lastDither=0;
uint32_t statsFrames=0,statsMaxUs=0,statsLate=0,fpsFrames=0,fpsAt=0;
uint64_t statsSendUs=0;
float actualFPS=0;

void recordSend(uint32_t start) {
  const uint32_t duration=micros()-start;
  ++statsFrames;++fpsFrames;statsSendUs+=duration;
  if(duration>statsMaxUs)statsMaxUs=duration;
}
void updateFPS(uint32_t now) {
  const uint32_t elapsed=now-fpsAt;
  if(elapsed>=1000000) {actualFPS=fpsFrames*1000000.0f/elapsed;fpsAt=now;fpsFrames=0;}
}
void sendPacket(const uint8_t* channels,bool measure=false) {
  constexpr unsigned packetBytes=4+Config::PIXELS*4+(Config::PIXELS+15)/16;
  // ESP32 SPI reads complete words internally, including the final short word.
  alignas(4) uint8_t packet[(packetBytes+3)&~3U]={};
  for(unsigned i=0;i<Config::PIXELS;++i) {
    packet[4+i*4]=0xff;packet[5+i*4]=channels[i*3+2];
    packet[6+i*4]=channels[i*3+1];packet[7+i*4]=channels[i*3];
  }
  for(unsigned i=4+Config::PIXELS*4;i<packetBytes;++i)packet[i]=0xff;
  const uint32_t start=micros();
  SPI.beginTransaction(SPISettings(8000000,MSBFIRST,SPI_MODE0));
  SPI.writeBytes(packet,packetBytes);
  SPI.endTransaction();
  if(measure)recordSend(start);
}
void sendDither(uint32_t now) {
  if(!targetReady || uint32_t(now-lastDither)<1000)return;
  const uint32_t elapsed=now-lastDither;
  if(lastDither && elapsed>=2000)statsLate+=elapsed/1000-1;
  lastDither=now;
  uint8_t channels[Config::PIXELS*3];
  const uint32_t sum=quantizer.quantize(targetChannels,channels,channelBudget);
  sendPacket(channels,true);
  currentMa=LED_BASE_MA+sum*(LED_CHANNEL_MA*Config::STRIPS/255);
}

float wave(float phase) { return 0.5f+0.5f*sinf(phase*2*PI_F); }
void clearFrame() { for(auto& c:frame)c={0,0,0}; }
void black() {
  targetReady=false;quantizer.reset();lastDither=0;
  const uint8_t channels[Config::PIXELS*3]={};
  sendPacket(channels);currentMa=LED_BASE_MA;appliedBrightness=0;
  forceFrame=true;
}
float gammaValue(float value) {
  float index=clamp(value,0,1)*256;
  unsigned lo=unsigned(index);
  if(lo>=256)return 1;
  return gammaTable[lo]+(gammaTable[lo+1]-gammaTable[lo])*(index-lo);
}
void output(uint8_t cap,uint32_t nowUs) {
  const bool usbLimited=usbPresent || powerSource==EngineMath::PowerSource::USB;
  if(usbLimited && cap>Config::USB_MAX_BRIGHTNESS)cap=Config::USB_MAX_BRIGHTNESS;
  const float budget=usbLimited?Config::USB_LED_CURRENT_MA:Config::MAX_LED_CURRENT_MA;
  float total=0;
  for(auto& c:frame) { c={gammaValue(c.r),gammaValue(c.g),gammaValue(c.b)}; total+=c.r+c.g+c.b; }
  const float user=float(cap)/255;
  const float demand=total*user*LED_CHANNEL_MA*Config::STRIPS;
  powerScale=EngineMath::limiter(demand,budget-LED_BASE_MA,powerScale,float(uint32_t(nowUs-lastOutput))*0.000001f);
  lastOutput=nowUs;
  const float scale=user*powerScale*255;
  for(unsigned i=0;i<Config::PIXELS;++i) {
    targetChannels[i*3]=frame[i].r*scale;
    targetChannels[i*3+1]=frame[i].g*scale;
    targetChannels[i*3+2]=frame[i].b*scale;
  }
  channelBudget=uint32_t((budget-LED_BASE_MA)*255/(LED_CHANNEL_MA*Config::STRIPS));
  targetReady=true;appliedBrightness=total>0?cap*powerScale:0;forceFrame=false;
  sendDither(micros());
}
void batteryFrame() {
  clearFrame();
  if(powerSource!=EngineMath::PowerSource::Battery)return;
  unsigned count=EngineMath::batteryPixels(batteryPercent,Config::PIXELS);
  RGB c=Patterns::hsv(batteryPercent/300.0f);
  for(unsigned i=0;i<count;++i)frame[i]=c;
}
void changeMode(Mode mode) {
  if(active==mode)return;
  active=mode; angle=0; povLine=0; lastAngle=micros(); forceFrame=true;
  swingPulse.reset();
  if(mode==Mode::Pov)Sequences::restartPlaybackClock();
  black(); // no stale bright frame survives an unavailable image/IMU
}
void sampleBattery(uint32_t now) {
  if(!sampling && uint32_t(now-lastBattery)>=BATTERY_PERIOD_MS) {
    sampling=true;sampleCount=0;adcSum=mvSum=0;
  }
  if(!sampling || uint32_t(now-sampleTime)<10)return;
  sampleTime=now;
  adcSum+=analogRead(Config::VSENSE_PIN);
  mvSum+=analogReadMilliVolts(Config::VSENSE_PIN);
  if(++sampleCount<BATTERY_SAMPLES)return;
  rawAdc=float(adcSum)/BATTERY_SAMPLES;
  rawMv=float(mvSum)/BATTERY_SAMPLES;
  batteryV=rawMv*0.001f*Config::BATTERY_MULTIPLIER+Config::BATTERY_OFFSET;
  powerSource=EngineMath::classifySupply(batteryV,usbPresent,Config::BATTERY_MAX_VOLTAGE,
                                       Config::USB_MIN_VOLTAGE,Config::USB_MAX_VOLTAGE);
  batteryPercent=powerSource==EngineMath::PowerSource::Battery?EngineMath::stateOfCharge(batteryV):0;
  if(powerSource==EngineMath::PowerSource::Invalid)critical=true;
  else if(powerSource==EngineMath::PowerSource::USB)critical=false;
  else if(batteryV<=Config::CRITICAL_VOLTAGE)critical=true;
  else if(batteryV>=BATTERY_RECOVERY_V)critical=false;
  sampling=false;lastBattery=now;
  if(critical)black();
  Serial.printf("Supply ADC %.0f, ADC mV %.1f, rail %.3f V, source=%s, critical=%d\n",rawAdc,rawMv,batteryV,
    powerSource==EngineMath::PowerSource::USB?"USB":powerSource==EngineMath::PowerSource::Battery?"battery":"invalid",critical);
}
void motion(uint32_t now) {
  if(!imuOK) {
    if(uint32_t(now-lastRetry)<5000)return;
    lastRetry=now;
    imuOK=imu.init(LSM6::device_DSO) && imu.configure();
    if(imuOK)Serial.println("IMU reconnected");
    return;
  }
  if(uint32_t(now-lastIMU)<10)return;
  lastIMU=now;
  if(!imu.readSample()) {
    swingPulse.reset();
    imuOK=false;speed=0;orientation="unknown";lastRetry=now;
    if(active==Mode::Pov)black();
    Serial.println("IMU read failed; motion modes suspended");return;
  }
  speed+=0.35f*(imu.getSpeed()-speed);
  accelX+=0.15f*(imu.a.x*0.000122f-accelX);
  accelY+=0.15f*(imu.a.y*0.000122f-accelY);
  accelZ+=0.15f*(imu.a.z*0.000122f-accelZ);
  float norm=sqrtf(accelX*accelX+accelY*accelY+accelZ*accelZ);
  float upright=norm>0.1f ? fabsf(accelX)/norm : 0.5f;
  orientation=upright<0.30f ? "horizontal" : (upright>0.75f ? "vertical" : "tilted");
}
void autoMode(uint32_t now) {
  if(locked || requested!=Mode::Auto)return;
  Mode next=active;
  if(!imuOK)next=Mode::Idle;
  else if(speed>(active==Mode::Pov?SPIN_EXIT_DPS:SPIN_ENTER_DPS))next=Mode::Pov;
  else if(speed<STILL_DPS) {
    if(strcmp(orientation,"horizontal")==0)next=Mode::Test;
    else if(strcmp(orientation,"vertical")==0)next=Mode::Walking;
    else next=Mode::Idle;
  } else if(active==Mode::Pov)next=Mode::Walking;
  if(next!=candidate) { candidate=next;candidateSince=now; }
  uint32_t debounce=next==Mode::Pov?SPIN_DEBOUNCE_MS:ORIENTATION_DEBOUNCE_MS;
  if(!imuOK || uint32_t(now-candidateSince)>=debounce)changeMode(next);
}
void pulseHeartbeat(uint32_t now) {
  // Error diffusion preserves fractional levels at a nominal 250 Hz. The
  // average follows the slow envelope even below one NeoPixel PWM step.
  static float remainder[3]={0,0,0};
  static uint32_t lastLinkCheck=0;
  static bool connected=false;
  if(uint32_t(now-lastHeartbeat)<4)return;
  lastHeartbeat=now;
  if(uint32_t(now-lastLinkCheck)>=250) {
    lastLinkCheck=now;
    connected=WiFi.status()==WL_CONNECTED || WiFi.softAPgetStationNum();
  }
  float intensity=1+3*wave(float(now%2800)/2800);
  RGB c;
  if(bootStage<3)c={1,0.35f,0};
  else if(critical)c={1,0,0};
  else if(!Sequences::mounted()) { c={1,0,0.5f}; intensity=(now%1600<100 || (now%1600>250 && now%1600<350))?4:0; }
  else if(!imuOK)c={1,0.3f,0};
  else if(connected)c={0,0.6f,1};
  else c={0,1,0.15f};
  const float target[3]={c.r*intensity,c.g*intensity,c.b*intensity};
  uint8_t output[3];
  for(unsigned channel=0;channel<3;++channel) {
    if(target[channel]<=0) {output[channel]=0;remainder[channel]=0;}
    else {
      float value=target[channel]+remainder[channel];
      output[channel]=uint8_t(value);
      remainder[channel]=value-output[channel];
    }
  }
  heartbeat.setPixelColor(0,output[0],output[1],output[2]);heartbeat.show();
}
void procedural(uint32_t now) {
  clearFrame();
  if(active==Mode::Battery) {batteryFrame();return;}
  const Patterns::Definition* scene=active==Mode::Test?pattern:Patterns::find(MODE_NAMES[unsigned(active)]);
  const float swing=swingPulse.update(speed,imuOK,now);
  if(scene)scene->render(frame,Config::PIXELS,{float(now%3600000)*0.001f,speed,color,swing});
}
void pov(uint32_t nowUs) {
  Sequences::tick(millis());
  uint32_t revision=Sequences::revision();
  if(revision!=povRevision) { povRevision=revision;povLine=0;angle=0;forceFrame=true; }
  uint32_t elapsed=uint32_t(nowUs-lastAngle);lastAngle=nowUs;
  if(!imuOK || speed<5 || !Sequences::lines()) { if(appliedBrightness>0)black();return; }
  uint32_t steps=EngineMath::advanceLines(speed,elapsed,Sequences::lines(),angle);
  if(!steps && !forceFrame)return;
  // Advance phase by every elapsed line, but render only the newest one.
  // There is never a catch-up loop that can starve web/OTA.
  povLine=(povLine+steps)%Sequences::lines();
  uint8_t pixels[Config::PIXELS*3];
  if(!Sequences::readLine(povLine,pixels)) { black();return; }
  for(unsigned i=0;i<Config::PIXELS;++i)frame[i]={pixels[3*i]/255.0f,pixels[3*i+1]/255.0f,pixels[3*i+2]/255.0f};
  output(brightness,nowUs);
}
}
namespace Engine {
void begin() {
  // Take ownership before Serial, filesystem or networking can stall startup.
  pinMode(DATA_PIN,OUTPUT);pinMode(CLOCK_PIN,OUTPUT);
  digitalWrite(DATA_PIN,LOW);digitalWrite(CLOCK_PIN,LOW);
  SPI.begin(CLOCK_PIN,-1,DATA_PIN,-1);black();
#ifdef NEOPIXEL_POWER
  pinMode(NEOPIXEL_POWER,OUTPUT);digitalWrite(NEOPIXEL_POWER,HIGH);
#endif
  heartbeat.begin();heartbeat.clear();heartbeat.show();
  for(unsigned i=0;i<=256;++i)gammaTable[i]=powf(float(i)/256,GAMMA);
  pinMode(Config::VSENSE_PIN,INPUT);analogReadResolution(12);
  analogSetPinAttenuation(Config::VSENSE_PIN,ADC_11db);
  Wire.begin();Wire.setClock(400000);Wire.setTimeOut(5);
  imuOK=imu.init(LSM6::device_DSO) && imu.configure();
  bootTime=millis();lastRetry=bootTime;lastOutput=lastAngle=micros();
}
void update() {
  uint32_t now=millis(),nowUs=micros();
  updateFPS(nowUs);
  if(uint32_t(now-lastUSBCheck)>=20) {
    lastUSBCheck=now;
    // isPlugged checks host SOF traffic, unlike CDC-open/DTR state. A short
    // grace interval avoids transient USB SOF watchdog false negatives.
    if(HWCDC::isPlugged()) {lastUSBSeen=now;sawUSB=true;}
    bool present=sawUSB && uint32_t(now-lastUSBSeen)<250;
    if(present!=usbPresent) {
      usbPresent=present;black(); // remove any old higher-current frame now
      sampling=true;sampleCount=0;adcSum=mvSum=0;sampleTime=now;
      if(!present && powerSource==EngineMath::PowerSource::USB)critical=true;
    }
  }
  pulseHeartbeat(now);
  if(servicing)return; // uploads cannot be interrupted by an animation or a battery display
  motion(now);
  if(bootStage==0) {
    if(uint32_t(now-bootTime)<30)return;
    black();bootStage=1;sampleTime=now;return;
  }
  sampleBattery(now);
  if(!critical && Sequences::mounted())sendDither(micros());
  if(bootStage==1) {
    if(sampling)return;
    Serial.printf("IMU %s; startup auto; brightness %u, cap %u, current budget %.0f mA\n",imuOK?"connected":"absent",brightness,Config::MAX_BRIGHTNESS,Config::MAX_LED_CURRENT_MA);
    if(!critical && Sequences::mounted() && requested!=Mode::Off) {batteryFrame();output(Config::STARTUP_BRIGHTNESS,nowUs);}
    else black();
    meterTime=now;bootStage=2;return;
  }
  if(bootStage==2) {
    if(uint32_t(now-meterTime)<BATTERY_DISPLAY_MS)return;
    black();bootStage=3;lastFrame=nowUs;
  }
  if(batteryOverlay && uint32_t(now-meterTime)>=BATTERY_DISPLAY_MS) {batteryOverlay=false;black();}
  if(critical) { if(appliedBrightness>0)black();return; }
  if(!Sequences::mounted()) { if(appliedBrightness>0)black();return; }
  if(batteryOverlay) {
    if(forceFrame){batteryFrame();output(Config::STARTUP_BRIGHTNESS,nowUs);}
    return;
  }
  autoMode(now);
  if(active==Mode::Pov) {pov(nowUs);return;}
  if(!forceFrame && uint32_t(nowUs-lastFrame)<Config::FRAME_INTERVAL_US)return;
  lastFrame=nowUs;procedural(now);
  uint8_t cap=brightness;
  if((active==Mode::Flashlight || (active==Mode::Test && strcmp(pattern->name,"flashlight")==0)) && cap>Config::FLASHLIGHT_MAX_BRIGHTNESS)cap=Config::FLASHLIGHT_MAX_BRIGHTNESS;
  if(active==Mode::Battery)cap=Config::STARTUP_BRIGHTNESS;
  output(cap,nowUs);
}
void maintenance(bool activeMaintenance) {
  swingPulse.reset();
  servicing=activeMaintenance;black();lastAngle=micros();
  if(!servicing)forceFrame=true;
}
bool setMode(const String& name) {
  if(name=="battery") {showBattery();return true;}
  for(unsigned i=0;i<sizeof(MODE_NAMES)/sizeof(MODE_NAMES[0]);++i)if(name==MODE_NAMES[i]) {
    requested=Mode(i);locked=requested!=Mode::Auto;batteryOverlay=false;
    if(requested==Mode::Auto) {candidateSince=millis();candidate=Mode::Idle;changeMode(Mode::Idle);}
    else changeMode(requested);
    forceFrame=true;return true;
  }
  return false;
}
bool setPattern(const String& name) {
  const Patterns::Definition* selected=Patterns::find(name.c_str());
  if(!selected)return false;
  pattern=selected;swingPulse.reset();forceFrame=true;return true;
}
void setLocked(bool value) {
  locked=value;
  if(!value) {requested=Mode::Auto;candidateSince=millis();candidate=active;}
  else if(requested==Mode::Auto)requested=active;
}
void setBrightness(uint8_t value) {brightness=value>Config::MAX_BRIGHTNESS?Config::MAX_BRIGHTNESS:value;forceFrame=true;}
void setColor(uint32_t value) {color=value&0xffffff;forceFrame=true;}
void showBattery() {batteryOverlay=true;meterTime=millis();forceFrame=true;}
String statusJSON() {
  char json[1536],voltageJSON[24],percentJSON[24];
  const bool isBattery=powerSource==EngineMath::PowerSource::Battery;
  const bool isExternal=powerSource==EngineMath::PowerSource::USB;
  const bool usbLimited=usbPresent || isExternal;
  if(isBattery) {snprintf(voltageJSON,sizeof(voltageJSON),"%.3f",batteryV);snprintf(percentJSON,sizeof(percentJSON),"%.1f",batteryPercent);}
  else {strcpy(voltageJSON,"null");strcpy(percentJSON,"null");}
  const char* mode=batteryOverlay?"battery":MODE_NAMES[unsigned(active)];
  snprintf(json,sizeof(json),
    "{\"mode\":\"%s\",\"requestedMode\":\"%s\",\"locked\":%s,\"pattern\":\"%s\",\"brightness\":%u,\"appliedBrightness\":%.2f,\"maxBrightness\":%u,\"currentMa\":%.1f,\"powerScale\":%.4f,\"batteryVoltage\":%s,\"batteryPercent\":%s,\"batteryRaw\":%.1f,\"batteryAdcMv\":%.1f,\"imu\":%s,\"orientation\":\"%s\",\"speed\":%.1f,\"acceleration\":[%.3f,%.3f,%.3f],\"booting\":%s,\"critical\":%s,\"maintenance\":%s,\"color\":\"#%06lx\"}",
    mode,MODE_NAMES[unsigned(requested)],locked?"true":"false",pattern->name,brightness,appliedBrightness,Config::MAX_BRIGHTNESS,currentMa,powerScale,voltageJSON,percentJSON,rawAdc,rawMv,imuOK?"true":"false",orientation,speed,accelX,accelY,accelZ,bootStage<3?"true":"false",critical?"true":"false",servicing?"true":"false",(unsigned long)color);
  String result(json);
  result.remove(result.length()-1);
  char supply[320];
  snprintf(supply,sizeof(supply),",\"supplyVoltage\":%.3f,\"powerSource\":\"%s\",\"externalPower\":%s,\"usbHostPresent\":%s,\"currentLimitMa\":%.0f,\"brightnessLimit\":%u}",
    batteryV,isExternal?"usb":isBattery?"battery":"invalid",isExternal?"true":"false",usbPresent?"true":"false",
    usbLimited?Config::USB_LED_CURRENT_MA:Config::MAX_LED_CURRENT_MA,usbLimited?Config::USB_MAX_BRIGHTNESS:Config::MAX_BRIGHTNESS);
  result+=supply;
  result.remove(result.length()-1);
  char renderer[240];
  snprintf(renderer,sizeof(renderer),",\"renderer\":\"%s\",\"renderStats\":{\"actualFPS\":%.1f,\"sendMeanUs\":%.1f,\"sendMaxUs\":%lu,\"lateFrames\":%lu,\"targetFPS\":%u,\"frames\":%lu}}",
    "dither",actualFPS,statsFrames?float(statsSendUs)/statsFrames:0,
    (unsigned long)statsMaxUs,(unsigned long)statsLate,1000,(unsigned long)statsFrames);
  result+=renderer;
  return result;
}
}
