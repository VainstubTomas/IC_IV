#ifndef CONFIG_NODO_H
#define CONFIG_NODO_H
#include <stdint.h>
#include <stddef.h>

// Propuesta IC_IV v1: FPort 10 (downlink), 11 (reporte). Ver seccion de configuracion remota del README.md.
const uint8_t PUERTO_CONFIG = 10;
const uint8_t PUERTO_REPORTE = 11;
const size_t TAM_CONFIG = 7;
struct ConfigNodo {
  uint16_t intervaloSeg;
  bool confirmado;
  bool adr;
  uint8_t dr;
  int16_t offsetCentesimas;
};
inline ConfigNodo configPorDefecto() { return {20, true, false, 2, 0}; }
inline bool decodificarConfig(const uint8_t* b, size_t n, ConfigNodo& salida) {
  if (!b || n != TAM_CONFIG || b[0] != 1 || (b[3] & 0xFC)) return false;
  uint16_t intervalo = (uint16_t(b[1]) << 8) | b[2];
  uint16_t raw = (uint16_t(b[5]) << 8) | b[6];
  int32_t offset = raw >= 0x8000 ? int32_t(raw) - 65536 : raw;
  if (intervalo < 20 || intervalo > 3600 || b[4] < 2 || b[4] > 5 || offset < -500 || offset > 500) return false;
  salida = {intervalo, bool(b[3] & 1), bool(b[3] & 2), b[4], int16_t(offset)};
  return true;
}
inline void codificarConfig(const ConfigNodo& c, uint8_t* b) {
  b[0] = 1; b[1] = c.intervaloSeg >> 8; b[2] = c.intervaloSeg & 0xFF;
  b[3] = (c.confirmado ? 1 : 0) | (c.adr ? 2 : 0); b[4] = c.dr;
  uint16_t offset = uint16_t(c.offsetCentesimas);
  b[5] = offset >> 8; b[6] = offset & 0xFF;
}
#endif
