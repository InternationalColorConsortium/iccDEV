#!/usr/bin/env python3
"""Check the packaged Doxygen navigation and local HTML links."""

from __future__ import annotations

import re
import sys
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import unquote, urlsplit


EXPECTED_ROOT_PAGES = [
    "curateddocumentation.html",
    "curatedlibraries.html",
    "curatedtools.html",
    "curatedexamples.html",
    "curatedmaintainerqa.html",
]
ROOT_ROW = re.compile(r"row_[0-9]+_")


class RelatedPagesParser(HTMLParser):
    def __init__(self) -> None:
        super().__init__()
        self.root_pages: list[str] = []
        self.in_root_row = False

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        values = dict(attrs)
        if tag == "tr":
            self.in_root_row = bool(ROOT_ROW.fullmatch(values.get("id") or ""))
        elif tag == "a" and self.in_root_row and values.get("href"):
            self.root_pages.append(values["href"])
            self.in_root_row = False

    def handle_endtag(self, tag: str) -> None:
        if tag == "tr":
            self.in_root_row = False


class LocalLinksParser(HTMLParser):
    def __init__(self) -> None:
        super().__init__()
        self.links: list[str] = []

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        for name, value in attrs:
            if name in ("href", "src") and value:
                self.links.append(value)


def check(html_dir: Path) -> int:
    html_dir = html_dir.resolve()
    pages_path = html_dir / "pages.html"
    if not pages_path.is_file():
        print(f"missing Doxygen page index: {pages_path}", file=sys.stderr)
        return 1

    index = RelatedPagesParser()
    index.feed(pages_path.read_text(encoding="utf-8", errors="replace"))
    if index.root_pages != EXPECTED_ROOT_PAGES:
        print(f"unexpected root pages: {index.root_pages}", file=sys.stderr)
        print(f"expected: {EXPECTED_ROOT_PAGES}", file=sys.stderr)
        return 1

    broken: list[tuple[Path, str]] = []
    checked = 0
    for page in html_dir.rglob("*.html"):
        links = LocalLinksParser()
        links.feed(page.read_text(encoding="utf-8", errors="replace"))
        for link in links.links:
            parsed = urlsplit(link)
            if parsed.scheme or parsed.netloc or not parsed.path:
                continue
            checked += 1
            target = (page.parent / unquote(parsed.path)).resolve()
            if not target.is_relative_to(html_dir) or not target.is_file():
                broken.append((page.relative_to(html_dir), link))

    if broken:
        for page, link in broken[:30]:
            print(f"broken packaged link: {page}: {link}", file=sys.stderr)
        print(f"{len(broken)} broken packaged links", file=sys.stderr)
        return 1

    print(f"Doxygen HTML QA passed: {len(index.root_pages)} root pages, "
          f"{checked} packaged links checked")
    return 0


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("usage: check-doxygen-html.py HTML_DIR", file=sys.stderr)
        sys.exit(2)
    sys.exit(check(Path(sys.argv[1])))
