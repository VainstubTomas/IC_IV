'use strict';
const sensors=['heladera','freezer'],thresholds={};
const fields={heladera:{heladeraInterval:'intervalo_heladera_s',heladeraMin:'min_heladera_c',heladeraMax:'max_heladera_c'},freezer:{freezerInterval:'intervalo_freezer_s',freezerMin:'min_freezer_c',freezerMax:'max_freezer_c'},recovery:{recoveryInterval:'recuperacion_s'}};
let polling=false;
const el=id=>document.getElementById(id);
function toast(text){const node=document.createElement('div');node.className='toast info';node.textContent=text;el('toastContainer').appendChild(node);setTimeout(()=>node.remove(),6000);}
async function api(path,options={}){const r=await fetch('/api/v1'+path,{headers:{'Content-Type':'application/json'},...options});const value=await r.json();if(!r.ok)throw new Error(value.message||'No se pudo consultar el servicio');return value;}
function dateText(value){if(!value)return 'hora desconocida';const d=new Date(value);return Number.isFinite(d.getTime())?d.toLocaleString('es-AR'):'hora desconocida';}
// Solo lectura: el broker local es un espejo y el gateway no acepta comandos desde él.
// La configuración se cambia en AURA; acá se muestra la vigente que reporta la placa.
function configText(c){
 if(!c.deviceId)return 'Falta registrar el UUID de la placa.';
 if(c.reported)return 'Configuración vigente reportada por la placa ('+dateText(c.reported.updatedAt)+'). Se cambia desde AURA.';
 return 'Esperando el primer reporte de configuración de la placa.';
}
async function loadConfig(){
 const c=await api('/dispositivo/config'),p=c.reported?.params;
 for(const group of ['heladera','freezer','recovery']){
  el(group+'ConfigState').textContent=configText(c);
  for(const [id,key] of Object.entries(fields[group]))el(id).value=p?p[key]:'';
 }
 for(const sensor of sensors){thresholds[sensor]=p?{min_c:p['min_'+sensor+'_c'],max_c:p['max_'+sensor+'_c']}:null;const t=thresholds[sensor];el(sensor+'Range').textContent=t?`Rango confirmado: ${t.min_c} a ${t.max_c} °C`:'Sin rango confirmado por el nodo';}
}
function renderReading(sensor,reading,alert){
 const t=thresholds[sensor],alertTime=alert?.measuredAt||alert?.createdAt,readingTime=reading?.measuredAt||reading?.createdAt;
 const failure=alert&&alert.details?.motivo!=='recuperada'&&(!readingTime||Date.parse(alertTime)>=Date.parse(readingTime));
 if(failure){el(sensor+'Value').textContent='--';el(sensor+'Badge').textContent='Falla de sonda';el(sensor+'Time').textContent='Alerta: '+dateText(alertTime);return;}
 if(!reading||!Number.isFinite(reading.temperatura)){el(sensor+'Value').textContent='--';el(sensor+'Badge').textContent='Sin datos';el(sensor+'Time').textContent='Sin lecturas recibidas';return;}
 el(sensor+'Value').textContent=reading.temperatura.toFixed(2);el(sensor+'Time').textContent=(reading.measuredAt?'Medición: ':'Recepción (hora de medición desconocida): ')+dateText(readingTime);
 el(sensor+'Badge').textContent=t?(reading.temperatura<t.min_c?'Bajo mínimo':reading.temperatura>t.max_c?'Sobre máximo':'Dentro del rango'):'Sin rango confirmado';
}
function renderHistory(sensor,history){const target=el(sensor+'History');target.replaceChildren();for(const r of history.data||[]){const line=document.createElement('p');line.textContent=`${r.temperatura.toFixed(2)} °C · ${r.measuredAt?'medición':'recepción'} ${dateText(r.measuredAt||r.createdAt)}`;target.appendChild(line);}if(!target.childElementCount)target.textContent='Sin historial';}
function describirEstadoAura(state){
 if(!state)return {text:'API disponible; estado desconocido',online:false};
 if(!state.mqttConnected)return {text:'Backend sin conexión al broker',online:false};
 if(state.bridge?.status==='offline')return {text:'Gateway offline',online:false};
 if(state.bridge?.details?.central==='offline')return {text:'Gateway accesible; central sin confirmar',online:false};
 if(state.device?.status==='offline')return {text:'Nodo offline; última lectura guardada',online:false};
 return {text:'Ver último estado y fecha de medición',online:state.bridge?.status==='online'&&state.bridge?.details?.central==='online'&&state.device?.status==='online'};
}
function renderState(state,energy){
 const gateway=state.bridge;el('gatewayState').textContent='Gateway: '+(gateway?.status||'sin estado recibido');
 const central=gateway?.details?.central;el('centralState').textContent='Última confirmación del central: '+(central==='online'?'correcta':central==='offline'?'pendiente o fallida':'sin información');
 const power=state.device?.details?.alimentacion;el('powerState').textContent='Última alimentación reportada: '+(power==='bateria'?'batería':power==='red'?'red':'detección sin configurar o sin reporte');
 const cut=energy.events?.[0];el('cutState').textContent=cut?'Inicio del último corte: '+dateText(cut.measuredAt):'Sin cortes reportados';
 const d=describirEstadoAura(state);el('statusLed').className='led-indicator '+(d.online?'connected':'simulation');el('statusText').textContent=d.text;
}
async function poll(){if(polling)return;polling=true;try{
 const [state,energy]=await Promise.all([api('/dispositivo/status'),api('/dispositivo/energia')]);renderState(state,energy);await loadConfig();
 await Promise.all(sensors.map(async sensor=>{try{const [reading,history]=await Promise.all([api('/telemetria/latest?sensor='+sensor),api('/telemetria/history?sensor='+sensor+'&limit=5')]);renderReading(sensor,reading,state.sensorAlerts?.[sensor]);renderHistory(sensor,history);}catch(_){el(sensor+'Time').textContent='No se pudo actualizar; última lectura visible';}}));
 }catch(_){el('statusText').textContent='API sin conexión; últimos datos visibles';el('statusLed').className='led-indicator simulation';}finally{polling=false;}}
async function loadEmails() {
  const {data=[]}=await api('/alertas/emails');el('emailList').replaceChildren();el('emailCountTag').textContent=data.length+' registrados';
  for(const item of data){const li=document.createElement('li');li.className='email-list-item';const text=document.createElement('span');text.textContent=item.email;const button=document.createElement('button');button.className='btn-action danger small';button.type='button';button.textContent='Eliminar';button.onclick=async()=>{try{await api('/alertas/emails/'+encodeURIComponent(item.id),{method:'DELETE'});await loadEmails();}catch(err){toast(err.message);}};li.append(text,button);el('emailList').appendChild(li);}
}
el('emailForm').addEventListener('submit',async event=>{event.preventDefault();try{await api('/alertas/emails',{method:'POST',body:JSON.stringify({email:el('inputEmail').value.trim()})});el('inputEmail').value='';await loadEmails();}catch(err){toast(err.message);}});
loadEmails().catch(()=>{});poll();setInterval(poll,5000);
