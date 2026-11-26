"use strict";
(() => {
  const $ = id => document.getElementById(id);
  const chartSamples = [];
  const MAX_POINTS = 60;
  // Static labels are stored in Ukrainian in the HTML. English translations
  // are paired by their data-i18n keys. No network requests are needed.
  const staticEnglish=["Overview","Charts","Settings","System","Connecting…","Local ESP32 web interface","Disconnected","Boiler temperature","Target","No temperature sensor data","Pressure","bar","Current reading","Time","Shot timer","Heating","Live data","Real-time temperature and pressure","Up to 60 recent readings · hover or tap the chart for details. Time starts when the browser begins recording.","Live readings · last 60 samples","Freeze","Clear","Freeze saves a snapshot of the chart in browser memory while live data continues.","Shot archive","Up to 60 shots of at least 15 seconds stored by ESP32 in LittleFS","Refresh","Return to LIVE","Download CSV","Loading recordings…","Replacing the entire LittleFS partition may erase the archive. Update individual web files via USB.","Pressure profile","Three brewing stages — duration and target pressure","Pre-infusion","Gently saturate the coffee puck","Ramp & hold","Reach main brewing pressure","Pressure decline","Finish extraction","Temperature","Brew and steam temperature targets","Boiler PID","Heating control coefficients","Pump PID","Pressure control coefficients","Save settings","Applied outside a shot, saved on ESP32 and sent to Nextion.","Waiting for ESP32 settings…","Reload values","Save settings","File system","Loading LittleFS status…","Total","Used","Free","Shot archive","Write errors*","* Archive write errors since the last ESP32 restart.","Restarting ESP32 will close the current WebSocket connection.","Restart ESP32","Gagguino • Standalone LittleFS interface • Data via WebSocket"];
  const dynamicText={"uk":{"connected":"Підключено","disconnected":"Немає з'єднання","online":"ESP32 у мережі","connecting":"Очікуємо з'єднання","live":"Поточні показники · останні 60 вимірювань","frozen":"Зафіксований графік · дані продовжують надходити","archived":"Архівний пролив #","unfreeze":"Повернутися до LIVE","freeze":"Зафіксувати","bar":"бар","sec":"с","time":"Час","temp":"Температура","pressure":"Тиск","waitReadings":"Очікування показників…","archiveFormat":"Непідтримуваний формат архіву","archiveCorrupt":"Пошкоджений архів","dateUnknown":"Дата невідома","loadingArchive":"Завантажуємо історію з ESP32…","invalidServer":"Неправильна відповідь сервера","archiveCountPrefix":"В архіві ","archiveCountSuffix":" із 60 приготувань.","shot":"Пролив #","samples":"вимірювань","view":"Переглянути","archiveLoadError":"Не вдалося завантажити архів: ","loadingShot":"Завантажуємо пролив #","shotSamplesSuffix":" вимірювань. Наведіть курсор на графік.","viewError":"Помилка перегляду: ","duration":"Тривалість","endPressure":"Кінцевий тиск","brew":"Приготування","steam":"Пара","settingInvalid":"Некоректне значення ","settingsLoaded":"Параметри завантажено з ESP32.","settingsLoadError":"Не вдалося завантажити налаштування: ","busy":"Дочекайтеся завершення попередньої операції.","checkValue":"Перевірте значення: ","confirmSave":"Зберегти всі параметри приготування та PID на ESP32?","queued":"Команду прийнято. Перевіряємо збереження…","notConfirmed":"ESP32 ще не підтвердила нові параметри","saved":"Усі 14 параметрів збережено та підтверджено ESP32.","saveFailed":"Не вдалося підтвердити збереження: ","invalidStorage":"Некоректні дані від ESP32","usedPercent":"% зайнято","fsAvailable":t("fsAvailable"),"fsFailed":"Не вдалося отримати стан LittleFS: ","noConnection":"Немає з'єднання з ESP32.","confirmReboot":"Перезапустити ESP32?","rebootSent":"Команду перезапуску надіслано.","mib":"МіБ","kib":"КіБ"},"en":{"connected":"Connected","disconnected":"Disconnected","online":"ESP32 online","connecting":"Waiting for connection","live":"Live readings · last 60 samples","frozen":"Frozen chart · live data continues","archived":"Archived shot #","unfreeze":"Return to LIVE","freeze":"Freeze","bar":"bar","sec":"s","time":"Time","temp":"Temperature","pressure":"Pressure","waitReadings":"Waiting for readings…","archiveFormat":"Unsupported archive format","archiveCorrupt":"Corrupted archive","dateUnknown":"Date unknown","loadingArchive":"Loading history from ESP32…","invalidServer":"Invalid server response","archiveCountPrefix":"Archive contains ","archiveCountSuffix":" of 60 shots.","shot":"Shot #","samples":"samples","view":"View","archiveLoadError":"Could not load archive: ","loadingShot":"Loading shot #","shotSamplesSuffix":" samples. Hover over the chart.","viewError":"Could not view shot: ","duration":"Duration","endPressure":"Final pressure","brew":"Brew","steam":"Steam","settingInvalid":"Invalid value: ","settingsLoaded":"Settings loaded from ESP32.","settingsLoadError":"Could not load settings: ","busy":"Wait for the previous operation to finish.","checkValue":"Check value: ","confirmSave":"Save all brewing and PID settings to ESP32?","queued":"Request accepted. Verifying the saved values…","notConfirmed":"ESP32 has not yet confirmed the new settings","saved":"All 14 settings saved and confirmed by ESP32.","saveFailed":"Could not confirm the save: ","invalidStorage":"Invalid data from ESP32","usedPercent":"% used","fsAvailable":"LittleFS available","fsFailed":"Could not read LittleFS status: ","noConnection":"Not connected to ESP32.","confirmReboot":"Restart ESP32?","rebootSent":"Restart command sent.","mib":"MiB","kib":"KiB"}};
  const staticUkrainian={};
  const staticNodes=[...document.querySelectorAll("[data-i18n]")];
  for(const node of staticNodes){
    staticUkrainian[node.dataset.i18n]=node.textContent;
  }
  let language="uk";
  try{
    if(localStorage.getItem("gagguino-language")==="en")language="en";
  }catch(_){}
  const t=key=>dynamicText[language][key];
  function applyLanguage(next){
    language=next==="en"?"en":"uk";
    document.documentElement.lang=language;
    for(const node of staticNodes){
      if(!node.isConnected)continue;
      const id=node.dataset.i18n;
      const index=Number(id.slice(1));
      node.textContent=language==="en"?staticEnglish[index]:staticUkrainian[id];
    }
    for(const button of document.querySelectorAll("[data-lang]")){
      const selected=button.dataset.lang===language;
      button.classList.toggle("active",selected);
      button.setAttribute("aria-pressed",String(selected));
    }
    try{localStorage.setItem("gagguino-language",language);}catch(_){}
    // Update dynamic content without changing device state or pending edits.
    setStatus(connected);
    updateFreezeButton();
    if(selectedArchive===null){
      $("archiveStatus").textContent=t("archiveCountPrefix")+archiveShots.length+t("archiveCountSuffix");
    }
    for(const group of settingGroups){
      for(const [key] of group.items){
        const input=$("setting-"+key);
        if(!input)continue;
        input.parentElement.parentElement.querySelector("label").textContent=fieldLabel(key);
        input.parentElement.querySelector("span").textContent=fieldUnit(key);
      }
    }
    shotDateFormatter=new Intl.DateTimeFormat(language==="uk"?"uk-UA":"en-GB",{
      timeZone:"Europe/Kyiv",year:"numeric",month:"2-digit",day:"2-digit",
      hour:"2-digit",minute:"2-digit",second:"2-digit",hourCycle:"h23"
    });
    drawCharts();
    if(archiveShots.length)renderShotList();
    if(location.hash==="#system")refreshStorage();
    if(location.hash==="#settings"&&!queuedEdit)reloadAllSettings();
    $("pageTitle").textContent=pageLabels()[currentPage()];
  }
  function currentPage(){
    const hash=(location.hash||"#dashboard").slice(1);
    return ["dashboard","graphs","settings","system"].includes(hash)?hash:"dashboard";
  }
  const pageLabels=()=>language==="uk"?
    {dashboard:"Огляд",graphs:"Графіки",settings:"Налаштування",system:"Система"}:
    {dashboard:"Overview",graphs:"Charts",settings:"Settings",system:"System"};
  function fieldLabel(key){
    if(key.endsWith("Dur"))return t("duration");
    if(key==="declinePress")return t("endPressure");
    if(key.endsWith("Press"))return t("pressure");
    if(key==="targetTemp")return t("brew");
    if(key==="steamTemp")return t("steam");
    return key.slice(-2)==="Kp"?"Kp":key.slice(-2)==="Ki"?"Ki":"Kd";
  }
  function fieldUnit(key){
    return key.endsWith("Dur")?t("sec"):key.endsWith("Press")?t("bar"):key.endsWith("Temp")?"°C":"";
  }
  document.querySelectorAll("[data-lang]").forEach(button=>{
    button.addEventListener("click",()=>applyLanguage(button.dataset.lang));
  });

  let ws, reconnectHandle, connected=false;
  let lastInfo = {temp:null,press:null,power:null,time:0};
  let queuedEdit = false;
  let frozenSamples = null;
  let selectedArchive = null;
  let archiveShots = [];
  let selectedArchiveId = null;
  let archiveBusy = false;
  const hoverPosition = {liveChart:null,detailChart:null};
  const plotBounds = {};
  let realTargetTemp = null;
  function validLiveTemperature(value) {
    // Firmware publishes -50 C when LMT01 data is unavailable.
    return Number.isFinite(value) && value > -40 && value < 150;
  }
  function showReading(value) {
    const tempOk=validLiveTemperature(value.temp);
    $("temperature").textContent=tempOk?fmt(value.temp):"—";
    $("tempSensorStatus").hidden=tempOk;
    $("temperature").closest(".metric").classList.toggle("sensor-unavailable",!tempOk);
    $("pressure").textContent=fmt(value.press);
    $("currentTime").textContent=clock(value.time);
    $("heatPower").textContent=value.power===null?"—":String(Math.round(value.power));
    $("heatBar").style.width=Math.max(0,Math.min(100,value.power||0))+"%";
  }
  function setStatus(value) {
    connected=value;
    $("connectionBadge").textContent=value?t("connected"):t("disconnected");
    $("connectionBadge").classList.toggle("ok",value);
    $("connectionLabel").textContent=value?t("online"):t("connecting");
    $("connectionDot").classList.toggle("ok",value);
  }
  function number(v) { const n=Number(v);return Number.isFinite(n)?n:null; }
  function fmt(v,d=1){return v===null?"—":v.toFixed(d);}
  function clock(value){let sec=Math.max(0,Math.floor(Number(value)||0));return String(Math.floor(sec/60)).padStart(2,"0")+":"+String(sec%60).padStart(2,"0");}
  function publish() {
    showReading(lastInfo);
  }
  function syncSettings(message) {
    if(message.targetTemp!==undefined) {
      const temp=number(message.targetTemp);
      if(temp!==null) {
        realTargetTemp=temp;
        $("targetTemperature").textContent=String(temp);
      }
    }
  }
  function onMessage(event) {
    let msg;try{msg=JSON.parse(event.data);}catch{return;}
    if(msg.name==="initInfo"){
      lastInfo.temp=number(msg.currentTemp);lastInfo.press=number(msg.currentPress);
      syncSettings(msg);publish();
    } else if(msg.name==="updateInfo"){
      lastInfo.temp=number(msg.currentTemp);lastInfo.press=number(msg.currentPress);
      lastInfo.power=number(msg.currentPower);lastInfo.time=number(msg.currentTime)||0;
      chartSamples.push({t:Date.now()/1000,temp:validLiveTemperature(lastInfo.temp)?lastInfo.temp:null,press:lastInfo.press});
      if(chartSamples.length>MAX_POINTS)chartSamples.shift();
      publish();drawCharts();
    } else if(["targetTemp","pidKp","pidKi","pidKd"].includes(msg.name)){
      syncSettings({[msg.name]:msg.value});
    }
  }
  function connect() {
    const url=(location.protocol==="https:"?"wss":"ws")+"://"+location.host+"/ws";
    ws=new WebSocket(url);
    ws.onopen=()=>{setStatus(true);ws.send("initInfo");};
    ws.onmessage=onMessage;
    ws.onerror=()=>ws.close();
    ws.onclose=()=>{setStatus(false);clearTimeout(reconnectHandle);reconnectHandle=setTimeout(connect,2500);};
  }
  function visibleSamples(id) {
    if(id==="detailChart" && selectedArchive!==null)return selectedArchive;
    return id==="detailChart" && frozenSamples!==null?frozenSamples:chartSamples;
  }
  function updateFreezeButton() {
    const frozen=frozenSamples!==null;
    $("freezeChart").textContent=frozen?t("unfreeze"):t("freeze");
    $("freezeChart").disabled=selectedArchive!==null;
    $("freezeChart").classList.toggle("freeze-active",frozen);
    $("chartModeLabel").textContent=selectedArchive!==null
      ?t("archived")+selectedArchiveId
      :frozen?t("frozen")
        :t("live");
  }
  // LIVE uses stable axes, archived shots use a padded, data-driven scale.
  // All values come from the existing packed shot data, never interpolated.
  function chartScale(samples, archived) {
    if(!archived)return {tempMin:0,tempMax:150,pressMin:0,pressMax:12};
    const temps=samples.map(s=>s.temp).filter(v=>Number.isFinite(v)&&v>-40&&v<150);
    const pressures=samples.map(s=>s.press).filter(v=>Number.isFinite(v)&&v>=0);
    let tempMin=0,tempMax=150,pressMax=12;
    if(temps.length) {
      const low=Math.min(...temps),high=Math.max(...temps);
      const pad=Math.max(1.5,(high-low)*0.17);
      tempMin=Math.floor((low-pad)*2)/2;
      tempMax=Math.ceil((high+pad)*2)/2;
      if(tempMax<=tempMin)tempMax=tempMin+3;
    }
    if(pressures.length) {
      const high=Math.max(...pressures);
      pressMax=Math.max(3,Math.ceil((high+Math.max(.5,high*.09))*2)/2);
    }
    return {tempMin,tempMax,pressMin:0,pressMax};
  }
  function draw(canvas) {
    const ctx=canvas.getContext("2d");if(!ctx)return;
    const w=canvas.clientWidth,h=canvas.clientHeight;
    if(w<110||h<110)return;
    const dpr=Math.min(window.devicePixelRatio||1,3);
    canvas.width=Math.round(w*dpr);canvas.height=Math.round(h*dpr);
    ctx.setTransform(dpr,0,0,dpr,0,0);ctx.clearRect(0,0,w,h);
    const left=50,right=w-49,top=39,bottom=h-35;
    const samples=visibleSamples(canvas.id);
    const first=samples.length?samples[0].t:0;
    const end=samples.length?samples[samples.length-1].t:first+60;
    const span=Math.max(1,end-first);
    const archived=canvas.id==="detailChart" && selectedArchive!==null;
    const scale=chartScale(samples,archived);
    plotBounds[canvas.id]={left,right,top,bottom,first,span};
    const xAt=s=>left+(right-left)*(s.t-first)/span;
    const yAt=(v,min,max)=>bottom-(Math.max(min,Math.min(max,v))-min)/(max-min)*(bottom-top);
    const tempY=v=>yAt(v,scale.tempMin,scale.tempMax);
    const pressureY=v=>yAt(v,scale.pressMin,scale.pressMax);
    const ink="#a5b4c5",orange="#f0b17c",blue="#78c7fb";

    ctx.font="11px system-ui";ctx.textBaseline="alphabetic";
    ctx.textAlign="right";ctx.fillStyle=orange;ctx.fillText("°C",left-8,16);
    ctx.textAlign="left";ctx.fillStyle=blue;ctx.fillText(t("bar"),right+8,16);
    for(let i=0;i<=4;i++){
      const y=top+(bottom-top)*i/4;
      ctx.strokeStyle="#344254";ctx.lineWidth=1;ctx.beginPath();
      ctx.moveTo(left,y);ctx.lineTo(right,y);ctx.stroke();
      const tv=scale.tempMax-(scale.tempMax-scale.tempMin)*i/4;
      const pv=scale.pressMax-(scale.pressMax-scale.pressMin)*i/4;
      ctx.fillStyle=ink;ctx.textAlign="right";
      ctx.fillText(tv.toFixed(archived?1:(i%2?1:0)),left-8,y+4);
      ctx.textAlign="left";
      ctx.fillText(pv.toFixed(archived?1:0),right+8,y+4);
    }
    ctx.textAlign="center";ctx.fillStyle=ink;
    for(let i=0;i<=4;i++){
      const x=left+(right-left)*i/4;
      ctx.fillText((span*i/4).toFixed(span<10?1:0)+t("sec"),x,h-11);
    }
    const tooltip=$(canvas.id==="liveChart"?"liveTooltip":"detailTooltip");
    if(samples.length<2){
      ctx.fillStyle=ink;
      ctx.fillText(t("waitReadings"),(left+right)/2,(top+bottom)/2);
      tooltip.hidden=true;return;
    }

    // Drawing is clipped to the plot so a reading outside the axis range
    // cannot overlap the labels, legend or title.
    ctx.save();
    ctx.beginPath();ctx.rect(left,top,right-left,bottom-top);ctx.clip();

    // One fill per uninterrupted pressure segment; missing values stay gaps.
    let segment=[];
    function fillPressureSegment(){
      if(segment.length<2){segment=[];return;}
      const gradient=ctx.createLinearGradient(0,top,0,bottom);
      gradient.addColorStop(0,"rgba(120,199,251,.23)");
      gradient.addColorStop(1,"rgba(120,199,251,.015)");
      ctx.beginPath();ctx.moveTo(xAt(segment[0]),bottom);
      for(const pt of segment)ctx.lineTo(xAt(pt),pressureY(pt.press));
      ctx.lineTo(xAt(segment[segment.length-1]),bottom);ctx.closePath();
      ctx.fillStyle=gradient;ctx.fill();segment=[];
    }
    for(const point of samples){
      if(Number.isFinite(point.press)&&point.press>=0)segment.push(point);
      else fillPressureSegment();
    }
    fillPressureSegment();

    for(const [key,color,yFn] of [["temp",orange,tempY],["press",blue,pressureY]]){
      ctx.strokeStyle=color;ctx.lineWidth=2.6;
      ctx.lineJoin="round";ctx.lineCap="round";ctx.beginPath();
      let started=false;
      for(const point of samples){
        const v=point[key];
        if(!Number.isFinite(v)){started=false;continue;}
        const x=xAt(point),y=yFn(v);
        if(!started){ctx.moveTo(x,y);started=true;}else ctx.lineTo(x,y);
      }
      ctx.stroke();
    }
    const pos=hoverPosition[canvas.id];
    if(pos===null||pos<left||pos>right){tooltip.hidden=true;ctx.restore();return;}
    const target=first+(pos-left)/(right-left)*span;
    let sample=samples[0];
    for(const candidate of samples){
      if(Math.abs(candidate.t-target)<Math.abs(sample.t-target))sample=candidate;
    }
    const x=xAt(sample);
    ctx.save();ctx.strokeStyle="#b4c5d5";ctx.setLineDash([4,5]);ctx.lineWidth=1;
    ctx.beginPath();ctx.moveTo(x,top);ctx.lineTo(x,bottom);ctx.stroke();ctx.restore();
    for(const [key,color,yFn] of [["temp",orange,tempY],["press",blue,pressureY]]){
      if(!Number.isFinite(sample[key]))continue;
      const y=yFn(sample[key]);
      ctx.beginPath();ctx.arc(x,y,5,0,Math.PI*2);
      ctx.fillStyle="#172230";ctx.fill();ctx.strokeStyle=color;ctx.lineWidth=2.5;ctx.stroke();
    }
    ctx.restore();
    tooltip.replaceChildren();
    for(const [label,value,color] of [
      [t("time"),(sample.t-first).toFixed(1)+" "+t("sec"),"#f1f4f7"],
      [t("temp"),Number.isFinite(sample.temp)?sample.temp.toFixed(1)+" °C":"—",orange],
      [t("pressure"),Number.isFinite(sample.press)?sample.press.toFixed(2)+" "+t("bar"):"—",blue]
    ]){
      const row=document.createElement("div");row.className="chart-tip-row";
      const name=document.createElement("span");name.textContent=label;
      const reading=document.createElement("strong");reading.textContent=value;reading.style.color=color;
      row.append(name,reading);tooltip.append(row);
    }
    tooltip.hidden=false;
    // Keep the popup away from the axis, even near the far-right samples.
    const tw=tooltip.offsetWidth,th=tooltip.offsetHeight;
    const preferred=x+13+tw<=w-4?x+13:x-tw-13;
    tooltip.style.left=Math.max(2,Math.min(w-tw-2,preferred))+"px";
    tooltip.style.top=Math.max(2,Math.min(h-th-2,top+15))+"px";
  }
  function drawCharts(){
    for(const id of ["liveChart","detailChart"]){
      const canvas=$(id);if(canvas&&canvas.clientWidth>0)draw(canvas);
    }
  }
  function initChartPointers(){
    for(const id of ["liveChart","detailChart"]){
      const canvas=$(id);
      canvas.addEventListener("pointermove",event=>{
        hoverPosition[id]=event.clientX-canvas.getBoundingClientRect().left;
        draw(canvas);
      });
      canvas.addEventListener("pointerleave",()=>{
        hoverPosition[id]=null;$(id==="liveChart"?"liveTooltip":"detailTooltip").hidden=true;draw(canvas);
      });
      canvas.addEventListener("pointerdown",event=>{
        hoverPosition[id]=event.clientX-canvas.getBoundingClientRect().left;draw(canvas);
      });
    }
  }
  function updateArchiveButtons() {
    $("returnLive").hidden=selectedArchive===null;
    $("downloadShot").hidden=selectedArchive===null;
    updateFreezeButton();
  }
  function parseArchive(buffer) {
    const data=new DataView(buffer);
    if(data.byteLength<16 || data.getUint32(0,true)!==0x31485347 ||
       data.getUint16(4,true)!==1)throw Error(t("archiveFormat"));
    const count=data.getUint16(6,true);
    if(count>600 || data.byteLength!==16+count*8)throw Error(t("archiveCorrupt"));
    const readings=[];
    for(let i=0;i<count;i++){
      const offset=16+i*8;
      readings.push({t:data.getUint16(offset,true)*0.2,
        temp:data.getInt16(offset+2,true)/10,
        press:data.getUint16(offset+4,true)/100,
        heat:data.getUint8(offset+6),pump:data.getUint8(offset+7)});
    }
    return readings;
  }
  // UTC timestamps are rendered in Kyiv time, including DST transitions.
  let shotDateFormatter=new Intl.DateTimeFormat("uk-UA",{
    timeZone:"Europe/Kyiv",year:"numeric",month:"2-digit",day:"2-digit",
    hour:"2-digit",minute:"2-digit",second:"2-digit",hourCycle:"h23"
  });
  function shotStartedAt(shot) {
    const seconds=Number(shot.startedUnix);
    return Number.isSafeInteger(seconds) && seconds>=1577836800
      ? shotDateFormatter.format(new Date(seconds*1000))
      : t("dateUnknown");
  }
  function renderShotList(){
    const container=$("shotList");container.replaceChildren();
    for(const shot of archiveShots){
      if(!Number.isSafeInteger(shot.id)||shot.id<=0)continue;
      const row=document.createElement("div");row.className="shot-row";
      if(shot.id===selectedArchiveId)row.classList.add("selected");
      const description=document.createElement("div");
      const label=document.createElement("strong");
      label.textContent=t("shot")+shot.id;
      const details=document.createElement("small");
      details.textContent=shotStartedAt(shot)+" · "+(shot.durationMs/1000).toFixed(1)+" "+t("sec")+" · "+shot.samples+" "+t("samples");
      description.append(label,details);
      const button=document.createElement("button");
      button.type="button";button.className="btn secondary";button.textContent=t("view");
      button.addEventListener("click",()=>viewArchive(shot.id));
      row.append(description,button);container.appendChild(row);
    }
  }
  async function refreshArchive() {
    $("archiveStatus").textContent=t("loadingArchive");
    try {
      const result=await fetch("/api/shots",{cache:"no-store"});
      if(!result.ok)throw Error("HTTP "+result.status);
      const list=await result.json();
      if(!Array.isArray(list))throw Error(t("invalidServer"));
      archiveShots=list.sort((a,b)=>b.id-a.id);
      $("archiveStatus").textContent=t("archiveCountPrefix")+archiveShots.length+t("archiveCountSuffix");
      renderShotList();
    } catch(err){
      $("archiveStatus").textContent=t("archiveLoadError")+err.message;
    }
  }
  async function viewArchive(id) {
    if(archiveBusy)return;
    archiveBusy=true;
    $("archiveStatus").textContent=t("loadingShot")+id+"…";
    try {
      const response=await fetch("/api/shot?id="+encodeURIComponent(id),{cache:"no-store"});
      if(!response.ok)throw Error("HTTP "+response.status);
      const readings=parseArchive(await response.arrayBuffer());
      selectedArchive=readings;selectedArchiveId=id;
      hoverPosition.detailChart=null;
      updateArchiveButtons();drawCharts();
      $("archiveStatus").textContent=t("shot")+id+": "+readings.length+t("shotSamplesSuffix");
      await refreshArchive();
    }catch(err){$("archiveStatus").textContent=t("viewError")+err.message;}
    finally{archiveBusy=false;}
  }
  function downloadArchiveCsv() {
    if(selectedArchive===null)return;
    const lines=["time_s,temperature_c,pressure_bar,heater_percent,pump_percent"];
    for(const s of selectedArchive){
      lines.push([s.t.toFixed(1),s.temp.toFixed(1),s.press.toFixed(2),s.heat,s.pump].join(","));
    }
    const url=URL.createObjectURL(new Blob([lines.join("\n")+"\n"],{type:"text/csv;charset=utf-8"}));
    const anchor=document.createElement("a");
    anchor.href=url;anchor.download="gagguino-shot-"+selectedArchiveId+".csv";
    document.body.appendChild(anchor);anchor.click();anchor.remove();
    setTimeout(()=>URL.revokeObjectURL(url),1000);
  }
  function navigate(){
    const page=(location.hash||"#dashboard").slice(1);
    const active=pageLabels()[page]?page:"dashboard";
    for(const key of Object.keys(labels)) $("page-"+key).hidden=key!==active;
    document.querySelectorAll(".nav-link").forEach(a=>{a.classList.toggle("active",a.dataset.page===active);});
    $("pageTitle").textContent=pageLabels()[active];drawCharts();
    if(active==="graphs")refreshArchive();
    if(active==="settings")reloadAllSettings();
    if(active==="system")refreshStorage();
  }
  function message(text,error=false){
    const el=$("systemActionStatus");
    el.textContent=text;el.style.color=error?"#ef8189":"#a0adbf";
  }
  const settingGroups = [
    {holder:"preinfSettingsFields",items:[
      ["preinfDur","Duration","s",0,600,1],
      ["preinfPress","Pressure","bar",0,12,0.1]]},
    {holder:"holdSettingsFields",items:[
      ["holdDur","Duration","s",0,600,1],
      ["holdPress","Pressure","bar",0,12,0.1]]},
    {holder:"declineSettingsFields",items:[
      ["declineDur","Duration","s",0,60,1],
      ["declinePress","Final pressure","bar",0,12,0.1]]},
    {holder:"temperatureSettingsFields",items:[
      ["targetTemp","Brew","°C",20,105,1],
      ["steamTemp","Steam","°C",100,144,1]]},
    {holder:"boilerSettingsFields",items:[
      ["pidKp","Kp","",0,100,0.1],["pidKi","Ki","",0,100,0.1],
      ["pidKd","Kd","",0,100,0.1]]},
    {holder:"pumpSettingsFields",items:[
      ["pumpKp","Kp","",0,100,0.1],["pumpKi","Ki","",0,100,0.1],
      ["pumpKd","Kd","",0,100,0.1]]}
  ];
  const settingSpecs=settingGroups.flatMap(g=>g.items);
  let settingsRevision=null;
  function buildSettingsFields() {
    for(const group of settingGroups) {
      const holder=$(group.holder);
      for(const [key,label,unit,min,max,step] of group.items) {
        const field=document.createElement("div");
        field.className="web-setting-row";
        const caption=document.createElement("label");
        caption.htmlFor="setting-"+key;
        caption.textContent=fieldLabel(key);
        const controls=document.createElement("div");
        controls.className="web-setting-controls";
        const input=document.createElement("input");
        input.id="setting-"+key; input.name=key; input.type="number";
        input.min=String(min);input.max=String(max);input.step=String(step);
        input.required=true;input.disabled=true;
        const suffix=document.createElement("span");
        suffix.textContent=fieldUnit(key);
        controls.append(input,suffix);field.append(caption,controls);
        holder.append(field);
      }
    }
  }
  function settingsStatus(text,error=false) {
    const el=$("allSettingsStatus");el.textContent=text;
    el.style.color=error?"#ef8189":"#a0adbf";
  }
  async function readAllSettings(overwrite=true) {
    const response=await fetch("/api/settings",{cache:"no-store"});
    if(!response.ok)throw Error("HTTP "+response.status);
    const data=await response.json();
    for(const [key,,,min,max,step] of settingSpecs){
      const value=Number(data[key]);
      if(!Number.isFinite(value))throw Error(t("settingInvalid")+key);
      const input=$("setting-"+key);
      if(overwrite)input.value=String(value);
      input.disabled=false;
    }
    settingsRevision=data.revision;
    syncSettings(data);
    return data;
  }
  async function reloadAllSettings() {
    if(queuedEdit)return;
    try {
      await readAllSettings(true);
      settingsStatus(t("settingsLoaded"));
    }catch(error){settingsStatus(t("settingsLoadError")+error.message,true);}
  }
  $("reloadAllSettings").addEventListener("click",reloadAllSettings);
  $("allSettingsForm").addEventListener("submit",async event=>{
    event.preventDefault();
    if(queuedEdit){settingsStatus(t("busy"),true);return;}
    const payload=new URLSearchParams();
    for(const [key,,,min,max,step] of settingSpecs){
      const input=$("setting-"+key);
      const n=Number(input.value);
      if(input.value.trim()===""||!Number.isFinite(n)||n<min||n>max||
         Math.abs(Math.round(n/step)*step-n)>0.000001){
        settingsStatus(t("checkValue")+key,true);input.focus();return;
      }
      payload.set(key,input.value);
    }
    if(!confirm(t("confirmSave")))return;
    queuedEdit=true;$("saveAllSettings").disabled=true;
    try {
      const response=await fetch("/api/settings",{
        method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded"},
        body:payload.toString(),cache:"no-store"
      });
      if(!response.ok)throw Error("HTTP "+response.status+" "+await response.text());
      settingsStatus(t("queued"));
      let confirmed=false;
      for(let attempt=0;attempt<12;attempt++){
        await new Promise(resolve=>setTimeout(resolve,250));
        const actual=await readAllSettings(false);
        if([...payload].every(([key,v])=>Math.abs(Number(actual[key])-Number(v))<0.00001)){
          confirmed=true;break;
        }
      }
      if(!confirmed)throw Error(t("notConfirmed"));
      await readAllSettings(true);
      settingsStatus(t("saved"));
    }catch(error){settingsStatus(t("saveFailed")+error.message,true);}
    finally{queuedEdit=false;$("saveAllSettings").disabled=false;}
  });
  function formatStorageBytes(value) {
    if(!Number.isFinite(value)||value<0)return "—";
    return value>=1048576?(value/1048576).toFixed(2)+" "+t("mib"):
      (value/1024).toFixed(1)+" "+t("kib");
  }
  async function refreshStorage() {
    const status=$("storageStatus");
    const button=$("refreshStorage");
    button.disabled=true;
    try {
      const response=await fetch("/api/system/storage",{cache:"no-store"});
      if(!response.ok)throw new Error("HTTP "+response.status);
      const data=await response.json();
      const values=["totalBytes","usedBytes","freeBytes","shots","maxShots","writeErrors"];
      if(values.some(key=>!Number.isSafeInteger(data[key])||data[key]<0)||
         data.totalBytes<=0||data.usedBytes>data.totalBytes||
         data.freeBytes!==data.totalBytes-data.usedBytes)
        throw new Error(t("invalidStorage"));
      $("storageTotal").textContent=formatStorageBytes(data.totalBytes);
      $("storageUsed").textContent=formatStorageBytes(data.usedBytes);
      $("storageFree").textContent=formatStorageBytes(data.freeBytes);
      $("storageShots").textContent=data.shots+" / "+data.maxShots;
      $("storageErrors").textContent=String(data.writeErrors);
      const percent=100*data.usedBytes/data.totalBytes;
      $("storageFill").style.width=percent.toFixed(1)+"%";
      $("storageMeter").setAttribute("aria-valuenow",String(Math.round(percent)));
      $("storagePercent").textContent=percent.toFixed(1)+t("usedPercent");
      $("storageDetails").hidden=false;
      status.textContent=t("fsAvailable");
    }catch(error) {
      $("storageDetails").hidden=true;
      status.textContent=t("fsFailed")+error.message;
    }finally{button.disabled=false;}
  }
  $("refreshStorage").addEventListener("click",refreshStorage);
  $("rebootButton").addEventListener("click",()=>{
    if(!connected){message(t("noConnection"),true);return;}
    if(confirm(t("confirmReboot"))){ws.send("reboot");message(t("rebootSent"));}
  });
  $("freezeChart").addEventListener("click",()=>{
    if(selectedArchive!==null)return;
    frozenSamples=frozenSamples===null?chartSamples.map(p=>({...p})):null;
    updateFreezeButton();hoverPosition.detailChart=null;drawCharts();
  });
  $("clearChart").addEventListener("click",()=>{
    chartSamples.length=0;
    frozenSamples=null;hoverPosition.detailChart=null;updateFreezeButton();drawCharts();
  });
  $("refreshShots").addEventListener("click",refreshArchive);
  $("returnLive").addEventListener("click",()=>{
    selectedArchive=null;selectedArchiveId=null;
    hoverPosition.detailChart=null;updateArchiveButtons();drawCharts();
    refreshArchive();
  });
  $("downloadShot").addEventListener("click",downloadArchiveCsv);
  window.addEventListener("hashchange",navigate);
  window.addEventListener("resize",drawCharts);
  buildSettingsFields();initChartPointers();updateArchiveButtons();navigate();publish();connect();applyLanguage(language);
})();
