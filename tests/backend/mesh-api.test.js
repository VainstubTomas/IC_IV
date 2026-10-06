import test from 'node:test';
import assert from 'node:assert/strict';
process.env.ICIV_TRANSPORT='mesh';process.env.AURA_CONFIG_EXPERIMENTAL='true'; // aun habilitado, mesh no publica comandos
process.env.AURA_DEVICE_ID='650e8400-e29b-41d4-a716-446655440001';
const device=process.env.AURA_DEVICE_ID;
const {app}=await import('../../software/backend/src/app.js');
const {default:Command}=await import('../../software/backend/src/models/device-command-model.js');
const {default:mqtt}=await import('../../software/backend/src/config/mqtt/mqtt-config.js');
const {default:events}=await import('../../software/backend/src/services/device-event-service.js');
const {default:thresholds}=await import('../../software/backend/src/repository/threshold-repository.js');
const {default:Readings}=await import('../../software/backend/src/models/mesh-reading-model.js');
const {default:Alerts}=await import('../../software/backend/src/models/mesh-alert-model.js');
const {default:Status}=await import('../../software/backend/src/models/device-status-model.js');
const {defaultMeshConfig}=await import('../../software/backend/src/config/mesh-protocol.js');
const records=[],thresholdMap=new Map(),published=[];let config=defaultMeshConfig();
const query=value=>({sort(){return this;},limit(){return this;},lean:async()=>value});
function find(filter){return records.filter(r=>Object.entries(filter).every(([k,v])=>k==='state'&&v?.$in?v.$in.includes(r[k]):r[k]===v)).at(-1);}
Command.findOne=f=>query(find(f)||null);Command.create=async data=>{const r={...data,createdAt:new Date()};records.push(r);return r;};
Command.updateOne=async(f,u)=>{const r=find(f);if(r&&u.$set)Object.assign(r,u.$set);};Command.findOneAndUpdate=(f,u)=>{const r=find(f);if(r)Object.assign(r,u.$set);return query(r);};
mqtt.publishCommand=async(id,payload)=>published.push({id,payload});thresholds.upsert=async p=>{thresholdMap.set(p.deviceId+':'+p.sensor,p);return p;};
Readings.findOne=f=>query({deviceId:f.deviceId,sensor:f.sensor,temperature:f.sensor==='freezer'?-18:4,measuredAt:new Date('2026-10-04T12:00:00Z'),createdAt:new Date('2026-10-04T13:00:00Z')});
Alerts.find=()=>query([{measuredAt:new Date('2026-10-04T12:00:00Z'),createdAt:new Date('2026-10-04T14:00:00Z'),details:{alimentacion:'bateria'}}]);
Status.findOne=()=>query({deviceId:device,status:'online',details:{config}});
Status.findOneAndUpdate=(f,u)=>{if(u.$set.details.config)config=u.$set.details.config;return query({...f,...u.$set});};
test('API mesh: configuración solo lectura; el status espejado de AURA actualiza umbrales',async()=>{
 const server=app.listen(0,'127.0.0.1');await new Promise(r=>server.once('listening',r));const url='http://127.0.0.1:'+server.address().port+'/api/v1';
 const post=p=>fetch(url+'/dispositivo/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(p)});
 try{
  for(const patch of [{intervalo_heladera_s:10},{recuperacion_s:900},{}]){const r=await post(patch);assert.equal(r.status,409);assert.match((await r.json()).message,/AURA/);}
  assert.equal(published.length,0);assert.equal(records.length,0);
  const before=await(await fetch(url+'/dispositivo/config')).json();assert.equal(before.readOnly,true);assert.equal(before.reported.params.min_heladera_c,2);
  // Comando emitido por AURA: su response espejado no tiene registro local y no toca nada.
  await events.processMessage(`devices/${device}/response`,JSON.stringify({status:'aplicado',details:{command_id:'aura-c-1',config:{...config,min_heladera_c:1}}}));
  assert.equal(thresholdMap.size,0);assert.equal(records.length,0);
  // El reporte posterior del nodo, espejado por el gateway, sí es la configuración vigente.
  await events.processMessage(`devices/${device}/status`,JSON.stringify({status:'online',details:{evento:'reporte',transporte:'mesh',config:{...config,min_heladera_c:1}}}));
  assert.equal(thresholdMap.get(device+':heladera').min,1);assert.equal(thresholdMap.get(device+':freezer').min,-25);
  const after=await(await fetch(url+'/dispositivo/config')).json();assert.equal(after.reported.params.min_heladera_c,1);
  for(const sensor of ['heladera','freezer']){const reading=await(await fetch(url+'/telemetria/latest?sensor='+sensor)).json();assert.equal(reading.deviceId,device);assert.equal(reading.temperatura,sensor==='heladera'?4:-18);assert.notEqual(reading.measuredAt,reading.createdAt);}
  const energy=await(await fetch(url+'/dispositivo/energia')).json();assert.equal(energy.events[0].powerCutAt,'2026-10-04T12:00:00.000Z');
 }finally{server.closeAllConnections();await new Promise(r=>server.close(r));}
});
