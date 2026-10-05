'use strict';
const sensors=['heladera','freezer'], configs={},thresholds={},busy={},dirty=new Set(),pending={};
const fields={heladera:{heladeraInterval:'intervalo_heladera_s',heladeraMin:'min_heladera_c',heladeraMax:'max_heladera_c'},freezer:{freezerInterval:'intervalo_freezer_s',freezerMin:'min_freezer_c',freezerMax:'max_freezer_c'},recovery:{recoveryInterval:'recuperacion_s'}};
let polling=false;
const el=id=>document.getElementById(id);
function toast(text){const node=document.createElement('div');node.className='toast info';node.textContent=text;el('toastContainer').appendChild(node);setTimeout(()=>node.remove(),6000);}
async function api(path,options={}){const r=await fetch('/api/v1'+path,{headers:{'Content-Type':'application/json'},...options});const value=await r.json();if(!r.ok)throw new Error(value.message||'No se pudo consultar el servicio');return value;}
function dateText(value){if(!value)return 'hora desconocida';const d=new Date(value);return Number.isFinite(d.getTime())?d.toLocaleString('es-AR'):'hora desconocida';}
function configText(c){
 if(!c.deviceId)return 'Falta registrar el UUID de la placa.';
 if(c.latest?.confirma_ejecucion)return 'El nodo confirmó que aplicó y guardó la última solicitud.';
 if(c.latest?.state==='rechazado')return 'Rechazado: '+(c.latest.responseDetails?.motivo||'consultar diagnóstico');
 if(c.latest?.state==='publish_failed')return 'No se pudo publicar: '+(c.latest.error||'sin conexión');
 if(c.latest)return 'Última solicitud: '+c.latest.state+'. Aún sin confirmación de aplicación.';
 if(c.reported)return 'Configuración vigente reportada por la placa.';
 return 'Esperando el primer reporte de configuración de la placa.';
}
async function loadConfig(){
 const c=await api('/dispositivo/config'),p=c.reported?.params;
 for(const group of ['heladera','freezer','recovery']){
  configs[group]=c;el(group+'Save').disabled=!c.experimental||!c.deviceId||!p||!!busy[group]||c.transport!=='mesh';
  el(group+'ConfigState').textContent=configText(c);
  for(const [id,key] of Object.entries(fields[group])){
   // Solo limpiar una edición cuando llega aplicado de SU solicitud, o cuando
   // un reporte posterior confirma exactamente ese parámetro pendiente.
   if(pending[id] && ((c.latest?.command_id===pending[id].command_id && c.latest.confirma_ejecucion)||
       (p && p[key]===pending[id].value && Date.parse(c.reported.updatedAt)>=pending[id].sentAt))){dirty.delete(id);delete pending[id];}
   if(p&&!dirty.has(id))el(id).value=p[key];
  }
 }
 for(const sensor of sensors){thresholds[sensor]=p?{min_c:p['min_'+sensor+'_c'],max_c:p['max_'+sensor+'_c']}:null;const t=thresholds[sensor];el(sensor+'Range').textContent=t?`Rango confirmado: ${t.min_c} a ${t.max_c} °C`:'Sin rango confirmado por el nodo';}
}
async function saveConfig(group,event){
 event.preventDefault();const c=configs[group],current=c?.reported?.params,params={};if(!current)return toast('Esperá el reporte vigente del nodo.');
 for(const [id,key] of Object.entries(fields[group])){const n=Number(el(id).value);if(n!==current[key])params[key]=n;}
 if(!Object.keys(params).length)return toast('No hay cambios para enviar.');
 const merged={...current,...params};
 for(const sensor of sensors)if(!Number.isInteger(merged['intervalo_'+sensor+'_s'])||merged['intervalo_'+sensor+'_s']<5||merged['intervalo_'+sensor+'_s']>86400||!Number.isFinite(merged['min_'+sensor+'_c'])||!Number.isFinite(merged['max_'+sensor+'_c'])||merged['min_'+sensor+'_c']<-55||merged['max_'+sensor+'_c']>125||merged['min_'+sensor+'_c']>=merged['max_'+sensor+'_c'])return toast('Revisá intervalos y rango mínimo/máximo.');
 if(!Number.isInteger(merged.recuperacion_s)||merged.recuperacion_s<60||merged.recuperacion_s>86400)return toast('La recuperación debe estar entre 60 y 86400 segundos.');
 busy[group]=true;el(group+'Save').disabled=true;
 try{const sentAt=Date.now(),result=await api('/dispositivo/config',{method:'POST',body:JSON.stringify(params)});for(const [id,key] of Object.entries(fields[group]))if(key in params){dirty.add(id);pending[id]={value:params[key],command_id:result.command_id,sentAt};}toast('Solicitud enviada. Esperando aplicado del nodo.');}
 catch(e){toast(e.message);}finally{busy[group]=false;await loadConfig().catch(()=>{});}
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
for(const group of ['heladera','freezer','recovery']){el(group+'Form').addEventListener('submit',event=>saveConfig(group,event));for(const id of Object.keys(fields[group]))el(id).addEventListener('input',()=>{dirty.add(id);delete pending[id];});}
loadEmails().catch(()=>{});poll();setInterval(poll,5000);
