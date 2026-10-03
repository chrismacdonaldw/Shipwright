import importlib.util
import io
import os
from pathlib import Path
import subprocess
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location("check_format", Path(__file__).with_name("check_format.py"))
check = importlib.util.module_from_spec(spec)
spec.loader.exec_module(check)


class SelectionTests(unittest.TestCase):
    def test_soh_native_source_and_private_headers(self):
        self.assertTrue(check.eligible("soh/soh/SaveManager.cpp", "soh"))
        self.assertTrue(check.eligible("soh/soh/SaveFile.h", "soh"))
        self.assertTrue(check.eligible("soh/src/code/file.c", "soh"))
        self.assertFalse(check.eligible("soh/include/z64.h", "soh"))
        self.assertFalse(check.eligible("soh/assets/object.h", "soh"))

    def test_mm_native_extensions_match_existing_script(self):
        self.assertTrue(check.eligible("mm/2s2h/SaveManager/SaveFile.h", "mm"))
        self.assertTrue(check.eligible("mm/src/code/file.c", "mm"))
        self.assertFalse(check.eligible("mm/src/code/file.h", "mm"))
        self.assertFalse(check.eligible("mm/2s2h/header.hpp", "mm"))

    def test_docs_dependencies_and_other_port_are_excluded(self):
        for path in ["docs/BUILDING.md", "libultraship/src/file.cpp", ".github/scripts/check_format.py", "mm/file.c"]:
            self.assertFalse(check.eligible(path, "soh"))

    def invoke(self, paths, output="", returncode=0):
        with patch.dict(os.environ, {"BASE_SHA": "1" * 40, "HEAD_SHA": "2" * 40,
                                     "GITHUB_REPOSITORY": "chrismacdonaldw/Shipwright"}), patch.object(check.subprocess, "check_output", side_effect=["1" * 40, paths]), patch.object(check.subprocess, "run", return_value=subprocess.CompletedProcess([], returncode, output, "")) as run, patch("sys.stdout", new=io.StringIO()):
            result = check.main()
        return result, run

    def test_docs_only_does_not_invoke_formatter(self):
        result, run = self.invoke(b"docs/BUILDING.md\0")
        self.assertEqual(result, 0)
        run.assert_not_called()

    def test_format_difference_fails_with_exact_commit_arguments(self):
        result, run = self.invoke(b"soh/soh/SaveManager.cpp\0", "diff --git a/file b/file\n")
        self.assertEqual(result, 1)
        args = run.call_args.args[0]
        self.assertEqual(args[-4:], ["1" * 40, "2" * 40, "--", "soh/soh/SaveManager.cpp"])

    def test_tool_failure_is_not_a_pass(self):
        result, _ = self.invoke(b"soh/soh/SaveManager.cpp\0", returncode=2)
        self.assertEqual(result, 2)


if __name__ == "__main__":
    unittest.main()
