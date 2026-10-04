#!/usr/bin/env python3
"""Generates the month and day names in `internal/number_format`.

LibreOffice is the reference for what a date style shows, so the names are
the ones it renders: a sheet of probe cells, one data style per language and
shape, goes through `soffice --convert-to csv`, and the csv holds the names.
A language that LibreOffice renders in English is left out.

The languages are the ones `number_format::symbols_of` knows.

Regenerate with:

    python3 tools/number_format/generate_calendar_names.py [path/to/soffice]
"""

from __future__ import annotations

import csv
import pathlib
import subprocess
import sys
import tempfile

SOFFICE = "/Applications/LibreOffice.app/Contents/MacOS/soffice"

# (language, the region LibreOffice renders it for)
LANGUAGES = [
    ("en", "US"),
    ("de", "DE"),
    ("es", "ES"),
    ("it", "IT"),
    ("nl", "NL"),
    ("pt", "PT"),
    ("id", "ID"),
    ("tr", "TR"),
    ("da", "DK"),
    ("el", "GR"),
    ("ro", "RO"),
    ("sl", "SI"),
    ("hr", "HR"),
    ("sr", "RS"),
    ("bs", "BA"),
    ("vi", "VN"),
    ("ca", "ES"),
    ("fr", "FR"),
    ("ru", "RU"),
    ("pl", "PL"),
    ("cs", "CZ"),
    ("sk", "SK"),
    ("sv", "SE"),
    ("fi", "FI"),
    ("nb", "NO"),
    ("nn", "NO"),
    ("uk", "UA"),
    ("hu", "HU"),
    ("bg", "BG"),
    ("lt", "LT"),
    ("lv", "LV"),
    ("et", "EE"),
    ("be", "BY"),
    ("kk", "KZ"),
]

MONTH = '<number:month number:textual="true" number:style="long"/>'
SHORT_MONTH = '<number:month number:textual="true"/>'
DAY = '<number:day number:style="long"/><number:text> </number:text>'
WEEKDAY = '<number:day-of-week number:style="long"/>'
SHORT_WEEKDAY = "<number:day-of-week/>"

# (field, data style, dates, prefix the cell shows ahead of the name)
MONTH_DATES = [f"2025-{month:02d}-15" for month in range(1, 13)]
DAY_DATES = [f"2025-01-{day:02d}" for day in range(5, 12)]  # Sunday first
FIELDS = [
    ("months", MONTH, MONTH_DATES, ""),
    ("months_after_day", DAY + MONTH, MONTH_DATES, "15 "),
    ("short_months", SHORT_MONTH, MONTH_DATES, ""),
    ("days", WEEKDAY, DAY_DATES, ""),
    ("short_days", SHORT_WEEKDAY, DAY_DATES, ""),
]

NAMESPACES = " ".join(
    f'xmlns:{prefix}="urn:oasis:names:tc:opendocument:xmlns:{name}:1.0"'
    for prefix, name in [
        ("office", "office"),
        ("style", "style"),
        ("text", "text"),
        ("table", "table"),
        ("number", "datastyle"),
    ]
)


def probe_sheet() -> str:
    data_styles, cell_styles, rows = [], [], []
    for language, region in LANGUAGES:
        for field, shape, dates, _ in FIELDS:
            name = f"{language}_{field}"
            data_styles.append(
                f'<number:date-style style:name="N_{name}" '
                f'number:language="{language}" number:country="{region}">'
                f"{shape}</number:date-style>"
            )
            cell_styles.append(
                f'<style:style style:name="C_{name}" style:family="table-cell" '
                f'style:data-style-name="N_{name}"/>'
            )
            cells = "".join(
                f'<table:table-cell table:style-name="C_{name}" '
                f'office:value-type="date" office:date-value="{date}"/>'
                for date in dates
            )
            rows.append(f"<table:table-row>{cells}</table:table-row>")
    return (
        '<?xml version="1.0" encoding="UTF-8"?>'
        f'<office:document {NAMESPACES} office:version="1.3" '
        'office:mimetype="application/vnd.oasis.opendocument.spreadsheet">'
        f'<office:styles>{"".join(data_styles)}</office:styles>'
        f'<office:automatic-styles>{"".join(cell_styles)}</office:automatic-styles>'
        '<office:body><office:spreadsheet><table:table table:name="probe">'
        f'{"".join(rows)}</table:table></office:spreadsheet></office:body>'
        "</office:document>"
    )


