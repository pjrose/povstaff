const assert=require('node:assert/strict');
const fs=require('node:fs');
const path=require('node:path');
const vm=require('node:vm');
const cpp=fs.readFileSync(path.join(__dirname,'../code/POV4.1/Preview.cpp'),'utf8');
const script=cpp.match(/R"POVPREVIEW\(([\s\S]*)\)POVPREVIEW"/)[1];
const preview=vm.runInThisContext('(function(){const module={exports:{}};'+script+';return module.exports;})()');

function bmp(topDown=false,rows=4){
  const buffer=new ArrayBuffer(54+72*3*rows),v=new DataView(buffer),b=new Uint8Array(buffer);
  v.setUint16(0,0x4d42,true);v.setUint32(2,buffer.byteLength,true);v.setUint32(10,54,true);v.setUint32(14,40,true);v.setInt32(18,72,true);v.setInt32(22,topDown?-rows:rows,true);v.setUint16(26,1,true);v.setUint16(28,24,true);
  for(let y=0;y<rows;y++)for(let x=0;x<72;x++){const i=54+((topDown?y:rows-1-y)*72+x)*3;b[i]=x;b[i+1]=y;b[i+2]=200-y;}
  return buffer;
}
const upright=preview.decodeBMP(bmp(true)),bottomUp=preview.decodeBMP(bmp(false));
assert.deepEqual(upright.rgb,bottomUp.rgb,'Top-down and bottom-up BMPs normalize to identical logical row order');
assert.deepEqual([...upright.rgb.slice(0,3)],[200,0,0]);
assert.deepEqual([...upright.rgb.slice((3*72+71)*3,(3*72+71)*3+3)],[197,3,71],'BGR channels convert to RGB');
for(const mutate of [v=>v.setUint16(0,0,true),v=>v.setInt32(18,73,true),v=>v.setUint16(28,32,true),v=>v.setUint32(30,1,true),v=>v.setUint32(14,125,true),v=>v.setUint32(10,5000,true),v=>v.setUint32(2,55,true),v=>v.setInt32(22,297,true)]){
  const bad=bmp();mutate(new DataView(bad));assert.throws(()=>preview.decodeBMP(bad));
}
assert.throws(()=>preview.decodeBMP(bmp().slice(0,-1)));
assert.equal(preview.frameAt(0,80,100).row,0);
assert.equal(preview.frameAt(375,80,100).row,50);
assert.equal(preview.frameAt(750,80,100).row,0,'80 RPM completes one BMP every 750 ms');
assert.equal(preview.frameAt(300,200,72).row,0);
assert.equal(preview.frameAt(1500,20,72).row,36);
const innerPositive=preview.pixelSpan(0,1),innerNegative=preview.pixelSpan(0,-1),tipPositive=preview.pixelSpan(71,1),tipNegative=preview.pixelSpan(71,-1);
assert.equal(innerPositive.start,63,'Positive arm starts outside the center grip');
assert.equal(innerNegative.start+innerNegative.length,-63,'Negative arm starts outside the center grip');
assert.equal(tipPositive.start+tipPositive.length,216,'Pixel 71 reaches positive tip');
assert.equal(tipNegative.start,-216,'Pixel 71 reaches negative tip');
for(let pixel=0;pixel<72;pixel++){const a=preview.pixelSpan(pixel,1),b=preview.pixelSpan(pixel,-1);assert.equal(a.length,b.length);assert.ok(Math.abs(a.start+b.start+b.length)<1e-10,'Each logical pixel is mirrored on the opposite arm');assert.ok(a.start>=63&&b.start+b.length<=-63);}
assert.equal(innerPositive.length*72,153,'Each illuminated end spans 153 canvas pixels');
const cache=preview.createCache();for(let i=0;i<12;i++)cache.put(i,{value:i});cache.get(0);cache.put(12,{value:12});
assert.equal(cache.size,12);assert.equal(cache.get(1),undefined,'Least recently used entry is evicted');assert.equal(cache.get(0).value,0);cache.clear();assert.equal(cache.size,0);

