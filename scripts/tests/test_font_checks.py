import base64
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "test/scripts"))
import check_fonts


class FontChecksTest(unittest.TestCase):
    def test_each_embedding_keeps_its_family_without_duplicate_orphans(self):
        payload = base64.b64encode(b"font bytes").decode()
        url = f"data:font/ttf;base64,{payload}"
        html = (f"<style>@font-face{{font-family:'A';src:url({url});}}"
                f"@font-face{{font-family:'B';src:url({url});}}</style>"
                f"<p>{url}</p>")
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "fonts.html"
            path.write_text(html)
            self.assertEqual(list(check_fonts.fonts_in(path)), [
                ("A", b"font bytes"), ("B", b"font bytes"),
                ("(no @font-face)", b"font bytes"),
            ])

    def test_empty_output_is_not_a_successful_check(self):
        with tempfile.TemporaryDirectory() as directory:
            with patch.object(sys, "argv", ["check_fonts.py", directory]), \
                 patch.object(check_fonts, "find_ots", return_value=[]):
                with self.assertRaisesRegex(SystemExit, "No HTML files"):
                    check_fonts.main()
