// Local mock for visual/interaction QA only. No staff, radio, flash, or LEDs.
// Run: node tests/dashboard-preview.cjs [port]
const http = require('node:http');
const fs = require('node:fs');
const path = require('node:path');

function dashboard() {
  const source = fs.readFileSync(path.join(__dirname, '../code/POV4.1/Dashboard.cpp'), 'utf8');
  return source.match(/R"POVDASH\(([\s\S]*)\)POVDASH"/)[1];
}
function previewScript() {
  const source=fs.readFileSync(path.join(__dirname,'../code/POV4.1/Preview.cpp'),'utf8');
  return source.match(/R"([A-Za-z0-9_]*)\(([\s\S]*)\)\1"/)[2];
}
function previewFixture(height,shift=0) {
  const bytes=Buffer.alloc(54+216*height);
  bytes.write('BM');bytes.writeUInt32LE(bytes.length,2);bytes.writeUInt32LE(54,10);bytes.writeUInt32LE(40,14);
  bytes.writeInt32LE(72,18);bytes.writeInt32LE(-height,22);bytes.writeUInt16LE(1,26);bytes.writeUInt16LE(24,28);
  for(let y=0;y<height;y++)for(let x=0;x<72;x++){
    const phase=x/72*2+y/height*3+shift;
    const offset=54+y*216+x*3;
    for(let c=0;c<3;c++)bytes[offset+c]=Math.round(255*Math.pow((1+Math.sin(phase*Math.PI*2+c*2.1))/2,2));
  }
  return bytes;
}

