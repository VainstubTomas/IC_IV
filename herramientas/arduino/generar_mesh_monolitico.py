"""Regenera copias autocontenidas de los nodos de banco (sala y gateway) para
Arduino IDE; no copia credenciales. El nodo sensor no se genera: su carpeta
dispositivos/E1-PB-LECA-HFR01/ ya es autocontenida y es la fuente del protocolo."""
from pathlib import Path
import re
root=Path(__file__).resolve().parents[2]
roles={
    'nodo_sala_mesh':root/'simulaciones/mesh/nodo_sala_mesh',
    'nodo_gateway_mesh':root/'simulaciones/mesh/nodo_gateway_mesh',
}
def expand(path,stack=()):
    if path in stack:raise ValueError('Include circular')
    text=path.read_text(encoding='utf-8')
    def include(match):
        candidate=(path.parent/match[1]).resolve()
        # Incrustar headers del repo; config_local queda como include al lado del sketch.
        if candidate.is_file() and not candidate.name.startswith('config_local'):return expand(candidate,stack+(path,))
        return match[0]
    return re.sub(r'^#include "([^"\n]+)"$',include,text,flags=re.M)
for role,source in roles.items():
    dest=root/'herramientas/arduino/monolitico'/role
    dest.mkdir(parents=True,exist_ok=True)
    (dest/f'{role}.ino').write_text('// Generado por herramientas/arduino/generar_mesh_monolitico.py\n#include "app.h"\n',encoding='utf-8')
    (dest/'app.h').write_text('// GENERADO: editar el rol original y regenerar.\n'+expand(source/'app.h'),encoding='utf-8')
    (dest/'config_local.h.example').write_bytes((source/'config_local.h.example').read_bytes())
    print(role)
