"""Проверки транслятора через командную строку."""

import json
from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
PROGRAM = ROOT / "build" / "config15"


class ConfigTests(unittest.TestCase):
    def run_cli(self, *args):
        return subprocess.run(
            [PROGRAM, *args],
            capture_output=True,
            encoding="utf-8",
            check=False,
        )

    def translate(self, text):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "config with spaces.cfg"
            path.write_text(text, encoding="utf-8")
            return self.run_cli("--input", path)

    def assert_json(self, text, expected):
        self.assert_output(self.translate(text), expected)

    def assert_output(self, result, expected):
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stderr, "")
        self.assertEqual(json.loads(result.stdout), expected)

    def assert_error(self, text, message):
        result = self.translate(text)
        self.assertEqual(result.returncode, 1)
        self.assertEqual(result.stdout, "")
        self.assertRegex(result.stderr, r"^Ошибка: \d+:\d+: ")
        self.assertIn(message, result.stderr)
        return result

    def test_numbers_and_strings(self):
        self.assert_json(
            "A := .25; B := 0.125; C := 'Текст'; D := '';",
            {"A": 0.25, "B": 0.125, "C": "Текст", "D": ""},
        )

    def test_json_escaping(self):
        text = """ESC := 'кавычки " и слеш \\ и табуляция\t';"""
        expected = 'кавычки " и слеш \\ и табуляция\t'
        self.assert_json(text, {"ESC": expected})

    def test_empty_config(self):
        self.assert_json(" \n\t", {})

    def test_empty_array_and_nested_values(self):
        self.assert_json(
            "A := { {}. { 'ряд'. .5. ![2 3 +]. }. 4.0. };",
            {"A": [[], ["ряд", 0.5, 5], 4]},
        )

    def test_standalone_values(self):
        cases = [
            (".25", 0.25),
            ("'текст'", "текст"),
            ("{ .5. }", [0.5]),
            ("![2 3 +]", 5),
            ("BASE := 1.5; ![BASE 2 *]", 3),
        ]
        for text, expected in cases:
            with self.subTest(text=text):
                self.assert_json(text, expected)

    def test_constant_references_and_operation_order(self):
        self.assert_json(
            "BASE := 2.5; NEXT := ![BASE 1 + 2 * 9 -];",
            {"BASE": 2.5, "NEXT": -2},
        )

    def test_nested_expressions(self):
        self.assert_json("![![2 3 +] ![7 4 -] *]", 15)

    def test_sort_does_not_change_the_constant(self):
        self.assert_json(
            "RAW := { 10.0. 2.0. .5. 2.0. }; SORTED := ![RAW sort()];",
            {"RAW": [10, 2, 0.5, 2], "SORTED": [0.5, 2, 2, 10]},
        )

    def test_sort_strings_and_empty_array(self):
        self.assert_json(
            "WORDS := ![{ 'z'. 'a'. 'b'. } sort()]; EMPTY := ![{} sort()];",
            {"WORDS": ["a", "b", "z"], "EMPTY": []},
        )

    def test_missing_semicolon(self):
        self.assert_error("A := 1.0", "syntax error")

    def test_missing_array_dot(self):
        result = self.assert_error("A := {\n  1.0\n};", "syntax error")
        self.assertIn("3:1:", result.stderr)

    def test_missing_closing_delimiters(self):
        for text in ("{ 1.0.", "![1 2 +"):
            with self.subTest(text=text):
                self.assert_error(text, "syntax error")

    def test_bad_name_or_number(self):
        for text, reason in [
            ("abc := 1.0;", "недопустимый символ"),
            ("A1 := 1.0;", "syntax error"),
            ("A := 1;", "syntax error"),
            ("A := -1.0;", "syntax error"),
            ("A := 1.;", "syntax error"),
            ("A := 1.0e2;", "недопустимый символ"),
        ]:
            with self.subTest(text=text):
                self.assert_error(text, reason)

    def test_unclosed_string(self):
        self.assert_error("A := 'строка", "незакрытая строка")

    def test_undefined_and_forward_references(self):
        self.assert_error("A := ![Z];", "неизвестная константа Z")
        self.assert_error("A := ![B]; B := 1.0;", "неизвестная константа B")

    def test_duplicate_declaration(self):
        self.assert_error("A := 1.0; A := 2.0;", "повторное объявление A")

    def test_stack_errors(self):
        self.assert_error("![1 +]", "два операнда")
        self.assert_error("![1 2]", "одно значение")
        self.assert_error("![]", "одно значение")
        self.assert_error("![sort()]", "нужен операнд")

    def test_type_errors(self):
        self.assert_error("!['a' 1 +]", "ожидалось число")
        self.assert_error("![1 sort()]", "ожидает массив")
        self.assert_error("![{ .5. 's'. } sort()]", "ожидает числа или строки")
        self.assert_error("![{ {}. } sort()]", "ожидает числа или строки")

    def test_numeric_range_errors(self):
        self.assert_error("9" * 309 + ".0", "число вне допустимого диапазона")
        number = "9" * 160 + ".0"
        self.assert_error(
            f"![{number} {number} *]", "результат вне допустимого диапазона"
        )

    def test_file_error_and_cli_options(self):
        with tempfile.TemporaryDirectory() as directory:
            missing = Path(directory) / "missing.cfg"
            result = self.run_cli("-i", missing)
            self.assertIn("не удалось открыть файл", result.stderr)
            self.assertEqual(result.stdout, "")
            self.assertEqual(result.returncode, 1)
        self.assertEqual(self.run_cli("--input").returncode, 2)
        self.assertEqual(self.run_cli("--help").returncode, 0)

    def test_example_configurations(self):
        cases = {
            "audio.cfg": {
                "TEMPO": 148,
                "SWING": 0.55,
                "DOUBLETEMPO": 296,
                "TRACKS": ["kick", "bass", "lead"],
            },
            "greenhouse.cfg": {
                "TARGET": 21.5,
                "DELTA": 1.5,
                "LOWEST": 20,
                "HIGHEST": 23,
                "ZONES": [["рассада", 20], ["томаты", 23]],
            },
            "delivery.cfg": {
                "WAREHOUSE": "Северный склад",
                "FEES": [320, 190, 250],
                "ROUTEFEES": [190, 250, 320],
                "ROUTES": ["A", "B", "C"],
            },
        }
        for file, expected in cases.items():
            with self.subTest(file=file):
                result = self.run_cli("-i", ROOT / "examples" / file)
                self.assert_output(result, expected)


if __name__ == "__main__":
    unittest.main()
