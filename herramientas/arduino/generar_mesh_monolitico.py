"""Regenera copias autocontenidas para Arduino IDE; no copia credenciales."""
from pathlib import Path
import re
root=Path(__file__).resolve().parents[2]
roles={
    'nodo_mesh':root/'firmware/nodo_mesh',
    'nodo_sala_mesh':root/'simulaciones/mesh/nodo_sala_mesh',
    'nodo_gateway_mesh':root/'simulaciones/mesh/nodo_gateway_mesh',
}
common=(root/'firmware/mesh_comun').resolve()
def expand(path,stack=()):
    if path in stack:raise ValueError('Include circular')
    text=path.read_text(encoding='utf-8')
    def include(match):
        candidate=(path.parent/match[1]).resolve()
        if candidate.is_relative_to(common):return expand(candidate,stack+(path,))
        return match[0]
    return re.sub(r'^#include "([^"\n]+)"$',include,text,flags=re.M)
for role,source in roles.items():
    dest=root/'herramientas/arduino/monolitico'/role
    dest.mkdir(parents=True,exist_ok=True)
    (dest/f'{role}.ino').write_text('// Generado por herramientas/arduino/generar_mesh_monolitico.py\n#include "app.h"\n',encoding='utf-8')
    (dest/'app.h').write_text('// GENERADO: editar el rol original y regenerar.\n'+expand(source/'app.h'),encoding='utf-8')
    (dest/'config_local.h.example').write_bytes((source/'config_local.h.example').read_bytes())
    if (source/'partitions.csv').is_file():
        (dest/'partitions.csv').write_bytes((source/'partitions.csv').read_bytes())
    print(role)
