#ifndef ICIV_MESH_STORAGE_H
#define ICIV_MESH_STORAGE_H
#include <Preferences.h>
#include "mesh_core.h"
struct StoredJournal { uint32_t magic; Journal journal; uint32_t crc; };
class MeshStorage {
  Preferences prefs;
  StoredJournal writeBuffer={}, readBuffer={}; // no ocupar el stack del loop con blobs
  uint8_t slot=0;
  bool readSlot(const char* key,StoredJournal& b) {
    if(prefs.getBytesLength(key)!=sizeof(b) || prefs.getBytes(key,&b,sizeof(b))!=sizeof(b))return false;
    return b.magic==0x49433432 && b.crc==crc32(reinterpret_cast<const uint8_t*>(&b.journal),sizeof(Journal)) && validJournal(b.journal);
  }
public:
  bool begin(Journal& j) {
    if(!prefs.begin("iciv_mesh",false))return false;
    StoredJournal& a=writeBuffer;StoredJournal& b=readBuffer;
    bool va=readSlot("q0",a),vb=readSlot("q1",b);
    if(va || vb) {slot=vb && (!va || int32_t(b.journal.generation-a.journal.generation)>0);j=slot?b.journal:a.journal;return true;}
    // No borrar silenciosamente una cola corrupta ni asumir perdida de datos como normal.
    if(prefs.isKey("q0") || prefs.isKey("q1"))return false;
    journalInit(j);return save(j);
  }
  bool save(Journal& j) {
    if(!validJournal(j))return false;
    StoredJournal& b=writeBuffer;b=StoredJournal{};b.magic=0x49433432;b.journal=j;++b.journal.generation;
    b.crc=crc32(reinterpret_cast<const uint8_t*>(&b.journal),sizeof(Journal));
    uint8_t target=1-slot;const char* key=target?"q1":"q0";
    if(prefs.putBytes(key,&b,sizeof(b))!=sizeof(b))return false;
    StoredJournal& check=readBuffer;if(!readSlot(key,check) || check.journal.generation!=b.journal.generation)return false;
    j.generation=b.journal.generation;slot=target;return true;
  }
};
#endif
