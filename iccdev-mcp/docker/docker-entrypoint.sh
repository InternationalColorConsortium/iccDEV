#!/usr/bin/env bash
set -euo pipefail

mode="${1:-mcp}"
case "$mode" in
  mcp|stdio)
    shift || true
    exec iccdev-mcp "$@"
    ;;
  rest|api)
    shift || true
    exec iccdev-mcp-rest --host 0.0.0.0 --port "${ICCDEV_MCP_PORT:-8080}" "$@"
    ;;
  sse)
    shift || true
    exec iccdev-mcp --transport sse --port "${ICCDEV_MCP_PORT:-8080}" "$@"
    ;;
  http|streamable-http)
    shift || true
    exec iccdev-mcp --transport streamable-http --port "${ICCDEV_MCP_PORT:-8080}" "$@"
    ;;
  *)
    exec "$@"
    ;;
esac
