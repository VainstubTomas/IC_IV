"""Genera el paquete de nodo para el repo AURA. No toca ficha ni secretos."""
from pathlib import Path
import re
root=Path(__file__).resolve().parents[2]
target=root/'dispositivos/E1-PB-LECA-HFR01'
target.mkdir(parents=True,exist_ok=True)
banner='// GENERADO desde IC IV; editar origen y regenerar.\n'
(target/(target.name+'.ino')).write_text(banner+'#include "app.h"\n',encoding='utf-8')
source=root/'firmware/nodo_mesh'
app=(source/'app.h').read_text(encoding='utf-8').replace('../mesh_comun/','')
(target/'app.h').write_text(banner+app,encoding='utf-8')
for name in ['mesh_core.h','mesh_radio.h','mesh_storage.h']:
    (target/name).write_text(banner+(root/'firmware/mesh_comun'/name).read_text(encoding='utf-8'),encoding='utf-8')
for name in ['config_local.h.example','partitions.csv']:
    (target/name).write_bytes((source/name).read_bytes())
(target/'bibliotecas.txt').write_text('OneWire@2.3.8\nDallasTemperature@4.0.6\nU8g2@2.36.19\nRTClib@2.1.4\nAdafruit BusIO@1.17.4\n',encoding='utf-8')
tests=target/'tests';(tests/'fakes').mkdir(parents=True,exist_ok=True)
for name in ['mesh_assertions.h','test_mesh.cpp','test_storage.cpp','fakes/Preferences.h']:
    text=(root/'tests/firmware/host'/name).read_text(encoding='utf-8')
    text=text.replace('../../../firmware/mesh_comun/','../')
    (tests/name).write_text(banner+text,encoding='utf-8')
(tests/'Makefile').write_text('''CXX ?= g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Ifakes
.PHONY: all clean
all: test_mesh test_storage
	./test_mesh
	./test_storage
test_mesh: test_mesh.cpp mesh_assertions.h ../mesh_core.h
	$(CXX) $(CXXFLAGS) test_mesh.cpp -o $@
test_storage: test_storage.cpp fakes/Preferences.h ../mesh_storage.h ../mesh_core.h
	$(CXX) $(CXXFLAGS) test_storage.cpp -o $@
clean:
	rm -f test_mesh test_storage *.exe
''',encoding='utf-8')
print('Paquete generado:',target)
