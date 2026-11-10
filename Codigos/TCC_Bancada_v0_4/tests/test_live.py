import tempfile
from pathlib import Path
from dataclasses import replace
import unittest
from bancada.protocol import Packet, Acquisition, Trace, encode, parse_line
from bancada.live_storage import LiveWriter, load_live, load_flash

SID = '0123456789abcdef'

class ProtocolTests(unittest.TestCase):
    def test_usb_and_exact_bravo_wrapper(self):
        p = Packet(SID, None, 0, 95123, -20360)
        self.assertEqual(parse_line(encode(p)), p)
        radio = replace(p, packet_seq=0)
        self.assertEqual(parse_line("Received packet '" + encode(radio) + "' with RSSI -65"), replace(radio, rssi=-65))
        self.assertIsNone(parse_line('LoRa Receiver'))
        self.assertIsNone(parse_line("Received packet 'Funciona 10' with RSSI -42"))

    def test_corruption_and_truncation_rejected(self):
        line = encode(Packet(SID, 0, 1, 100, 123))
        for bad in [line[:-2], line.replace(';123;', ';124;'), line+';0', "Received packet '"+line]:
            with self.assertRaises(ValueError): parse_line(bad)

    def test_limits_and_saturation(self):
        for raw in [-8388608, 8388607]:
            p = Packet(SID, None, 0, 0, raw, 'SAT')
            self.assertEqual(parse_line(encode(p)), p)
        for p in [Packet(SID,None,0,0,8388608), Packet(SID,None,0,0,-8388608), Packet(SID,None,0,-1,1)]:
            with self.assertRaises(ValueError): encode(p)

    def test_radio_decimation_is_not_packet_loss(self):
        radio = Trace('LORA')
        for i, seq in enumerate((0, 5, 11)):
            self.assertTrue(radio.add(Packet(SID,i,seq,seq*95000,100)))
        self.assertEqual(radio.gaps, 0)
        radio.add(Packet(SID,4,20,1900000,120))
        self.assertEqual(radio.gaps, 1)

    def test_duplicates_wrong_port_and_other_session(self):
        a = Acquisition(SID)
        p = Packet(SID,None,0,1,99)
        self.assertTrue(a.add('USB',p))
        self.assertFalse(a.add('USB',p))
        self.assertEqual(a.traces['USB'].duplicates,1)
        with self.assertRaises(ValueError): a.add('LORA',p)
        with self.assertRaises(ValueError): a.add('USB',replace(p,raw=100))
        self.assertFalse(a.add('USB',replace(p,session='0000000000000000')))
        self.assertEqual(a.foreign,1)

    def test_radio_usb_comparison_both_orders(self):
        a = Acquisition(SID)
        for i in range(5):
            p = Packet(SID,None,i,(i+1)*95000,i*100)
            order = ('USB','LORA') if i % 2 else ('LORA','USB')
            for source in order:
                a.add(source,p if source=='USB' else replace(p,packet_seq=i,rssi=-60))
        self.assertEqual(len(a.verified),5)
        p = Packet(SID,None,5,600000,999)
        a.add('USB',p); a.add('LORA',replace(p,packet_seq=5,raw=1000))
        self.assertEqual(len(a.mismatches),1)

class StorageTests(unittest.TestCase):
    def test_live_roundtrip_and_raw_only(self):
        with tempfile.TemporaryDirectory() as td:
            a=Acquisition(SID)
            w=LiveWriter(td,SID,'Teste','5 kg sem calibração',-20000,60,{'USB':'COM3'},True)
            for i in range(30):
                p=Packet(SID,None,i,(i+1)*95000,-20000+i*30)
                a.add('USB',p); w.append('USB',p)
                if i%5==0:
                    radio=replace(p,packet_seq=i//5,rssi=-60)
                    a.add('LORA',radio); w.append('LORA',radio)
            # Duplicata auditada não é adicionada uma segunda vez ao CSV.
            a.add('USB',p)
            w.finish('stopped',a.stats(),True,'MANUAL',30)
            meta,loaded=load_live(w.meta_path)
            self.assertEqual(len(loaded.traces['USB'].samples),30)
            self.assertEqual(len(loaded.verified),6)
            self.assertIsNone(meta['calibration'])
            self.assertFalse(meta['calibrated'])
            self.assertNotIn('force_N',(w.folder/'usb.csv').read_text(encoding='utf-8-sig'))

    def test_removed_csv_row_detected(self):
        with tempfile.TemporaryDirectory() as td:
            a=Acquisition(SID)
            w=LiveWriter(td,SID,'Teste','',None,60,{},False)
            for i in range(3):
                p=Packet(SID,None,i,i+1,i)
                a.add('USB',p);w.append('USB',p)
            w.finish('stopped',a.stats(),True,'MANUAL',3)
            path=w.folder/'usb.csv'
            lines=path.read_text(encoding='utf-8-sig').splitlines()
            path.write_text('\n'.join(lines[:-1])+'\n',encoding='utf-8-sig')
            with self.assertRaises(ValueError):load_live(w.meta_path)

    def test_unfinished_record_is_flagged(self):
        with tempfile.TemporaryDirectory() as td:
            w=LiveWriter(td,SID,'Teste','',None,60,{},False)
            w.append('USB',Packet(SID,None,0,1,10));w.flush()
            meta,a=load_live(w.meta_path)
            self.assertFalse(meta['stop_confirmed'])
            self.assertEqual(len(a.traces['USB'].samples),1)
            w.finish('pc_error',a.stats(),False,'TEST')

    def test_flash_crc_sequence_and_end_marker(self):
        with tempfile.TemporaryDirectory() as td:
            path=Path(td)/'esp32.log'
            data=f'META;TCC2;{SID}\n'+''.join(encode(Packet(SID,None,i,(i+1)*95000,i))+'\n' for i in range(10))
            path.write_text(data,encoding='ascii')
            meta,a=load_flash(path)
            self.assertEqual(meta['state'],'flash_incomplete')
            path.write_text(data+f'END;{SID};MANUAL;10\n',encoding='ascii')
            self.assertTrue(load_flash(path)[0]['stop_confirmed'])
            path.write_text(data+f'END;{SID};MANUAL;11\n',encoding='ascii')
            with self.assertRaises(ValueError):load_flash(path)
            path.write_text(data.replace(';95000;', ';95001;'),encoding='ascii')
            with self.assertRaises(ValueError):load_flash(path)

if __name__=='__main__':unittest.main()
