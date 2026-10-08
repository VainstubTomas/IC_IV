import test from 'node:test';
import assert from 'node:assert/strict';
import vm from 'node:vm';
import fs from 'node:fs';
import { parseAuraData, validateConfig, parseObject, parseAuraEvent, previousResponseStates } from '../../software/backend/src/config/mqtt/aura-protocol.js';
import { commandTopic } from '../../software/backend/src/config/mqtt/mqtt-topics.js';
import { validateSensor, sensorDeviceId } from '../../software/backend/src/config/mesh-protocol.js';
const codec = vm.createContext({});
vm.runInContext(fs.readFileSync(new URL('../../../configuration/codec-heladera.js', import.meta.url), 'utf8'), codec);
const normalize = value => JSON.parse(JSON.stringify(value));
const device = '650e8400-e29b-41d4-a716-446655440001';
const good = { interval_s: 300, confirmed: false, adr: false, dr: 2, offset_c: -0.25 };
test('temperatura positiva, negativa, centinela y payload mal formado', () => {
  assert.deepEqual(normalize(codec.decodeUplink({fPort:1, bytes:[0,12,1,169]})), {data:{temp_c:4.25}});
  assert.deepEqual(normalize(codec.decodeUplink({fPort:1, bytes:[0,12,255,131]})), {data:{temp_c:-1.25}});
  assert.deepEqual(normalize(codec.decodeUplink({fPort:1, bytes:[0,12,127,255]})).data, {});
  assert.ok(codec.decodeUplink({fPort:1, bytes:[0,1]}).errors);
  assert.ok(codec.decodeUplink({fPort:11, bytes:[1,1,44,0,2,255,231]}).errors);
});
test('configuracion exacta, extremos, roundtrip y acuerdo de validadores', () => {
  assert.deepEqual(normalize(codec.encodeDownlink({data:{command:'set_config',params:good}})),
    {fPort:10,bytes:[1,1,44,0,2,255,231]});
  for (const interval_s of [20,300,3600]) for (const offset_c of [-5,0,5]) for (const adr of [false,true]) {
    const params = {...good,interval_s,offset_c,adr};
    validateConfig(params);
    const encoded=codec.encodeDownlink({data:{command:'set_config',params}});
    assert.deepEqual(normalize(codec.decodeConfigReport(encoded.bytes)),params);
  }
  for (const changed of [{interval_s:0},{interval_s:3601},{interval_s:20.5},{dr:0},{dr:6},
    {offset_c:5.01},{offset_c:NaN},{offset_c:0.001},{confirmed:1},{adr:'false'},{extra:1}]) {
    const params={...good,...changed};
    assert.throws(()=>validateConfig(params));
    assert.ok(codec.encodeDownlink({data:{command:'set_config',params}}).errors);
  }
  assert.ok(codec.encodeDownlink({data:{command:'restart',params:good}}).errors);
  assert.throws(()=>codec.decodeConfigReport([1,1,44,4,2,0,0]));
});
test('AURA usa UUID del topico y solo mediciones validas',()=>{
  assert.deepEqual(parseAuraData(`devices/${device}/data`,JSON.stringify({deviceId:'falso',values:{temp_c:4}})),
    {deviceId:device,temperature:4,source:'mqtt'});
  for (const temp_c of [null,'4',true,{},1e99]) assert.equal(parseAuraData(`devices/${device}/data`,JSON.stringify({values:{temp_c}})),null);
  assert.equal(parseAuraData(`devices/${device}/data`,'{"values":{}}'),null);
  assert.equal(parseAuraData('devices/Heladera1/data','{"values":{"temp_c":4}}'),null);
  assert.equal(parseAuraData('application/a/device/b/event/up','{}'),null);
  assert.equal(parseAuraData('iciv/test','4'),null);
  assert.equal(parseAuraData(`devices/${device}/status`,'{}'),null);
  assert.equal(commandTopic(device),`devices/${device}/command`);
  assert.throws(()=>commandTopic('Heladera1'));
});
test('MQTT persistente QoS 1 confirma solo tras persistir y reintenta si DB falla',async()=>{
  const source=fs.readFileSync(new URL('../../software/backend/src/config/mqtt/mqtt-config.js',import.meta.url),'utf8')
    .replace(/^import .*;$/gm,'').replace('export default','globalThis.api =');
  const events={}, messages=[], subscriptions=[], retries=[];
  let persistCount=0,fail=false,ackCount=0;
  const client={connected:true,on:(event,fn)=>events[event]=fn,
    subscribe:(topics,options,cb)=>{subscriptions.push({topics,options});cb(null,topics.map(topic=>({topic,qos:1})));},
    publish:(topic,payload,options,cb)=>{messages.push({topic,payload:JSON.parse(payload),options});cb(null);}};
  const context=vm.createContext({mqtt:{connect:(url,options)=>{
      assert.equal(url,'mqtt://aura');assert.equal(options.clean,false);assert.equal(options.clientId,'iciv-test-fixed');return client;}},
    fs:{existsSync:()=>false},config:{MQTTBROKERURL:'mqtt://aura',MQTT_CLIENT_ID:'iciv-test-fixed'},
    TOPICS:{DATA:'devices/+/data',STATUS:'devices/+/status',RESPONSE:'devices/+/response'},
    commandTopic,setTimeout:fn=>retries.push(fn),console:{log(){},error(){}}});
  vm.runInContext(source,context);
  context.api.init({emit(){}},async()=>{persistCount++;if(fail)throw new Error('DB offline');});events.connect();
  assert.deepEqual(normalize(subscriptions[0]),{topics:['devices/+/data','devices/+/status','devices/+/response','alerts/+/+'],options:{qos:1}});
  const packet={topic:`devices/${device}/data`,payload:Buffer.from('{"values":{"temp_c":4}}')};
  await new Promise(resolve=>client.handleMessage(packet,()=>{ackCount++;resolve();}));
  assert.equal(ackCount,1);assert.equal(persistCount,1);
  fail=true;client.handleMessage(packet,()=>ackCount++);
  await new Promise(resolve=>setImmediate(resolve));
  assert.equal(ackCount,1);assert.equal(retries.length,1);
  fail=false;await retries[0]();assert.equal(ackCount,2);
  const msg={command:'set_config',params:good,command_id:'command-test'};
  await context.api.publishCommand(device,msg);
  assert.deepEqual(normalize(messages[0]),{topic:`devices/${device}/command`,payload:msg,options:{qos:1,retain:false}});
  client.connected=false;
  assert.equal(context.api.isConnected(),false);
  await assert.rejects(context.api.publishCommand(device,msg),/desconectado/);
});

