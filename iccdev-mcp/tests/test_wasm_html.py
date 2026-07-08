"""
Browser-based validation of WASM HTML pages using Playwright.

Tests HTML structure, JavaScript correctness, accessibility basics,
and interactive element functionality for all 15 WASM tool pages.
"""

import http.server
import re
import threading
from pathlib import Path

import pytest

# Skip entire module if playwright is not installed
pw = pytest.importorskip("playwright")
from playwright.sync_api import sync_playwright  # noqa: E402

WASM_DIR = Path(__file__).resolve().parent.parent.parent / "wasm"
PAGES = sorted(p.name for p in WASM_DIR.glob("*.html"))


class LocalServer:
    """Serve wasm/ directory on a random port for testing."""

    def __init__(self, directory: str, port: int = 0):
        self.directory = directory
        handler = http.server.SimpleHTTPRequestHandler
        self.httpd = http.server.HTTPServer(
            ("127.0.0.1", port),
            lambda *a, **kw: handler(*a, directory=directory, **kw),
        )
        self.port = self.httpd.server_address[1]
        self.thread = threading.Thread(target=self.httpd.serve_forever, daemon=True)

    def start(self):
        self.thread.start()
        return self

    def stop(self):
        self.httpd.shutdown()

    @property
    def base_url(self):
        return f"http://127.0.0.1:{self.port}"


@pytest.fixture(scope="module")
def server():
    srv = LocalServer(str(WASM_DIR)).start()
    yield srv
    srv.stop()


@pytest.fixture(scope="module")
def browser_ctx():
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        ctx = browser.new_context()
        yield ctx
        browser.close()


@pytest.fixture(scope="module")
def html_files():
    return sorted(WASM_DIR.glob("*.html"))


# ---- Structure Tests ----

def test_all_pages_present():
    """Verify expected WASM HTML pages exist."""
    expected = {
        "index.html", "dump.html", "roundtrip.html", "toxml.html",
        "fromxml.html", "fromcube.html", "apply.html", "link.html",
        "namedcmm.html", "search.html", "v5dsp.html", "specsep.html",
        "tiffdump.html", "jpegdump.html", "pngdump.html",
        "tojson.html", "fromjson.html",
    }
    actual = set(PAGES)
    missing = expected - actual
    assert not missing, f"Missing WASM pages: {missing}"


@pytest.mark.parametrize("page", PAGES)
def test_html_structure(page):
    """Validate basic HTML structure of each page."""
    content = (WASM_DIR / page).read_text()

    assert "<!DOCTYPE html>" in content, f"{page}: missing DOCTYPE"
    assert '<html lang="en">' in content, f"{page}: missing lang attr"
    assert '<meta charset="utf-8">' in content, f"{page}: missing charset"
    assert '<meta name="viewport"' in content, f"{page}: missing viewport"
    assert "<title>" in content, f"{page}: missing title"
    assert 'rel="stylesheet"' in content, f"{page}: missing stylesheet link"


@pytest.mark.parametrize("page", [p for p in PAGES if p != "index.html"])
def test_tool_page_elements(page):
    """Verify tool pages have required interactive elements."""
    content = (WASM_DIR / page).read_text()

    # Must have a run button
    assert 'id="run-btn"' in content, f"{page}: missing run button"
    # Must have output area
    assert 'id="output"' in content, f"{page}: missing output element"
    # Must have status indicator
    assert 'id="status"' in content, f"{page}: missing status element"
    # Must include app.js
    assert "app.js" in content, f"{page}: missing app.js include"
    # Must have breadcrumb back to index
    assert 'href="index.html"' in content, f"{page}: missing index link"


def test_index_links_to_all_tools():
    """Verify index.html links to every tool page."""
    index_content = (WASM_DIR / "index.html").read_text()
    tool_pages = [p for p in PAGES if p != "index.html"]

    for page in tool_pages:
        assert f'href="{page}"' in index_content, \
            f"index.html missing link to {page}"


# ---- JavaScript Tests ----

