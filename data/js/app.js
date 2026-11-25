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
    for(const id of ["tempInput","kpInput","kiInput","kdInput","rebootButton"])
      $(id).disabled=demo;
    for(const id of ["temperatureForm","pidForm"])
      $(id).querySelector("button[type=submit]").disabled=demo;
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
    const props={targetTemp:["targetTemperature","tempInput"],pidKp:[null,"kpInput"],pidKi:[null,"kiInput"],pidKd:[null,"kdInput"]};
    for(const key of Object.keys(props)) {
      if(message[key]===undefined) continue;
      const n=number(message[key]);if(n===null) continue;
      const [display,input]=props[key];
      if(key==="targetTemp")realTargetTemp=n;
      if(display && !demo)$(display).textContent=String(n);
      if(input && !demo && document.activeElement!==$(input) && !queuedEdit)$(input).value=String(n);
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
  }
  function message(text,error=false){$("formStatus").textContent=text;$("formStatus").style.color=error?"#ef8189":"#a0adbf";}
  async function send(path,params) {
    if(demo)throw new Error("В DEMO запрещены команды ESP32.");
    const qs=new URLSearchParams(params);
    const response=await fetch(path+"?"+qs.toString(),{cache:"no-store"});
    if(!response.ok)throw new Error("HTTP "+response.status+": "+await response.text());
    return response;
  }
  async function submitSettings(events) {
    if(demo){message("В DEMO изменение настроек ESP32 запрещено.",true);return;}
    queuedEdit=true;
    try {
      for(const [path,params] of events)await send(path,params);
      message("Изменения приняты сервером и поставлены в очередь. Обновление значений придёт через WebSocket.");
    }catch(error){message("Не удалось применить настройки: "+error.message,true);}
    finally{queuedEdit=false;}
  }
  $("temperatureForm").addEventListener("submit",event=>{
    event.preventDefault();
    const v=Number($("tempInput").value);
    if(!Number.isInteger(v)||v<20||v>105){message("Температура должна быть целым числом от 20 до 105 °C.",true);return;}
    submitSettings([["/targetTemp",{temp:String(v)}]]);
  });
  $("pidForm").addEventListener("submit",event=>{
    event.preventDefault();
    const entries=[["kpInput","/changePidKp","Kp"],["kiInput","/changePidKi","Ki"],["kdInput","/changePidKd","Kd"]];
    const actions=[];
    for(const [id,path,key] of entries){
      const value=Number($(id).value);
      if(!Number.isFinite(value)||value<0||value>100){message("PID-коэффициенты должны быть от 0 до 100.",true);return;}
      actions.push([path,{[key]:String(value)}]);
    }
    if(!confirm("Применить новые PID-коэффициенты бойлера?"))return;
    submitSettings(actions);
  });
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
  initChartPointers();updateArchiveButtons();navigate();publish();connect();
})();