test('servicio no inventa lectura inicial, filtra dispositivo y no guarda sensor ausente',async()=>{
  const source=fs.readFileSync(new URL('../../software/backend/src/services/sensor-data-service.js',import.meta.url),'utf8')
    .replace(/^import .*;$/gm,'').replace('export default new SensorDataService();','globalThis.service = new SensorDataService();');
  const created=[],queried=[];
  const context=vm.createContext({config:{AURA_DEVICE_ID:device},parseAuraData,
    isDeviceId:id=>id===device,alertService:{checkThresholdAndNotify:async()=>{}},console,
    sensorDataRepository:{getLatest:async id=>{queried.push(id);return null;},
      getHistory:async()=>[],create:async reading=>{created.push(reading);return {record:{...reading,_id:'record',createdAt:new Date()},inserted:true};}}});
  vm.runInContext(source,context);
  assert.equal(await context.service.getLatestTelemetry(),null);
  assert.deepEqual(queried,[device]);
  assert.equal(await context.service.parseAndSaveMqttMessage(`devices/${device}/data`,'{"values":{}}'),null);
  assert.equal(created.length,0);
  const saved=await context.service.parseAndSaveMqttMessage(`devices/${device}/data`,'{"values":{"temp_c":-2.5}}');
  assert.equal(saved.temperatura,-2.5);
  assert.equal(saved.rssi,null);
  assert.equal(created.length,1);
  await assert.rejects(context.service.saveTelemetry({deviceId:device,temperature:Infinity}));
});

