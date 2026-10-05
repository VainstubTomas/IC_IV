// Las mismas pruebas de host se evaluan en compilacion; un fallo impide generar firmware.
#include "../../host/mesh_assertions.h"
void setup(){Serial.begin(115200);Serial.println("22 comprobaciones C++ de FIFO/ACK/CRC OK");}
void loop(){delay(1000);}