def test_app_js_syntax():
    """Verify app.js has no obvious syntax errors."""
    app_js = WASM_DIR / "app.js"
    assert app_js.exists(), "app.js not found"
    content = app_js.read_text()

    # Basic structure checks
    assert "class IccWasmApp" in content, "Missing IccWasmApp class"
    assert "constructor(" in content, "Missing constructor"
    assert "async init()" in content or "init()" in content, "Missing init method"
    assert "downloadOutput" in content, "Missing downloadOutput method"


@pytest.mark.parametrize("page", [p for p in PAGES if p != "index.html"])
def test_page_js_instantiation(page):
    """Verify each tool page properly instantiates IccWasmApp."""
    content = (WASM_DIR / page).read_text()
    assert "new IccWasmApp(" in content, \
        f"{page}: missing IccWasmApp instantiation"
    assert "app.init()" in content or ".init()" in content, \
        f"{page}: missing app.init() call"


# ---- Browser Rendering Tests ----

@pytest.mark.parametrize("page", PAGES)
def test_page_loads_no_errors(server, browser_ctx, page):
    """Load each page in headless Chromium and verify no JS errors."""
    errors = []
    pg = browser_ctx.new_page()
    pg.on("pageerror", lambda err: errors.append(str(err)))

    url = f"{server.base_url}/{page}"
    pg.goto(url, wait_until="domcontentloaded", timeout=10000)
    # Brief wait for any deferred JS
    pg.wait_for_timeout(500)

    # Filter out expected WASM loading errors (modules not available in test)
    real_errors = [
        e for e in errors
        if "wasm" not in e.lower()
        and "fetch" not in e.lower()
        and "module" not in e.lower()
        and "net::" not in e.lower()
    ]

    pg.close()
    assert not real_errors, \
        f"{page}: JS errors: {real_errors}"


@pytest.mark.parametrize("page", PAGES)
def test_page_title_set(server, browser_ctx, page):
    """Verify each page has a non-empty title when rendered."""
    pg = browser_ctx.new_page()
    pg.goto(f"{server.base_url}/{page}", wait_until="domcontentloaded",
            timeout=10000)
    title = pg.title()
    pg.close()
    assert title and len(title) > 3, f"{page}: empty or short title: {title!r}"


@pytest.mark.parametrize("page", [p for p in PAGES if p != "index.html"])
def test_tool_page_interactive_elements(server, browser_ctx, page):
    """Verify tool pages render all interactive elements."""
    pg = browser_ctx.new_page()
    pg.goto(f"{server.base_url}/{page}", wait_until="domcontentloaded",
            timeout=10000)

    # Run button exists and is visible
    run_btn = pg.query_selector("#run-btn")
    assert run_btn is not None, f"{page}: run button not found in DOM"
    assert run_btn.is_visible(), f"{page}: run button not visible"

    # Output area exists
    output = pg.query_selector("#output")
    assert output is not None, f"{page}: output element not found"

    # Status text exists
    status = pg.query_selector("#status")
    assert status is not None, f"{page}: status element not found"

    pg.close()


def test_index_tool_cards_clickable(server, browser_ctx):
    """Verify index page tool cards are clickable links."""
    pg = browser_ctx.new_page()
    pg.goto(f"{server.base_url}/index.html", wait_until="domcontentloaded",
            timeout=10000)

    cards = pg.query_selector_all(".tool-card")
    assert len(cards) >= 14, \
        f"Expected >= 14 tool cards, found {len(cards)}"

    # Each card should be a link
    for card in cards:
        href = card.get_attribute("href")
        assert href and href.endswith(".html"), \
            f"Tool card missing href: {card.inner_text()[:30]}"

    pg.close()


@pytest.mark.parametrize("page", [p for p in PAGES if p != "index.html"])
def test_drop_zone_file_input(server, browser_ctx, page):
    """Verify drop zones have file input elements."""
    pg = browser_ctx.new_page()
    pg.goto(f"{server.base_url}/{page}", wait_until="domcontentloaded",
            timeout=10000)

    # At least one file input should exist
    file_inputs = pg.query_selector_all('input[type="file"]')
    assert len(file_inputs) >= 1, \
        f"{page}: no file input found"

    pg.close()


