#ifndef ICIV_MESH_INGEST_H
#define ICIV_MESH_INGEST_H
#include <ArduinoJson.h>
#include "../../../../firmware/nodo_mesh_v3/mesh_core.h"
// Validar tipos ANTES de convertir: ausencias, arrays, booleanos y strings no
// acreditan persistencia. El gateway siempre manda exactamente un evento.
inline bool acceptedIngestReply(int http,const char* body) {
  JsonDocument d;
  return !deserializeJson(d,body) && d["inserted"].is<int>() &&
    d["duplicates"].is<int>() && d["errors"].is<int>() &&
    centralAckAllowed(http,d["inserted"],d["duplicates"],d["errors"]);
}
#endif
