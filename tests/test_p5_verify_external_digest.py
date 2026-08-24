import hashlib
import importlib.util
import json
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "p5_verify_external_digest", ROOT / "scripts" / "p5_verify_external_digest.py"
)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)


class P5ExternalDigestTests(unittest.TestCase):
    def test_match(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "evidence-manifest.json"
            path.write_bytes(b'{"status":"PASS"}\n')
            expected = hashlib.sha256(path.read_bytes()).hexdigest()
            code, report = MODULE.verify(path, expected.upper())
            self.assertEqual(code, 0)
            self.assertEqual(report["status"], "match")

    def test_mismatch_is_distinct_from_missing(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "manifest.json"
            path.write_bytes(b"payload")
            code, report = MODULE.verify(path, "0" * 64)
            self.assertEqual(code, 2)
            self.assertEqual(report["status"], "mismatch")
            missing_code, missing = MODULE.verify(path.with_name("missing.json"), "0" * 64)
            self.assertEqual(missing_code, 3)
            self.assertEqual(missing["status"], "missing")

    def test_invalid_digest_is_rejected_before_read(self):
        code, report = MODULE.verify(Path("does-not-matter"), "not-a-digest")
        self.assertEqual(code, 3)
        self.assertEqual(report["status"], "invalid-expected-digest")

    def test_cli_writes_json(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "manifest.json"
            output = Path(temp_dir) / "digest-check.json"
            path.write_bytes(b"payload")
            expected = hashlib.sha256(path.read_bytes()).hexdigest()
            code = MODULE.main(["--file", str(path), "--expected-sha256", expected, "--output", str(output)])
            self.assertEqual(code, 0)
            self.assertEqual(json.loads(output.read_text(encoding="utf-8"))["status"], "match")


if __name__ == "__main__":
    unittest.main()