test('contrato v3 acepta status/response y rechaza estados legacy y payloads no objeto',()=>{
  assert.deepEqual(parseAuraEvent(`devices/${device}/status`,'{"status":"online","details":{"rssi":-87,"snr":6.5}}','status'),
    {deviceId:device,status:'online',details:{rssi:-87,snr:6.5}});
  for(const status of ['encolado','transmitido','recibido','aplicado','rechazado'])
    assert.equal(parseAuraEvent(`devices/${device}/response`,JSON.stringify({status,details:{command_id:'c-2026-0001'}}),'response').status,status);
  assert.throws(()=>parseAuraEvent(`devices/${device}/response`,'{"status":"enviado_a_mesh"}','response'));
  assert.throws(()=>parseAuraEvent(`devices/${device}/status`,'{"status":"online","details":[]}','status'));
  for(const json of ['null','[]','4','"string"']) assert.throws(()=>parseObject(json));
  assert.equal(previousResponseStates('encolado').includes('recibido'),false);
  assert.equal(previousResponseStates('transmitido').includes('rechazado'),false);
});
test('ingest_id opcional se conserva; IDs invalidos no se aceptan',()=>{
  const ingest='5f0c1b1e-8a6d-4a55-9f2b-7c3e2d1a0b99';
  assert.equal(parseAuraData(`devices/${device}/data`,JSON.stringify({values:{temp_c:4},ingest_id:ingest})).ingest_id,ingest);
  assert.throws(()=>parseAuraData(`devices/${device}/data`,'{"values":{"temp_c":4},"ingest_id":"wrong"}'));
});
test('duplicado de Mongo recupera registro existente; errores reales se propagan',async()=>{
  const source=fs.readFileSync(new URL('../../software/backend/src/repository/sensor-data-repository.js',import.meta.url),'utf8')
    .replace(/^import .*;$/gm,'').replace('export default new SensorDataRepository();','globalThis.repo = new SensorDataRepository();');
  let error=null,stored=null;
  class Model {
    constructor(data){this.data=data;}
    async save(){if(error)throw error;stored=this.data;return stored;}
    static findOne(){return {lean:async()=>stored};}
  }
  const ctx=vm.createContext({SensorData:Model});vm.runInContext(source,ctx);
  const reading={deviceId:device,temperature:4,ingest_id:'5f0c1b1e-8a6d-4a55-9f2b-7c3e2d1a0b99'};
  assert.equal((await ctx.repo.create(reading)).inserted,true);
  error={code:11000};assert.equal((await ctx.repo.create(reading)).inserted,false);
  error=new Error('DB caida');await assert.rejects(ctx.repo.create(reading),/DB caida/);
});
test('una entrega duplicada no dispara dos chequeos de alerta',async()=>{
  const source=fs.readFileSync(new URL('../../software/backend/src/services/sensor-data-service.js',import.meta.url),'utf8')
    .replace(/^import .*;$/gm,'').replace('export default new SensorDataService();','globalThis.service = new SensorDataService();');
  let inserted=true,alerts=0;
  const ctx=vm.createContext({config:{AURA_DEVICE_ID:device},parseAuraData,isDeviceId:()=>true,
    alertService:{checkThresholdAndNotify:async()=>{alerts++;}},console,
    sensorDataRepository:{create:async reading=>({record:{...reading,createdAt:new Date()},inserted})}});
  vm.runInContext(source,ctx);
  await ctx.service.saveTelemetry({temperature:4,deviceId:device});inserted=false;
  const duplicated=await ctx.service.saveTelemetry({temperature:4,deviceId:device});
  assert.equal(alerts,1);assert.equal(duplicated.duplicate,true);
});
test('status del bridge se conserva y response correlaciona UUID+command_id sin confirmar ejecucion',async()=>{
  const source=fs.readFileSync(new URL('../../software/backend/src/services/device-event-service.js',import.meta.url),'utf8')
    .replace(/^import .*;$/gm,'').replace('export default new DeviceEventService();','globalThis.service = new DeviceEventService();');
  const bridge='650e8400-e29b-41d4-a716-446655440002';
  let state='pending',deviceState=null,history=[];
  const ctx=vm.createContext({config:{AURA_DEVICE_ID:device,AURA_BRIDGE_DEVICE_ID:bridge},parseAuraEvent,previousResponseStates,
    console,DeviceStatus:{findOneAndUpdate:(filter,update)=>({lean:async()=>deviceState={...filter,...update.$set}})},
    DeviceCommand:{findOne:()=>({lean:async()=>({params:good})}),updateOne:async(filter,update)=>{assert.equal(filter.deviceId,device);assert.equal(filter.command_id,'c-1');history.push(update.$addToSet.responses);},
      findOneAndUpdate:(filter,update)=>({lean:async()=>{if(filter.state.$in.includes(state)){state=update.$set.state;assert.notEqual(update.$set.confirma_ejecucion,true);}return {state};}})}});
  vm.runInContext(source,ctx);
  await ctx.service.processMessage(`devices/${bridge}/status`,'{"status":"offline"}');assert.equal(deviceState.status,'offline');
  await ctx.service.processMessage(`devices/${device}/response`,'{"status":"recibido","details":{"command_id":"c-1"}}');
  assert.equal(state,'recibido');
  await ctx.service.processMessage(`devices/${device}/response`,'{"status":"encolado","details":{"command_id":"c-1"}}');
  assert.equal(state,'recibido');assert.equal(history.length,2);
  await ctx.service.processMessage(`devices/${device}/response`,'{"status":"enviado_a_mesh","details":{"command_id":"c-1"}}');assert.equal(history.length,2);
});
test('bridge offline tiene prioridad sobre ultimo online del nodo en el dashboard',()=>{
  const source=fs.readFileSync(new URL('../../software/frontend/dashboard.js',import.meta.url),'utf8');
  const ctx=vm.createContext({});vm.runInContext('function describirEstadoAura'+source.split('function describirEstadoAura')[1].split('function renderState')[0],ctx);
  const state={mqttConnected:true,bridgeConfigured:true,device:{status:'online'},bridge:{status:'offline'}};
  assert.equal(ctx.describirEstadoAura(state).online,false);
  assert.match(ctx.describirEstadoAura(state).text,/Gateway offline/);
  state.bridge.status='online';state.bridge.details={central:'online'};assert.equal(ctx.describirEstadoAura(state).online,true);
  state.mqttConnected=false;assert.equal(ctx.describirEstadoAura(state).online,false);
});

