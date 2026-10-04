'use strict';
const sensors=['heladera','freezer'];
const configs={},thresholds={},busy={};
let polling=false;
const el=id=>document.getElementById(id);
function toast(text) {const node=document.createElement('div');node.className='toast info';node.textContent=text;el('toastContainer').appendChild(node);setTimeout(()=>node.remove(),6000);}
async function api(path,options={}) {
  const response=await fetch('/api/v1'+path,{headers:{'Content-Type':'application/json'},...options});
  const value=await response.json();if(!response.ok)throw new Error(value.message||'No se pudo consultar el servicio');return value;
}
function dateText(value) {if(!value)return 'hora desconocida';const d=new Date(value);return Number.isFinite(d.getTime())?d.toLocaleString('es-AR'):'hora desconocida';}
function configText(c) {
  if(!c.deviceId)return 'Falta registrar el UUID de esta sonda.';
  const latest=c.latest;
  if(!latest && c.reported)return 'El micro reportó su configuración vigente.';
  if(latest?.confirma_ejecucion)return 'El nodo confirmó estos ajustes y los guardó.';
  if(latest?.state==='rechazado')return 'Rechazado: '+(latest.responseDetails?.motivo||'consultar el diagnóstico');
  if(latest?.state==='publish_failed')return 'No se pudo publicar: '+(latest.error||'sin conexión');
  if(latest)return 'Solicitud '+latest.state+'. Aún sin confirmación de aplicación.';
  return c.experimental?'Valores iniciales; el nodo todavía no reportó ajustes aplicados.':'Adaptación mesh pendiente de habilitar con el gateway y AURA.';
}
async function loadConfig(sensor,fill=false) {
  const c=await api('/dispositivo/config?sensor='+sensor);configs[sensor]=c;
  el(sensor+'Save').disabled=!c.experimental||!c.deviceId||!!busy[sensor]||c.transport!=='mesh';
  el(sensor+'ConfigState').textContent=configText(c);
  if(c.reported)thresholds[sensor]=c.reported.params;
  else if(c.latest?.confirma_ejecucion)thresholds[sensor]=c.latest.params;
  if(fill) {
    const p=c.reported?.params||(c.latest?.confirma_ejecucion?c.latest.params:c.defaults);
    if(p){el(sensor+'Interval').value=p.interval_s;el(sensor+'Min').value=p.min_c;el(sensor+'Max').value=p.max_c;el('recoveryInterval').value=p.recovery_s;}
  }
  const t=thresholds[sensor];el(sensor+'Range').textContent=t?`Rango confirmado: ${t.min_c} a ${t.max_c} °C`:'Sin rango confirmado por el nodo';
}
async function saveConfig(sensor,event) {
  event.preventDefault();
  const params={sensor,interval_s:Number(el(sensor+'Interval').value),min_c:Number(el(sensor+'Min').value),max_c:Number(el(sensor+'Max').value),recovery_s:Number(el('recoveryInterval').value)};
  if(!Number.isInteger(params.interval_s)||params.interval_s<5||params.interval_s>86400||!Number.isInteger(params.recovery_s)||params.recovery_s<60||params.recovery_s>86400||!Number.isFinite(params.min_c)||!Number.isFinite(params.max_c)||params.min_c<-55||params.max_c>125||params.min_c>=params.max_c){toast('Revisá intervalos y rango mínimo/máximo.');return;}
  busy[sensor]=true;el(sensor+'Save').disabled=true;
  try{await api('/dispositivo/config',{method:'POST',body:JSON.stringify(params)});toast('Solicitud enviada. Esperando que el nodo la aplique.');}
  catch(err){toast(err.message);}
  finally{busy[sensor]=false;await loadConfig(sensor).catch(()=>{});}
}
function renderReading(sensor,reading,status) {
  const t=thresholds[sensor];
  const statusEpoch=status?.details?.iciv?.sample_epoch;
  const readingEpoch=reading?.measuredAt?Date.parse(reading.measuredAt)/1000:0;
  if(status?.details?.iciv?.sensor_valid===false && (!readingEpoch || !statusEpoch || statusEpoch>=readingEpoch)){el(sensor+'Value').textContent='--';el(sensor+'Badge').textContent='Falla de sonda';el(sensor+'Time').textContent='Sin medición válida según el último estado recibido';return;}
  if(!reading||!Number.isFinite(reading.temperatura)){el(sensor+'Value').textContent='--';el(sensor+'Badge').textContent='Sin datos';el(sensor+'Time').textContent='Sin lecturas recibidas';return;}
  el(sensor+'Value').textContent=reading.temperatura.toFixed(2);
  el(sensor+'Time').textContent=(reading.measuredAt?'Medición: '+dateText(reading.measuredAt):'Recepción: '+dateText(reading.createdAt))+(reading.onBattery===true?' · batería':'');
  el(sensor+'Badge').textContent=t?(reading.temperatura<t.min_c?'Bajo mínimo':reading.temperatura>t.max_c?'Sobre máximo':'Dentro del rango'):'Sin rango confirmado';
}
function renderHistory(sensor,history) {
  const target=el(sensor+'History');target.replaceChildren();
  for(const r of history.data||[]){const line=document.createElement('p');line.textContent=`${r.temperatura.toFixed(2)} °C · ${dateText(r.measuredAt||r.createdAt)}${r.powerFirst?' · Primera del corte':''}`;target.appendChild(line);}
  if(!target.childElementCount)target.textContent='Sin historial';
}
function describirEstadoAura(state) {
  if(!state)return {text:'API disponible; estado desconocido',online:false};
  if(!state.mqttConnected)return {text:'Backend sin conexión al broker',online:false};
  if(state.bridge?.status==='offline')return {text:'Gateway offline',online:false};
  if(state.bridge?.details?.iciv?.central==='offline')return {text:'Gateway accesible; central sin confirmar',online:false};
  if(state.sensors?.heladera?.status==='offline'||state.sensors?.freezer?.status==='offline')return {text:'Sonda offline; última lectura guardada',online:false};
  return {text:'Ver último estado y fecha de medición',online:state.bridge?.status==='online'&&state.bridge?.details?.iciv?.central==='online'};
}
function renderState(state,energy) {
  const gateway=state.bridge;
  el('gatewayState').textContent='Gateway: '+(gateway?.status||'sin estado recibido');
  const central=gateway?.details?.iciv?.central;
  el('centralState').textContent='Última confirmación del central: '+(central==='online'?'correcta':central==='offline'?'pendiente o fallida':'sin información');
  const battery=state.sensors?.heladera?.details?.iciv?.on_battery??state.sensors?.freezer?.details?.iciv?.on_battery;
  el('powerState').textContent='Última alimentación reportada: '+(battery===true?'batería':battery===false?'red':'detección sin configurar o sin reporte');
  const cut=energy.events?.[0];el('cutState').textContent=cut?'Primer dato del corte: '+dateText(cut.powerCutAt||cut.measuredAt):'Sin cortes reportados';
  const described=describirEstadoAura(state);
  el('statusLed').className='led-indicator '+(described.online?'connected':'simulation');
  el('statusText').textContent=described.text;
}
async function poll() {
  if(polling)return;polling=true;
  try {
    const [state,energy]=await Promise.all([api('/dispositivo/status'),api('/dispositivo/energia')]);renderState(state,energy);
    await Promise.all(sensors.map(async sensor=>{
      try{const [reading,history]=await Promise.all([api('/telemetria/latest?sensor='+sensor),api('/telemetria/history?sensor='+sensor+'&limit=5')]);await loadConfig(sensor);renderReading(sensor,reading,state.sensors?.[sensor]);renderHistory(sensor,history);}
      catch(_){el(sensor+'Time').textContent='No se pudo actualizar; última lectura visible';}
    }));
  }catch(_){el('statusText').textContent='API sin conexión; últimos datos visibles';el('statusLed').className='led-indicator simulation';}
  finally{polling=false;}
}
async function loadEmails() {
  const {data=[]}=await api('/alertas/emails');el('emailList').replaceChildren();el('emailCountTag').textContent=data.length+' registrados';
  for(const item of data){const li=document.createElement('li');li.className='email-list-item';const text=document.createElement('span');text.textContent=item.email;const button=document.createElement('button');button.className='btn-action danger small';button.type='button';button.textContent='Eliminar';button.onclick=async()=>{try{await api('/alertas/emails/'+encodeURIComponent(item.id),{method:'DELETE'});await loadEmails();}catch(err){toast(err.message);}};li.append(text,button);el('emailList').appendChild(li);}
}
el('emailForm').addEventListener('submit',async event=>{event.preventDefault();try{await api('/alertas/emails',{method:'POST',body:JSON.stringify({email:el('inputEmail').value.trim()})});el('inputEmail').value='';await loadEmails();}catch(err){toast(err.message);}});
for(const sensor of sensors){el(sensor+'Form').addEventListener('submit',event=>saveConfig(sensor,event));loadConfig(sensor,true).catch(()=>{});}
loadEmails().catch(()=>{});poll();setInterval(poll,5000);