def rendered(soffice: str) -> list[list[str]]:
    with tempfile.TemporaryDirectory() as directory:
        sheet = pathlib.Path(directory) / "probe.fods"
        sheet.write_text(probe_sheet(), encoding="utf-8")
        subprocess.run(
            [
                soffice,
                "--headless",
                "--convert-to",
                "csv:Text - txt - csv (StarCalc):44,34,76,1,,0,false,true,true",
                "--outdir",
                directory,
                str(sheet),
            ],
            check=True,
            capture_output=True,
        )
        with open(pathlib.Path(directory) / "probe.csv", encoding="utf-8") as file:
            return list(csv.reader(file))


def names_by_language(rows: list[list[str]]) -> dict[str, dict[str, list[str]]]:
    result = {}
    row = iter(rows)
    for language, _ in LANGUAGES:
        names = {}
        for field, _, dates, prefix in FIELDS:
            cells = next(row)[: len(dates)]
            if not all(cell.startswith(prefix) for cell in cells):
                sys.exit(f"{language} {field}: no prefix {prefix!r} in {cells}")
            names[field] = [cell[len(prefix) :] for cell in cells]
        result[language] = names
    english = result["en"]
    return {
        language: names
        for language, names in result.items()
        if language == "en" or names != english
    }


def quoted(names: list[str]) -> str:
    return ", ".join('"' + name.replace('"', '\\"') + '"' for name in names)


def write(languages: dict[str, dict[str, list[str]]], root: pathlib.Path) -> None:
    note = (
        "// Generated by tools/number_format/generate_calendar_names.py; do not edit.\n"
        "// Regenerate with: python3 tools/number_format/generate_calendar_names.py\n"
        "// Data: the names LibreOffice renders for each language.\n\n"
        "// clang-format off\n\n"
    )
    (root / "calendar_names.hpp").write_text(
        note
        + "#pragma once\n\n"
        + "#include <array>\n#include <string_view>\n\n"
        + "namespace odr::internal::number_format {\n\n"
        + "/// The names a date shows in one language.\n"
        + "struct CalendarNames final {\n"
        + "  std::string_view language;\n"
        + "  std::array<std::string_view, 12> months;\n"
        + "  /// A month after a day, where the language declines it.\n"
        + "  std::array<std::string_view, 12> months_after_day;\n"
        + "  std::array<std::string_view, 12> short_months;\n"
        + "  /// Sunday first.\n"
        + "  std::array<std::string_view, 7> days;\n"
        + "  std::array<std::string_view, 7> short_days;\n"
        + "};\n\n"
        + "/// English first.\n"
        + f"extern const std::array<CalendarNames, {len(languages)}> calendar_names;\n\n"
        + "} // namespace odr::internal::number_format\n",
        encoding="utf-8",
    )
    entries = "".join(
        f'    {{"{language}",\n'
        + "".join(
            f"     {{{quoted(names[field])}}},\n" for field, *_ in FIELDS
        ).rstrip(",\n")
        + "},\n"
        for language, names in languages.items()
    )
    (root / "calendar_names.cpp").write_text(
        note
        + "#include <odr/internal/number_format/calendar_names.hpp>\n\n"
        + "namespace odr::internal::number_format {\n\n"
        + f"const std::array<CalendarNames, {len(languages)}> calendar_names{{{{\n"
        + entries
        + "}};\n\n"
        + "} // namespace odr::internal::number_format\n",
        encoding="utf-8",
    )


def main() -> None:
    soffice = sys.argv[1] if len(sys.argv) > 1 else SOFFICE
    root = (
        pathlib.Path(__file__).resolve().parents[2]
        / "src/odr/internal/number_format"
    )
    write(names_by_language(rendered(soffice)), root)


if __name__ == "__main__":
    main()
