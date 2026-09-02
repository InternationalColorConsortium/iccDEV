# iccdev-mcp -- MCP server for ICC color profile tools
#
# Copyright (c) International Color Consortium.
# BSD 3-Clause License. See LICENSE.md for details.

"""
MCP server exposing iccDEV ICC color profile tools to AI assistants.

Provides 26 tools:
- 7 Python-native (header inspection, validation, color transforms, enums)
- 17 subprocess-backed (dump, XML/JSON conversion, image I/O)
- 2 service tools (health and profile discovery)

Quick start::

    # Install
    pip install iccdev-mcp

    # Run MCP server (stdio transport for Claude Desktop / VS Code)
    iccdev-mcp

    # Run with SSE transport for remote access
    iccdev-mcp --transport sse --port 8080
"""

__version__ = "0.1.0"
