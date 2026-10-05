import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';
import {randomUUID,createHash} from 'node:crypto';
import {validateMeshPatch,validateMeshConfig,defaultMeshConfig,parseMeshData,parseMeshAlert,sensorDeviceId} from '../../software/backend/src/config/mesh-protocol.js';
const device='650e8400-e29b-41d4-a716-446655440001',ingest='5f0c1b1e-8a6d-4a55-9f2b-7c3e2d1a0b99';
const data=p=>parseMeshData(`devices/${device}/data`,JSON.stringify(p));
test('una placa, dos campos; no metadata en values ni sentinelas',()=>{
 const readings=data({values:{temp_heladera_c:4,temp_freezer_c:-18},ingest_id:ingest,ts:'2026-10-04T12:00:00Z'});
 assert.equal(readings.length,2);assert.ok(readings.every(r=>r.deviceId===device&&r.ingest_id===ingest));assert.deepEqual(readings.map(r=>r.sensor),['heladera','freezer']);
 for(const s of ['heladera','freezer'])assert.equal(sensorDeviceId({AURA_DEVICE_ID:device},s),device);
 assert.deepEqual(data({values:{}}),[]);assert.equal(data({values:{temp_heladera_c:3}}).length,1);
 for(const t of [null,'4',85,-127,126])assert.throws(()=>data({values:{temp_heladera_c:t}}));
 assert.throws(()=>data({values:{temp_heladera_c:4,rssi:-80}}));
});
test('ts UTC original; sin RTC no se inventa measuredAt',()=>{
 assert.equal(data({values:{temp_heladera_c:4},ts:'2026-10-04T12:00:00Z'})[0].measuredAt.toISOString(),'2026-10-04T12:00:00.000Z');
 assert.equal(data({values:{temp_heladera_c:4}})[0].measuredAt,undefined);
 for(const ts of ['2026-02-30T12:00:00Z','2026-10-04T12:00:00-03:00',null,123])assert.throws(()=>data({values:{temp_heladera_c:4},ts}));
});
test('parches independientes y recuperación común validada sin sobrescribir otra sonda',()=>{
 const c=defaultMeshConfig();assert.deepEqual(validateMeshPatch({intervalo_heladera_s:5},c),{intervalo_heladera_s:5});
 assert.deepEqual(validateMeshPatch({recuperacion_s:600},c),{recuperacion_s:600});assert.equal(c.intervalo_freezer_s,300);
 for(const patch of [{},{sensor:'freezer'},{intervalo_heladera_s:4},{recuperacion_s:59},{min_heladera_c:7},{min_freezer_c:-100},{max_freezer_c:-30},{min_heladera_c:2.001},{intervalo_freezer_s:true}])assert.throws(()=>validateMeshPatch(patch,c));
 assert.throws(()=>validateMeshConfig({recuperacion_s:300}));
});
test('falla, recuperación y corte se informan en alerts, sin temperatura ficticia',()=>{
 for(const motivo of ['sin_respuesta','fuera_de_rango','recuperada'])assert.equal(parseMeshAlert(`alerts/${device}/sensor`,JSON.stringify({severity:'warning',details:{campo:'temp_freezer_c',motivo}})).details.motivo,motivo);
 const cut=parseMeshAlert(`alerts/${device}/energia`,JSON.stringify({severity:'warning',ts:'2026-10-04T12:00:00Z',details:{alimentacion:'bateria'}}));assert.equal(cut.measuredAt.toISOString(),'2026-10-04T12:00:00.000Z');
 assert.equal(parseMeshAlert(`alerts/${device}/energia`,'{"details":{"alimentacion":"red"}}').measuredAt,undefined);
 assert.throws(()=>parseMeshAlert(`alerts/${device}/sensor`,'{"details":{"campo":"temp_c","motivo":"error"}}'));
});
test('dedup local por ingest_id + sonda, conserva ts y propaga errores de base',async()=>{
 const source=fs.readFileSync(new URL('../../software/backend/src/services/mesh-data-service.js',import.meta.url),'utf8').replace(/^import .*;$/gm,'').replace('export default new MeshDataService();','globalThis.service = new MeshDataService();');
 const records=new Map();let alerts=0,fail=false;
 const Model={create:async r=>{if(fail)throw new Error('DB offline');const key=r.ingest_id+':'+r.sensor;if(records.has(key))throw {code:11000};const record={...r,createdAt:new Date()};records.set(key,record);return record;},findOne:r=>({lean:async()=>records.get(r.ingest_id+':'+r.sensor)})};
 const ctx=vm.createContext({config:{AURA_DEVICE_ID:device},MeshReading:Model,parseMeshData,randomUUID,console,alertService:{checkThresholdAndNotify:async()=>alerts++}});vm.runInContext(source,ctx);
 const topic=`devices/${device}/data`,payload=JSON.stringify({values:{temp_heladera_c:4,temp_freezer_c:-18},ingest_id:ingest,ts:'2026-10-04T12:00:00Z'});
 await ctx.service.parseAndSaveMqttMessage(topic,payload);const dup=await ctx.service.parseAndSaveMqttMessage(topic,payload);assert.equal(records.size,2);assert.equal(alerts,2);assert.ok(dup.every(r=>r.duplicate));assert.equal(dup[0].orderAt.toISOString(),'2026-10-04T12:00:00.000Z');
 fail=true;await assert.rejects(ctx.service.parseAndSaveMqttMessage(topic,payload),/DB offline/);
});
test('dashboard muestra la configuración vigente en solo lectura; sin reporte no inventa valores',async()=>{
 const source=fs.readFileSync(new URL('../../software/frontend/dashboard.js',import.meta.url),'utf8');
 assert.ok(!source.includes("method:'POST',body:JSON.stringify(params)"),'el dashboard no publica configuración');
 const elements={},get=id=>elements[id]??=( {value:'',textContent:'',disabled:false} );
 const c={deviceId:device,transport:'mesh',readOnly:true,reported:{params:defaultMeshConfig(),updatedAt:new Date().toISOString()}};
 const cut=source.slice(0,source.indexOf('function renderReading'));
 const ctx=vm.createContext({document:{getElementById:get},fetch:async()=>({ok:true,json:async()=>c}),Date});vm.runInContext(cut,ctx);
 await vm.runInContext('loadConfig()',ctx);assert.equal(get('freezerInterval').value,300);assert.equal(get('recoveryInterval').value,300);assert.match(get('heladeraConfigState').textContent,/AURA/);
 c.reported.params={...c.reported.params,intervalo_heladera_s:10};await vm.runInContext('loadConfig()',ctx);assert.equal(get('heladeraInterval').value,10);
 c.reported=null;await vm.runInContext('loadConfig()',ctx);assert.equal(get('heladeraInterval').value,'');assert.equal(get('heladeraRange').textContent,'Sin rango confirmado por el nodo');
});
test('alerta sin RTC ordena por recepción sin inventar hora del corte ni dedupKey',async()=>{
 const source=fs.readFileSync(new URL('../../software/backend/src/services/mesh-alert-service.js',import.meta.url),'utf8').replace(/^import .*;$/gm,'').replaceAll('export async function','async function')+'\nglobalThis.save=saveMeshAlert;';
 const stored=[],ctx=vm.createContext({parseMeshAlert,createHash,config:{AURA_DEVICE_ID:device},MeshAlert:{create:async e=>{stored.push(e);return e;}},console});vm.runInContext(source,ctx);
 await ctx.save(`alerts/${device}/energia`,'{"details":{"alimentacion":"bateria"}}');assert.equal(stored[0].measuredAt,undefined);assert.equal(stored[0].dedupKey,undefined);assert.ok(Number.isFinite(stored[0].orderAt.getTime()));
 await ctx.save(`alerts/${device}/energia`,'{"details":{"alimentacion":"bateria"},"ts":"2026-10-04T12:00:00Z"}');assert.equal(stored[1].orderAt.toISOString(),'2026-10-04T12:00:00.000Z');assert.ok(stored[1].dedupKey);
});
