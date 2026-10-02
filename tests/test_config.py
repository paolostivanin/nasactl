#!/usr/bin/env python3
"""Validate defaults and every catalog entity using the installed ESPHome schema."""
import sys
from pathlib import Path
import unittest

from esphome.core import CORE
from esphome.config import read_config
from esphome import config_validation as cv

ROOT = Path(__file__).resolve().parents[1]
CORE.config_path = ROOT / 'tests' / 'compile.yaml'
assert read_config({}) is not None
from esphome.components import nasactl as component


class ConfigurationTests(unittest.TestCase):
    def test_defaults(self):
        config = component.CONFIG_SCHEMA({'devices': [
            {'address': '20.00.AA', 'type': 'ac', 'climate': {'name': 'AC'}}]})
        for key in ('debug_log_packets', 'packet_information', 'standard_command_header'):
            self.assertIs(config[key], False)
        device = config['devices'][0]
        self.assertEqual(device['address'], '20.00.aa')
        self.assertEqual(device['control_data_type'], 'write')
        self.assertIs(device['targeted_reads'], False)
        self.assertIs(device['climate']['batch_writes'], False)
        self.assertEqual(config['send_timeout'], 4000)
        self.assertEqual(config['retry_interval'], 500)

    def test_catalog(self):
        for key, definition in component.ENTITIES.items():
            with self.subTest(entity=key):
                kind = 'outdoor' if component._is_outdoor_code(definition['code']) else 'hydro'
                config = component.DEVICE_SCHEMA({'address': '20.00.00', 'type': kind,
                                                  key: {'name': 'Schema Test ' + key}})
                self.assertIn(key, config)

    def test_rejections(self):
        cases = [
            {'devices': [{'address': 'bad', 'type': 'hydro'}]},
            {'devices': [{'address': '20.00.00', 'type': 'hydro', 'climate': {'name': 'AC'}}]},
            {'devices': [{'address': '20.00.00', 'type': 'ac', 'control_data_type': 'notification'}]},
            {'devices': [{'address': '20.00.00', 'type': 'ac', 'climate': {'name': 'AC'},
                          'power': {'name': 'Power'}}]},
            {'devices': [{'address': '20.00.AA', 'type': 'ac'},
                         {'address': '20.00.aa', 'type': 'ac'}]},
            {'devices': [{'address': '10.00.00', 'type': 'outdoor',
                          'water_target_temperature': {'name': 'DHW'}}]},
            {'devices': [{'address': '20.00.00', 'type': 'hydro',
                          'outdoor_temperature': {'name': 'Outdoor'}}]},
            {'devices': [{'address': '20.00.00', 'type': 'hydro',
                          'custom_sensor': [{'name': 'Invalid', 'message': 0x4237, 'divisor': 0}]}]},
        ]
        for case in cases:
            with self.subTest(config=case), self.assertRaises(cv.Invalid):
                component.CONFIG_SCHEMA(case)

    def test_diagnostics(self):
        config = component.CONFIG_SCHEMA({'devices': [], **{
            key: {'name': 'Schema Test ' + key} for key in component.TX_COUNTERS}})
        for key in component.TX_COUNTERS:
            self.assertEqual(config[key]['accuracy_decimals'], 0)
            self.assertEqual(config[key]['state_class'], 'total_increasing')
            self.assertEqual(config[key]['entity_category'], 'diagnostic')


if __name__ == '__main__':
    unittest.main()
