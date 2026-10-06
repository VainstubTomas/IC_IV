// Utilidad de lectura: NO borra NVS. Cargar con la tabla de particiones ANTERIOR.
// Guardar el JSON del monitor serie en un archivo LOCAL antes de cambiar tabla.
#include <Preferences.h>
Preferences old;
void setup(){
  Serial.begin(115200);delay(2500);
  Serial.println("--- RESPALDO NVS ICIV MESH V2 (lectura, sin borrado) ---");
  if(!old.begin("iciv_mesh",true)){Serial.println("No se encontro namespace iciv_mesh");return;}
  Serial.print("{\"namespace\":\"iciv_mesh\"");
  const char* keys[]={"q0","q1"};
  for(const char* key:keys){
    size_t n=old.getBytesLength(key);Serial.print(",\"");Serial.print(key);Serial.print("\":\"");
    if(n&&n<=8192){auto bytes=new uint8_t[n];if(bytes&&old.getBytes(key,bytes,n)==n)for(size_t i=0;i<n;++i)Serial.printf("%02x",bytes[i]);delete[] bytes;}
    Serial.print("\"");
  }
  Serial.println("}");old.end();
  Serial.println("--- FIN RESPALDO: validar archivo antes de cualquier borrado ---");
}
void loop(){delay(1000);}
