#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later

from __future__ import annotations

import struct
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import generate_aard_cancellation_fixture as fixture


class GenerateAardCancellationFixtureTest(unittest.TestCase):
    def test_generates_deterministic_bounded_disposable_fixture(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            first = Path(temporary) / "first"
            second = Path(temporary) / "second"
            first_path = fixture.generate(first, 3)
            second_path = fixture.generate(second, 3)
            content = first_path.read_bytes()
            self.assertEqual(content, second_path.read_bytes())
            self.assertEqual(b"aard", content[:4])
            self.assertEqual(3, struct.unpack(">I", content[70:74])[0])
            self.assertLess(len(content), fixture.MAX_FIXTURE_BYTES)
            self.assertEqual(
                fixture.MARKER_CONTENT,
                (first / fixture.DISPOSABLE_MARKER).read_text(encoding="utf-8"),
            )

    def test_rejects_unsafe_output_and_entry_counts(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "fixture"
            output.mkdir()
            (output / "owned.txt").write_text("preserve", encoding="utf-8")
            with self.assertRaisesRegex(fixture.FixtureError, "absent or empty"):
                fixture.generate(output, 1)
            self.assertEqual(
                "preserve", (output / "owned.txt").read_text(encoding="utf-8")
            )
            for count in (0, fixture.MAX_ENTRY_COUNT + 1):
                with self.subTest(count=count), self.assertRaisesRegex(
                    fixture.FixtureError, "entry count"
                ):
                    fixture._fixture_bytes(count)


if __name__ == "__main__":
    unittest.main()
