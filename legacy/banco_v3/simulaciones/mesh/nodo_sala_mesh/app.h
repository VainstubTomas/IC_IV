// Rele fijo del ejemplo AURA: NO fabrica ACK del gateway ni del central.
// El sensor conserva su copia hasta el ACK central, por lo que la sala no
// necesita asumir propiedad de las muestras ni borrarlas al confirmar un salto.
#include "../../../../firmware/nodo_mesh_v3/mesh_radio.h"
#if __has_include("config_local.h")
#include "config_local.h"
#endif
#ifndef ICIV_SENSOR_MAC
#define ICIV_SENSOR_MAC {0,0,0,0,0,0}
#endif
#ifndef ICIV_GATEWAY_MAC
#define ICIV_GATEWAY_MAC {0,0,0,0,0,0}
#endif
#ifndef ICIV_CHANNEL
#define ICIV_CHANNEL 1
#endif
const uint8_t sensorMac[6]=ICIV_SENSOR_MAC,gatewayMac[6]=ICIV_GATEWAY_MAC;
bool ready=false;
void setup() {
  Serial.begin(115200);ready=radioInit(ICIV_CHANNEL);
  if(ready)ready=addPeer(sensorMac)&&addPeer(gatewayMac);
  Serial.println(ready?"Sala ICIV lista":"Sala: configurar MAC/canal o revisar radio");
}
void loop() {
  if(!ready){delay(1000);return;}
  RadioPacket p;
  while(radioRead(p)) {
    if(!memcmp(p.sender,sensorMac,6) && !memcmp(p.trama.origen,sensorMac,6) && !memcmp(p.trama.destino,gatewayMac,6))radioSend(gatewayMac,p.trama);
    else if(!memcmp(p.sender,gatewayMac,6) && !memcmp(p.trama.origen,gatewayMac,6) && !memcmp(p.trama.destino,sensorMac,6))radioSend(sensorMac,p.trama);
  }
  delay(5);
}
