import tempfile
import time
import unittest
from unittest.mock import Mock
from bancada.timing import AcquisitionTiming
from bancada.protocol import Packet, encode
from bancada.live_storage import load_live
from test_integration import headless_app, Var

SID = '0123456789abcdef'


class TimingTests(unittest.TestCase):
    def test_clock_is_independent_from_pc_clock_and_samples(self):
        clock = AcquisitionTiming(SID, 30)
        clock.accept(f'CLOCK;{SID};RECORDING;12000000;30000000', 100)
        self.assertEqual(clock.elapsed_us, 12000000)
        self.assertFalse(clock.snapshot()['duration_confirmed'])
        clock.accept(f'CLOCK;{SID};STOPPED;30001000;30000000', 99999)
        self.assertEqual(clock.snapshot(True)['actual_duration_us'], 30001000)
        self.assertIsNone(clock.snapshot(False)['actual_duration_us'])

    def test_stale_session_regression_wrong_limit_and_changed_final_rejected(self):
        clock = AcquisitionTiming(SID, 30)
        self.assertFalse(clock.accept('CLOCK;ffffffffffffffff;STOPPED;1000000;30000000', 0))
        clock.accept(f'CLOCK;{SID};RECORDING;12000000;30000000', 0)
        for state, elapsed, limit in [('RECORDING', 11000000, 30000000), ('RECORDING', 12000000, 7000000)]:
            with self.assertRaises(ValueError):
                clock.accept(f'CLOCK;{SID};{state};{elapsed};{limit}', 1)
        clock.accept(f'CLOCK;{SID};STOPPED;30000000;30000000', 2)
        with self.assertRaises(ValueError):
            clock.accept(f'CLOCK;{SID};RECORDING;30000000;30000000', 3)
        with self.assertRaises(ValueError):
            clock.accept(f'CLOCK;{SID};STOPPED;31000000;30000000', 3)

    def run_record(self, root, reason, elapsed, last_sample):
        app = headless_app(root)
        app.duration.set('30')
        app.start()
        sid = app.acquisition.session
        for line in [f'ACK;START;{sid}', f'CLOCK;{sid};RECORDING;0;30000000',
                     encode(Packet(sid, None, 0, last_sample, 0)),
                     f'CLOCK;{sid};STOPPED;{elapsed};30000000', f'ACK;STOP;{sid};{reason};1']:
            app.handle_line('USB', line, time.monotonic())
        app.finish_local('stopped' if reason == 'LIMIT' else 'hardware_error', True, reason, 1)
        return app

    def test_normal_completion_stores_duration_distinct_from_last_sample(self):
        with tempfile.TemporaryDirectory() as td:
            app = self.run_record(td, 'LIMIT', 30000000, 29925000)
            meta, _ = load_live(app.writer.meta_path)
            self.assertEqual(meta['timing']['actual_duration_us'], 30000000)
            self.assertEqual(meta['last_sample_us']['USB'], 29925000)
            self.assertIn('DURAÇÃO ATINGIDA', app.status.get())
            self.assertEqual(AcquisitionTiming.from_metadata(app.acquisition.session, meta).elapsed_us, 30000000)

    def test_early_hx_stops_show_failure_and_preserve_actual_duration(self):
        for elapsed in (3001000, 7001000, 12001000):
            with self.subTest(elapsed=elapsed), tempfile.TemporaryDirectory() as td:
                app = self.run_record(td, 'HX711_TIMEOUT', elapsed, elapsed - 2001000)
                meta, _ = load_live(app.writer.meta_path)
                self.assertEqual(meta['timing']['actual_duration_us'], elapsed)
                self.assertEqual(meta['reason'], 'HX711_TIMEOUT')
                self.assertIn('INTERROMPIDO', app.status.get())
                self.assertIn('mais de 2 segundos', app.detail.get())

    def test_clock_missing_does_not_invent_duration_from_samples_or_requested_limit(self):
        with tempfile.TemporaryDirectory() as td:
            app = headless_app(td)
            app.start()
            sid = app.acquisition.session
            app.handle_line('USB', encode(Packet(sid, None, 0, 7000000, 50)), time.monotonic())
            app.finish_local('stopped', True, 'LIMIT', 1)
            meta, _ = load_live(app.writer.meta_path)
            self.assertIsNone(meta['timing']['actual_duration_us'])
            self.assertIn('duração não confirmada', app.status.get())

    def test_new_test_resets_clock_and_ignores_old_session_clock(self):
        with tempfile.TemporaryDirectory() as td:
            app = self.run_record(td, 'LIMIT', 30000000, 29925000)
            old_sid = app.acquisition.session
            app.start()
            self.assertIsNone(app.timing.elapsed_us)
            app.handle_line('USB', f'CLOCK;{old_sid};STOPPED;30000000;30000000', time.monotonic())
            self.assertIsNone(app.timing.elapsed_us)
            app.finish_local('cancelled', False, 'TEST_CLEANUP')

    def test_current_clock_recovers_lost_start_ack(self):
        with tempfile.TemporaryDirectory() as td:
            app = headless_app(td)
            app.duration.set('30'); app.start()
            sid = app.acquisition.session
            app.handle_line('USB', f'CLOCK;{sid};RECORDING;250000;30000000', time.monotonic())
            self.assertEqual(app.phase, 'RUNNING')
            self.assertTrue(app.writer.meta['start_confirmed'])
            app.finish_local('cancelled', False, 'TEST_CLEANUP')

    def test_start_confirmation_timeout_is_not_reported_as_user_stop(self):
        with tempfile.TemporaryDirectory() as td:
            app = headless_app(td)
            app.duration.set('30'); app.start()
            app.stop('start_confirmation_timeout')
            app.finish_local('stopped', True, 'MANUAL', 0)
            meta, _ = load_live(app.writer.meta_path)
            self.assertEqual(meta['state'], 'start_confirmation_timeout')
            self.assertEqual(meta['stop_requested_by'], 'start_confirmation_timeout')
            self.assertIn('INTERROMPIDO', app.status.get())
            self.assertIn('7 s', app.detail.get())

    def test_early_limit_message_is_inconsistent_not_successful(self):
        with tempfile.TemporaryDirectory() as td:
            app = self.run_record(td, 'LIMIT', 7000000, 6900000)
            meta, _ = load_live(app.writer.meta_path)
            self.assertEqual(meta['state'], 'timing_error')
            self.assertIn('INCONSISTÊNCIA', app.status.get())

    def test_display_uses_clock_even_when_plot_has_no_new_sample(self):
        with tempfile.TemporaryDirectory() as td:
            app = self.run_record(td, 'LIMIT', 30000000, 29925000)
            app.source, app.relative = Var('USB da bancada'), Var(False)
            app.ax = Mock(); app.line = Mock(); app.canvas = Mock()
            app.values = [Var() for _ in range(4)]
            app.timing_text, app.summary, app.link_text = Var(), Var(), Var()
            app.draw()
            self.assertEqual(app.values[2].get(), '30.00')
            self.assertIn('29.925', app.timing_text.get())
            app.source.set('LoRa da estação')
            app.draw()
            self.assertEqual(app.values[2].get(), '30.00')
            self.assertIn('nenhuma', app.timing_text.get())


if __name__ == '__main__':
    unittest.main()
