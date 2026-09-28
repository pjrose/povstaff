// Dependency-free behavioral checks of the actual embedded dashboard script.
// This small DOM double intentionally throws on innerHTML assignments.
const assert=require('node:assert/strict');
const vm=require('node:vm');
const {dashboard,createMockServer}=require('./dashboard-preview.cjs');
const html=dashboard(), script=html.match(/<script>([\s\S]*?)<\/script>/)[1];
assert.ok(html.includes('open Wi-Fi network by default'));
assert.ok(html.includes('Arduino OTA is also available from your computer without a password.'));
assert.ok(html.includes('id="wifiPassword" type="password"'),'Home Wi-Fi credentials remain available');
assert.ok(!html.includes('Authentication expired')&&!html.includes('configured OTA password'));
new vm.Script(script);
class Element {
  constructor(tag='div'){this.tagName=tag.toUpperCase();this.children=[];this.value='';this.textContent='';this.hidden=false;this.disabled=false;this.checked=false;this.isConnected=true;this.style={};this.dataset={};this.attributes={};this.listeners={};this.classes=new Set();this.classList={toggle:(name,on)=>on?this.classes.add(name):this.classes.delete(name)};}
  set innerHTML(value){throw new Error('Untrusted HTML sink used: '+value);}
  setAttribute(key,value){this.attributes[key]=value;}
  addEventListener(name,fn){const previous=this.listeners[name];this.listeners[name]=event=>{previous?.(event);return fn(event);};}
  append(...children){this.children.push(...children);}
  replaceChildren(...children){this.children=children;}
  contains(element){return this.children.includes(element)||this.children.some(child=>child.contains?.(element));}
}
const elements=new Map();
for(const match of html.matchAll(/<([a-z][a-z0-9]*)\b([^>]*\bid="([^"]+)"[^>]*)>/g)) {
  const element=new Element(match[1]);element.value=match[2].match(/\bvalue="([^"]*)"/)?.[1]||'';elements.set(match[3],element);
}
const modes=[...html.matchAll(/data-mode="([^"]+)"/g)].map(match=>{const e=new Element('button');e.dataset.mode=match[1];return e;});
const colors=[...html.matchAll(/data-color="([^"]+)"/g)].map(match=>{const e=new Element('button');e.dataset.color=match[1];return e;});
const requests=[], uploads=[];
class XHR {
  constructor(){this.upload={};this.headers={};uploads.push(this);}
  open(method,url){this.method=method;this.url=url;}
  setRequestHeader(name,value){this.headers[name]=value;}
  send(body){this.body=body;this.upload.onprogress?.({lengthComputable:true,loaded:body.size,total:body.size});this.status=200;this.responseText='{"ok":true}';queueMicrotask(()=>this.onload());}
}
const document={visibilityState:'hidden',activeElement:null,getElementById:id=>{assert.ok(elements.has(id),'Unknown DOM id '+id);return elements.get(id);},
  createElement:tag=>new Element(tag),querySelectorAll:query=>query==='[data-mode]'?modes:query==='[data-color]'?colors:[...elements.values(),...modes,...colors].filter(e=>['BUTTON','INPUT','SELECT'].includes(e.tagName))};
const context=vm.createContext({document,URLSearchParams,AbortController,XMLHttpRequest:XHR,console,Date,Promise,Error,Number,Math,String,Set,Array,
  setTimeout:()=>1,clearTimeout:()=>{},confirm:()=>true,prompt:()=>'',
  fetch:async(path,options)=>{requests.push({path,options});return{ok:true,status:200,json:async()=>({ok:true})};}});
vm.runInContext(script,context);
const evaluate=expression=>vm.runInContext(expression,context);

