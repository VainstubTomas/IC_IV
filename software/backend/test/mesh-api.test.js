import test from 'node:test';
import assert from 'node:assert/strict';
// API real de Express, modelos y transporte aislados: no necesita MongoDB ni AURA.
process.env.ICIV_TRANSPORT='mesh';
process.env.AURA_MESH_EXTENSIONS_ENABLED='true';
process.env.AURA_CONFIG_EXPERIMENTAL='true';
process.env.AURA_FRIDGE_DEVICE_ID='650e8400-e29b-41d4-a716-446655440001';
process.env.AURA_FREEZER_DEVICE_ID='650e8400-e29b-41d4-a716-446655440002';
const {app}=await import('../src/app.js');
const {default:Command}=await import('../src/models/device-command-model.js');
const {default:mqtt}=await import('../src/config/mqtt/mqtt-config.js');
const {default:events}=await import('../src/services/device-event-service.js');
const {default:thresholds}=await import('../src/repository/threshold-repository.js');
const {default:Readings}=await import('../src/models/sensor-data-model.js');
const {default:Power}=await import('../src/models/power-event-model.js');
const {default:Status}=await import('../src/models/device-status-model.js');
const records=[],thresholdMap=new Map(),published=[];
function find(filter) {return records.filter(r=>Object.entries(filter).every(([k,v])=>k==='state'&&v?.$in?v.$in.includes(r[k]):r[k]===v)).at(-1);}
const query=value=>({sort(){return this;},limit(){return this;},lean:async()=>value});
Command.findOne=filter=>query(find(filter)||null);
Command.create=async data=>{const r={...data,createdAt:new Date()};records.push(r);return r;};
Command.updateOne=async(filter,update)=>{const r=find(filter);if(r&&update.$set)Object.assign(r,update.$set);};
Command.findOneAndUpdate=(filter,update)=>{const r=find(filter);if(r)Object.assign(r,update.$set);return query(r);};
mqtt.publishCommand=async(id,payload)=>{published.push({id,payload});};
thresholds.upsert=async p=>{thresholdMap.set(p.deviceId,p);return p;};
Readings.findOne=filter=>query({deviceId:filter.deviceId,temperature:filter.deviceId.endsWith('2')?-18:4,measuredAt:new Date('2026-10-04T12:00:00Z'),createdAt:new Date('2026-10-04T13:00:00Z')});
Power.find=()=>query([]);
Status.findOne=()=>query(null);
test('API envia ajustes a cada UUID y solo aplica umbrales despues del reporte del micro',async()=>{
  const server=app.listen(0,'127.0.0.1');await new Promise(resolve=>server.once('listening',resolve));
  const url='http://127.0.0.1:'+server.address().port+'/api/v1';
  try {
    for(const sensor of ['heladera','freezer']){
      const params={sensor,interval_s:sensor==='heladera'?60:300,min_c:sensor==='heladera'?2:-25,max_c:sensor==='heladera'?6:-15,recovery_s:300};
      const response=await fetch(url+'/dispositivo/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(params)});
      assert.equal(response.status,202);const command=await response.json();const publication=published.at(-1);
      assert.ok(publication.id.endsWith(sensor==='heladera'?'1':'2'));assert.equal(thresholdMap.has(publication.id),false);
      await events.processMessage(`devices/${publication.id}/response`,JSON.stringify({status:'recibido',details:{command_id:command.command_id,iciv:{...params,applied:true}}}));
      assert.equal(thresholdMap.get(publication.id).min,params.min_c);
      const current=await(await fetch(url+'/dispositivo/config?sensor='+sensor)).json();assert.equal(current.latest.confirma_ejecucion,true);
      const reading=await(await fetch(url+'/telemetria/latest?sensor='+sensor)).json();assert.equal(reading.temperatura,sensor==='heladera'?4:-18);assert.notEqual(reading.measuredAt,reading.createdAt);
    }
    const bad=await fetch(url+'/dispositivo/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({sensor:'heladera',interval_s:1,min_c:2,max_c:6,recovery_s:300})});assert.equal(bad.status,400);assert.equal(published.length,2);
    const energy=await(await fetch(url+'/dispositivo/energia')).json();assert.deepEqual(energy.events,[]);
  }finally{server.closeAllConnections();await new Promise(resolve=>server.close(resolve));}
});
