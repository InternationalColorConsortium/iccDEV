#!/usr/bin/env python3
# Copyright (c) International Color Consortium.
# SPDX-License-Identifier: BSD-3-Clause

"""Prove IIS stress verdicts against a disposable loopback HTTP fixture."""

import json
import pathlib
import shutil
import subprocess
import threading
import unittest
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlsplit


class StressVerdicts(unittest.TestCase):
    def test_phase_failures(self):
        pwsh = shutil.which("pwsh")
        self.assertIsNotNone(pwsh, "PowerShell 7 is required for IIS stress QA")
        script = (
            pathlib.Path(__file__).resolve().parents[2]
            / "Tools" / "Winnt" / "IccIisIsapi" / "Stress-IccIisIsapi.ps1"
        )
        for fault in ("none", "reads", "methods", "headers", "oversize", "mime", "filename"):
            with self.subTest(fault=fault):
                class Handler(BaseHTTPRequestHandler):
                    def log_message(self, *_args):
                        pass

                    def handle_request(self):
                        query = parse_qs(urlsplit(self.path).query)
                        mode = query.get("mode", [""])[0]
                        status, body = 200, "ok\r\n"
                        length = int(self.headers.get("Content-Length", "0"))
                        remaining = length
                        while remaining:
                            chunk = self.rfile.read(min(remaining, 65536))
                            if not chunk:
                                break
                            remaining -= len(chunk)
                        if self.command != "GET" and mode != "tools":
                            status = 200 if fault == "methods" else 405
                        elif self.command == "GET" and mode == "tools":
                            status = 200 if fault == "methods" else 405
                        elif mode == "tools":
                            content_type = self.headers.get("Content-Type", "").split(";")[0]
                            if length > 16 * 1024 * 1024:
                                status = 200 if fault == "oversize" else 413
                            elif content_type != "application/octet-stream":
                                status = 200 if fault == "mime" else 415
                            body = json.dumps({
                                "input": {
                                    "filename": "../escape.icc" if fault == "filename" else "upload.icc",
                                    "url": "/_tool-work/" + "a" * 32 + "/upload.icc",
                                }
                            })
                        elif mode not in ("", "health"):
                            status, body = 400, "Bad Request"
                        elif mode == "":
                            status = 500 if fault == "reads" else 200
                            body = "<xml/>" if "format" in query else "iccDEV IIS service"
                        self.send_response(status)
                        if fault != "headers":
                            for name, value in {
                                "X-Content-Type-Options": "nosniff",
                                "X-Frame-Options": "DENY",
                                "Content-Security-Policy": "default-src 'self'",
                                "Cache-Control": "no-store",
                                "Referrer-Policy": "no-referrer",
                            }.items():
                                self.send_header(name, value)
                        encoded = body.encode("ascii")
                        self.send_header("Content-Type", "text/plain; charset=utf-8")
                        self.send_header("Content-Length", str(len(encoded)))
                        self.end_headers()
                        self.wfile.write(encoded)

                    do_GET = handle_request
                    do_POST = handle_request
                    do_PUT = handle_request
                    do_DELETE = handle_request

                server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
                worker = threading.Thread(target=server.serve_forever, daemon=True)
                worker.start()
                try:
                    result = subprocess.run(
                        [pwsh, "-NoProfile", "-File", str(script), "-BaseUrl",
                         f"http://127.0.0.1:{server.server_port}",
                         "-Concurrency", "1", "-Duration", "1"],
                        capture_output=True, text=True, timeout=45,
                    )
                    self.assertEqual(result.returncode, 0 if fault == "none" else 1,
                                     result.stdout + result.stderr)
                    if fault != "none":
                        self.assertIn("QA CHECKS FAILED", result.stdout)
                finally:
                    server.shutdown()
                    server.server_close()
                    worker.join(timeout=5)


if __name__ == "__main__":
    unittest.main()
