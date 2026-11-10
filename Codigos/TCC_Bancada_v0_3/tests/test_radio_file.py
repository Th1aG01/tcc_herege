from dataclasses import replace
from pathlib import Path
import tempfile
import unittest
import zlib
from bancada.protocol import Packet, encode
from bancada.radio_file import BLOCK_SIZE, FileFrame, FileReceiver, encode_frame, parse_frame

SID='0123456789abcdef'

def fixture():
    body=f'META;TCC2;{SID}\n'
    for i in range(30): body+=encode(Packet(SID,None,i,(i+1)*95000,-20000+i*123))+'\n'
    body+=f'END;{SID};MANUAL;30\n'
    data=body.encode('ascii')
    total=(len(data)+BLOCK_SIZE-1)//BLOCK_SIZE
    crc=zlib.crc32(data)&0xffffffff
    frames=[FileFrame(SID,'B',i,total,len(data),crc,data[i*BLOCK_SIZE:(i+1)*BLOCK_SIZE],-60) for i in range(total)]
    return data,frames

class RadioFileTests(unittest.TestCase):
    def test_bravo_frames_roundtrip_and_255_byte_limit(self):
        data,frames=fixture()
        for frame in frames:
            line=encode_frame(frame)
            self.assertLessEqual(len(line),255)
            self.assertEqual(parse_frame(f"Received packet '{line}' with RSSI -60"),frame)
        # Máximo de campos e tamanho permitido no arquivo.
        size=16*1024*1024;total=(size+119)//120
        self.assertLessEqual(len(encode_frame(FileFrame(SID,'B',total-2,total,size,0xffffffff,b'x'*120))),255)

    def test_corrupt_frame_and_wrong_index_rejected(self):
        _,frames=fixture();line=encode_frame(frames[0])
        with self.assertRaises(ValueError):parse_frame(line[:-2])
        with self.assertRaises(ValueError):parse_frame(line.replace('TCCF3;0','TCCF3;1',1))
        with self.assertRaises(ValueError):encode_frame(replace(frames[0],index=frames[0].blocks))
        with self.assertRaises(ValueError):encode_frame(replace(frames[0],data=b'x'))

    def test_missing_blocks_never_report_complete(self):
        data,frames=fixture()
        with tempfile.TemporaryDirectory() as td:
            r=FileReceiver(td,frames[0])
            for frame in frames:
                if frame.index!=3:r.accept(frame)
            r.accept(replace(frames[0],kind='E',index=0,data=b''))
            self.assertEqual(r.missing,1)
            self.assertEqual(r.missing_indices(),[3])
            self.assertFalse(r.complete)
            self.assertFalse(r.path.exists())
            self.assertIsNone(r.validation_error)
            r.close()

    def test_repeat_after_restart_fills_gaps_without_duplicates(self):
        data,frames=fixture()
        with tempfile.TemporaryDirectory() as td:
            r=FileReceiver(td,frames[0])
            for frame in frames[::2]:r.accept(frame)
            r.close()
            r=FileReceiver(td,frames[0])
            for frame in frames:r.accept(frame)
            self.assertTrue(r.complete)
            self.assertGreater(r.duplicates,0)
            self.assertEqual(r.path.read_bytes(),data)
            self.assertEqual(r.record_status['sample_count'],30)
            self.assertEqual(len(r.csv_path.read_text(encoding='utf-8-sig').splitlines()),31)
            r.close()

    def test_out_of_order_and_missing_start_end_are_supported(self):
        data,frames=fixture()
        with tempfile.TemporaryDirectory() as td:
            r=FileReceiver(td,frames[-1])
            for frame in reversed(frames):r.accept(frame)
            self.assertTrue(r.complete)
            self.assertEqual(r.path.read_bytes(),data)
            r.close()

    def test_crc32_mismatch_does_not_publish_file(self):
        _,frames=fixture()
        bad=[replace(frame,checksum=frame.checksum^1) for frame in frames]
        with tempfile.TemporaryDirectory() as td:
            r=FileReceiver(td,bad[0])
            with self.assertRaises(ValueError):
                for frame in bad:r.accept(frame)
            self.assertFalse(r.complete)
            self.assertTrue(r.partial_path.exists())
            self.assertFalse(r.path.exists())
            self.assertTrue(r.validation_error)
            r.close()

    def test_unchanged_completed_file_can_be_received_again(self):
        data,frames=fixture()
        with tempfile.TemporaryDirectory() as td:
            r=FileReceiver(td,frames[0])
            for frame in frames:r.accept(frame)
            r.close()
            again=FileReceiver(td,frames[0]);again.accept(frames[0])
            self.assertTrue(again.complete)
            self.assertEqual(again.path.read_bytes(),data)
            again.close()

    def test_other_files_and_conflicting_duplicates_rejected(self):
        _,frames=fixture()
        with tempfile.TemporaryDirectory() as td:
            r=FileReceiver(td,frames[0]);r.accept(frames[0])
            with self.assertRaises(ValueError):r.accept(replace(frames[0],session='0000000000000000'))
            with self.assertRaises(ValueError):r.accept(replace(frames[0],data=b'Z'*120))
            r.close()

    def test_received_incomplete_source_record_is_labeled(self):
        data,frames=fixture()
        data=data[:data.rfind(b'END;')]
        total=(len(data)+119)//120
        frames=[FileFrame(SID,'B',i,total,len(data),zlib.crc32(data)&0xffffffff,data[i*120:(i+1)*120]) for i in range(total)]
        with tempfile.TemporaryDirectory() as td:
            r=FileReceiver(td,frames[0])
            for frame in frames:r.accept(frame)
            self.assertTrue(r.complete)
            self.assertFalse(r.record_status['stop_confirmed'])
            self.assertEqual(r.record_status['state'],'flash_incomplete')
            r.close()

if __name__=='__main__':unittest.main()