# ---- Accessibility Basics ----

@pytest.mark.parametrize("page", PAGES)
def test_accessibility_basics(server, browser_ctx, page):
    """Check basic accessibility: lang, labels, heading hierarchy."""
    pg = browser_ctx.new_page()
    pg.goto(f"{server.base_url}/{page}", wait_until="domcontentloaded",
            timeout=10000)

    # Page has lang attribute
    lang = pg.evaluate("document.documentElement.lang")
    assert lang == "en", f"{page}: lang={lang!r}, expected 'en'"

    # Page has at least one h1
    h1s = pg.query_selector_all("h1")
    assert len(h1s) >= 1, f"{page}: no h1 heading"

    pg.close()


# ---- CSS Stylesheet ----

def test_stylesheet_exists():
    """Verify style.css exists and has content."""
    css = WASM_DIR / "style.css"
    assert css.exists(), "style.css not found"
    content = css.read_text()
    assert len(content) > 100, "style.css appears empty"
    # Check for key selectors
    assert ".container" in content, "Missing .container styles"
    assert ".tool-card" in content or ".tool-grid" in content, \
        "Missing tool card styles"


def test_css_loads(server, browser_ctx):
    """Verify CSS loads without errors."""
    pg = browser_ctx.new_page()
    css_errors = []
    pg.on("requestfailed", lambda req: css_errors.append(req.url)
          if "style.css" in req.url else None)
    pg.goto(f"{server.base_url}/index.html", wait_until="domcontentloaded",
            timeout=10000)
    pg.wait_for_timeout(500)
    pg.close()
    assert not css_errors, f"CSS failed to load: {css_errors}"


# ---- Cross-page Navigation ----

def test_breadcrumb_navigation(server, browser_ctx):
    """Test clicking breadcrumb navigates back to index."""
    pg = browser_ctx.new_page()
    pg.goto(f"{server.base_url}/dump.html", wait_until="domcontentloaded",
            timeout=10000)

    breadcrumb = pg.query_selector('.breadcrumb a[href="index.html"]')
    assert breadcrumb is not None, "Breadcrumb link to index not found"

    breadcrumb.click()
    pg.wait_for_load_state("domcontentloaded")
    assert "index.html" in pg.url or pg.url.endswith("/"), \
        f"Breadcrumb did not navigate to index: {pg.url}"

    pg.close()


# ---- Security Checks ----

@pytest.mark.parametrize("page", PAGES)
def test_no_inline_event_handlers(page):
    """Check for inline event handlers (XSS risk)."""
    content = (WASM_DIR / page).read_text()

    # Allow onclick on clear/download buttons (safe, hardcoded actions)
    # Flag dangerous patterns like onload, onerror with dynamic content
    dangerous = re.findall(
        r'\bon(?:load|error|mouseover|focus|blur)\s*=\s*["\']',
        content, re.IGNORECASE
    )
    assert not dangerous, \
        f"{page}: dangerous inline event handlers: {dangerous}"


@pytest.mark.parametrize("page", PAGES)
def test_no_eval_usage(page):
    """Check that pages do not use eval() or Function() constructor."""
    content = (WASM_DIR / page).read_text()
    # Check inline scripts only
    scripts = re.findall(r'<script[^>]*>(.*?)</script[\s>]', content,
                        re.DOTALL | re.IGNORECASE)
    for script in scripts:
        assert "eval(" not in script, f"{page}: uses eval()"
        assert "new Function(" not in script, \
            f"{page}: uses Function() constructor"


def test_app_js_no_eval():
    """Check app.js does not use eval() or Function()."""
    content = (WASM_DIR / "app.js").read_text()
    assert "eval(" not in content, "app.js uses eval()"
    assert "new Function(" not in content, \
        "app.js uses Function() constructor"


def test_sanitize_js_exists():
    """Verify sanitize.js is present in wasm/ directory."""
    assert (WASM_DIR / "sanitize.js").is_file(), "sanitize.js not found"


