"use strict";
(() => {
  const $ = id => document.getElementById(id);
  const chartSamples = [];
  const MAX_POINTS = 60;
  const labels = {dashboard:"Обзор",graphs:"Графики",settings:"Настройки"};
  let ws, reconnectHandle, connected=false;
  let lastInfo = {temp:null,press:null,power:null,time:0};
  let queuedEdit = false;
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
    $("temperature").textContent=fmt(lastInfo.temp);
    $("pressure").textContent=fmt(lastInfo.press);
    $("currentTime").textContent=clock(lastInfo.time);
    $("heatPower").textContent=lastInfo.power===null?"—":String(Math.round(lastInfo.power));
    $("heatBar").style.width=Math.max(0,Math.min(100,lastInfo.power||0))+"%";
  }
  function syncSettings(message) {
    const props={targetTemp:["targetTemperature","tempInput"],pidKp:[null,"kpInput"],pidKi:[null,"kiInput"],pidKd:[null,"kdInput"]};
    for(const key of Object.keys(props)) {
      if(message[key]===undefined) continue;
      const n=number(message[key]);if(n===null) continue;
      const [display,input]=props[key];
      if(display) $(display).textContent=String(n);
      if(input && document.activeElement!==$(input) && !queuedEdit)$(input).value=String(n);
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
      chartSamples.push({temp:lastInfo.temp,press:lastInfo.press});
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
  function draw(canvas) {
    const context=canvas.getContext("2d");
    if(!context)return;
    const w=canvas.clientWidth,h=canvas.clientHeight,dpr=window.devicePixelRatio||1;
    canvas.width=Math.round(w*dpr);canvas.height=Math.round(h*dpr);
    context.setTransform(dpr,0,0,dpr,0,0);context.clearRect(0,0,w,h);
    const left=37,right=w-36,top=16,bottom=h-28;
    context.strokeStyle="#394355";context.lineWidth=1;
    context.font="11px system-ui";context.fillStyle="#9aa8bd";
    for(let i=0;i<=4;i++){
      const y=top+(bottom-top)*i/4;
      context.beginPath();context.moveTo(left,y);context.lineTo(right,y);context.stroke();
      context.textAlign="right";context.fillText(String(150-i*37.5).replace(".5","٫5"),left-8,y+4);
      context.textAlign="left";context.fillText((12-i*3)+"",right+6,y+4);
    }
    context.textAlign="left";context.fillStyle="#9aa8bd";context.fillText("°C",5,12);context.fillText("бар",right+7,12);
    if(chartSamples.length<2){context.textAlign="center";context.fillText("Ожидание показаний…",(left+right)/2,(top+bottom)/2);return;}
    const drawLine=(key,color,max)=>{
      context.strokeStyle=color;context.lineWidth=2;context.lineJoin="round";context.beginPath();let started=false;
      chartSamples.forEach((v,i)=>{
        const val=v[key];if(val===null)return;
        const x=left+(right-left)*i/(MAX_POINTS-1);
        const y=bottom-(Math.max(0,Math.min(max,val))/max)*(bottom-top);
        if(!started){context.moveTo(x,y);started=true;}else context.lineTo(x,y);
      });
      context.stroke();
    };
    drawLine("temp","#e5a36e",150);drawLine("press","#69bfff",12);
  }
  function drawCharts(){for(const id of ["liveChart","detailChart"]){const canvas=$(id);if(canvas && canvas.clientWidth>0)draw(canvas);}}
  function navigate(){
    const page=(location.hash||"#dashboard").slice(1);
    const active=labels[page]?page:"dashboard";
    for(const key of Object.keys(labels)) $("page-"+key).hidden=key!==active;
    document.querySelectorAll(".nav-link").forEach(a=>{a.classList.toggle("active",a.dataset.page===active);});
    $("pageTitle").textContent=labels[active];drawCharts();
  }
  function message(text,error=false){$("formStatus").textContent=text;$("formStatus").style.color=error?"#ef8189":"#a0adbf";}
  async function send(path,params) {
    const qs=new URLSearchParams(params);
    const response=await fetch(path+"?"+qs.toString(),{cache:"no-store"});
    if(!response.ok)throw new Error("HTTP "+response.status+": "+await response.text());
    return response;
  }
  async function submitSettings(events) {
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
    if(!connected){message("Нет соединения с ESP32.",true);return;}
    if(confirm("Перезагрузить ESP32?")){ws.send("reboot");message("Команда перезагрузки отправлена.");}
  });
  $("clearChart").addEventListener("click",()=>{chartSamples.length=0;drawCharts();});
  window.addEventListener("hashchange",navigate);
  window.addEventListener("resize",drawCharts);
  navigate();publish();connect();
})();
