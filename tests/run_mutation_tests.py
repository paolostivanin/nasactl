#!/usr/bin/env python3
"""Prove regression tests detect selected transport and DHW defects."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
MUTATIONS = [
    ('foreign ACK accepted', 'nasa_client.cpp',
     'packet.source != front.packet.destination', 'false'),
    ('NACK reported as success', 'nasa_client.cpp',
     'WriteOutcome::Acknowledged : WriteOutcome::Rejected',
     'WriteOutcome::Acknowledged : WriteOutcome::Acknowledged'),
    ('partial read batch stranded', 'nasa_queue.h',
     'queue_.empty() && pending_.empty()', 'queue_.empty()'),
    ('read waits for ACK', 'nasa_client.cpp',
     'Packet::create_read(dest, message_numbers), false, queue_lifetime',
     'Packet::create_read(dest, message_numbers), true, queue_lifetime'),
    ('DHW target scaling omitted', 'nasactl_number.h',
     'if (divisor_ != 1.0f) raw *= divisor_;',
     'if (divisor_ != 1.0f) raw *= 1.0f;'),
]

# Failures count only after successful compilation and an explicit assertion.
# A compiler failure or sanitizer startup error is not evidence of detection.
for name, filename, original, replacement in MUTATIONS:
    with tempfile.TemporaryDirectory(prefix='nasactl-mutation-') as directory:
        sources = Path(directory) / 'nasactl'
        shutil.copytree(ROOT / 'components' / 'nasactl', sources)
        path = sources / filename
        text = path.read_text()
        assert text.count(original) == 1, (filename, original)
        path.write_text(text.replace(original, replacement))
        result = subprocess.run(['python3', str(ROOT / 'tests' / 'run_host_tests.py')],
                                env={**os.environ, 'NASACTL_TEST_SOURCES': str(sources)},
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        assert result.returncode != 0 and 'Assertion' in result.stdout, result.stdout
        print('Regression detected: ' + name, flush=True)
