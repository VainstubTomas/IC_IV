"""Verifica/exporta un RESPALDO LOCAL del journal IC IV v2; no escribe la placa."""
import argparse,json,struct,zlib,uuid
from pathlib import Path

def decode(text):
    blob=bytes.fromhex(text)
    # StoredJournal usa padding nativo alrededor del Journal packed de 1803 B.
    if len(blob)!=1812 or struct.unpack_from('<I',blob)[0]!=0x49433432:
        raise ValueError('Formato StoredJournal v2 no reconocido (esperado 1812 bytes)')
    journal=blob[4:1807]
    if zlib.crc32(journal)!=struct.unpack_from('<I',blob,1808)[0]:
        raise ValueError('CRC inválido: no utilizar este slot como respaldo confirmado')
    generation=struct.unpack_from('<I',journal)[0]
    ih,mnh,mxh,iff,mnf,mxf,recovery=struct.unpack_from('<IhhIhhI',journal,4)
    cfg={'intervalo_heladera_s':ih,'intervalo_freezer_s':iff,'min_heladera_c':mnh/100,'max_heladera_c':mxh/100,'min_freezer_c':mnf/100,'max_freezer_c':mxf/100,'recuperacion_s':recovery}
    if not(5<=ih<=86400 and 5<=iff<=86400 and 60<=recovery<=86400 and -5500<=mnh<mxh<=12500 and -5500<=mnf<mxf<=12500):
        raise ValueError('Configuración almacenada inválida')
    heads=journal[1760:1762];counts=journal[1762:1764];has=journal[1764:1766];acked=journal[1766:1768]
    if any(v>=30 for v in heads) or any(v>30 for v in counts) or any(v>1 for v in has+acked):raise ValueError('Índice de cola inválido')
    rows=[]
    def sample(offset,sensor,protected=False,confirmed=False):
        ident,ts,cut,temp,s,flags=struct.unpack_from('<16sIIhBB',journal,offset)
        if s!=sensor or flags&~31 or (flags&8 and ts<1700000000) or (not flags&8 and ts!=0):raise ValueError('Registro inválido')
        if flags&1 and not -5500<=temp<=12500:raise ValueError('Temperatura inválida en journal')
        if not flags&1 and temp!=32767:raise ValueError('Centinela inválido en journal')
        rows.append({'ingest_id':str(uuid.UUID(bytes=ident)),'sensor':'freezer' if s else 'heladera','epoch_original':ts or None,'corte_epoch':cut or None,'temperatura_c':temp/100 if flags&1 and temp!=8500 else None,'invalidada_en_v3':not(flags&1) or temp==8500,'flags_originales':flags,'primera_protegida':protected,'confirmada_v2':confirmed})
    for s in range(2):
        if has[s]:sample(1704+s*28,s,True,bool(acked[s]))
        for i in range(counts[s]):sample(24+(s*30+(heads[s]+i)%30)*28,s)
    dh,df,missed=struct.unpack_from('<III',journal,1768)
    powerKnown,battery,capture=journal[1780:1783];cut=struct.unpack_from('<I',journal,1783)[0]
    return {'generation':generation,'config':cfg,'records':rows,'descartadas':[dh,df],'capturas_no_protegidas':missed,'alimentacion_conocida':bool(powerKnown),'bateria':bool(battery),'captura_pendiente':bool(capture),'corte_epoch':cut or None}

def verify(path):
    data=json.loads(path.read_text(encoding='utf-8-sig'));valid=[];errors={}
    for key in ('q0','q1'):
        try:valid.append((key,decode(data.get(key,''))))
        except (ValueError,struct.error) as e:errors[key]=str(e)
    if not valid:raise ValueError('Ningún slot válido: conservar el archivo original para recuperación. '+str(errors))
    chosen=valid[0]
    if len(valid)==2:
        delta=(valid[1][1]['generation']-chosen[1]['generation'])&0xFFFFFFFF
        if 0<delta<0x80000000:chosen=valid[1]
    output=path.with_suffix('.revisado.json')
    output.write_text(json.dumps({'slot':chosen[0],'errores_otro_slot':errors,**chosen[1]},indent=2,ensure_ascii=False),encoding='utf-8')
    print('CRC y formato válidos. Respaldo exportado:',output)
    print('No se importó ni borró ningún dato de la placa; revisar registros y configuración antes de migrar.')
    return output

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('respaldo',type=Path)
    args=parser.parse_args()
    try:verify(args.respaldo)
    except (ValueError,struct.error,OSError) as e:parser.exit(1,str(e)+'\n')
