"""Testes de integração sem hardware: controlador e, em Linux, portas virtuais."""
import os
import queue
import select
import tempfile
import time
from types import SimpleNamespace
from unittest.mock import Mock, patch
import unittest
from bancada.protocol import Packet, encode
from bancada.live_gui import LiveApp
from bancada.live_storage import load_live
from bancada.serial_link import SerialLink

class Var:
    def __init__(self, value=''): self.value=value
    def get(self): return self.value
    def set(self, value): self.value=value

def headless_app(root):
    app=LiveApp.__new__(LiveApp)
    app.root=Mock()
    app.folder=root
    app.writer=None
    app.acquisition=None
    app.reference=-20000
    app.board={'state':'IDLE','flash':'OK','radio':'ON'}
    app.phase='IDLE'
    app.invalid=0
    app.pending_end=None
    app.radio_transfer=None
    app.closing=False
    app.viewing_file=False
    app.links={'USB':SimpleNamespace(port=SimpleNamespace(port='COM3'),send=Mock())}
    app.name=Var('Teste de integração')
    app.duration=Var('60')
    app.flash=Var(False)
    app.notes=Mock();app.notes.get.return_value='Célula 5 kg; sem calibração'
    for name in ('status','detail','path_text','board_text'):setattr(app,name,Var())
    app.update_controls=Mock()
    app.file_tree=None
    return app

class ControllerTests(unittest.TestCase):
    def test_start_ack_data_stop_and_reload(self):
        with tempfile.TemporaryDirectory() as td:
            app=headless_app(td)
            app.start()
            sid=app.acquisition.session
            self.assertEqual(app.phase,'STARTING')
            app.links['USB'].send.assert_called_with(f'START {sid} 60 0')
            app.handle_line('USB',f'ACK;START;{sid}',time.monotonic())
            self.assertEqual(app.phase,'RUNNING')
            for i in range(20):
                app.handle_line('USB',encode(Packet(sid,None,i,(i+1)*95000,-20000+i*10)),time.monotonic())
                if i%5==0:
                    radio=encode(Packet(sid,i//5,i,(i+1)*95000,-20000+i*10))
                    app.handle_line('LORA',f"Received packet '{radio}' with RSSI -63",time.monotonic())
            app.stop()
            self.assertEqual(app.phase,'STOPPING')
            app.handle_line('USB',f'ACK;STOP;{sid};MANUAL;20',time.monotonic())
            self.assertEqual(app.phase,'DRAINING')
            reason,count=app.pending_end
            app.finish_local('stopped',True,reason,count)
            meta,a=load_live(app.writer.meta_path)
            self.assertTrue(meta['stop_confirmed'])
            self.assertEqual(len(a.verified),4)
            self.assertEqual(len(a.traces['USB'].samples),20)
            self.assertEqual(len(a.traces['LORA'].samples),4)

    def test_usb_does_not_create_fake_radio_reception(self):
        with tempfile.TemporaryDirectory() as td:
            app=headless_app(td);app.start();sid=app.acquisition.session
            app.handle_line('USB',f'STATUS;RECORDING;{sid};0;OK;1000000;ON;OK',time.monotonic())
            self.assertEqual(app.phase,'RUNNING') # recupera ACK de início perdido
            app.handle_line('USB',encode(Packet(sid,None,0,95000,99)),time.monotonic())
            self.assertEqual(app.acquisition.traces['LORA'].samples,[])
            app.handle_line('USB','EVT;READY;0.2.0',time.monotonic())
            meta,_=load_live(app.writer.meta_path)
            self.assertEqual(meta['state'],'board_reset')
            self.assertFalse(meta['stop_confirmed'])

    def test_flash_unavailable_rejects_before_start(self):
        with tempfile.TemporaryDirectory() as td, patch('bancada.live_gui.messagebox.showerror') as error:
            app=headless_app(td);app.board['flash']='UNAVAILABLE';app.flash.set(True)
            app.start()
            error.assert_called_once()
            app.links['USB'].send.assert_not_called()
            self.assertIsNone(app.writer)

    def test_disk_error_still_closes_record_files(self):
        with tempfile.TemporaryDirectory() as td:
            app=headless_app(td);app.start()
            with patch.object(app.writer,'flush',side_effect=OSError('disco cheio')):
                app.finish_local('pc_error',False,'DISK_ERROR')
            self.assertTrue(app.writer.closed)
            self.assertFalse(app.active)
            self.assertTrue(all(f.closed for f in app.writer.files.values()))
            self.assertIn('FALHA AO SALVAR',app.status.get())

@unittest.skipUnless(hasattr(os,'openpty'),'Portas PTY disponíveis apenas em sistemas POSIX')
class SerialTests(unittest.TestCase):
    def setUp(self):
        self.master,self.slave=os.openpty()
        self.events=queue.Queue()
        self.link=SerialLink(os.ttyname(self.slave),'USB',self.events)

    def tearDown(self):
        self.link.close();os.close(self.master);os.close(self.slave)

    def test_fragmented_serial_and_command(self):
        line=encode(Packet('0123456789abcdef',None,0,95000,-20000))
        os.write(self.master,line[:10].encode())
        time.sleep(0.15)
        self.assertTrue(self.events.empty())
        os.write(self.master,(line[10:]+'\r\n').encode())
        event=self.events.get(timeout=2)
        self.assertEqual(event[:3],('USB','line',line))
        self.link.send('STATUS')
        self.assertTrue(select.select([self.master],[],[],2)[0])
        self.assertEqual(os.read(self.master,100),b'STATUS\n')

    def test_long_line_resynchronizes(self):
        os.write(self.master,b'x'*600+b'\nSTATUS;IDLE;-;0;OK;1000;ON;OK\n')
        first=self.events.get(timeout=2)
        second=self.events.get(timeout=2)
        self.assertEqual(first[1],'invalid')
        self.assertEqual(second[1:3],('line','STATUS;IDLE;-;0;OK;1000;ON;OK'))

if __name__=='__main__':unittest.main()
