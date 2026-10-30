#include "Preview.h"

// Preview assets live in program flash; no generated images consume LittleFS.
const char PREVIEW_JS[] PROGMEM = R"POVPREVIEW(
(function(root,factory){
  'use strict';
  const core=factory();
  if(typeof module==='object'&&module.exports)module.exports=core;
  if(typeof document!=='undefined'){
    const start=()=>core.mount(root,document);
    if(document.readyState==='loading')document.addEventListener('DOMContentLoaded',start,{once:true});else start();
  }
})(typeof globalThis!=='undefined'?globalThis:this,function(){
  'use strict';
  function decodeBMP(buffer){
    if(!(buffer instanceof ArrayBuffer)||buffer.byteLength<54||buffer.byteLength>68096)throw new Error('BMP must contain 54–68096 bytes.');
    const view=new DataView(buffer),u16=p=>view.getUint16(p,true),u32=p=>view.getUint32(p,true),i32=p=>view.getInt32(p,true);
    if(u16(0)!==0x4d42)throw new Error('Not a BMP file.');
    const offset=u32(10),dib=u32(14),width=i32(18),signedHeight=i32(22),height=Math.abs(signedHeight);
    if(dib<40||dib>124||offset<14+dib||offset>4096||offset>buffer.byteLength)throw new Error('Unsupported BMP header.');
    if(width!==72||height<1||height>296||u16(26)!==1||u16(28)!==24||u32(30)!==0)throw new Error('Preview requires a 72-pixel-wide, uncompressed 24-bit BMP with up to 296 rows.');
    const stride=(width*3+3)&~3,pixelBytes=stride*height,declared=u32(2);
    if(pixelBytes>64000||offset+pixelBytes>buffer.byteLength||(declared&&(declared>buffer.byteLength||declared<offset+pixelBytes)))throw new Error('Incomplete BMP pixel data.');
    const source=new Uint8Array(buffer),rgb=new Uint8Array(width*height*3);
    for(let y=0;y<height;y++)for(let x=0;x<width;x++){
      const src=offset+(signedHeight<0?y:height-1-y)*stride+x*3,dst=(y*width+x)*3;
      rgb[dst]=source[src+2];rgb[dst+1]=source[src+1];rgb[dst+2]=source[src];
    }
    return {width,height,rgb};
  }
  function frameAt(milliseconds,rpm,rows){
    const phase=((milliseconds*rpm/60000)%1+1)%1;
    return {phase,row:Math.min(rows-1,Math.floor(phase*rows))};
  }
  // 141 cm staff: two 50 cm LED arms repeat the same 72 logical pixels.
  // Pixel zero is next to the 41 cm unlit grip; pixel 71 is at each tip.
  function pixelSpan(pixel,side){
    const step=153/72,inner=63+pixel*step;
    return {start:side<0?-inner-step:inner,length:step};
  }
  function createCache(limit=12){
    const entries=new Map();
    return {get(key){const value=entries.get(key);if(value){entries.delete(key);entries.set(key,value);}return value;},
      put(key,value){entries.delete(key);entries.set(key,value);while(entries.size>limit)entries.delete(entries.keys().next().value);},
      clear(){entries.clear();},get size(){return entries.size;}};
  }
  function mount(root,doc){
    const $=id=>doc.getElementById(id),canvas=$('previewCanvas');if(!canvas)return;
    const ctx=canvas.getContext('2d',{alpha:false});if(!ctx){$('previewStatus').textContent='Canvas preview is unavailable in this browser.';return;}
    const cache=createCache(),motion=root.matchMedia?root.matchMedia('(prefers-reduced-motion: reduce)'):{matches:false};
    let image=null,selected='',revision=null,catalogueKey='',names=[],request=null,generation=0,mutating=false,visible=true;
    let playing=!motion.matches,phase=0,rpm=80,lastTime=0,raf=0,exposure=null,clockText=0;
    function status(message){$('previewStatus').textContent=message;}
    function playLabel(){$('previewPlay').textContent=playing?'Pause preview':'Play preview';$('previewPlay').setAttribute('aria-pressed',String(playing));}
    function cancel(){generation++;if(request){request.abort();request=null;}}
    function stop(){if(raf)root.cancelAnimationFrame(raf);raf=0;lastTime=0;}
    function active(){return playing&&image&&!mutating&&visible&&doc.visibilityState!=='hidden'&&$('previewView').value!=='exposure';}
    function schedule(){if(active()&&!raf)raf=root.requestAnimationFrame(tick);}
    function background(context){context.fillStyle='#0c1410';context.fillRect(0,0,512,512);context.strokeStyle='#24362b';context.lineWidth=1;context.beginPath();context.arc(256,256,216,0,Math.PI*2);context.stroke();context.beginPath();context.moveTo(28,256);context.lineTo(484,256);context.moveTo(256,28);context.lineTo(256,484);context.stroke();}
    function staff(context,row,angle,alpha,width){
      context.save();context.translate(256,256);context.rotate(angle);context.globalAlpha=alpha;
      for(let x=0;x<72;x++){const i=(row*72+x)*3;context.fillStyle='rgb('+image.rgb[i]+','+image.rgb[i+1]+','+image.rgb[i+2]+')';for(const side of [-1,1]){const span=pixelSpan(x,side);context.fillRect(-width/2,span.start,width,span.length);}}
      context.restore();
    }
    function draw(){
      background(ctx);if(!image)return;
      const mode=$('previewView').value,row=Math.min(image.height-1,Math.floor(phase*image.height));
      if(mode==='exposure'){
        if(!exposure){exposure=doc.createElement('canvas');exposure.width=exposure.height=512;const ec=exposure.getContext('2d',{alpha:false});background(ec);ec.globalCompositeOperation='lighten';for(let y=0;y<image.height;y++)staff(ec,y,y/image.height*Math.PI*2,1,Math.max(2,1360/image.height));}
        ctx.drawImage(exposure,0,0);
      }else if(mode==='stationary'){staff(ctx,row,0,1,22);}
      else{
        // Integrate an 85 ms exposure, sampling at most 48 rows. This keeps
        // short persistence visible without requesting any network frames.
        const trail=Math.min(.45,85*rpm/60000),steps=Math.max(2,Math.min(48,Math.ceil(trail*image.height)));
        ctx.globalCompositeOperation='lighten';for(let s=steps;s>=0;s--){const p=(phase-trail*s/steps+1)%1;staff(ctx,Math.min(image.height-1,Math.floor(p*image.height)),p*Math.PI*2,.15+.85*(1-s/steps),5);}ctx.globalCompositeOperation='source-over';
      }
      $('previewFrame').value=String(row);$('previewFrameValue').textContent=(row+1)+' / '+image.height;
    }
    function tick(time){raf=0;if(!active()){lastTime=0;return;}if(lastTime)phase=frameAt(phase*60000/rpm+Math.max(0,time-lastTime),rpm,image.height).phase;lastTime=time;draw();if(time-clockText>250){$('previewCycle').textContent=rpm+' RPM · '+Math.round(60000/rpm)+' ms / cycle';clockText=time;}schedule();}
    async function load(name){
      cancel();selected=name;$('previewFile').value=name;image=null;exposure=null;phase=0;stop();draw();
      if(!name){status('Upload a BMP to see it in motion.');return;}
      if(mutating){status('Preview waits while the staff is busy.');return;}
      const cached=cache.get(name);if(cached){image=cached;ready();return;}
      const token=generation,controller=new AbortController();request=controller;
      const timeout=root.setTimeout(()=>controller.abort(),10000);status('Loading '+name+'…');
      try{
        const response=await root.fetch('/api/files/content?name='+encodeURIComponent(name),{cache:'no-store',credentials:'omit',signal:controller.signal});
        if(!response.ok)throw new Error('Cannot load this BMP ('+response.status+'). Rescan the library and retry.');
        const length=Number(response.headers.get('content-length'));if(length>68096)throw new Error('BMP is too large to preview.');
        const decoded=decodeBMP(await response.arrayBuffer());if(token!==generation||mutating)return;
        cache.put(name,decoded);image=decoded;ready();
      }catch(error){if(token===generation){status(error.name==='AbortError'?'Preview request interrupted. Choose the file again to retry.':error.message);}}
      finally{root.clearTimeout(timeout);if(token===generation)request=null;}
    }
    function ready(){
      $('previewFrame').max=String(image.height-1);status(selected+' · 72 × '+image.height+' pixels');draw();schedule();
    }
    function catalogue(data){
      const files=data.files||[],key=files.map(f=>f.name+':'+f.bytes+':'+f.width+':'+f.height).join('|'),nextRevision=data.libraryRevision??data.revision??null;
      if(key===catalogueKey&&nextRevision===revision)return;
      catalogueKey=key;revision=nextRevision;names=files.map(f=>f.name);cache.clear();cancel();image=null;exposure=null;stop();
      const selector=$('previewFile');selector.replaceChildren();for(const file of files){const option=doc.createElement('option');option.value=file.name;option.textContent=file.name;selector.append(option);}
      const next=names.includes(selected)?selected:(files.find(f=>f.active)?.name||files[0]?.name||'');load(next);
    }
    function setBusy(value){mutating=value;if(value){cancel();stop();if(!image&&selected)status('Preview waits while the staff is busy.');}else if(!image&&selected)load(selected);else schedule();}
    function show(name){if(!names.includes(name))return;load(name);$('previewPanel').scrollIntoView({behavior:motion.matches?'auto':'smooth',block:'nearest'});}
    function invalidate(){catalogueKey='';revision=null;cache.clear();cancel();image=null;exposure=null;stop();draw();}
    root.POVPreview={catalogue,setBusy,show,invalidate};
    $('previewFile').addEventListener('change',()=>load($('previewFile').value));
    $('previewView').addEventListener('change',()=>{stop();draw();schedule();});
    $('previewPlay').addEventListener('click',()=>{playing=!playing;playLabel();stop();schedule();});
    $('previewRPM').addEventListener('input',()=>{rpm=Math.max(20,Math.min(200,Number($('previewRPM').value)||80));$('previewRPMValue').textContent=rpm+' RPM';$('previewCycle').textContent=rpm+' RPM · '+Math.round(60000/rpm)+' ms / cycle';lastTime=0;draw();});
    $('previewFrame').addEventListener('input',()=>{if(!image)return;playing=false;playLabel();stop();phase=Number($('previewFrame').value)/image.height;draw();});
    doc.addEventListener('visibilitychange',()=>{stop();schedule();});
    if(root.IntersectionObserver){const observer=new root.IntersectionObserver(entries=>{visible=entries[0].isIntersecting;stop();schedule();},{threshold:.01});observer.observe(canvas);}
    if(motion.addEventListener)motion.addEventListener('change',event=>{if(event.matches){playing=false;playLabel();stop();}});
    playLabel();background(ctx);setBusy(!!root.POVPreviewBusy);if(root.POVPreviewCatalogue)catalogue(root.POVPreviewCatalogue);
  }
  return {decodeBMP,frameAt,pixelSpan,createCache,mount};
});
)POVPREVIEW";
