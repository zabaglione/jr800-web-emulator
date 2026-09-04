#!/usr/bin/env python3
# SPDX-License-Identifier: MIT

from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import tempfile


def run(command: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, check=False, capture_output=True, text=True)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def make_command(
    make: Path,
    source_dir: Path,
    build_dir: Path,
    assembler: Path,
    linker: Path,
    runner: Path,
    target: str,
) -> list[str]:
    return [
        str(make),
        "-C",
        str(source_dir),
        f"BUILD_DIR={build_dir}",
        f"JR8AS={assembler}",
        f"JR8LD={linker}",
        f"JR8RUN={runner}",
        target,
    ]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--make", type=Path, required=True)
    parser.add_argument("--source-dir", type=Path, required=True)
    parser.add_argument("--assembler", type=Path, required=True)
    parser.add_argument("--linker", type=Path, required=True)
    parser.add_argument("--runner", type=Path, required=True)
    parser.add_argument("--application-fixture", type=Path, required=True)
    parser.add_argument("--debug-fixture", type=Path, required=True)
    args = parser.parse_args()

    with tempfile.TemporaryDirectory(prefix="jr800-sample-") as temporary:
        build_dir = Path(temporary) / "build"
        build_command = make_command(
            args.make,
            args.source_dir,
            build_dir,
            args.assembler,
            args.linker,
            args.runner,
            "all",
        )
        build_command.insert(1, "-j2")
        build = run(build_command)
        require(build.returncode == 0, f"sample make failed: {build.stderr}")
        require(
            build.stdout.count(str(args.linker)) == 1,
            f"sample link ran more than once:\n{build.stdout}",
        )
        for filename in (
            "main.jro",
            "write-watch.j8a",
            "write-watch.j8d",
            "write-watch.map",
            "write-watch.sym",
        ):
            require((build_dir / filename).is_file(), f"missing sample output: {filename}")
        expected_application = bytes.fromhex(
            args.application_fixture.read_text(encoding="ascii")
        )
        expected_debug = bytes.fromhex(args.debug_fixture.read_text(encoding="ascii"))
        require(
            (build_dir / "write-watch.j8a").read_bytes() == expected_application,
            "generated JR8APP differs from the shared parity fixture",
        )
        require(
            (build_dir / "write-watch.j8d").read_bytes() == expected_debug,
            "generated JR8DBG differs from the shared parity fixture",
        )

        test = run(
            make_command(
                args.make,
                args.source_dir,
                build_dir,
                args.assembler,
                args.linker,
                args.runner,
                "test",
            )
        )
        require(test.returncode == 0, f"sample test failed: {test.stderr}\n{test.stdout}")
        require("Stop: memory-watchpoint" in test.stdout, "watchpoint stop is missing")
        require(
            "Memory watchpoint: $0001 (data-write)" in test.stdout,
            "watchpoint access kind is missing",
        )
        require("Source: main.s:9:5" in test.stdout, "source mapping is missing")

        missing_symbol_expectation = run(
            [
                str(args.runner),
                "--debug",
                str(build_dir / "write-watch.j8d"),
                "--max-instructions",
                "1",
                "--expect",
                'symbol("missing") == 0',
                str(build_dir / "write-watch.j8a"),
            ]
        )
        require(
            missing_symbol_expectation.returncode != 0,
            "missing JR8DBG symbol expectation passed",
        )
        require(
            'expectation evaluation failed: symbol("missing") == 0: '
            "symbol-not-found" in missing_symbol_expectation.stderr,
            "missing JR8DBG symbol expectation hid its evaluation error",
        )

        bounded = run(
            make_command(
                args.make,
                args.source_dir,
                build_dir,
                args.assembler,
                args.linker,
                args.runner,
                "run",
            )
        )
        require(
            bounded.returncode == 0,
            f"bounded sample run failed: {bounded.stderr}\n{bounded.stdout}",
        )
        require("Stop: instruction-limit" in bounded.stdout, "bounded stop is missing")
        require("Memory $0000: 42 99" in bounded.stdout, "RAM result is missing")

        false_expectation = run(
            [
                str(args.runner),
                "--max-instructions",
                "1",
                "--expect",
                "A == $99",
                str(build_dir / "write-watch.j8a"),
            ]
        )
        require(false_expectation.returncode != 0, "false state expectation passed")
        require(
            "expectation failed: A == $99" in false_expectation.stderr,
            "false state expectation did not report its expression",
        )

        invalid_expectation = run(
            [
                str(args.runner),
                "--expect",
                "A ==",
                str(build_dir / "missing.j8a"),
            ]
        )
        require(invalid_expectation.returncode != 0, "invalid expectation passed")
        require(
            "invalid expectation at byte" in invalid_expectation.stderr,
            "invalid expectation did not report its compile error",
        )
        require(
            "cannot open input" not in invalid_expectation.stderr,
            "invalid expectation was compiled after application input",
        )

        failed_expectation_read = run(
            [
                str(args.runner),
                "--max-instructions",
                "1",
                "--expect",
                "mem8[$10000] == 0",
                str(build_dir / "write-watch.j8a"),
            ]
        )
        require(
            failed_expectation_read.returncode != 0,
            "state expectation with an invalid memory address passed",
        )
        require(
            "expectation evaluation failed: mem8[$10000] == 0: "
            "address-out-of-range" in failed_expectation_read.stderr,
            "state expectation evaluation error was hidden",
        )

        obsolete_expect_byte = run(
            [
                str(args.runner),
                "--expect-byte",
                "0x0000=0x42",
                str(build_dir / "write-watch.j8a"),
            ]
        )
        require(obsolete_expect_byte.returncode != 0, "obsolete expectation passed")
        require(
            "unknown option: --expect-byte" in obsolete_expect_byte.stderr,
            "obsolete byte-expectation path remains reachable",
        )

        trace = run(
            make_command(
                args.make,
                args.source_dir,
                build_dir,
                args.assembler,
                args.linker,
                args.runner,
                "trace",
            )
        )
        require(trace.returncode == 0, f"trace sample failed: {trace.stderr}\n{trace.stdout}")
        require(
            "data-write address=$0000 value=$42" in trace.stdout,
            "filtered write trace is missing",
        )
        require(
            "instruction-fetch" not in trace.stdout
            and "data-write address=$0001" not in trace.stdout,
            "trace filter included an excluded access",
        )

        invalid_trace = run(
            [
                str(args.runner),
                "--trace",
                "write:0x0010:0x0000",
                str(build_dir / "write-watch.j8a"),
            ]
        )
        require(invalid_trace.returncode != 0, "reversed trace range was accepted")
        require(
            "invalid access trace filter" in invalid_trace.stderr,
            "invalid trace filter did not explain the error",
        )

        run_to = run(
            make_command(
                args.make,
                args.source_dir,
                build_dir,
                args.assembler,
                args.linker,
                args.runner,
                "run-to",
            )
        )
        require(
            run_to.returncode == 0,
            f"run-to sample failed: {run_to.stderr}\n{run_to.stdout}",
        )
        require("Stop: address-reached" in run_to.stdout, "run-to stop is missing")
        require("Address reached: $0207" in run_to.stdout, "run-to target is missing")

        run_to_source = run(
            make_command(
                args.make,
                args.source_dir,
                build_dir,
                args.assembler,
                args.linker,
                args.runner,
                "run-to-source",
            )
        )
        require(
            run_to_source.returncode == 0,
            "run-to-source sample failed: "
            f"{run_to_source.stderr}\n{run_to_source.stdout}",
        )
        require(
            "Stop: address-reached" in run_to_source.stdout,
            "run-to-source stop is missing",
        )
        require(
            "Source target: main.s:9 -> $0207" in run_to_source.stdout,
            "run-to-source resolution is missing",
        )

        run_to_symbol = run(
            make_command(
                args.make,
                args.source_dir,
                build_dir,
                args.assembler,
                args.linker,
                args.runner,
                "run-to-symbol",
            )
        )
        require(
            run_to_symbol.returncode == 0,
            "run-to-symbol sample failed: "
            f"{run_to_symbol.stderr}\n{run_to_symbol.stdout}",
        )
        require(
            "Stop: address-reached" in run_to_symbol.stdout,
            "run-to-symbol stop is missing",
        )
        require(
            "Symbol target: loop -> $020A" in run_to_symbol.stdout,
            "run-to-symbol resolution is missing",
        )

        break_if = run(
            make_command(
                args.make,
                args.source_dir,
                build_dir,
                args.assembler,
                args.linker,
                args.runner,
                "break-if",
            )
        )
        require(
            break_if.returncode == 0,
            f"conditional breakpoint sample failed: {break_if.stderr}\n{break_if.stdout}",
        )
        require(
            "Stop: execution-breakpoint" in break_if.stdout,
            "conditional breakpoint stop is missing",
        )
        require(
            "Instructions: 4" in break_if.stdout,
            "conditional breakpoint instruction count is missing",
        )

        invalid_condition = run(
            [
                str(args.runner),
                "--break-if",
                "0x020a:A ==",
                str(build_dir / "write-watch.j8a"),
            ]
        )
        require(invalid_condition.returncode != 0, "invalid condition was accepted")
        require(
            "invalid breakpoint condition" in invalid_condition.stderr,
            "invalid condition did not explain the error",
        )

        missing_source = run(
            [
                str(args.runner),
                "--debug",
                str(build_dir / "write-watch.j8d"),
                "--run-to-source",
                "missing.s:9",
                str(build_dir / "write-watch.j8a"),
            ]
        )
        require(missing_source.returncode != 0, "missing source line was accepted")
        require(
            "source location not found: missing.s:9" in missing_source.stderr,
            "missing source line did not explain the error",
        )

        missing_debug = run(
            [
                str(args.runner),
                "--run-to-source",
                "main.s:9",
                str(build_dir / "write-watch.j8a"),
            ]
        )
        require(missing_debug.returncode != 0, "source run without JR8DBG was accepted")
        require(
            "source and symbol targets require --debug" in missing_debug.stderr,
            "missing JR8DBG did not explain the source lookup failure",
        )

        missing_symbol = run(
            [
                str(args.runner),
                "--debug",
                str(build_dir / "write-watch.j8d"),
                "--run-to-symbol",
                "missing",
                str(build_dir / "write-watch.j8a"),
            ]
        )
        require(missing_symbol.returncode != 0, "missing symbol was accepted")
        require(
            "symbol not found: missing" in missing_symbol.stderr,
            "missing symbol did not explain the lookup failure",
        )

        missing_symbol_debug = run(
            [
                str(args.runner),
                "--run-to-symbol",
                "loop",
                str(build_dir / "write-watch.j8a"),
            ]
        )
        require(
            missing_symbol_debug.returncode != 0,
            "symbol run without JR8DBG was accepted",
        )
        require(
            "source and symbol targets require --debug"
                in missing_symbol_debug.stderr,
            "missing JR8DBG did not explain the symbol lookup failure",
        )

        step_over = run(
            make_command(
                args.make,
                args.source_dir,
                build_dir,
                args.assembler,
                args.linker,
                args.runner,
                "step-over",
            )
        )
        require(
            step_over.returncode == 0,
            f"step-over sample failed: {step_over.stderr}\n{step_over.stdout}",
        )
        require(
            "Stop: step-complete" in step_over.stdout,
            "linear step-over stop is missing",
        )
        require("Instructions: 1" in step_over.stdout, "step-over count is missing")

        step_out = run(
            make_command(
                args.make,
                args.source_dir,
                build_dir,
                args.assembler,
                args.linker,
                args.runner,
                "step-out",
            )
        )
        require(
            step_out.returncode == 0,
            f"step-out sample failed: {step_out.stderr}\n{step_out.stdout}",
        )
        require(
            "Stop: instruction-limit" in step_out.stdout,
            "bounded step-out stop is missing",
        )
        require(
            "Step-out continuation: continued=true depth=0"
            in step_out.stdout,
            "step-out continuation state is missing",
        )

        conflict = run(
            [
                str(args.runner),
                "--step-over",
                "--run-to",
                "0x0200",
                str(build_dir / "write-watch.j8a"),
            ]
        )
        require(conflict.returncode != 0, "conflicting run modes were accepted")
        require(
            "step-over, step-out, run-to, run-to-source, and run-to-symbol "
            "are mutually exclusive"
            in conflict.stderr,
            "conflicting run modes did not explain the error",
        )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
