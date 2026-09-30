import test from 'node:test';
import assert from 'node:assert/strict';
import vm from 'node:vm';
import fs from 'node:fs';
import { parseAuraData, validateConfig } from '../src/config/mqtt/aura-protocol.js';
import { commandTopic } from '../src/config/mqtt/mqtt-topics.js';
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
test('cliente MQTT usa broker AURA y publica sin retain; confirma solo publicacion',async()=>{
  const source=fs.readFileSync(new URL('../src/config/mqtt/mqtt-config.js',import.meta.url),'utf8')
    .replace(/^import .*;$/gm,'').replace('export default','globalThis.api =');
  const events={}, messages=[],subscriptions=[];
  let persistCount=0;
  const client={connected:true,on:(event,fn)=>events[event]=fn,
    subscribe:(topics,cb)=>{subscriptions.push(...topics);cb(null);},
    publish:(topic,payload,options,cb)=>{messages.push({topic,payload:JSON.parse(payload),options});cb(null);}};
  const context=vm.createContext({mqtt:{connect:url=>{assert.equal(url,'mqtt://aura');return client;}},
    fs:{existsSync:()=>false},config:{MQTTBROKERURL:'mqtt://aura'},TOPICS:{DATA:'devices/+/data',STATUS:'devices/+/status',RESPONSE:'devices/+/response'},
    commandTopic,console:{log(){},error(){}}});
  vm.runInContext(source,context);
  context.api.init({emit(){}},async()=>{persistCount++;});events.connect();
  assert.deepEqual(subscriptions,['devices/+/data','devices/+/status','devices/+/response']);
  await events.message(`devices/${device}/status`,Buffer.from('{}'));
  assert.equal(persistCount,0);
  await events.message(`devices/${device}/data`,Buffer.from('{"values":{"temp_c":4}}'));
  assert.equal(persistCount,1);
  const msg={command:'set_config',params:good,command_id:'command-test'};
  await context.api.publishCommand(device,msg);
  assert.deepEqual(normalize(messages[0]),{topic:`devices/${device}/command`,payload:msg,options:{qos:1,retain:false}});
  client.connected=false;
  await assert.rejects(context.api.publishCommand(device,msg),/desconectado/);
});

test('servicio no inventa lectura inicial, filtra dispositivo y no guarda sensor ausente',async()=>{
  const source=fs.readFileSync(new URL('../src/services/sensor-data-service.js',import.meta.url),'utf8')
    .replace(/^import .*;$/gm,'').replace('export default new SensorDataService();','globalThis.service = new SensorDataService();');
  const created=[],queried=[];
  const context=vm.createContext({config:{AURA_DEVICE_ID:device},parseAuraData,
    isDeviceId:id=>id===device,alertService:{checkThresholdAndNotify:async()=>{}},console,
    sensorDataRepository:{getLatest:async id=>{queried.push(id);return null;},
      getHistory:async()=>[],create:async reading=>{created.push(reading);return {...reading,_id:'record',createdAt:new Date()};}}});
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
