"use strict";
(() => {
  const $ = id => document.getElementById(id);
  const chartSamples = [];
  const MAX_POINTS = 60;
  const labels = {dashboard:"Обзор",graphs:"Графики",settings:"Настройки"};
  let ws, reconnectHandle, connected=false;
  let lastInfo = {temp:null,press:null,power:null,time:0};
  let queuedEdit = false;
  let demo = false, demoTimer = null, demoSecond = 0;
  const realSamples = [];
  let frozenSamples = null;
  let selectedArchive = null;
  let archiveShots = [];
  let selectedArchiveId = null;
  let archiveBusy = false;
  const hoverPosition = {liveChart:null,detailChart:null};
  const plotBounds = {};
  let realTargetTemp = null;
  function demoReading(second) {
    const t = second % 44;
    let pressure;
    if(t < 5) pressure = 0;
    else if(t < 12) pressure = 1.2 + (t-5)*0.05;
    else if(t < 19) pressure = 1.55 + (t-12)*7.5/7;
    else if(t < 29) pressure = 9.05 + 0.08*Math.sin(t*1.4);
    else if(t < 39) pressure = 9 - (t-29)*0.45;
    else pressure = Math.max(0,4.5-(t-39)*1.2);
    return {temp:93.2+0.45*Math.sin(t/5)-0.1*Math.sin(t*1.7),
            press:Math.max(0,pressure),power:Math.max(0,Math.min(100,45+23*Math.sin(t/4))),time:t};
  }
  function showReading(value) {
    $("temperature").textContent=fmt(value.temp);
    $("pressure").textContent=fmt(value.press);
    $("currentTime").textContent=clock(value.time);
    $("heatPower").textContent=value.power===null?"—":String(Math.round(value.power));
    $("heatBar").style.width=Math.max(0,Math.min(100,value.power||0))+"%";
  }
  function demoTick() {
    const sample=demoReading(demoSecond++);
    showReading(sample);
    chartSamples.push({t:Date.now()/1000,temp:sample.temp,press:sample.press});
    if(chartSamples.length>MAX_POINTS)chartSamples.shift();
    drawCharts();
  }
  function setDemo(enabled) {
    if(demo===enabled)return;
    if(enabled && queuedEdit){message("Сначала дождитесь окончания отправки настроек.",true);return;}
    demo=enabled;
    if(demoTimer!==null){clearInterval(demoTimer);demoTimer=null;}
    $("demoBadge").hidden=!demo;
    $("demoNotice").hidden=!demo;
    $("demoToggle").textContent=demo?"Отключить DEMO":"Включить DEMO";
    $("demoToggle").classList.toggle("demo-active",demo);
    $("demoToggle").setAttribute("aria-pressed",String(demo));
    for(const input of document.querySelectorAll("#allSettingsForm input"))input.disabled=demo;
    $("saveAllSettings").disabled=demo;
    $("reloadAllSettings").disabled=demo;
    $("rebootButton").disabled=demo;
    $("targetTemperature").textContent=demo?"93 (DEMO)":realTargetTemp===null?"—":String(realTargetTemp);
    chartSamples.length=0;
    frozenSamples=null;
    selectedArchive=null; selectedArchiveId=null;
    updateFreezeButton();
    updateArchiveButtons();
    if(demo){
      demoSecond=0;demoTick();
      demoTimer=setInterval(demoTick,1000);
      message("DEMO: команды ESP32 отключены.");
    }else{
      chartSamples.push(...realSamples);
      publish();drawCharts();
      message("DEMO отключён. Возврат к реальным показаниям ESP32.");
    }
  }
  function setStatus(value) {
    connected=value;
    $("connectionBadge").textContent=value?"Подключено":"Нет соединения";
    $("connectionBadge").classList.toggle("ok",value);
    $("connectionLabel").textContent=value?"ESP32 онлайн":"Ожидание подключения";
    $("connectionDot").classList.toggle("ok",value);
    $("connectionText").textContent=value?"WebSocket соединён; данные поступают автоматически.":"Соединение потеряно. Повторное подключение выполняется автоматически.";
  }
  function number(v) { const n=Number(v);return Number.isFinite(n)?n:null; }
  function fmt(v,d=1){return v===null?"—":v.toFixed(d);}
  function clock(value){let sec=Math.max(0,Math.floor(Number(value)||0));return String(Math.floor(sec/60)).padStart(2,"0")+":"+String(sec%60).padStart(2,"0");}
  function publish() {
    if(!demo) showReading(lastInfo);
  }
  function syncSettings(message) {
    if(message.targetTemp!==undefined) {
      const temp=number(message.targetTemp);
      if(temp!==null) {
        realTargetTemp=temp;
        if(!demo)$("targetTemperature").textContent=String(temp);
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
      realSamples.push({t:Date.now()/1000,temp:lastInfo.temp,press:lastInfo.press});
      if(realSamples.length>MAX_POINTS)realSamples.shift();
      if(!demo){
        chartSamples.push({t:Date.now()/1000,temp:lastInfo.temp,press:lastInfo.press});
        if(chartSamples.length>MAX_POINTS)chartSamples.shift();
        publish();drawCharts();
      }
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
    $("freezeChart").textContent=frozen?"Вернуться к LIVE":"Зафиксировать";
    $("freezeChart").disabled=selectedArchive!==null;
    $("freezeChart").classList.toggle("freeze-active",frozen);
    $("chartModeLabel").textContent=selectedArchive!==null
      ?"Архивный пролив #"+selectedArchiveId
      :frozen?"Зафиксированный снимок · данные продолжают поступать"
        :"Живые показания · последние 60 измерений";
  }
  function draw(canvas) {
    const ctx=canvas.getContext("2d");if(!ctx)return;
    const w=canvas.clientWidth,h=canvas.clientHeight;
    if(w<110||h<110)return;
    const dpr=window.devicePixelRatio||1;
    canvas.width=Math.round(w*dpr);canvas.height=Math.round(h*dpr);
    ctx.setTransform(dpr,0,0,dpr,0,0);ctx.clearRect(0,0,w,h);
    const left=48,right=w-52,top=38,bottom=h-35;
    const samples=visibleSamples(canvas.id);
    const first=samples.length?samples[0].t:0;
    const end=samples.length?samples[samples.length-1].t:first+60;
    const span=Math.max(1,end-first);
    plotBounds[canvas.id]={left,right,top,bottom,first,span};
    ctx.font="11px system-ui";ctx.textBaseline="alphabetic";
    ctx.textAlign="right";ctx.fillStyle="#e5a36e";ctx.fillText("°C",left-8,16);
    ctx.textAlign="left";ctx.fillStyle="#69bfff";ctx.fillText("бар",right+8,16);
    for(let i=0;i<=4;i++){
      const y=top+(bottom-top)*i/4;
      ctx.strokeStyle="#394355";ctx.lineWidth=1;
      ctx.beginPath();ctx.moveTo(left,y);ctx.lineTo(right,y);ctx.stroke();
      ctx.fillStyle="#9aa8bd";
      ctx.textAlign="right";ctx.fillText((150-i*37.5).toFixed(i%2?1:0),left-8,y+4);
      ctx.textAlign="left";ctx.fillText(String(12-i*3),right+8,y+4);
    }
    ctx.textAlign="center";ctx.fillStyle="#9aa8bd";
    for(let i=0;i<=4;i++){
      const x=left+(right-left)*i/4;
      ctx.fillText((span*i/4).toFixed(span<10?1:0)+"с",x,h-11);
    }
    if(samples.length<2){
      ctx.fillText("Ожидание показаний…",(left+right)/2,(top+bottom)/2);
      $(canvas.id==="liveChart"?"liveTooltip":"detailTooltip").hidden=true;
      return;
    }
    const xAt=s=>left+(right-left)*(s.t-first)/span;
    const yAt=(v,max)=>bottom-(Math.max(0,Math.min(max,v))/max)*(bottom-top);
    for(const [key,color,max] of [["temp","#e5a36e",150],["press","#69bfff",12]]){
      ctx.strokeStyle=color;ctx.lineWidth=2;ctx.lineJoin="round";ctx.beginPath();
      let started=false;
      for(const s of samples){
        if(s[key]===null||!Number.isFinite(s[key])){started=false;continue;}
        const x=xAt(s),y=yAt(s[key],max);
        if(!started){ctx.moveTo(x,y);started=true;}else ctx.lineTo(x,y);
      }
      ctx.stroke();
    }
    const pos=hoverPosition[canvas.id];
    const tooltip=$(canvas.id==="liveChart"?"liveTooltip":"detailTooltip");
    if(pos===null||pos<left||pos>right){tooltip.hidden=true;return;}
    const target=first+(pos-left)/(right-left)*span;
    let sample=samples[0];
    for(const s of samples)if(Math.abs(s.t-target)<Math.abs(sample.t-target))sample=s;
    const x=xAt(sample);
    ctx.save();ctx.strokeStyle="#96a7ba";ctx.setLineDash([4,4]);ctx.beginPath();ctx.moveTo(x,top);ctx.lineTo(x,bottom);ctx.stroke();ctx.restore();
    for(const [key,color,max] of [["temp","#e5a36e",150],["press","#69bfff",12]]){
      if(sample[key]===null)continue;
      ctx.beginPath();ctx.arc(x,yAt(sample[key],max),4,0,Math.PI*2);ctx.fillStyle=color;ctx.fill();
    }
    tooltip.textContent="";
    for(const [txt,color] of [
      [(sample.t-first).toFixed(1)+" с","#f2f4f8"],
      ["Температура: "+fmt(sample.temp)+" °C","#e5a36e"],
      ["Давление: "+fmt(sample.press)+" бар","#69bfff"]]){
      const line=document.createElement("div");line.textContent=txt;line.style.color=color;tooltip.appendChild(line);
    }
    tooltip.hidden=false;
    // Keep tooltip inside chart container rather than letting it cover the right axis.
    const tw=tooltip.offsetWidth,th=tooltip.offsetHeight;
    tooltip.style.left=Math.min(Math.max(0,x+12),w-tw-3)+"px";
    tooltip.style.top=Math.max(0,Math.min(h-th-3,top+18))+"px";
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
       data.getUint16(4,true)!==1)throw Error("Неподдерживаемый формат архива");
    const count=data.getUint16(6,true);
    if(count>600 || data.byteLength!==16+count*8)throw Error("Повреждённый архив");
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
  const shotDateFormatter=new Intl.DateTimeFormat("ru-RU",{
    timeZone:"Europe/Kyiv",year:"numeric",month:"2-digit",day:"2-digit",
    hour:"2-digit",minute:"2-digit",second:"2-digit",hourCycle:"h23"
  });
  function shotStartedAt(shot) {
    const seconds=Number(shot.startedUnix);
    return Number.isSafeInteger(seconds) && seconds>=1577836800
      ? shotDateFormatter.format(new Date(seconds*1000))
      : "Дата неизвестна";
  }
  async function refreshArchive() {
    $("archiveStatus").textContent="Загружаем историю с ESP32…";
    try {
      const result=await fetch("/api/shots",{cache:"no-store"});
      if(!result.ok)throw Error("HTTP "+result.status);
      const list=await result.json();
      if(!Array.isArray(list))throw Error("Неверный ответ сервера");
      archiveShots=list.sort((a,b)=>b.id-a.id);
      $("archiveStatus").textContent="В архиве "+archiveShots.length+" из 60 приготовлений.";
      const container=$("shotList");container.replaceChildren();
      for(const shot of archiveShots){
        if(!Number.isSafeInteger(shot.id)||shot.id<=0)continue;
        const row=document.createElement("div");row.className="shot-row";
        if(shot.id===selectedArchiveId)row.classList.add("selected");
        const description=document.createElement("div");
        const label=document.createElement("strong");
        label.textContent="Пролив #"+shot.id;
        const details=document.createElement("small");
        details.textContent=shotStartedAt(shot)+" · "+(shot.durationMs/1000).toFixed(1)+" с · "+shot.samples+" измерений";
        description.append(label,details);
        const button=document.createElement("button");
        button.type="button";button.className="btn secondary";button.textContent="Просмотреть";
        button.addEventListener("click",()=>viewArchive(shot.id));
        row.append(description,button);container.appendChild(row);
      }
    } catch(err){
      $("archiveStatus").textContent="Не удалось загрузить архив: "+err.message;
    }
  }
  async function viewArchive(id) {
    if(archiveBusy)return;
    archiveBusy=true;
    $("archiveStatus").textContent="Загружаем пролив #"+id+"…";
    try {
      const response=await fetch("/api/shot?id="+encodeURIComponent(id),{cache:"no-store"});
      if(!response.ok)throw Error("HTTP "+response.status);
      const readings=parseArchive(await response.arrayBuffer());
      selectedArchive=readings;selectedArchiveId=id;
      hoverPosition.detailChart=null;
      updateArchiveButtons();drawCharts();
      $("archiveStatus").textContent="Пролив #"+id+": "+readings.length+" измерений. Наведи курсор на график.";
      await refreshArchive();
    }catch(err){$("archiveStatus").textContent="Ошибка просмотра: "+err.message;}
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
    const active=labels[page]?page:"dashboard";
    for(const key of Object.keys(labels)) $("page-"+key).hidden=key!==active;
    document.querySelectorAll(".nav-link").forEach(a=>{a.classList.toggle("active",a.dataset.page===active);});
    $("pageTitle").textContent=labels[active];drawCharts();
    if(active==="graphs")refreshArchive();
    if(active==="settings")reloadAllSettings();
  }
  function message(text,error=false){const el=$("allSettingsStatus");el.textContent=text;el.style.color=error?"#ef8189":"#a0adbf";}
  const settingGroups = [
    {holder:"profileSettingsFields",items:[
      ["preinfDur","Предсмачивание · время","с",0,600,1],
      ["preinfPress","Предсмачивание · давление","бар",0,12,0.1],
      ["holdDur","Набор и удержание · время","с",0,600,1],
      ["holdPress","Набор и удержание · давление","бар",0,12,0.1],
      ["declineDur","Снижение · время","с",0,60,1],
      ["declinePress","Снижение · конечное давление","бар",0,12,0.1]]},
    {holder:"temperatureSettingsFields",items:[
      ["targetTemp","Температура приготовления","°C",20,105,1],
      ["steamTemp","Температура пара","°C",100,144,1]]},
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
        caption.textContent=label;
        const controls=document.createElement("div");
        controls.className="web-setting-controls";
        const input=document.createElement("input");
        input.id="setting-"+key; input.name=key; input.type="number";
        input.min=String(min);input.max=String(max);input.step=String(step);
        input.required=true;input.disabled=true;
        const suffix=document.createElement("span");
        suffix.textContent=unit;
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
      if(!Number.isFinite(value))throw Error("Некорректное значение "+key);
      const input=$("setting-"+key);
      if(overwrite)input.value=String(value);
      input.disabled=demo;
    }
    settingsRevision=data.revision;
    syncSettings(data);
    return data;
  }
  async function reloadAllSettings() {
    if(queuedEdit)return;
    try {
      await readAllSettings(true);
      settingsStatus("Параметры загружены с ESP32.");
    }catch(error){settingsStatus("Не удалось загрузить настройки: "+error.message,true);}
  }
  $("reloadAllSettings").addEventListener("click",reloadAllSettings);
  $("allSettingsForm").addEventListener("submit",async event=>{
    event.preventDefault();
    if(demo||queuedEdit){settingsStatus("Сохранение недоступно в DEMO или пока идёт другая операция.",true);return;}
    const payload=new URLSearchParams();
    for(const [key,,,min,max,step] of settingSpecs){
      const input=$("setting-"+key);
      const n=Number(input.value);
      if(input.value.trim()===""||!Number.isFinite(n)||n<min||n>max||
         Math.abs(Math.round(n/step)*step-n)>0.000001){
        settingsStatus("Проверь значение: "+key,true);input.focus();return;
      }
      payload.set(key,input.value);
    }
    if(!confirm("Сохранить все параметры приготовления и PID на ESP32?"))return;
    queuedEdit=true;$("saveAllSettings").disabled=true;
    try {
      const response=await fetch("/api/settings",{
        method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded"},
        body:payload.toString(),cache:"no-store"
      });
      if(!response.ok)throw Error("HTTP "+response.status+" "+await response.text());
      settingsStatus("Команда принята. Проверяем сохранение…");
      let confirmed=false;
      for(let attempt=0;attempt<12;attempt++){
        await new Promise(resolve=>setTimeout(resolve,250));
        const actual=await readAllSettings(false);
        if([...payload].every(([key,v])=>Math.abs(Number(actual[key])-Number(v))<0.00001)){
          confirmed=true;break;
        }
      }
      if(!confirmed)throw Error("ESP32 ещё не подтвердила новые параметры");
      await readAllSettings(true);
      settingsStatus("Все 14 параметров сохранены и подтверждены ESP32.");
    }catch(error){settingsStatus("Не удалось подтвердить сохранение: "+error.message,true);}
    finally{queuedEdit=false;$("saveAllSettings").disabled=demo;}
  });
  function formatStorageBytes(value) {
    if(!Number.isFinite(value)||value<0)return "—";
    return value>=1048576?(value/1048576).toFixed(2)+" МиБ":
      (value/1024).toFixed(1)+" КиБ";
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
        throw new Error("Некорректные данные от ESP32");
      $("storageTotal").textContent=formatStorageBytes(data.totalBytes);
      $("storageUsed").textContent=formatStorageBytes(data.usedBytes);
      $("storageFree").textContent=formatStorageBytes(data.freeBytes);
      $("storageShots").textContent=data.shots+" / "+data.maxShots;
      $("storageErrors").textContent=String(data.writeErrors);
      const percent=100*data.usedBytes/data.totalBytes;
      $("storageFill").style.width=percent.toFixed(1)+"%";
      $("storageMeter").setAttribute("aria-valuenow",String(Math.round(percent)));
      $("storagePercent").textContent=percent.toFixed(1)+"% занято";
      $("storageDetails").hidden=false;
      status.textContent="LittleFS доступна";
    }catch(error) {
      $("storageDetails").hidden=true;
      status.textContent="Не удалось получить состояние LittleFS: "+error.message;
    }finally{button.disabled=false;}
  }
  $("refreshStorage").addEventListener("click",refreshStorage);
  $("rebootButton").addEventListener("click",()=>{
    if(demo){message("В DEMO перезагрузка ESP32 запрещена.",true);return;}
    if(!connected){message("Нет соединения с ESP32.",true);return;}
    if(confirm("Перезагрузить ESP32?")){ws.send("reboot");message("Команда перезагрузки отправлена.");}
  });
  $("demoToggle").addEventListener("click",()=>setDemo(!demo));
  $("freezeChart").addEventListener("click",()=>{
    if(selectedArchive!==null)return;
    frozenSamples=frozenSamples===null?chartSamples.map(p=>({...p})):null;
    updateFreezeButton();hoverPosition.detailChart=null;drawCharts();
  });
  $("clearChart").addEventListener("click",()=>{
    chartSamples.length=0;
    if(!demo)realSamples.length=0;
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
  buildSettingsFields();initChartPointers();updateArchiveButtons();navigate();publish();connect();
})();
