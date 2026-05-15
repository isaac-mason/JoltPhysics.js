#!/usr/bin/env python3
"""Cross-platform helpers for JoltPhysics.js CMake custom commands (Windows cmd has no cat/sed)."""
from __future__ import annotations

import argparse
import pathlib
import sys


def concat_idl(output: str, sources: list[str]) -> None:
    chunks: list[str] = []
    for src in sources:
        chunks.append(
            pathlib.Path(src).read_text(encoding="utf-8", errors="replace")
        )
    pathlib.Path(output).write_text("".join(chunks), encoding="utf-8", newline="\n")


def strip_thread_local(path: str) -> None:
    p = pathlib.Path(path)
    text = p.read_text(encoding="utf-8", errors="replace")
    p.write_text(text.replace("thread_local", ""), encoding="utf-8")


def replace_import_token(path: str) -> None:
    p = pathlib.Path(path)
    text = p.read_text(encoding="utf-8", errors="replace")
    p.write_text(text.replace("replace_by_import", "import"), encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="cmd", required=True)

    p_cat = sub.add_parser("concat-idl", help="Merge IDL fragments into jolt.idl")
    p_cat.add_argument("output")
    p_cat.add_argument("sources", nargs="+")

    p_strip = sub.add_parser(
        "strip-thread-local", help="Remove thread_local from generated glue.cpp (ST only)"
    )
    p_strip.add_argument("path")

    p_rep = sub.add_parser(
        "replace-import-token",
        help="replace_by_import -> import in generated JS (Emscripten workaround)",
    )
    p_rep.add_argument("path")

    args = parser.parse_args()
    if args.cmd == "concat-idl":
        concat_idl(args.output, args.sources)
    elif args.cmd == "strip-thread-local":
        strip_thread_local(args.path)
    elif args.cmd == "replace-import-token":
        replace_import_token(args.path)
    else:
        parser.error(f"unknown command {args.cmd!r}")
        sys.exit(2)


if __name__ == "__main__":
    main()
