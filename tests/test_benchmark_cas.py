from pathlib import Path
import tempfile
import unittest

from benchmark_cas import prepare_rom


class BenchmarkROMTests(unittest.TestCase):
    def test_app_chain_removed_from_copy(self):
        # Two apps, including a language pack which OS Reset Apps preserves.
        data = bytearray(b"\xff" * 0x400000)
        data[:16] = b"OS must survive!"
        end = 0x3B0000
        for size in (0xC712, 0x2000):
            start = end - 3 - size
            data[start:end - 3] = b"\x81\x0f" + b"\x55" * (size - 2)
            data[end - 3:end] = size.to_bytes(3, "little")
            end = start
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "source.rom"
            destination = Path(directory) / "copy.rom"
            source.write_bytes(data)
            removed = prepare_rom(source, destination)
            self.assertEqual(len(removed), 2)
            self.assertEqual(source.read_bytes(), data)
            expected = data[:end] + b"\xff" * (0x3B0000 - end) + data[0x3B0000:]
            self.assertEqual(destination.read_bytes(), expected)

    def test_empty_app_chain_preserved(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "source.rom"
            destination = Path(directory) / "copy.rom"
            data = b"\xff" * 0x400000
            source.write_bytes(data)
            self.assertEqual(prepare_rom(source, destination), [])
            self.assertEqual(destination.read_bytes(), data)

    def test_original_rom_cannot_be_overwritten(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "source.rom"
            data = b"\xff" * 0x400000
            source.write_bytes(data)
            with self.assertRaises(ValueError):
                prepare_rom(source, source)
            self.assertEqual(source.read_bytes(), data)


if __name__ == "__main__":
    unittest.main()
