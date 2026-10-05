import test from 'node:test';
import assert from 'node:assert/strict';
process.env.ICIV_TRANSPORT='mesh';process.env.AURA_CONFIG_EXPERIMENTAL='true';process.env.AURA_DEVICE_ID='650e8400-e29b-41d4-a716-446655440001';
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
test('API: mismo UUID, parches, recibido no aplica; aplicado confirma configuración de ambas sondas',async()=>{
 const server=app.listen(0,'127.0.0.1');await new Promise(r=>server.once('listening',r));const url='http://127.0.0.1:'+server.address().port+'/api/v1';
 const post=p=>fetch(url+'/dispositivo/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(p)});
 try{
  const patch={intervalo_heladera_s:10,min_heladera_c:1};const r=await post(patch);assert.equal(r.status,202);const command=await r.json();assert.equal(published.at(-1).id,device);assert.deepEqual(published.at(-1).payload.params,patch);
  await events.processMessage(`devices/${device}/response`,JSON.stringify({status:'recibido',details:{command_id:command.command_id,config:{...config,...patch}}}));assert.equal(thresholdMap.size,0);
  await events.processMessage(`devices/${device}/response`,JSON.stringify({status:'aplicado',details:{command_id:command.command_id,config:{...config,...patch}}}));assert.equal(thresholdMap.get(device+':heladera').min,1);assert.equal(thresholdMap.get(device+':freezer').min,-25);
  const current=await(await fetch(url+'/dispositivo/config')).json();assert.equal(current.latest.confirma_ejecucion,true);
  await events.processMessage(`devices/${device}/response`,JSON.stringify({status:'recibido',details:{command_id:command.command_id}}));assert.equal(records.at(-1).state,'aplicado');
  const f=await post({intervalo_freezer_s:600});assert.equal(f.status,202);assert.deepEqual(published.at(-1).payload.params,{intervalo_freezer_s:600});assert.equal(published.at(-1).id,device);
  const recovery=await post({recuperacion_s:900});assert.equal(recovery.status,202);assert.deepEqual(published.at(-1).payload.params,{recuperacion_s:900});
  for(const sensor of ['heladera','freezer']){const reading=await(await fetch(url+'/telemetria/latest?sensor='+sensor)).json();assert.equal(reading.deviceId,device);assert.equal(reading.temperatura,sensor==='heladera'?4:-18);assert.notEqual(reading.measuredAt,reading.createdAt);}
  for(const bad of [{intervalo_heladera_s:1},{min_heladera_c:7},{sensor:'freezer',interval_s:60},{}])assert.equal((await post(bad)).status,400);
  assert.equal(published.length,3);const energy=await(await fetch(url+'/dispositivo/energia')).json();assert.equal(energy.events[0].powerCutAt,'2026-10-04T12:00:00.000Z');
 }finally{server.closeAllConnections();await new Promise(r=>server.close(r));}
});