async function main(){
  assert.equal(requests.length,0,'Loading dashboard must not switch the physical renderer');
  evaluate(`renderStatus({version:'5.0.0',uptime:3661,heap:65536,engine:{mode:'walking_stick',requestedMode:'walking_stick',locked:true,batteryPercent:7,batteryVoltage:3.4,critical:false,brightness:24,appliedBrightness:20,maxBrightness:64,currentMa:1000,powerScale:.8,imu:true,orientation:'vertical',speed:42.2,color:'#aa3311',pattern:'rainbow'},network:{mode:'ap',savedMode:'ap',ssid:'POVSTAFF',ip:'192.168.4.1'},storage:{mounted:true,count:0,used:4096,total:100000}})`);
  assert.equal(elements.get('activeMode').textContent,'Walking stick');
  assert.equal(elements.get('batteryPercent').textContent,'7');
  assert.equal(elements.get('batteryVoltage').textContent,'3.40 V');
  assert.equal(elements.get('current').textContent,'1000 mA');
  assert.equal(elements.get('powerScale').textContent,'80%');
  assert.equal(elements.get('gyro').textContent,'42.2 °/s');
  assert.equal(elements.get('brightness').value,'24');
  assert.equal(elements.get('modeLock').checked,true);
  assert.equal(elements.get('wifiPassword').value,'');
  assert.equal(modes.find(e=>e.dataset.mode==='walking_stick').attributes['aria-pressed'],'true');
  assert.equal(elements.get('rendererBaseline').attributes['aria-pressed'],'true');
  evaluate(`renderStatus({...lastState,engine:{...lastState.engine,renderer:'dither',renderStats:{actualFPS:812.4,sendMeanUs:170.25,sendMaxUs:280,lateFrames:19,targetFPS:1000,frames:8124}}})`);
  assert.equal(elements.get('rendererDither').attributes['aria-pressed'],'true');assert.equal(elements.get('rendererBaseline').attributes['aria-pressed'],'false');
  assert.equal(elements.get('rendererFPS').textContent,'812.4 Hz');assert.equal(elements.get('rendererMean').textContent,'170.3 µs');assert.equal(elements.get('rendererMax').textContent,'280 µs');assert.equal(elements.get('rendererLate').textContent,'19');
  const beforeRenderer=requests.length;await elements.get('rendererDither').listeners.click();
  const rendererWrites=requests.slice(beforeRenderer).filter(r=>r.options.method==='POST');assert.equal(rendererWrites.length,1);assert.equal(rendererWrites[0].path,'/api/renderer');assert.equal(rendererWrites[0].options.body.toString(),'name=dither');assert.equal(rendererWrites[0].options.headers['X-POV-Request'],'1');
  const beforeBreathing=requests.length;await elements.get('rendererBreathing').listeners.click();
  assert.deepEqual(requests.slice(beforeBreathing).filter(r=>r.options.method==='POST').map(r=>[r.path,r.options.body.toString()]),[['/api/pattern','name=breathing'],['/api/mode','name=led_test']],'Breathing test changes only pattern and mode');
  evaluate(`renderStatus({...lastState,engine:{...lastState.engine,powerSource:'usb',externalPower:true,usbHostPresent:true,supplyVoltage:4.71,batteryVoltage:null,batteryPercent:null,critical:false,appliedBrightness:.007}})`);
  assert.equal(elements.get('powerSourceLabel').textContent,'USB power');
  assert.equal(elements.get('batteryPercent').textContent,'USB','USB supply must not display a false charge percentage');
  assert.equal(elements.get('batteryUnit').hidden,true);
  assert.equal(elements.get('batteryMeter').hidden,true);
  assert.equal(elements.get('batteryVoltage').textContent,'Supply · 4.71 V');
  assert.equal(elements.get('batteryWarning').hidden,true);
  assert.equal(elements.get('appliedBrightness').textContent,'0.007 / 255','Tiny applied levels must remain visible');
  assert.ok(elements.get('powerHelp').textContent.includes('16 / 255')&&elements.get('powerHelp').textContent.includes('500 mA'));
  evaluate(`renderStatus({...lastState,engine:{...lastState.engine,critical:true}})`);
  assert.equal(elements.get('batteryWarning').hidden,false,'Critical hold-off remains visible during a USB power transition');
  evaluate(`renderStatus({...lastState,engine:{...lastState.engine,brightnessLimit:6,currentLimitMa:450}})`);
  assert.ok(elements.get('powerHelp').textContent.includes('6 / 255')&&elements.get('powerHelp').textContent.includes('450 mA'),'USB limits come from firmware telemetry');
  evaluate(`renderStatus({...lastState,engine:{...lastState.engine,powerSource:'invalid',externalPower:false,usbHostPresent:false,supplyVoltage:1.2,batteryVoltage:null,batteryPercent:null,critical:true}})`);
  assert.equal(elements.get('batteryPercent').textContent,'—','Invalid battery telemetry must not coerce null to 0%');
  assert.equal(elements.get('batteryMeter').hidden,true);assert.equal(elements.get('batteryWarning').hidden,false);
  evaluate(`renderStatus({...lastState,engine:{...lastState.engine,powerSource:'battery',supplyVoltage:3.4,batteryVoltage:3.4,batteryPercent:7,critical:false}})`);
  assert.equal(elements.get('batteryPercent').textContent,'7');assert.equal(elements.get('batteryUnit').hidden,false);assert.equal(elements.get('batteryMeter').hidden,false);
  elements.get('brightness').value='33';elements.get('brightness').listeners.input();
  evaluate(`updateControl('brightness',24)`);assert.equal(elements.get('brightness').value,'33','Polling must preserve dirty slider');
  document.activeElement=elements.get('duration');elements.get('duration').value='99';evaluate(`updateControl('duration',15)`);assert.equal(elements.get('duration').value,'99','Polling must preserve focused input');document.activeElement=null;
  evaluate(`renderFiles({files:[{index:0,name:'<img src=x onerror=alert(1)>.bmp',width:72,height:100,bytes:21654,active:true}],currentName:'<svg/onload=alert(1)>',duration:15,count:1,error:'<script>bad</script>'})`);
  assert.equal(elements.get('fileList').children[0].children[0].children[0].textContent,'<img src=x onerror=alert(1)>.bmp');
  assert.equal(elements.get('currentFile').textContent,'<svg/onload=alert(1)>');
  assert.equal(elements.get('fileError').textContent,'<script>bad</script>');
  await evaluate(`api('/api/color',{value:'#aa3311'})`);
  assert.equal(requests.at(-1).options.headers['X-POV-Request'],'1');
  assert.equal(requests.at(-1).options.credentials,'omit');
  assert.equal(requests.at(-1).options.headers.Authorization,undefined);
  assert.equal(requests.at(-1).options.body.toString(),'value=%23aa3311');
  await evaluate(`api('/api/files?name=Test.bmp',undefined,'DELETE')`);
  assert.equal(requests.at(-1).options.method,'DELETE');assert.equal(requests.at(-1).options.headers['X-POV-Request'],'1');
  await evaluate(`upload('/api/files/upload',{name:'Test_file.bmp',size:1234},()=>{})`);
  assert.equal(uploads.at(-1).url,'/api/files/upload');
  assert.equal(uploads.at(-1).headers['X-POV-Filename'],'Test_file.bmp');
  assert.equal(uploads.at(-1).headers['X-POV-Size'],'1234');
  assert.equal(uploads.at(-1).headers['Content-Type'],'application/octet-stream');
  assert.equal(uploads.at(-1).body.size,1234,'Raw file body must be used, not multipart FormData');
  await evaluate(`upload('/api/ota',{name:'firmware app.bin',size:900000},()=>{})`);
  assert.equal(uploads.at(-1).url,'/api/ota');
  assert.equal(uploads.at(-1).headers['X-POV-Filename'],'firmware%20app.bin');
  assert.equal(uploads.at(-1).headers['X-POV-Size'],'900000');
  document.visibilityState='visible';
  const beforePoll=requests.length;
  await evaluate('poll()');
  assert.ok(requests.length>beforePoll,'Visible dashboard polls telemetry');
  assert.equal(elements.get('connectionText').textContent,'Staff connected','Poll must complete without a rendering error');
  evaluate('busy=true');const beforeBusy=requests.length;await evaluate('poll()');
  assert.equal(requests.length,beforeBusy,'Telemetry must pause during uploads and other operations');evaluate('busy=false');
  const server=createMockServer();await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
  try{const base='http://127.0.0.1:'+server.address().port;const response=await fetch(base+'/');assert.equal(response.status,200);assert.equal(response.headers.get('www-authenticate'),null);assert.ok((await response.text()).includes('Mock preview · simulated staff'));
    const state=await(await fetch(base+'/api/status')).json();assert.equal(state.engine.batteryPercent,70.5);assert.equal(state.network.mode,'ap');
    const denied=await fetch(base+'/api/mode',{method:'POST',body:'name=off'});assert.equal(denied.status,403);
    const result=await fetch(base+'/api/mode',{method:'POST',headers:{'X-POV-Request':'1'},body:'name=off'});assert.equal(result.status,200);
    assert.equal((await(await fetch(base+'/api/status')).json()).engine.mode,'off');
  }finally{await new Promise(resolve=>server.close(resolve));}
  console.log('Dashboard JS syntax, telemetry, editing protection, safe text rendering, mutation headers, raw uploads, and mock routes passed.');
}
main().catch(error=>{console.error(error);process.exitCode=1;});