class Element{
  constructor(value=''){this.value=value;this.textContent='';this.listeners={};this.attributes={};this.children=[];this.context={fillRect(){},beginPath(){},arc(){},stroke(){},moveTo(){},lineTo(){},save(){},restore(){},translate(){},rotate(){},drawImage(){}};}
  setAttribute(k,v){this.attributes[k]=v;} addEventListener(event,fn){this.listeners[event]=fn;} append(child){this.children.push(child);}replaceChildren(){this.children=[];}getContext(){return this.context;}scrollIntoView(){}
}
async function main(){
  const ids=['previewCanvas','previewStatus','previewPlay','previewView','previewFrame','previewFrameValue','previewCycle','previewFile','previewPanel','previewRPM','previewRPMValue'];
  const elements=Object.fromEntries(ids.map(id=>[id,new Element(id==='previewView'?'spinning':id==='previewRPM'?'80':'')]));
  const doc={visibilityState:'visible',listeners:{},getElementById:id=>elements[id],createElement:()=>new Element(),addEventListener(event,fn){this.listeners[event]=fn;}};
  let calls=[],pending=null,rafCount=0,frameCallback=null,intersection=null;
  const root={matchMedia:()=>({matches:true,addEventListener(){}}),setTimeout,clearTimeout,requestAnimationFrame(fn){frameCallback=fn;rafCount++;return rafCount;},cancelAnimationFrame(){},IntersectionObserver:class{constructor(fn){intersection=fn;}observe(){}},fetch:async(url,options)=>{
    calls.push({url,options});if(pending)return new Promise(resolve=>pending.resolve=resolve);
    return {ok:true,headers:{get:()=>String(bmp().byteLength)},arrayBuffer:async()=>bmp()};
  }};
  const settle=()=>new Promise(resolve=>setImmediate(resolve));
  preview.mount(root,doc);const api=root.POVPreview;
  assert.equal(elements.previewPlay.textContent,'Play preview','Reduced motion starts paused');
  const files=[{name:'One.bmp',bytes:918,width:72,height:4,active:true},{name:'Two.bmp',bytes:918,width:72,height:4,active:false}];
  api.catalogue({files,revision:1});await settle();
  assert.equal(calls.length,1);assert.equal(calls[0].url,'/api/files/content?name=One.bmp');
  assert.equal(calls[0].options.credentials,'omit');assert.equal(elements.previewFrameValue.textContent,'1 / 4');
  elements.previewPlay.listeners.click();assert.equal(elements.previewPlay.textContent,'Pause preview');assert.ok(rafCount>0);
  frameCallback(100);frameCallback(475);assert.equal(elements.previewFrameValue.textContent,'3 / 4','Slow animation frames still advance at real 80 RPM');
  doc.visibilityState='hidden';doc.listeners.visibilitychange();let before=rafCount;frameCallback(500);assert.equal(rafCount,before,'Hidden tabs stop scheduling frames');doc.visibilityState='visible';doc.listeners.visibilitychange();
  intersection([{isIntersecting:false}]);before=rafCount;frameCallback(600);assert.equal(rafCount,before,'Offscreen canvas stops scheduling frames');intersection([{isIntersecting:true}]);
  elements.previewFrame.value='2';elements.previewFrame.listeners.input();assert.equal(elements.previewPlay.textContent,'Play preview');assert.equal(elements.previewFrameValue.textContent,'3 / 4','Scrubbing pauses at chosen row');
  api.show('Two.bmp');await settle();api.show('One.bmp');await settle();assert.equal(calls.length,2,'Cached preview does not fetch again');
  api.catalogue({files:files.map(f=>({...f,active:!f.active})),revision:1});await settle();assert.equal(calls.length,2,'Staff selection changes do not reload or change browser preview');assert.equal(elements.previewFile.value,'One.bmp');
  api.setBusy(true);api.catalogue({files,revision:2});await settle();assert.equal(calls.length,2,'Mutations suppress preview fetches');
  api.setBusy(false);await settle();assert.equal(calls.length,3,'Same-name replacement revision invalidates cached bytes');
  pending={};api.show('Two.bmp');await settle();const stale=calls.at(-1);api.setBusy(true);assert.equal(stale.options.signal.aborted,true,'Busy mutation aborts active download');
  pending.resolve({ok:true,headers:{get:()=>String(bmp().byteLength)},arrayBuffer:async()=>bmp()});await settle();assert.ok(elements.previewStatus.textContent.includes('busy'),'Aborted stale response cannot replace preview state');pending=null;
  api.setBusy(false);await settle();assert.equal(elements.previewFile.value,'Two.bmp');
  api.invalidate();api.catalogue({files,revision:2});await settle();assert.equal(calls.length,6,'Reboot invalidation fetches even if revision repeats');
  elements.previewView.value='exposure';elements.previewView.listeners.change();assert.equal(elements.previewFrameValue.textContent,'1 / 4');
  assert.ok(calls.every(call=>call.url.startsWith('/api/files/content?name=')),'Preview never mutates actual staff controls');
  console.log('Preview BMP validation/order/color, 80 RPM timing, 12-entry LRU, paused scrubbing, file isolation, revision/reboot invalidation, upload suppression, and stale-fetch cancellation passed.');
}
main().catch(error=>{console.error(error);process.exitCode=1;});
