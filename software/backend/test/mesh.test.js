import test from 'node:test';
import assert from 'node:assert/strict';
import vm from 'node:vm';
import fs from 'node:fs';
import { validateMeshConfig, defaultMeshConfig, sensorDeviceId, parseMeshMetadata, appliedMeshConfig } from '../src/config/mesh-protocol.js';
const fridge='650e8400-e29b-41d4-a716-446655440001', freezer='650e8400-e29b-41d4-a716-446655440002';
test('sondas independientes, rangos y muestreo sin parametros LoRaWAN',()=>{
  assert.equal(defaultMeshConfig('heladera').interval_s,60);assert.equal(defaultMeshConfig('freezer').interval_s,300);
  assert.equal(sensorDeviceId({AURA_DEVICE_ID:fridge,AURA_FREEZER_DEVICE_ID:freezer},'freezer'),freezer);
  for(const sensor of ['heladera','freezer'])for(const interval_s of [5,60,300,86400])validateMeshConfig({...defaultMeshConfig(sensor),interval_s});
  for(const bad of [{interval_s:4},{interval_s:86401},{interval_s:5.1},{recovery_s:59},{min_c:7},{min_c:-56},{min_c:null},{max_c:126},{sensor:'otra'},{dr:2}])assert.throws(()=>validateMeshConfig({...defaultMeshConfig('heladera'),...bad}));
});
test('horas originales y corte viajan fuera de values; no inventar hora de medicion',()=>{
  const m=parseMeshMetadata(`devices/${fridge}/data`,JSON.stringify({values:{temp_c:4},ingest_id:freezer,measured_at:'2026-10-04T12:00:00Z',power_cut_at:'2026-10-04T11:59:59Z',power_first:true,on_battery:true}));
  assert.equal(m.measuredAt.toISOString(),'2026-10-04T12:00:00.000Z');assert.equal(m.powerFirst,true);
  const noClock=parseMeshMetadata(`devices/${fridge}/data`,'{"values":{},"power_first":true}');assert.equal(noClock.measuredAt,undefined);
  for(const bad of ['2026-02-30T12:00:00Z','ayer',3,'2026-10-04 12:00:00'])assert.throws(()=>parseMeshMetadata(`devices/${fridge}/data`,JSON.stringify({measured_at:bad})));
  assert.throws(()=>parseMeshMetadata(`devices/${fridge}/data`,'{"on_battery":1}'));
});
test('acuse de transporte no aplica umbrales; solo reporte explicito coherente',()=>{
  const params=defaultMeshConfig('freezer');
  assert.equal(appliedMeshConfig({status:'recibido',details:{command_id:fridge}},params),null);
  assert.equal(appliedMeshConfig({status:'transmitido',details:{iciv:{...params,applied:true}}},params),null);
  assert.deepEqual(appliedMeshConfig({status:'recibido',details:{iciv:{...params,applied:true}}},params),params);
  assert.throws(()=>appliedMeshConfig({status:'recibido',details:{iciv:{...params,interval_s:60,applied:true}}},params));
});
test('micro reporta freezer: guardar umbrales de freezer, no de heladera',async()=>{
  const source=fs.readFileSync(new URL('../src/services/device-event-service.js',import.meta.url),'utf8').replace(/^import .*;$/gm,'').replace('export default new DeviceEventService();','globalThis.service=new DeviceEventService();');
  const params=defaultMeshConfig('freezer');let saved,updated;
  const ctx=vm.createContext({config:{AURA_DEVICE_ID:fridge,AURA_FREEZER_DEVICE_ID:freezer,ICIV_TRANSPORT:'mesh',AURA_MESH_EXTENSIONS_ENABLED:true},
    console,parseAuraEvent:()=>({deviceId:freezer,status:'recibido',details:{command_id:fridge,iciv:{...params,applied:true}}}),appliedMeshConfig,previousResponseStates:()=>['pending'],
    thresholdService:{saveThresholds:async value=>saved=value},DeviceCommand:{findOne:()=>({lean:async()=>({params})}),updateOne:async()=>{},findOneAndUpdate:(filter,update)=>({lean:async()=>{updated=update.$set;return updated;}})}});
  vm.runInContext(source,ctx);await ctx.service.processMessage(`devices/${freezer}/response`,'{}');
  assert.equal(saved.deviceId,freezer);assert.equal(saved.min,-25);assert.equal(updated.confirma_ejecucion,true);
});
test('primera muestra con sonda fallida conserva el evento de corte sin temperatura ficticia',async()=>{
  const source=fs.readFileSync(new URL('../src/services/power-event-service.js',import.meta.url),'utf8').replace(/^import .*;$/gm,'').replace('export async function','async function')+'\nglobalThis.save=savePowerEvent;';
  let stored;
  const ctx=vm.createContext({config:{AURA_DEVICE_ID:fridge,AURA_MESH_EXTENSIONS_ENABLED:true},parseMeshMetadata,
    PowerEvent:{findOneAndUpdate:(filter,update)=>({lean:async()=>stored=update.$setOnInsert})}});
  vm.runInContext(source,ctx);await ctx.save(`devices/${fridge}/data`,JSON.stringify({values:{},ingest_id:freezer,power_first:true,on_battery:true,power_cut_at:'2026-10-04T12:00:00Z'}));
  assert.equal(stored.temperature,undefined);assert.equal(stored.powerCutAt.toISOString(),'2026-10-04T12:00:00.000Z');
});
