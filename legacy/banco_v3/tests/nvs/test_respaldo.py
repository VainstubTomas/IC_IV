"""Pruebas del respaldo previo a cambiar particiones; no acceden a una placa."""
import importlib.util,struct,zlib,unittest,json,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('backup',root/'herramientas/nvs/verificar_mesh_v2.py')
backup=importlib.util.module_from_spec(spec);spec.loader.exec_module(backup)
def blob(gen=1):
    journal=bytearray(1803);struct.pack_into('<I',journal,0,gen)
    struct.pack_into('<IhhIhhI',journal,4,60,200,600,300,-2500,-1500,300)
    # Una lectura de cada sonda, incluida lectura 85 v2 que v3 debe invalidar.
    for s,t in [(0,425),(1,8500)]:
        struct.pack_into('<16sIIhBB',journal,24+s*30*28,bytes([s+1])*16,1700000001,0,t,s,9)
    journal[1762:1764]=bytes([1,1])
    return (struct.pack('<I',0x49433432)+journal+b'\0'+struct.pack('<I',zlib.crc32(journal))).hex()
class BackupTest(unittest.TestCase):
    def test_config_ids_and_invalid85(self):
        result=backup.decode(blob());self.assertEqual(result['config']['intervalo_freezer_s'],300);self.assertEqual(len(result['records']),2)
        self.assertEqual(result['records'][0]['temperatura_c'],4.25);self.assertIsNone(result['records'][1]['temperatura_c']);self.assertTrue(result['records'][1]['invalidada_en_v3'])
    def test_corruption_is_not_valid_backup(self):
        data=bytearray.fromhex(blob());data[100]^=1
        with self.assertRaises(ValueError):backup.decode(data.hex())
        with self.assertRaises(ValueError):backup.decode('0102')
    def test_generation_wrap_and_one_bad_copy(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).parent) as temp:
            path=Path(temp)/'respaldo_nvs.json';path.write_text(json.dumps({'q0':blob(0xffffffff),'q1':blob(1)}))
            output=backup.verify(path);self.assertEqual(json.loads(output.read_text())['slot'],'q1')
            path.write_text(json.dumps({'q0':'abcd','q1':blob(2)}));output=backup.verify(path);self.assertEqual(json.loads(output.read_text())['generation'],2)
if __name__=='__main__':unittest.main()