test('indice unico de ingest_id es parcial para permitir lecturas sin identificador',()=>{
  const source=fs.readFileSync(new URL('../../software/backend/src/models/sensor-data-model.js',import.meta.url),'utf8')
    .replace(/^import .*;$/gm,'').replace('export default mongoose.model','globalThis.model = mongoose.model');
  const indexes=[];
  class Schema { constructor(){} index(key,options){indexes.push({key,options});} }
  const ctx=vm.createContext({mongoose:{Schema,model:()=>({})}});vm.runInContext(source,ctx);
  const index=normalize(indexes.find(x=>x.key.ingest_id));
  assert.deepEqual(index,{key:{ingest_id:1},options:{unique:true,partialFilterExpression:{ingest_id:{$type:'string'}}}});
});
test('respuesta recibida durante publish no se sobrescribe con pending',async()=>{
  const source=fs.readFileSync(new URL('../../software/backend/src/routes/device-config-routes.js',import.meta.url),'utf8')
    .replace(/^import .*;$/gm,'').replace('export default router;','globalThis.router = router;');
  const handlers={};let state='publishing';
  const commandModel={create:async()=>({}),updateOne:async(filter,update)=>{if(state===filter.state)state=update.$set.state;}};
  const ctx=vm.createContext({Router:()=>({get:(path,fn)=>{},post:(path,fn)=>handlers[path]=fn}),randomUUID:()=> 'c-1',
    config:{AURA_DEVICE_ID:device,AURA_CONFIG_EXPERIMENTAL:true},isDeviceId:()=>true,validateConfig,validateSensor,sensorDeviceId,DeviceCommand:commandModel,
    DeviceStatus:{findOne:()=>({lean:async()=>null})},deviceEventService:{},mqttConfig:{publishCommand:async()=>{state='recibido';}}});
  vm.runInContext(source,ctx);
  let statusCode;
  const response={status:code=>{statusCode=code;return response;},json:()=>{}};
  await handlers['/dispositivo/config']({body:good},response);
  assert.equal(statusCode,202);assert.equal(state,'recibido');
});
