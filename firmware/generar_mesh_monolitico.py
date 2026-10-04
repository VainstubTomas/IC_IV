"""Regenera copias autocontenidas para Arduino IDE; no copia credenciales."""
from pathlib import Path
import re

root=Path(__file__).resolve().parent
roles=['nodo_mesh','nodo_sala_mesh','nodo_gateway_mesh']
def expand(path,stack=()):
    if path in stack: raise ValueError('Include circular')
    text=path.read_text(encoding='utf-8')
    def include(match):
        target=match[1]
        if target.startswith('../mesh_comun/') or (path.parent.name=='mesh_comun' and target.startswith('mesh_')):
            return expand((path.parent/target).resolve(),stack+(path,))
        return match[0]
    return re.sub(r'^#include "([^"]+)"$',include,text,flags=re.M)
for role in roles:
    dest=root/'monolitico'/role
    dest.mkdir(parents=True,exist_ok=True)
    (dest/f'{role}.ino').write_text('// Generado por firmware/generar_mesh_monolitico.py\n#include "app.h"\n',encoding='utf-8')
    (dest/'app.h').write_text('// GENERADO: editar el rol original y regenerar.\n'+expand(root/role/'app.h'),encoding='utf-8')
    (dest/'config_local.h.example').write_bytes((root/role/'config_local.h.example').read_bytes())
    print(role)
