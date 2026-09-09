"""Keep the versioned Dictd smoke fixture's byte-addressed ranges intact."""

from pathlib import Path
import unittest


class DictdSmokeFixtureTest(unittest.TestCase):
    def test_index_ranges_address_canonical_payload(self):
        root = Path(__file__).resolve().parents[2]
        fixture = root / "apps/goldendict/tests/fixtures/dictd/fixture"
        data = fixture.with_suffix(".dict").read_bytes()
        self.assertEqual(data, b"Fixture Dictionary\nA fruit.A program.\n")
        expected = {
            b"00databaseshort": b"Fixture Dictionary\n",
            b"apple": b"A fruit.",
            b"application": b"A program.",
        }
        digits = b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"

        def decode(field):
            value = 0
            for byte in field:
                value = value * 64 + digits.index(byte)
            return value

        rows = fixture.with_suffix(".index").read_bytes().splitlines()
        self.assertEqual(len(rows), len(expected))
        for row in rows:
            word, offset, size = row.split(b"\t")
            start = decode(offset)
            self.assertEqual(data[start:start + decode(size)], expected[word])


if __name__ == "__main__":
    unittest.main()
