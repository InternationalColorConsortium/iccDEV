#!/usr/bin/env python3
# Copyright (c) International Color Consortium.
# SPDX-License-Identifier: BSD-3-Clause

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
EXPECTED_CHILDREN = {
    "curateddocumentation.html": [
        "md_docs_2using-iccdev.html",
        "md_docs_2install.html",
        "md_docs_2build.html",
    ],
    "curatedlibraries.html": [
        "curatedproflib.html",
        "curatedxmllib.html",
        "curatedjsonlib.html",
        "curatedconnectlib.html",
    ],
    "curatedxmllib.html": ["md_docs_2iccxml.html"],
    "curatedtools.html": [
        "md_docs_2tools-cli-reference.html",
        "curatedapplytools.html",
        "curatedprofiletools.html",
        "curatedimagetools.html",
        "curatedwindowstools.html",
    ],
    "curatedexamples.html": [
        "md_examples_2hello-iccdev_2README.html",
        "md_examples_2c-validation-dlopen_2README.html",
        "md_examples_2ios-apply-preview_2README.html",
        "md_examples_2ios-clut-editor_2README.html",
        "md_examples_2ios-benchapply_2README.html",
    ],
    "curatedmaintainerqa.html": [
        "curatedmaintainerworkflows.html",
        "curatedmaintainertesting.html",
        "curatedmaintainerreports.html",
    ],
}
PAGE_ROW = re.compile(r"row_((?:[0-9]+_)+)")


class RelatedPagesParser(HTMLParser):
    def __init__(self) -> None:
        super().__init__()
        self.root_pages: list[str] = []
        self.page_paths: dict[str, tuple[int, ...]] = {}
        self.row_path: tuple[int, ...] | None = None

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        values = dict(attrs)
        if tag == "tr":
            match = PAGE_ROW.fullmatch(values.get("id") or "")
            self.row_path = None
            if match:
                self.row_path = tuple(
                    int(part) for part in match.group(1).rstrip("_").split("_")
                )
        elif tag == "a" and self.row_path is not None and values.get("href"):
            href = values["href"]
            self.page_paths[href] = self.row_path
            if len(self.row_path) == 1:
                self.root_pages.append(href)
            self.row_path = None

    def handle_endtag(self, tag: str) -> None:
        if tag == "tr":
            self.row_path = None


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

    ordered_pages = sorted(index.page_paths.items(), key=lambda item: item[1])
    for parent, expected in EXPECTED_CHILDREN.items():
        parent_path = index.page_paths.get(parent)
        if parent_path is None:
            print(f"missing curated parent: {parent}", file=sys.stderr)
            return 1
        children = [
            page for page, path in ordered_pages if path[:-1] == parent_path
        ]
        if children != expected:
            print(f"unexpected children for {parent}: {children}", file=sys.stderr)
            print(f"expected: {expected}", file=sys.stderr)
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

    curated_links = sum(len(children) for children in EXPECTED_CHILDREN.values())
    print(f"Doxygen HTML QA passed: {len(index.root_pages)} root pages, "
          f"{curated_links} curated child links, {checked} packaged links checked")
    return 0


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("usage: check-doxygen-html.py HTML_DIR", file=sys.stderr)
        sys.exit(2)
    sys.exit(check(Path(sys.argv[1])))