function createMockServer() {
  const started = Date.now();
  const engine = {mode:'sword', requestedMode:'sword', locked:true, pattern:'rainbow',
    brightness:24, appliedBrightness:21.8, maxBrightness:64, currentMa:923, powerScale:.91,
    batteryVoltage:3.87, batteryPercent:70.5, batteryRaw:2381, batteryAdcMv:1935,
    powerSource:'battery', externalPower:false, supplyVoltage:3.87, usbHostPresent:false,brightnessLimit:64,currentLimitMa:1800,
    imu:true, orientation:'vertical', speed:12.4, acceleration:[.02, .01, .99],
    booting:false, critical:false, maintenance:false, color:'#6090ff'};
  const network = {mode:'ap', savedMode:'ap', savedSSID:'', pendingReboot:false,
    connected:false, ssid:'POVSTAFF-A4B2', ip:'192.168.4.1', apSSID:'POVSTAFF-A4B2',
    apIP:'192.168.4.1', hostname:'povstaff.local', fallback:false, rssi:null};
  let files = [{name:'Aurora.bmp', bytes:38934, width:72, height:180},
    {name:'Electric_Garden.bmp', bytes:51990, width:72, height:240},
    {name:'Golden_Hour.bmp', bytes:32454, width:72, height:150}];
  const contents=new Map(files.map((file,index)=>[file.name,previewFixture(file.height,index/3)]));
  let selected = 0, duration = 15, revision = 1;
  return http.createServer(async(req,res) => {
    const url = new URL(req.url, 'http://127.0.0.1');
    const json = (data, status=200) => {res.writeHead(status, {'Content-Type':'application/json','Cache-Control':'no-store'});res.end(JSON.stringify(data));};
    if(req.method==='GET' && url.pathname==='/preview.js') {
      res.writeHead(200,{'Content-Type':'text/javascript; charset=utf-8','Cache-Control':'no-store'});res.end(previewScript());return;
    }
    if(req.method==='GET' && url.pathname==='/api/files/content') {
      const name=url.searchParams.get('name');
      if(!/^[A-Za-z0-9_-]{1,44}\.bmp$/i.test(name||''))return json({ok:false,error:'Invalid file name'},400);
      const data=contents.get(name);if(!data)return json({ok:false,error:'Image unavailable'},404);
      res.writeHead(200,{'Content-Type':'image/bmp','Content-Length':data.length,'Cache-Control':'no-store'});res.end(data);return;
    }
    if(req.method==='GET' && url.pathname==='/') {
      res.writeHead(200, {'Content-Type':'text/html; charset=utf-8','Cache-Control':'no-store'});
      // Persistent visible identification: screenshots must never imply hardware validation.
      res.end(dashboard().replace('<title>POV / Staff control</title>', '<title>MOCK PREVIEW / POV Staff</title>')
        .replace('Your light. In motion.', 'Mock preview · simulated staff'));
      return;
    }
    if(req.method==='GET' && url.pathname==='/api/status') return json({version:'5.0.0 MOCK',
      uptime:3720+Math.floor((Date.now()-started)/1000), heap:178920, engine, network,
      storage:{mounted:true, used:files.reduce((n,f)=>n+f.bytes,0),total:1441792,count:files.length,error:''}});
    if(req.method==='GET' && url.pathname==='/api/files') return json({files:files.map((f,index)=>({...f,index,active:index===selected})),
      currentIndex:selected,currentName:files[selected]?.name||'',duration,count:files.length,revision,error:''});
    if(req.method!=='GET' && req.headers['x-pov-request']!=='1') return json({ok:false,error:'Required request header missing'},403);
    const chunks=[]; let size=0;
    for await(const chunk of req){size+=chunk.length;if(size>4*1024*1024){json({ok:false,error:'Mock payload limit exceeded'},413);return;}chunks.push(chunk);}
    const body=Buffer.concat(chunks), form=new URLSearchParams(body.toString());
    switch(url.pathname) {
      case '/api/mode': engine.mode=engine.requestedMode=form.get('name');engine.locked=engine.mode!=='auto';break;
      case '/api/pattern': engine.pattern=form.get('name');break;
      case '/api/brightness': engine.brightness=Number(form.get('value'));engine.appliedBrightness=engine.brightness*.91;break;
      case '/api/color': engine.color=form.get('value');break;
      case '/api/lock': engine.locked=form.get('value')==='1';if(!engine.locked)engine.requestedMode='auto';break;
      case '/api/battery/show': break;
      case '/api/sequences/select': selected=Number(form.get('index'));break;
      case '/api/sequences/next': if(files.length)selected=(selected+Number(form.get('direction'))+files.length)%files.length;break;
      case '/api/sequences/duration': duration=Number(form.get('seconds'));break;
      case '/api/sequences/reload': revision++;break;
      case '/api/files': if(req.method!=='DELETE')return json({ok:false,error:'Not found'},404);contents.delete(url.searchParams.get('name'));files=files.filter(f=>f.name!==url.searchParams.get('name'));selected=Math.min(selected,files.length-1);revision++;break;
      case '/api/files/upload': {
        if(req.headers['content-type']!=='application/octet-stream'||size!==Number(req.headers['x-pov-size']))return json({ok:false,error:'Mock expected raw bytes and exact size'},400);
        const name=decodeURIComponent(req.headers['x-pov-filename']||'');if(!/^[A-Za-z0-9_-]{1,44}\.bmp$/i.test(name))return json({ok:false,error:'Invalid BMP filename'},400);
        const item={name,bytes:size,width:72,height:Math.max(1,Math.floor((size-54)/216))};
        const existing=files.findIndex(f=>f.name===name);if(existing<0)files.push(item);else files[existing]=item;files.sort((a,b)=>a.name.localeCompare(b.name));contents.set(name,body);revision++;if(selected<0)selected=0;break;
      }
      case '/api/wifi': network.savedMode=form.get('mode');network.savedSSID=form.get('ssid');network.pendingReboot=true;break;
      case '/api/ota': if(req.headers['content-type']!=='application/octet-stream'||size!==Number(req.headers['x-pov-size']))return json({ok:false,error:'Mock expected raw bytes and exact size'},400);break;
      case '/api/storage/format': if(form.get('confirm')!=='ERASE')return json({ok:false,error:'Confirmation required'},400);files=[];contents.clear();revision++;selected=-1;break;
      case '/api/reboot': break;
      default: return json({ok:false,error:'Not found'},404);
    }
    json({ok:true});
  });
}
module.exports={dashboard,createMockServer};
if(require.main===module) {
  const port=Number(process.argv[2]||8866);
  createMockServer().listen(port,'127.0.0.1',()=>console.log('Mock dashboard only (no hardware): http://127.0.0.1:'+port));
}