TOOL_PAGES = [p for p in PAGES if p != "index.html"]


@pytest.mark.parametrize("page", TOOL_PAGES)
def test_sanitizer_loaded_before_app(page):
    """Verify sanitize.js is loaded before app.js in tool pages."""
    content = (WASM_DIR / page).read_text()
    san_pos = content.find('src="sanitize.js"')
    app_pos = content.find('src="app.js"')
    assert san_pos != -1, f"{page}: sanitize.js not included"
    assert app_pos != -1, f"{page}: app.js not included"
    assert san_pos < app_pos, \
        f"{page}: sanitize.js must load before app.js"


def test_wasmbase_no_url_param():
    """Verify app.js does not read wasmBase from URL query params (CWE-94)."""
    content = (WASM_DIR / "app.js").read_text()
    assert "params.get('wasmBase')" not in content, \
        "app.js still reads wasmBase from URL query params (CWE-94)"
    assert 'params.get("wasmBase")' not in content, \
        "app.js still reads wasmBase from URL query params (CWE-94)"


def test_sanitizer_available_in_browser(server, browser_ctx):
    """Verify iccSanitize global is available when a tool page loads."""
    page = browser_ctx.new_page()
    try:
        page.goto(f"{server.base_url}/dump.html")
        result = page.evaluate("typeof iccSanitize")
        assert result == "object", "iccSanitize not loaded in browser"
    finally:
        page.close()


def test_sanitizer_html_escape(server, browser_ctx):
    """Test iccSanitize.htmlEscape works in the browser."""
    page = browser_ctx.new_page()
    try:
        page.goto(f"{server.base_url}/dump.html")
        result = page.evaluate('iccSanitize.htmlEscape("<script>alert(1)</script>")')
        assert "&lt;script&gt;" in result
        assert "<script>" not in result
    finally:
        page.close()


def test_sanitizer_uri_blocks_javascript(server, browser_ctx):
    """Test iccSanitize.sanitizeUri blocks javascript: scheme."""
    page = browser_ctx.new_page()
    try:
        page.goto(f"{server.base_url}/dump.html")
        result = page.evaluate('iccSanitize.sanitizeUri("javascript:alert(1)")')
        assert result == "", "javascript: URI was not blocked"
    finally:
        page.close()


def test_sanitizer_strip_control(server, browser_ctx):
    """Test iccSanitize.stripControl removes C0 control characters."""
    page = browser_ctx.new_page()
    try:
        page.goto(f"{server.base_url}/dump.html")
        result = page.evaluate(r'iccSanitize.stripControl("hello\x00world\x07test")')
        assert result == "helloworldtest", \
            f"Unexpected stripControl result: {result}"
    finally:
        page.close()


def test_sanitizer_strip_unicode(server, browser_ctx):
    """Test iccSanitize.stripUnicode removes bidi and zero-width chars."""
    page = browser_ctx.new_page()
    try:
        page.goto(f"{server.base_url}/dump.html")
        result = page.evaluate(
            r'iccSanitize.stripUnicode("hello\u200Bworld\u202Etest\uFEFF")')
        assert result == "helloworldtest", \
            f"Unexpected stripUnicode result: {result}"
    finally:
        page.close()


def test_sanitizer_strip_ansi(server, browser_ctx):
    """Test iccSanitize.stripAnsi removes ANSI escape sequences."""
    page = browser_ctx.new_page()
    try:
        page.goto(f"{server.base_url}/dump.html")
        result = page.evaluate(
            r'iccSanitize.stripAnsi("hello\x1B[31mred\x1B[0m world")')
        assert result == "hellored world", \
            f"Unexpected stripAnsi result: {result}"
    finally:
        page.close()


def test_sanitizer_sanitize_filename(server, browser_ctx):
    """Test iccSanitize.sanitizeFilename strips path traversal."""
    page = browser_ctx.new_page()
    try:
        page.goto(f"{server.base_url}/dump.html")
        result = page.evaluate(
            'iccSanitize.sanitizeFilename("../../../etc/passwd")')
        assert "/" not in result, f"Path separator in sanitized name: {result}"
        assert ".." not in result, f"Traversal in sanitized name: {result}"
        assert not result.startswith("."), \
            f"Leading dot in sanitized name: {result}"
    finally:
        page.close()


