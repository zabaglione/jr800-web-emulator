#!/usr/bin/env python3
# SPDX-License-Identifier: MIT

from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any


IDENTIFIER_PATTERN = re.compile(r"^[a-z][a-z0-9_]*$")
MNEMONIC_PATTERN = re.compile(r"^[A-Z][A-Z0-9]*$")
OPCODE_PATTERN = re.compile(r"^0x[0-9A-F]{2}$")
PROFILE_STATUSES = {"documented", "unresolved"}
PROFILE_ROLES = {"family_baseline", "documented_variant", "unresolved_machine"}


class IsaValidationError(ValueError):
    pass


@dataclass(frozen=True)
class ValidatedIsa:
    document: dict[str, Any]
    profile_order: tuple[str, ...]
    addressing_mode_order: tuple[str, ...]
    classification_order: tuple[str, ...]
    operation_order: tuple[str, ...]
    instructions: tuple[dict[str, Any], ...]
    effective_profile_masks: dict[str, int]


def _fail(context: str, message: str) -> None:
    raise IsaValidationError(f"{context}: {message}")


def _expect_object(value: Any, context: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        _fail(context, "expected an object")
    return value


def _expect_list(value: Any, context: str) -> list[Any]:
    if not isinstance(value, list):
        _fail(context, "expected an array")
    return value


def _expect_string(value: Any, context: str, *, allow_empty: bool = False) -> str:
    if not isinstance(value, str):
        _fail(context, "expected a string")
    if not allow_empty and not value:
        _fail(context, "must not be empty")
    return value


def _expect_integer(
    value: Any,
    context: str,
    *,
    minimum: int,
    maximum: int,
) -> int:
    if type(value) is not int:
        _fail(context, "expected an integer")
    if not minimum <= value <= maximum:
        _fail(context, f"expected a value from {minimum} through {maximum}")
    return value


def _expect_boolean(value: Any, context: str) -> bool:
    if type(value) is not bool:
        _fail(context, "expected a boolean")
    return value


def _expect_exact_keys(
    value: dict[str, Any],
    context: str,
    required: set[str],
) -> None:
    actual = set(value)
    missing = sorted(required - actual)
    extra = sorted(actual - required)
    if missing:
        _fail(context, f"missing fields: {', '.join(missing)}")
    if extra:
        _fail(context, f"unknown fields: {', '.join(extra)}")


def _expect_identifier(value: Any, context: str) -> str:
    identifier = _expect_string(value, context)
    if IDENTIFIER_PATTERN.fullmatch(identifier) is None:
        _fail(context, "expected a lower-case identifier")
    return identifier


def _expect_unique_strings(
    value: Any,
    context: str,
    *,
    allowed: set[str] | None = None,
    require_nonempty: bool = False,
) -> list[str]:
    items = _expect_list(value, context)
    if require_nonempty and not items:
        _fail(context, "must not be empty")

    result: list[str] = []
    for index, item in enumerate(items):
        text = _expect_string(item, f"{context}[{index}]")
        if allowed is not None and text not in allowed:
            _fail(f"{context}[{index}]", f"unknown value: {text}")
        if text in result:
            _fail(context, f"duplicate value: {text}")
        result.append(text)
    return result


def _validate_source_locations(
    value: Any,
    context: str,
    source_ids: set[str],
    *,
    require_nonempty: bool,
) -> None:
    locations = _expect_list(value, context)
    if require_nonempty and not locations:
        _fail(context, "must contain at least one primary-source location")

    for index, item in enumerate(locations):
        item_context = f"{context}[{index}]"
        location = _expect_object(item, item_context)
        _expect_exact_keys(location, item_context, {"source_id", "location"})
        source_id = _expect_identifier(location["source_id"], f"{item_context}.source_id")
        if source_id not in source_ids:
            _fail(f"{item_context}.source_id", f"unknown source: {source_id}")
        _expect_string(location["location"], f"{item_context}.location")


def _validate_sources(value: Any) -> set[str]:
    sources = _expect_list(value, "sources")
    if not sources:
        _fail("sources", "must not be empty")

    source_ids: set[str] = set()
    required = {"id", "title", "publisher", "document_date", "url", "notes"}
    for index, item in enumerate(sources):
        context = f"sources[{index}]"
        source = _expect_object(item, context)
        _expect_exact_keys(source, context, required)
        source_id = _expect_identifier(source["id"], f"{context}.id")
        if source_id in source_ids:
            _fail(f"{context}.id", f"duplicate source id: {source_id}")
        source_ids.add(source_id)
        _expect_string(source["title"], f"{context}.title")
        _expect_string(source["publisher"], f"{context}.publisher")
        _expect_string(source["document_date"], f"{context}.document_date")
        url = _expect_string(source["url"], f"{context}.url")
        if not url.startswith(("https://", "http://")):
            _fail(f"{context}.url", "expected an HTTP(S) URL")
        _expect_string(source["notes"], f"{context}.notes", allow_empty=True)
    return source_ids


def _validate_addressing_modes(value: Any) -> tuple[tuple[str, ...], dict[str, int]]:
    modes = _expect_list(value, "addressing_modes")
    if not modes:
        _fail("addressing_modes", "must not be empty")

    order: list[str] = []
    operand_bytes: dict[str, int] = {}
    for index, item in enumerate(modes):
        context = f"addressing_modes[{index}]"
        mode = _expect_object(item, context)
        _expect_exact_keys(mode, context, {"id", "operand_bytes"})
        mode_id = _expect_identifier(mode["id"], f"{context}.id")
        if mode_id in operand_bytes:
            _fail(f"{context}.id", f"duplicate addressing mode: {mode_id}")
        order.append(mode_id)
        operand_bytes[mode_id] = _expect_integer(
            mode["operand_bytes"],
            f"{context}.operand_bytes",
            minimum=0,
            maximum=3,
        )
    return tuple(order), operand_bytes


def _validate_classifications(value: Any) -> tuple[tuple[str, ...], set[str]]:
    classifications = _expect_list(value, "instruction_classes")
    if not classifications:
        _fail("instruction_classes", "must not be empty")

    order: list[str] = []
    step_over: set[str] = set()
    for index, item in enumerate(classifications):
        context = f"instruction_classes[{index}]"
        classification = _expect_object(item, context)
        _expect_exact_keys(classification, context, {"id", "step_over"})
        classification_id = _expect_identifier(classification["id"], f"{context}.id")
        if classification_id in order:
            _fail(f"{context}.id", f"duplicate instruction class: {classification_id}")
        order.append(classification_id)
        if _expect_boolean(classification["step_over"], f"{context}.step_over"):
            step_over.add(classification_id)
    return tuple(order), step_over


def _validate_profiles(
    value: Any,
    source_ids: set[str],
) -> tuple[tuple[str, ...], dict[str, dict[str, Any]]]:
    profiles = _expect_list(value, "cpu_profiles")
    if not profiles:
        _fail("cpu_profiles", "must not be empty")
    if len(profiles) > 32:
        _fail("cpu_profiles", "at most 32 profiles are supported by the generated mask")

    order: list[str] = []
    by_id: dict[str, dict[str, Any]] = {}
    required = {
        "id",
        "label",
        "status",
        "role",
        "unknown_id",
        "source_locations",
    }
    for index, item in enumerate(profiles):
        context = f"cpu_profiles[{index}]"
        profile = _expect_object(item, context)
        _expect_exact_keys(profile, context, required)
        profile_id = _expect_identifier(profile["id"], f"{context}.id")
        if profile_id in by_id:
            _fail(f"{context}.id", f"duplicate profile: {profile_id}")
        order.append(profile_id)
        by_id[profile_id] = profile

        _expect_string(profile["label"], f"{context}.label")
        status = _expect_string(profile["status"], f"{context}.status")
        if status not in PROFILE_STATUSES:
            _fail(f"{context}.status", f"unknown profile status: {status}")

        role = _expect_identifier(profile["role"], f"{context}.role")
        if role not in PROFILE_ROLES:
            _fail(f"{context}.role", f"unknown profile role: {role}")

        unknown_id = profile["unknown_id"]
        if status == "documented":
            if role == "unresolved_machine":
                _fail(f"{context}.role", "documented profiles require a documented role")
            if unknown_id is not None:
                _fail(f"{context}.unknown_id", "documented profiles must use null")
            _validate_source_locations(
                profile["source_locations"],
                f"{context}.source_locations",
                source_ids,
                require_nonempty=True,
            )
        else:
            if role != "unresolved_machine":
                _fail(f"{context}.role", "unresolved profiles require unresolved_machine")
            _expect_string(unknown_id, f"{context}.unknown_id")
            _validate_source_locations(
                profile["source_locations"],
                f"{context}.source_locations",
                source_ids,
                require_nonempty=False,
            )

    return tuple(order), by_id


def validate_document(document: Any) -> ValidatedIsa:
    root = _expect_object(document, "root")
    _expect_exact_keys(
        root,
        "root",
        {
            "schema_version",
            "sources",
            "status_flags",
            "addressing_modes",
            "instruction_classes",
            "cpu_profiles",
            "instructions",
        },
    )
    _expect_integer(root["schema_version"], "schema_version", minimum=1, maximum=1)

    source_ids = _validate_sources(root["sources"])
    status_flags = _expect_unique_strings(
        root["status_flags"],
        "status_flags",
        require_nonempty=True,
    )
    if status_flags != ["H", "I", "N", "Z", "V", "C"]:
        _fail("status_flags", "expected the explicit HD6301 H,I,N,Z,V,C order")
    status_flag_set = set(status_flags)

    addressing_order, operand_bytes_by_mode = _validate_addressing_modes(
        root["addressing_modes"]
    )
    classification_order, _ = _validate_classifications(root["instruction_classes"])
    profile_order, profiles_by_id = _validate_profiles(
        root["cpu_profiles"], source_ids
    )

    instructions = _expect_list(root["instructions"], "instructions")
    if not instructions:
        _fail("instructions", "must contain a reviewed subset")

    instruction_ids: set[str] = set()
    operations: set[str] = set()
    effective_masks: dict[str, int] = {}
    validated_instructions: list[dict[str, Any]] = []
    required_instruction_fields = {
        "id",
        "opcode",
        "mnemonic",
        "addressing_mode",
        "operand_bytes",
        "base_cycles",
        "flags",
        "classification",
        "operation",
        "profiles",
        "source_locations",
    }

    for index, item in enumerate(instructions):
        context = f"instructions[{index}]"
        instruction = _expect_object(item, context)
        _expect_exact_keys(instruction, context, required_instruction_fields)

        instruction_id = _expect_identifier(instruction["id"], f"{context}.id")
        if instruction_id in instruction_ids:
            _fail(f"{context}.id", f"duplicate instruction id: {instruction_id}")
        instruction_ids.add(instruction_id)

        opcode = _expect_string(instruction["opcode"], f"{context}.opcode")
        if OPCODE_PATTERN.fullmatch(opcode) is None:
            _fail(f"{context}.opcode", "expected an uppercase byte such as 0x01")
        mnemonic = _expect_string(instruction["mnemonic"], f"{context}.mnemonic")
        if MNEMONIC_PATTERN.fullmatch(mnemonic) is None:
            _fail(f"{context}.mnemonic", "expected an uppercase mnemonic")

        mode = _expect_identifier(
            instruction["addressing_mode"], f"{context}.addressing_mode"
        )
        if mode not in operand_bytes_by_mode:
            _fail(f"{context}.addressing_mode", f"unknown addressing mode: {mode}")
        operand_bytes = _expect_integer(
            instruction["operand_bytes"],
            f"{context}.operand_bytes",
            minimum=0,
            maximum=3,
        )
        if operand_bytes != operand_bytes_by_mode[mode]:
            _fail(
                f"{context}.operand_bytes",
                f"{mode} requires {operand_bytes_by_mode[mode]} operand bytes",
            )
        _expect_integer(
            instruction["base_cycles"],
            f"{context}.base_cycles",
            minimum=1,
            maximum=255,
        )

        flags = _expect_object(instruction["flags"], f"{context}.flags")
        _expect_exact_keys(
            flags,
            f"{context}.flags",
            {"read", "written", "preserved", "undefined"},
        )
        for field in ("read", "written", "preserved", "undefined"):
            _expect_unique_strings(
                flags[field],
                f"{context}.flags.{field}",
                allowed=status_flag_set,
            )
        output_partition = (
            flags["written"] + flags["preserved"] + flags["undefined"]
        )
        if len(output_partition) != len(set(output_partition)):
            _fail(
                f"{context}.flags",
                "written, preserved, and undefined flags must not overlap",
            )
        if set(output_partition) != status_flag_set:
            _fail(
                f"{context}.flags",
                "written, preserved, and undefined must partition every status flag",
            )

        classification = _expect_identifier(
            instruction["classification"], f"{context}.classification"
        )
        if classification not in classification_order:
            _fail(
                f"{context}.classification",
                f"unknown instruction class: {classification}",
            )
        operation = _expect_identifier(instruction["operation"], f"{context}.operation")
        operations.add(operation)

        declared_profiles = _expect_unique_strings(
            instruction["profiles"],
            f"{context}.profiles",
            allowed=set(profile_order),
            require_nonempty=True,
        )
        for profile_id in declared_profiles:
            if profiles_by_id[profile_id]["status"] != "documented":
                _fail(
                    f"{context}.profiles",
                    f"unresolved profile cannot receive instruction data: {profile_id}",
                )

        effective_mask = 0
        for profile_id in declared_profiles:
            effective_mask |= 1 << profile_order.index(profile_id)
        if effective_mask == 0:
            _fail(f"{context}.profiles", "instruction has no effective documented profile")
        effective_masks[instruction_id] = effective_mask

        _validate_source_locations(
            instruction["source_locations"],
            f"{context}.source_locations",
            source_ids,
            require_nonempty=True,
        )
        validated_instructions.append(instruction)

    for left_index, left in enumerate(validated_instructions):
        left_mask = effective_masks[left["id"]]
        for right in validated_instructions[left_index + 1 :]:
            if left_mask & effective_masks[right["id"]] == 0:
                continue
            if left["opcode"] == right["opcode"]:
                _fail(
                    "instructions",
                    f"opcode {left['opcode']} overlaps for {left['id']} and {right['id']}",
                )
            if (
                left["mnemonic"],
                left["addressing_mode"],
            ) == (
                right["mnemonic"],
                right["addressing_mode"],
            ):
                _fail(
                    "instructions",
                    "assembler encoding overlaps for "
                    f"{left['id']} and {right['id']}",
                )

    sorted_instructions = tuple(
        sorted(
            validated_instructions,
            key=lambda item: (
                int(item["opcode"], 16),
                item["mnemonic"],
                item["addressing_mode"],
                item["id"],
            ),
        )
    )
    return ValidatedIsa(
        document=root,
        profile_order=profile_order,
        addressing_mode_order=addressing_order,
        classification_order=classification_order,
        operation_order=tuple(sorted(operations)),
        instructions=sorted_instructions,
        effective_profile_masks=effective_masks,
    )


def load_document(path: Path) -> dict[str, Any]:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise IsaValidationError(f"{path}: {error}") from error


def _render_enum(name: str, values: tuple[str, ...]) -> list[str]:
    lines = [f"enum class {name} : std::uint8_t {{"]
    lines.extend(f"    {value}," for value in values)
    lines.append("};")
    return lines


def _flag_mask(flags: list[str], flag_order: list[str]) -> int:
    result = 0
    for flag in flags:
        result |= 1 << flag_order.index(flag)
    return result


def render_header(validated: ValidatedIsa, input_name: str) -> str:
    lines = [
        "// SPDX-License-Identifier: MIT",
        f"// Generated from {input_name}. Do not edit.",
        "",
        "#pragma once",
        "",
        "#include <cstdint>",
        "#include <optional>",
        "#include <span>",
        "#include <string_view>",
        "",
        "namespace jr800::isa {",
        "",
    ]
    lines.extend(_render_enum("CpuProfile", validated.profile_order))
    lines.append("")
    lines.extend(_render_enum("AddressingMode", validated.addressing_mode_order))
    lines.append("")
    lines.extend(_render_enum("InstructionClass", validated.classification_order))
    lines.append("")
    lines.extend(_render_enum("Operation", validated.operation_order))
    lines.extend(
        [
            "",
            "enum class StatusFlag : std::uint8_t {",
            "    h,",
            "    i,",
            "    n,",
            "    z,",
            "    v,",
            "    c,",
            "};",
            "",
            "[[nodiscard]] constexpr std::uint8_t flag_mask(StatusFlag flag) noexcept {",
            "    return static_cast<std::uint8_t>(1U << static_cast<std::uint8_t>(flag));",
            "}",
            "",
            "struct FlagMetadata {",
            "    std::uint8_t read_mask;",
            "    std::uint8_t written_mask;",
            "    std::uint8_t preserved_mask;",
            "    std::uint8_t undefined_mask;",
            "};",
            "",
            "struct InstructionMetadata {",
            "    std::uint8_t opcode;",
            "    std::string_view id;",
            "    std::string_view mnemonic;",
            "    AddressingMode addressing_mode;",
            "    std::uint8_t operand_bytes;",
            "    std::uint8_t instruction_length;",
            "    std::uint8_t base_cycles;",
            "    FlagMetadata flags;",
            "    InstructionClass classification;",
            "    Operation operation;",
            "    std::uint32_t applicable_profiles;",
            "};",
            "",
            "[[nodiscard]] std::span<const InstructionMetadata> all_instructions() noexcept;",
            "[[nodiscard]] std::span<const InstructionMetadata> instruction_test_cases() noexcept;",
            "[[nodiscard]] std::string_view profile_name(CpuProfile profile) noexcept;",
            "[[nodiscard]] std::optional<CpuProfile> find_profile(std::string_view name) noexcept;",
            "[[nodiscard]] const InstructionMetadata* decode_instruction(",
            "    CpuProfile profile,",
            "    std::uint8_t opcode",
            ") noexcept;",
            "[[nodiscard]] const InstructionMetadata* find_encoding(",
            "    CpuProfile profile,",
            "    std::string_view mnemonic,",
            "    AddressingMode addressing_mode",
            ") noexcept;",
            "[[nodiscard]] bool instruction_applies_to(",
            "    const InstructionMetadata& instruction,",
            "    CpuProfile profile",
            ") noexcept;",
            "[[nodiscard]] bool is_step_over_candidate(",
            "    const InstructionMetadata& instruction",
            ") noexcept;",
            "",
            "}  // namespace jr800::isa",
            "",
        ]
    )
    return "\n".join(lines)


def render_source(validated: ValidatedIsa, input_name: str) -> str:
    flag_order = validated.document["status_flags"]
    step_over_classes = {
        item["id"]
        for item in validated.document["instruction_classes"]
        if item["step_over"]
    }
    lines = [
        "// SPDX-License-Identifier: MIT",
        f"// Generated from {input_name}. Do not edit.",
        "",
        '#include "jr800/isa/instruction_metadata.hpp"',
        "",
        "#include <array>",
        "",
        "namespace jr800::isa {",
        "namespace {",
        "",
        f"constexpr std::array<InstructionMetadata, {len(validated.instructions)}> kInstructions{{{{",
    ]

    for instruction in validated.instructions:
        flags = instruction["flags"]
        masks = [
            _flag_mask(flags[field], flag_order)
            for field in ("read", "written", "preserved", "undefined")
        ]
        lines.extend(
            [
                "    InstructionMetadata{",
                f"        {instruction['opcode']}U,",
                f"        {json.dumps(instruction['id'])},",
                f"        {json.dumps(instruction['mnemonic'])},",
                f"        AddressingMode::{instruction['addressing_mode']},",
                f"        {instruction['operand_bytes']}U,",
                f"        {instruction['operand_bytes'] + 1}U,",
                f"        {instruction['base_cycles']}U,",
                "        FlagMetadata{"
                + ", ".join(f"0x{mask:02X}U" for mask in masks)
                + "},",
                f"        InstructionClass::{instruction['classification']},",
                f"        Operation::{instruction['operation']},",
                "        0x"
                f"{validated.effective_profile_masks[instruction['id']]:08X}U,",
                "    },",
            ]
        )

    lines.extend(
        [
            "}};",
            "",
            "}  // namespace",
            "",
            "std::span<const InstructionMetadata> all_instructions() noexcept {",
            "    return kInstructions;",
            "}",
            "",
            "std::span<const InstructionMetadata> instruction_test_cases() noexcept {",
            "    return kInstructions;",
            "}",
            "",
            "std::string_view profile_name(CpuProfile profile) noexcept {",
            "    switch (profile) {",
        ]
    )
    for profile in validated.profile_order:
        lines.extend(
            [
                f"    case CpuProfile::{profile}:",
                f"        return {json.dumps(profile)};",
            ]
        )
    lines.extend(
        [
            "    }",
            "    return {};",
            "}",
            "",
            "std::optional<CpuProfile> find_profile(std::string_view name) noexcept {",
        ]
    )
    for profile in validated.profile_order:
        lines.extend(
            [
                f"    if (name == {json.dumps(profile)}) {{",
                f"        return CpuProfile::{profile};",
                "    }",
            ]
        )
    lines.extend(
        [
            "    return std::nullopt;",
            "}",
            "",
            "bool instruction_applies_to(",
            "    const InstructionMetadata& instruction,",
            "    CpuProfile profile",
            ") noexcept {",
            "    const auto profile_bit =",
            "        static_cast<std::uint32_t>(1U << static_cast<std::uint8_t>(profile));",
            "    return (instruction.applicable_profiles & profile_bit) != 0U;",
            "}",
            "",
            "const InstructionMetadata* decode_instruction(",
            "    CpuProfile profile,",
            "    std::uint8_t opcode",
            ") noexcept {",
            "    for (const auto& instruction : kInstructions) {",
            "        if (instruction.opcode == opcode && instruction_applies_to(instruction, profile)) {",
            "            return &instruction;",
            "        }",
            "    }",
            "    return nullptr;",
            "}",
            "",
            "const InstructionMetadata* find_encoding(",
            "    CpuProfile profile,",
            "    std::string_view mnemonic,",
            "    AddressingMode addressing_mode",
            ") noexcept {",
            "    for (const auto& instruction : kInstructions) {",
            "        if (instruction.mnemonic == mnemonic",
            "            && instruction.addressing_mode == addressing_mode",
            "            && instruction_applies_to(instruction, profile)) {",
            "            return &instruction;",
            "        }",
            "    }",
            "    return nullptr;",
            "}",
            "",
            "bool is_step_over_candidate(const InstructionMetadata& instruction) noexcept {",
            "    switch (instruction.classification) {",
        ]
    )
    for classification in validated.classification_order:
        if classification in step_over_classes:
            lines.append(f"    case InstructionClass::{classification}:")
    if step_over_classes:
        lines.append("        return true;")
    lines.extend(
        [
            "    default:",
            "        return false;",
            "    }",
            "}",
            "",
            "}  // namespace jr800::isa",
            "",
        ]
    )
    return "\n".join(lines)


def write_if_changed(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists() and path.read_text(encoding="utf-8") == content:
        return
    path.write_text(content, encoding="utf-8", newline="\n")


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Validate and generate JR-800 ISA metadata")
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output-header", type=Path)
    parser.add_argument("--output-source", type=Path)
    parser.add_argument("--validate-only", action="store_true")
    args = parser.parse_args(argv)
    if not args.validate_only and (args.output_header is None or args.output_source is None):
        parser.error("--output-header and --output-source are required for generation")
    return args


def main(argv: list[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    try:
        document = load_document(args.input)
        validated = validate_document(document)
        if not args.validate_only:
            input_name = args.input.name
            write_if_changed(args.output_header, render_header(validated, input_name))
            write_if_changed(args.output_source, render_source(validated, input_name))
    except IsaValidationError as error:
        print(f"ISA validation failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
