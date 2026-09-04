#!/usr/bin/env python3
# SPDX-License-Identifier: MIT

from __future__ import annotations

import argparse
import copy
import importlib.util
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


def load_generator(path: Path):
    spec = importlib.util.spec_from_file_location("jr800_isa_generator", path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Unable to load generator: {path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


class IsaGeneratorTests(unittest.TestCase):
    generator_path: Path
    input_path: Path
    generator: object
    document: dict

    @classmethod
    def setUpClass(cls) -> None:
        cls.generator = load_generator(cls.generator_path)
        cls.document = cls.generator.load_document(cls.input_path)

    def test_reviewed_document_validates(self) -> None:
        validated = self.generator.validate_document(copy.deepcopy(self.document))
        self.assertEqual(232, len(validated.instructions))

    def test_cli_generation_is_byte_deterministic(self) -> None:
        with tempfile.TemporaryDirectory() as first_dir, tempfile.TemporaryDirectory() as second_dir:
            first_header = Path(first_dir) / "instruction_metadata.hpp"
            first_source = Path(first_dir) / "instruction_metadata.cpp"
            second_header = Path(second_dir) / "instruction_metadata.hpp"
            second_source = Path(second_dir) / "instruction_metadata.cpp"
            for header, source in (
                (first_header, first_source),
                (second_header, second_source),
            ):
                subprocess.run(
                    [
                        sys.executable,
                        str(self.generator_path),
                        "--input",
                        str(self.input_path),
                        "--output-header",
                        str(header),
                        "--output-source",
                        str(source),
                    ],
                    check=True,
                )
            self.assertEqual(first_header.read_bytes(), second_header.read_bytes())
            self.assertEqual(first_source.read_bytes(), second_source.read_bytes())

    def test_unresolved_profile_cannot_receive_metadata(self) -> None:
        document = copy.deepcopy(self.document)
        document["instructions"][0]["profiles"] = ["jr800_unresolved"]
        with self.assertRaises(self.generator.IsaValidationError):
            self.generator.validate_document(document)

    def test_addressing_mode_owns_operand_length(self) -> None:
        document = copy.deepcopy(self.document)
        document["instructions"][0]["operand_bytes"] = 1
        with self.assertRaises(self.generator.IsaValidationError):
            self.generator.validate_document(document)

    def test_flag_outputs_must_form_complete_partition(self) -> None:
        document = copy.deepcopy(self.document)
        document["instructions"][0]["flags"]["preserved"].remove("C")
        with self.assertRaises(self.generator.IsaValidationError):
            self.generator.validate_document(document)

    def test_overlapping_opcode_is_rejected(self) -> None:
        document = copy.deepcopy(self.document)
        document["instructions"][-1]["opcode"] = "0x86"
        with self.assertRaises(self.generator.IsaValidationError):
            self.generator.validate_document(document)

    def test_schema_version_must_be_integer_one(self) -> None:
        document = copy.deepcopy(self.document)
        document["schema_version"] = True
        with self.assertRaises(self.generator.IsaValidationError):
            self.generator.validate_document(document)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--generator", required=True, type=Path)
    parser.add_argument("--input", required=True, type=Path)
    return parser.parse_args()


if __name__ == "__main__":
    arguments = parse_args()
    IsaGeneratorTests.generator_path = arguments.generator
    IsaGeneratorTests.input_path = arguments.input
    unittest.main(argv=[sys.argv[0]])