def test_sanitizer_truncate(server, browser_ctx):
    """Test iccSanitize.truncate limits output length."""
    page = browser_ctx.new_page()
    try:
        page.goto(f"{server.base_url}/dump.html")
        result = page.evaluate(
            'iccSanitize.truncate("a".repeat(2000), 100)')
        assert len(result) <= 130, f"Truncate did not limit: {len(result)}"
        assert "[truncated" in result, "Missing truncation marker"
    finally:
        page.close()


def test_csp_meta_present(html_files):
    """Every page must have a Content-Security-Policy meta tag."""
    for path in html_files:
        content = path.read_text()
        assert "Content-Security-Policy" in content, \
            f"{path.name}: missing CSP meta tag"


def test_no_onclick_anywhere(html_files):
    """No inline onclick handlers in any HTML file."""
    for path in html_files:
        content = path.read_text()
        assert "onclick=" not in content.lower(), \
            f"{path.name}: has inline onclick handler"


# ---- WCAG 2.1 AA Accessibility Tests ----


@pytest.mark.parametrize("page", PAGES)
def test_skip_link(page):
    """Every page must have a skip-to-content link (WCAG 2.4.1)."""
    content = (WASM_DIR / page).read_text()
    assert 'class="skip-link"' in content, \
        f"{page}: missing skip-to-content link"
    assert 'href="#main-content"' in content, \
        f"{page}: skip link does not target #main-content"


@pytest.mark.parametrize("page", PAGES)
def test_main_landmark(page):
    """Every page must have a <main> landmark (WCAG 1.3.1)."""
    content = (WASM_DIR / page).read_text()
    assert '<main' in content, f"{page}: missing <main> landmark"
    assert 'id="main-content"' in content, \
        f"{page}: <main> missing id=main-content"


@pytest.mark.parametrize("page", TOOL_PAGES)
def test_status_bar_aria(page):
    """Status bar must have role=status and aria-live (WCAG 4.1.3)."""
    content = (WASM_DIR / page).read_text()
    assert 'role="status"' in content, \
        f"{page}: status bar missing role=status"
    assert 'aria-live="polite"' in content, \
        f"{page}: status bar missing aria-live"


@pytest.mark.parametrize("page", TOOL_PAGES)
def test_drop_zone_keyboard(page):
    """Drop zones must be keyboard accessible (WCAG 2.1.1)."""
    content = (WASM_DIR / page).read_text()
    assert 'tabindex="0"' in content, \
        f"{page}: drop zone missing tabindex=0"
    assert 'role="button"' in content, \
        f"{page}: drop zone missing role=button"


@pytest.mark.parametrize("page", TOOL_PAGES)
def test_decorative_icons_hidden(page):
    """Decorative icons must be hidden from AT (WCAG 1.1.1)."""
    content = (WASM_DIR / page).read_text()
    if 'class="icon"' in content:
        assert 'aria-hidden="true"' in content, \
            f"{page}: decorative icon missing aria-hidden"


@pytest.mark.parametrize("page", TOOL_PAGES)
def test_output_panel_aria(page):
    """Output panel must have ARIA region and live announcement."""
    content = (WASM_DIR / page).read_text()
    if 'class="output-panel"' in content:
        assert 'role="region"' in content or 'role="log"' in content, \
            f"{page}: output panel missing ARIA role"


def test_focus_visible_styles():
    """style.css must define :focus-visible for interactive elements."""
    content = (WASM_DIR / "style.css").read_text()
    assert ":focus-visible" in content, \
        "style.css missing :focus-visible styles"


def test_reduced_motion():
    """style.css must respect prefers-reduced-motion (WCAG 2.3.3)."""
    content = (WASM_DIR / "style.css").read_text()
    assert "prefers-reduced-motion" in content, \
        "style.css missing prefers-reduced-motion"
