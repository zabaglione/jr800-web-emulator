// SPDX-License-Identifier: MIT

#include <array>
#include <cstdint>
#include <cstddef>
#include <initializer_list>
#include <iostream>

#include "jr800/isa/instruction_metadata.hpp"

namespace {

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

constexpr std::uint8_t mask(
    std::initializer_list<jr800::isa::StatusFlag> flags
) {
    std::uint8_t result = 0;
    for (const auto flag : flags) {
        result = static_cast<std::uint8_t>(result | jr800::isa::flag_mask(flag));
    }
    return result;
}

}  // namespace

int main() {
    using jr800::isa::AddressingMode;
    using jr800::isa::CpuProfile;
    using jr800::isa::InstructionClass;
    using jr800::isa::StatusFlag;

    bool passed = true;
    const auto instructions = jr800::isa::all_instructions();
    passed &= expect(
        instructions.size() == 232,
        "Unexpected reviewed subset size"
    );
    passed &= expect(
        jr800::isa::instruction_test_cases().size() == instructions.size(),
        "Test enumeration must expose every metadata row"
    );
    passed &= expect(
        jr800::isa::profile_name(CpuProfile::hd6301v1) == "hd6301v1",
        "Profile name metadata mismatch"
    );
    passed &= expect(
        jr800::isa::find_profile("mc6801") == CpuProfile::mc6801,
        "Profile lookup metadata mismatch"
    );
    passed &= expect(
        !jr800::isa::find_profile("hd6301").has_value(),
        "Unknown generic profile must not resolve"
    );

    for (std::size_t index = 1; index < instructions.size(); ++index) {
        passed &= expect(
            instructions[index - 1].opcode <= instructions[index].opcode,
            "Generated metadata is not sorted by opcode"
        );
    }

    const auto* mc6801_nop = jr800::isa::decode_instruction(CpuProfile::mc6801, 0x01);
    const auto* hd6301v1_nop =
        jr800::isa::decode_instruction(CpuProfile::hd6301v1, 0x01);
    passed &= expect(mc6801_nop != nullptr, "MC6801 NOP metadata missing");
    passed &= expect(hd6301v1_nop != nullptr, "HD6301V1 NOP metadata missing");
    if (mc6801_nop != nullptr && hd6301v1_nop != nullptr) {
        passed &= expect(mc6801_nop->instruction_length == 1, "MC6801 NOP length mismatch");
        passed &= expect(mc6801_nop->base_cycles == 2, "MC6801 NOP cycle mismatch");
        passed &= expect(hd6301v1_nop->base_cycles == 1, "HD6301V1 NOP cycle mismatch");
    }

    const auto* hd6301v1_slp = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "SLP",
        AddressingMode::implied
    );
    passed &= expect(hd6301v1_slp != nullptr, "HD6301V1 SLP encoding missing");
    if (hd6301v1_slp != nullptr) {
        const auto all_flags = mask({
            StatusFlag::h,
            StatusFlag::i,
            StatusFlag::n,
            StatusFlag::z,
            StatusFlag::v,
            StatusFlag::c,
        });
        passed &= expect(
            hd6301v1_slp->opcode == 0x1AU
                && hd6301v1_slp->operand_bytes == 0U
                && hd6301v1_slp->instruction_length == 1U
                && hd6301v1_slp->base_cycles == 4U,
            "SLP opcode, length, or cycle metadata mismatch"
        );
        passed &= expect(
            hd6301v1_slp->flags.read_mask == 0U
                && hd6301v1_slp->flags.written_mask == 0U
                && hd6301v1_slp->flags.preserved_mask == all_flags
                && hd6301v1_slp->flags.undefined_mask == 0U,
            "SLP flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_slp->classification == InstructionClass::suspend
                && !jr800::isa::is_step_over_candidate(*hd6301v1_slp)
                && hd6301v1_slp->operation
                    == jr800::isa::Operation::enter_sleep,
            "SLP operation or debugger classification mismatch"
        );
        passed &= expect(
            jr800::isa::decode_instruction(CpuProfile::hd6301v1, 0x1AU)
                == hd6301v1_slp,
            "SLP decoder lookup mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "SLP",
            AddressingMode::implied
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x1AU)
                == nullptr,
        "Undefined MC6801 opcode 0x1A inherited SLP metadata"
    );

    const auto* hd6301v1_wai = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "WAI",
        AddressingMode::implied
    );
    passed &= expect(hd6301v1_wai != nullptr, "HD6301V1 WAI encoding missing");
    if (hd6301v1_wai != nullptr) {
        const auto all_flags = mask({
            StatusFlag::h,
            StatusFlag::i,
            StatusFlag::n,
            StatusFlag::z,
            StatusFlag::v,
            StatusFlag::c,
        });
        passed &= expect(
            hd6301v1_wai->opcode == 0x3EU
                && hd6301v1_wai->operand_bytes == 0U
                && hd6301v1_wai->instruction_length == 1U
                && hd6301v1_wai->base_cycles == 9U,
            "WAI opcode, length, or cycle metadata mismatch"
        );
        passed &= expect(
            hd6301v1_wai->flags.read_mask == 0U
                && hd6301v1_wai->flags.written_mask == 0U
                && hd6301v1_wai->flags.preserved_mask == all_flags
                && hd6301v1_wai->flags.undefined_mask == 0U,
            "WAI flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_wai->classification == InstructionClass::suspend
                && !jr800::isa::is_step_over_candidate(*hd6301v1_wai)
                && hd6301v1_wai->operation
                    == jr800::isa::Operation::wait_for_interrupt
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0x3EU
                ) == hd6301v1_wai,
            "WAI operation, classification, or decoder lookup mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "WAI",
            AddressingMode::implied
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x3EU)
                == nullptr,
        "Unreviewed MC6801 opcode 0x3E inherited WAI metadata"
    );

    const auto* hd6301v1_swi = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "SWI",
        AddressingMode::implied
    );
    passed &= expect(hd6301v1_swi != nullptr, "HD6301V1 SWI encoding missing");
    if (hd6301v1_swi != nullptr) {
        passed &= expect(
            hd6301v1_swi->opcode == 0x3FU
                && hd6301v1_swi->operand_bytes == 0U
                && hd6301v1_swi->instruction_length == 1U
                && hd6301v1_swi->base_cycles == 12U,
            "SWI opcode, length, or cycle metadata mismatch"
        );
        passed &= expect(
            hd6301v1_swi->flags.read_mask == 0U
                && hd6301v1_swi->flags.written_mask
                    == mask({StatusFlag::i})
                && hd6301v1_swi->flags.preserved_mask
                    == mask({
                        StatusFlag::h,
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_swi->flags.undefined_mask == 0U,
            "SWI flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_swi->classification == InstructionClass::call
                && jr800::isa::is_step_over_candidate(*hd6301v1_swi)
                && hd6301v1_swi->operation
                    == jr800::isa::Operation::software_interrupt
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0x3FU
                ) == hd6301v1_swi,
            "SWI operation, classification, or decoder lookup mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "SWI",
            AddressingMode::implied
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x3FU)
                == nullptr,
        "Unreviewed MC6801 opcode 0x3F inherited SWI metadata"
    );

    const auto* hd6301v1_daa = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "DAA",
        AddressingMode::implied
    );
    passed &= expect(hd6301v1_daa != nullptr, "HD6301V1 DAA encoding missing");
    if (hd6301v1_daa != nullptr) {
        passed &= expect(
            hd6301v1_daa->opcode == 0x19U
                && hd6301v1_daa->operand_bytes == 0U
                && hd6301v1_daa->instruction_length == 1U
                && hd6301v1_daa->base_cycles == 2U,
            "DAA opcode, length, or cycle metadata mismatch"
        );
        passed &= expect(
            hd6301v1_daa->flags.read_mask
                    == mask({StatusFlag::h, StatusFlag::c})
                && hd6301v1_daa->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::c})
                && hd6301v1_daa->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::v})
                && hd6301v1_daa->flags.undefined_mask == 0U,
            "DAA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_daa->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_daa)
                && hd6301v1_daa->operation
                    == jr800::isa::Operation::decimal_adjust_accumulator_a
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0x19U
                ) == hd6301v1_daa,
            "DAA operation, classification, or decoder lookup mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "DAA",
            AddressingMode::implied
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x19U)
                == nullptr,
        "Unreviewed MC6801 opcode 0x19 inherited DAA metadata"
    );

    const auto* hd6301v1_tap = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "TAP",
        AddressingMode::implied
    );
    passed &= expect(hd6301v1_tap != nullptr, "HD6301V1 TAP encoding missing");
    if (hd6301v1_tap != nullptr) {
        passed &= expect(hd6301v1_tap->opcode == 0x06, "TAP opcode mismatch");
        passed &= expect(
            hd6301v1_tap->operand_bytes == 0
                && hd6301v1_tap->instruction_length == 1,
            "TAP length mismatch"
        );
        passed &= expect(hd6301v1_tap->base_cycles == 1, "TAP cycle mismatch");
        const auto all_flags = mask({
            StatusFlag::h,
            StatusFlag::i,
            StatusFlag::n,
            StatusFlag::z,
            StatusFlag::v,
            StatusFlag::c,
        });
        passed &= expect(
            hd6301v1_tap->flags.read_mask == 0U
                && hd6301v1_tap->flags.written_mask == all_flags
                && hd6301v1_tap->flags.preserved_mask == 0U,
            "TAP flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_tap->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_tap),
            "TAP debugger classification mismatch"
        );
        passed &= expect(
            hd6301v1_tap->operation
                == jr800::isa::Operation::transfer_accumulator_a_to_condition_code,
            "TAP operation mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "TAP",
            AddressingMode::implied
        ) == nullptr,
        "Unreviewed MC6801 TAP metadata was inherited"
    );

    const auto* hd6301v1_tpa = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "TPA",
        AddressingMode::implied
    );
    passed &= expect(hd6301v1_tpa != nullptr, "HD6301V1 TPA encoding missing");
    if (hd6301v1_tpa != nullptr) {
        passed &= expect(hd6301v1_tpa->opcode == 0x07, "TPA opcode mismatch");
        passed &= expect(
            hd6301v1_tpa->operand_bytes == 0
                && hd6301v1_tpa->instruction_length == 1,
            "TPA length mismatch"
        );
        passed &= expect(hd6301v1_tpa->base_cycles == 1, "TPA cycle mismatch");
        const auto all_flags = mask({
            StatusFlag::h,
            StatusFlag::i,
            StatusFlag::n,
            StatusFlag::z,
            StatusFlag::v,
            StatusFlag::c,
        });
        passed &= expect(
            hd6301v1_tpa->flags.read_mask == all_flags
                && hd6301v1_tpa->flags.written_mask == 0U
                && hd6301v1_tpa->flags.preserved_mask == all_flags,
            "TPA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_tpa->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_tpa),
            "TPA debugger classification mismatch"
        );
        passed &= expect(
            hd6301v1_tpa->operation
                == jr800::isa::Operation::transfer_condition_code_to_accumulator_a,
            "TPA operation mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "TPA",
            AddressingMode::implied
        ) == nullptr,
        "Unreviewed MC6801 TPA metadata was inherited"
    );

    const auto* hd6301v1_inx = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "INX",
        AddressingMode::implied
    );
    passed &= expect(hd6301v1_inx != nullptr, "HD6301V1 INX encoding missing");
    if (hd6301v1_inx != nullptr) {
        passed &= expect(hd6301v1_inx->opcode == 0x08, "INX opcode mismatch");
        passed &= expect(hd6301v1_inx->instruction_length == 1, "INX length mismatch");
        passed &= expect(hd6301v1_inx->base_cycles == 1, "INX cycle mismatch");
        passed &= expect(
            hd6301v1_inx->flags.written_mask == mask({StatusFlag::z})
                && hd6301v1_inx->flags.preserved_mask
                    == mask({
                        StatusFlag::h,
                        StatusFlag::i,
                        StatusFlag::n,
                        StatusFlag::v,
                        StatusFlag::c,
                    }),
            "INX flag metadata mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "INX",
            AddressingMode::implied
        ) == nullptr,
        "Unreviewed MC6801 INX metadata was inherited"
    );

    const auto* hd6301v1_dex = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "DEX",
        AddressingMode::implied
    );
    passed &= expect(hd6301v1_dex != nullptr, "HD6301V1 DEX encoding missing");
    if (hd6301v1_dex != nullptr) {
        passed &= expect(hd6301v1_dex->opcode == 0x09, "DEX opcode mismatch");
        passed &= expect(hd6301v1_dex->instruction_length == 1, "DEX length mismatch");
        passed &= expect(hd6301v1_dex->base_cycles == 1, "DEX cycle mismatch");
        passed &= expect(
            hd6301v1_dex->flags.written_mask == mask({StatusFlag::z})
                && hd6301v1_dex->flags.preserved_mask
                    == mask({
                        StatusFlag::h,
                        StatusFlag::i,
                        StatusFlag::n,
                        StatusFlag::v,
                        StatusFlag::c,
                    }),
            "DEX flag metadata mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "DEX",
            AddressingMode::implied
        ) == nullptr,
        "Unreviewed MC6801 DEX metadata was inherited"
    );

    const auto* hd6301v1_tab = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "TAB",
        AddressingMode::implied
    );
    passed &= expect(hd6301v1_tab != nullptr, "HD6301V1 TAB encoding missing");
    if (hd6301v1_tab != nullptr) {
        passed &= expect(hd6301v1_tab->opcode == 0x16, "TAB opcode mismatch");
        passed &= expect(
            hd6301v1_tab->operand_bytes == 0
                && hd6301v1_tab->instruction_length == 1,
            "TAB length mismatch"
        );
        passed &= expect(hd6301v1_tab->base_cycles == 1, "TAB cycle mismatch");
        passed &= expect(
            hd6301v1_tab->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_tab->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c}),
            "TAB flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_tab->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_tab),
            "TAB debugger classification mismatch"
        );
        passed &= expect(
            hd6301v1_tab->operation
                == jr800::isa::Operation::transfer_accumulator_a_to_b,
            "TAB operation mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "TAB",
            AddressingMode::implied
        ) == nullptr,
        "Unreviewed MC6801 TAB metadata was inherited"
    );

    const auto* hd6301v1_tba = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "TBA",
        AddressingMode::implied
    );
    passed &= expect(hd6301v1_tba != nullptr, "HD6301V1 TBA encoding missing");
    if (hd6301v1_tba != nullptr) {
        passed &= expect(hd6301v1_tba->opcode == 0x17, "TBA opcode mismatch");
        passed &= expect(
            hd6301v1_tba->operand_bytes == 0
                && hd6301v1_tba->instruction_length == 1,
            "TBA length mismatch"
        );
        passed &= expect(hd6301v1_tba->base_cycles == 1, "TBA cycle mismatch");
        passed &= expect(
            hd6301v1_tba->flags.read_mask == 0U
                && hd6301v1_tba->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_tba->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c}),
            "TBA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_tba->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_tba)
                && hd6301v1_tba->operation
                    == jr800::isa::Operation::transfer_accumulator_b_to_a
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0x17
                ) == hd6301v1_tba,
            "TBA operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "TBA",
            AddressingMode::implied
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x17)
                == nullptr,
        "Unreviewed MC6801 TBA metadata was inherited"
    );

    const auto* hd6301v1_xgdx = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "XGDX",
        AddressingMode::implied
    );
    passed &= expect(hd6301v1_xgdx != nullptr, "HD6301V1 XGDX encoding missing");
    if (hd6301v1_xgdx != nullptr) {
        passed &= expect(hd6301v1_xgdx->opcode == 0x18, "XGDX opcode mismatch");
        passed &= expect(
            hd6301v1_xgdx->operand_bytes == 0
                && hd6301v1_xgdx->instruction_length == 1,
            "XGDX length mismatch"
        );
        passed &= expect(hd6301v1_xgdx->base_cycles == 2, "XGDX cycle mismatch");
        passed &= expect(
            hd6301v1_xgdx->flags.written_mask == 0U
                && hd6301v1_xgdx->flags.preserved_mask
                    == mask({
                        StatusFlag::h,
                        StatusFlag::i,
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    }),
            "XGDX flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_xgdx->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_xgdx),
            "XGDX debugger classification mismatch"
        );
        passed &= expect(
            hd6301v1_xgdx->operation
                == jr800::isa::Operation::
                    exchange_double_accumulator_and_index_register,
            "XGDX operation mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "XGDX",
            AddressingMode::implied
        ) == nullptr,
        "Unreviewed MC6801 XGDX metadata was inherited"
    );

    const auto* hd6301v1_aba = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "ABA",
        AddressingMode::implied
    );
    passed &= expect(hd6301v1_aba != nullptr, "HD6301V1 ABA metadata missing");
    if (hd6301v1_aba != nullptr) {
        passed &= expect(hd6301v1_aba->opcode == 0x1B, "ABA opcode mismatch");
        passed &= expect(
            hd6301v1_aba->operand_bytes == 0
                && hd6301v1_aba->instruction_length == 1,
            "ABA length mismatch"
        );
        passed &= expect(hd6301v1_aba->base_cycles == 1, "ABA cycle mismatch");
        passed &= expect(
            hd6301v1_aba->flags.read_mask == 0U
                && hd6301v1_aba->flags.written_mask
                    == mask({
                        StatusFlag::h,
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_aba->flags.preserved_mask
                    == mask({StatusFlag::i}),
            "ABA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_aba->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_aba)
                && hd6301v1_aba->operation
                    == jr800::isa::Operation::
                        add_accumulator_b_to_accumulator_a
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0x1B
                ) == hd6301v1_aba,
            "ABA operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "ABA",
            AddressingMode::implied
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x1B)
                == nullptr,
        "Unreviewed MC6801 ABA metadata was inherited"
    );

    const auto* hd6301v1_cba = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "CBA",
        AddressingMode::implied
    );
    passed &= expect(hd6301v1_cba != nullptr, "HD6301V1 CBA metadata missing");
    if (hd6301v1_cba != nullptr) {
        passed &= expect(hd6301v1_cba->opcode == 0x11, "CBA opcode mismatch");
        passed &= expect(
            hd6301v1_cba->operand_bytes == 0
                && hd6301v1_cba->instruction_length == 1,
            "CBA length mismatch"
        );
        passed &= expect(hd6301v1_cba->base_cycles == 1, "CBA cycle mismatch");
        passed &= expect(
            hd6301v1_cba->flags.read_mask == 0U
                && hd6301v1_cba->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_cba->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i})
                && hd6301v1_cba->flags.undefined_mask == 0U,
            "CBA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_cba->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_cba)
                && hd6301v1_cba->operation
                    == jr800::isa::Operation::compare_accumulators
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0x11
                ) == hd6301v1_cba,
            "CBA operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "CBA",
            AddressingMode::implied
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x11)
                == nullptr,
        "Unreviewed MC6801 CBA metadata was inherited"
    );

    const auto* hd6301v1_sba = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "SBA",
        AddressingMode::implied
    );
    passed &= expect(hd6301v1_sba != nullptr, "HD6301V1 SBA metadata missing");
    if (hd6301v1_sba != nullptr) {
        passed &= expect(hd6301v1_sba->opcode == 0x10, "SBA opcode mismatch");
        passed &= expect(
            hd6301v1_sba->operand_bytes == 0
                && hd6301v1_sba->instruction_length == 1,
            "SBA length mismatch"
        );
        passed &= expect(hd6301v1_sba->base_cycles == 1, "SBA cycle mismatch");
        passed &= expect(
            hd6301v1_sba->flags.read_mask == 0U
                && hd6301v1_sba->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_sba->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i})
                && hd6301v1_sba->flags.undefined_mask == 0U,
            "SBA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_sba->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_sba)
                && hd6301v1_sba->operation
                    == jr800::isa::Operation::
                        subtract_accumulator_b_from_accumulator_a
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0x10
                ) == hd6301v1_sba,
            "SBA operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "SBA",
            AddressingMode::implied
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x10)
                == nullptr,
        "Unreviewed MC6801 SBA metadata was inherited"
    );

    const auto* hd6301v1_lsrd = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "LSRD",
        AddressingMode::implied
    );
    passed &= expect(hd6301v1_lsrd != nullptr, "HD6301V1 LSRD metadata missing");
    if (hd6301v1_lsrd != nullptr) {
        passed &= expect(hd6301v1_lsrd->opcode == 0x04, "LSRD opcode mismatch");
        passed &= expect(
            hd6301v1_lsrd->operand_bytes == 0
                && hd6301v1_lsrd->instruction_length == 1,
            "LSRD length mismatch"
        );
        passed &= expect(
            hd6301v1_lsrd->base_cycles == 1,
            "LSRD cycle mismatch"
        );
        passed &= expect(
            hd6301v1_lsrd->flags.read_mask == 0U
                && hd6301v1_lsrd->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_lsrd->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i})
                && hd6301v1_lsrd->flags.undefined_mask == 0U,
            "LSRD flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_lsrd->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_lsrd)
                && hd6301v1_lsrd->operation
                    == jr800::isa::Operation::
                        logical_shift_right_double_accumulator
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0x04
                ) == hd6301v1_lsrd,
            "LSRD operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "LSRD",
            AddressingMode::implied
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x04)
                == nullptr,
        "Unreviewed MC6801 LSRD metadata was inherited"
    );

    const auto* hd6301v1_asld = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "ASLD",
        AddressingMode::implied
    );
    passed &= expect(hd6301v1_asld != nullptr, "HD6301V1 ASLD metadata missing");
    if (hd6301v1_asld != nullptr) {
        passed &= expect(hd6301v1_asld->opcode == 0x05, "ASLD opcode mismatch");
        passed &= expect(
            hd6301v1_asld->operand_bytes == 0
                && hd6301v1_asld->instruction_length == 1,
            "ASLD length mismatch"
        );
        passed &= expect(
            hd6301v1_asld->base_cycles == 1,
            "ASLD cycle mismatch"
        );
        passed &= expect(
            hd6301v1_asld->flags.read_mask == 0U
                && hd6301v1_asld->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_asld->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i})
                && hd6301v1_asld->flags.undefined_mask == 0U,
            "ASLD flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_asld->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_asld)
                && hd6301v1_asld->operation
                    == jr800::isa::Operation::
                        arithmetic_shift_left_double_accumulator
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0x05
                ) == hd6301v1_asld,
            "ASLD operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "ASLD",
            AddressingMode::implied
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x05)
                == nullptr,
        "Unreviewed MC6801 ASLD metadata was inherited"
    );

    const auto* hd6301v1_abx = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "ABX",
        AddressingMode::implied
    );
    passed &= expect(hd6301v1_abx != nullptr, "HD6301V1 ABX encoding missing");
    if (hd6301v1_abx != nullptr) {
        passed &= expect(hd6301v1_abx->opcode == 0x3A, "ABX opcode mismatch");
        passed &= expect(
            hd6301v1_abx->operand_bytes == 0
                && hd6301v1_abx->instruction_length == 1,
            "ABX length mismatch"
        );
        passed &= expect(hd6301v1_abx->base_cycles == 1, "ABX cycle mismatch");
        passed &= expect(
            hd6301v1_abx->flags.written_mask == 0U
                && hd6301v1_abx->flags.preserved_mask
                    == mask({
                        StatusFlag::h,
                        StatusFlag::i,
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    }),
            "ABX flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_abx->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_abx),
            "ABX debugger classification mismatch"
        );
        passed &= expect(
            hd6301v1_abx->operation
                == jr800::isa::Operation::add_accumulator_b_to_index_register,
            "ABX operation mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "ABX",
            AddressingMode::implied
        ) == nullptr,
        "Unreviewed MC6801 ABX metadata was inherited"
    );

    const auto* hd6301v1_stx_direct = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "STX",
        AddressingMode::direct8
    );
    passed &= expect(
        hd6301v1_stx_direct != nullptr,
        "HD6301V1 direct STX encoding missing"
    );
    if (hd6301v1_stx_direct != nullptr) {
        passed &= expect(
            hd6301v1_stx_direct->opcode == 0xDF,
            "Direct STX opcode mismatch"
        );
        passed &= expect(
            hd6301v1_stx_direct->instruction_length == 2,
            "Direct STX length mismatch"
        );
        passed &= expect(
            hd6301v1_stx_direct->base_cycles == 4,
            "Direct STX cycle mismatch"
        );
        passed &= expect(
            hd6301v1_stx_direct->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_stx_direct->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c}),
            "Direct STX flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_stx_direct->classification == InstructionClass::linear
                && hd6301v1_stx_direct->operation
                    == jr800::isa::Operation::store_index_register,
            "Direct STX debugger classification mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "STX",
            AddressingMode::direct8
        ) == nullptr,
        "Unreviewed MC6801 direct STX metadata was inherited"
    );

    const auto* hd6301v1_stx_indexed = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "STX",
        AddressingMode::indexed8
    );
    passed &= expect(
        hd6301v1_stx_indexed != nullptr,
        "HD6301V1 indexed STX encoding missing"
    );
    if (hd6301v1_stx_indexed != nullptr) {
        passed &= expect(
            hd6301v1_stx_indexed->opcode == 0xEFU
                && hd6301v1_stx_indexed->operand_bytes == 1U
                && hd6301v1_stx_indexed->instruction_length == 2U
                && hd6301v1_stx_indexed->base_cycles == 5U,
            "Indexed STX opcode, length, or cycle metadata mismatch"
        );
        passed &= expect(
            hd6301v1_stx_indexed->flags.read_mask == 0U
                && hd6301v1_stx_indexed->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_stx_indexed->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c})
                && hd6301v1_stx_indexed->flags.undefined_mask == 0U,
            "Indexed STX flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_stx_indexed->classification
                    == InstructionClass::linear
                && hd6301v1_stx_indexed->operation
                    == jr800::isa::Operation::store_index_register
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xEFU
                ) == hd6301v1_stx_indexed,
            "Indexed STX classification, operation, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "STX",
            AddressingMode::indexed8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xEFU)
                == nullptr,
        "Unreviewed MC6801 indexed STX metadata was inherited"
    );

    const auto* hd6301v1_stx_extended = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "STX",
        AddressingMode::extended16
    );
    passed &= expect(
        hd6301v1_stx_extended != nullptr,
        "HD6301V1 extended STX encoding missing"
    );
    if (hd6301v1_stx_extended != nullptr) {
        passed &= expect(
            hd6301v1_stx_extended->opcode == 0xFF,
            "Extended STX opcode mismatch"
        );
        passed &= expect(
            hd6301v1_stx_extended->operand_bytes == 2
                && hd6301v1_stx_extended->instruction_length == 3,
            "Extended STX length mismatch"
        );
        passed &= expect(
            hd6301v1_stx_extended->base_cycles == 5,
            "Extended STX cycle mismatch"
        );
        passed &= expect(
            hd6301v1_stx_extended->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_stx_extended->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c}),
            "Extended STX flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_stx_extended->classification
                    == InstructionClass::linear
                && hd6301v1_stx_extended->operation
                    == jr800::isa::Operation::store_index_register,
            "Extended STX classification or operation mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "STX",
            AddressingMode::extended16
        ) == nullptr,
        "Unreviewed MC6801 extended STX metadata was inherited"
    );

    const auto* hd6301v1_sts_direct = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "STS",
        AddressingMode::direct8
    );
    passed &= expect(
        hd6301v1_sts_direct != nullptr,
        "HD6301V1 direct STS encoding missing"
    );
    if (hd6301v1_sts_direct != nullptr) {
        passed &= expect(
            hd6301v1_sts_direct->opcode == 0x9F,
            "Direct STS opcode mismatch"
        );
        passed &= expect(
            hd6301v1_sts_direct->operand_bytes == 1
                && hd6301v1_sts_direct->instruction_length == 2,
            "Direct STS length mismatch"
        );
        passed &= expect(
            hd6301v1_sts_direct->base_cycles == 4,
            "Direct STS cycle mismatch"
        );
        passed &= expect(
            hd6301v1_sts_direct->flags.read_mask == 0U
                && hd6301v1_sts_direct->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_sts_direct->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c})
                && hd6301v1_sts_direct->flags.undefined_mask == 0U,
            "Direct STS flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_sts_direct->classification
                    == InstructionClass::linear
                && hd6301v1_sts_direct->operation
                    == jr800::isa::Operation::store_stack_pointer
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_sts_direct
                )
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0x9F
                ) == hd6301v1_sts_direct,
            "Direct STS operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "STS",
            AddressingMode::direct8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x9F)
                == nullptr,
        "Unreviewed MC6801 direct STS metadata was inherited"
    );
    struct StsAddressedMetadataCase {
        AddressingMode mode;
        std::uint8_t opcode;
        std::uint8_t operand_bytes;
        std::uint8_t instruction_length;
    };
    constexpr std::array sts_addressed_cases{
        StsAddressedMetadataCase{
            AddressingMode::indexed8,
            0xAFU,
            1U,
            2U,
        },
        StsAddressedMetadataCase{
            AddressingMode::extended16,
            0xBFU,
            2U,
            3U,
        },
    };
    for (const auto& test_case : sts_addressed_cases) {
        const auto* instruction = jr800::isa::find_encoding(
            CpuProfile::hd6301v1,
            "STS",
            test_case.mode
        );
        passed &= expect(
            instruction != nullptr,
            "HD6301V1 addressed STS encoding missing"
        );
        if (instruction != nullptr) {
            passed &= expect(
                instruction->opcode == test_case.opcode
                    && instruction->operand_bytes
                        == test_case.operand_bytes
                    && instruction->instruction_length
                        == test_case.instruction_length
                    && instruction->base_cycles == 5U
                    && instruction->flags.read_mask == 0U
                    && instruction->flags.written_mask
                        == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                    && instruction->flags.preserved_mask
                        == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c})
                    && instruction->flags.undefined_mask == 0U
                    && instruction->classification
                        == InstructionClass::linear
                    && instruction->operation
                        == jr800::isa::Operation::store_stack_pointer
                    && jr800::isa::decode_instruction(
                        CpuProfile::hd6301v1,
                        test_case.opcode
                    ) == instruction,
                "Addressed STS metadata differs"
            );
        }
        passed &= expect(
            jr800::isa::find_encoding(
                CpuProfile::mc6801,
                "STS",
                test_case.mode
            ) == nullptr
                && jr800::isa::decode_instruction(
                    CpuProfile::mc6801,
                    test_case.opcode
                ) == nullptr,
            "Unreviewed MC6801 addressed STS metadata was inherited"
        );
    }

    const auto* hd6301v1_std_direct = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "STD",
        AddressingMode::direct8
    );
    passed &= expect(
        hd6301v1_std_direct != nullptr,
        "HD6301V1 direct STD encoding missing"
    );
    if (hd6301v1_std_direct != nullptr) {
        passed &= expect(
            hd6301v1_std_direct->opcode == 0xDD,
            "Direct STD opcode mismatch"
        );
        passed &= expect(
            hd6301v1_std_direct->operand_bytes == 1
                && hd6301v1_std_direct->instruction_length == 2,
            "Direct STD length mismatch"
        );
        passed &= expect(
            hd6301v1_std_direct->base_cycles == 4,
            "Direct STD cycle mismatch"
        );
        passed &= expect(
            hd6301v1_std_direct->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_std_direct->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c}),
            "Direct STD flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_std_direct->classification == InstructionClass::linear,
            "Direct STD debugger classification mismatch"
        );
        passed &= expect(
            hd6301v1_std_direct->operation
                == jr800::isa::Operation::store_double_accumulator,
            "Direct STD operation mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "STD",
            AddressingMode::direct8
        ) == nullptr,
        "Unreviewed MC6801 direct STD metadata was inherited"
    );

    const auto* hd6301v1_std_indexed = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "STD",
        AddressingMode::indexed8
    );
    passed &= expect(
        hd6301v1_std_indexed != nullptr,
        "HD6301V1 indexed STD encoding missing"
    );
    if (hd6301v1_std_indexed != nullptr) {
        passed &= expect(
            hd6301v1_std_indexed->opcode == 0xED,
            "Indexed STD opcode mismatch"
        );
        passed &= expect(
            hd6301v1_std_indexed->operand_bytes == 1
                && hd6301v1_std_indexed->instruction_length == 2,
            "Indexed STD length mismatch"
        );
        passed &= expect(
            hd6301v1_std_indexed->base_cycles == 5,
            "Indexed STD cycle mismatch"
        );
        passed &= expect(
            hd6301v1_std_indexed->flags.read_mask == 0U
                && hd6301v1_std_indexed->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_std_indexed->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c})
                && hd6301v1_std_indexed->flags.undefined_mask == 0U,
            "Indexed STD flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_std_indexed->classification
                    == InstructionClass::linear
                && hd6301v1_std_indexed->operation
                    == jr800::isa::Operation::store_double_accumulator
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_std_indexed
                )
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xED
                ) == hd6301v1_std_indexed,
            "Indexed STD operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "STD",
            AddressingMode::indexed8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xED)
                == nullptr,
        "Unreviewed MC6801 indexed STD metadata was inherited"
    );

    const auto* hd6301v1_std_extended = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "STD",
        AddressingMode::extended16
    );
    passed &= expect(
        hd6301v1_std_extended != nullptr,
        "HD6301V1 extended STD encoding missing"
    );
    if (hd6301v1_std_extended != nullptr) {
        passed &= expect(
            hd6301v1_std_extended->opcode == 0xFD,
            "Extended STD opcode mismatch"
        );
        passed &= expect(
            hd6301v1_std_extended->operand_bytes == 2
                && hd6301v1_std_extended->instruction_length == 3,
            "Extended STD length mismatch"
        );
        passed &= expect(
            hd6301v1_std_extended->base_cycles == 5,
            "Extended STD cycle mismatch"
        );
        passed &= expect(
            hd6301v1_std_extended->flags.read_mask == 0U
                && hd6301v1_std_extended->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_std_extended->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c})
                && hd6301v1_std_extended->flags.undefined_mask == 0U,
            "Extended STD flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_std_extended->classification
                    == InstructionClass::linear
                && hd6301v1_std_extended->operation
                    == jr800::isa::Operation::store_double_accumulator
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_std_extended
                )
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xFD
                ) == hd6301v1_std_extended,
            "Extended STD operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "STD",
            AddressingMode::extended16
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xFD)
                == nullptr,
        "Unreviewed MC6801 extended STD metadata was inherited"
    );
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::hd6301v1,
            "STD",
            AddressingMode::immediate16
        ) == nullptr,
        "Unreviewed immediate STD metadata was staged"
    );

    const auto* hd6301v1_adda = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "ADDA",
        AddressingMode::immediate8
    );
    passed &= expect(hd6301v1_adda != nullptr, "HD6301V1 ADDA metadata missing");
    if (hd6301v1_adda != nullptr) {
        passed &= expect(hd6301v1_adda->opcode == 0x8B, "ADDA opcode mismatch");
        passed &= expect(
            hd6301v1_adda->operand_bytes == 1
                && hd6301v1_adda->instruction_length == 2,
            "ADDA length mismatch"
        );
        passed &= expect(hd6301v1_adda->base_cycles == 2, "ADDA cycle mismatch");
        passed &= expect(
            hd6301v1_adda->flags.read_mask == 0U
                && hd6301v1_adda->flags.written_mask
                    == mask({
                        StatusFlag::h,
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_adda->flags.preserved_mask
                    == mask({StatusFlag::i}),
            "ADDA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_adda->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_adda),
            "ADDA debugger classification mismatch"
        );
        passed &= expect(
            hd6301v1_adda->operation
                == jr800::isa::Operation::add_to_accumulator_a,
            "ADDA operation mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "ADDA",
            AddressingMode::immediate8
        ) == nullptr,
        "Unreviewed MC6801 ADDA metadata was inherited"
    );
    const auto* hd6301v1_adda_direct = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "ADDA",
        AddressingMode::direct8
    );
    passed &= expect(
        hd6301v1_adda_direct != nullptr,
        "HD6301V1 direct ADDA metadata missing"
    );
    if (hd6301v1_adda_direct != nullptr) {
        passed &= expect(
            hd6301v1_adda_direct->opcode == 0x9B,
            "Direct ADDA opcode mismatch"
        );
        passed &= expect(
            hd6301v1_adda_direct->operand_bytes == 1
                && hd6301v1_adda_direct->instruction_length == 2,
            "Direct ADDA length mismatch"
        );
        passed &= expect(
            hd6301v1_adda_direct->base_cycles == 3,
            "Direct ADDA cycle mismatch"
        );
        passed &= expect(
            hd6301v1_adda_direct->flags.read_mask == 0U
                && hd6301v1_adda_direct->flags.written_mask
                    == mask({
                        StatusFlag::h,
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_adda_direct->flags.preserved_mask
                    == mask({StatusFlag::i}),
            "Direct ADDA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_adda_direct->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_adda_direct
                )
                && hd6301v1_adda_direct->operation
                    == jr800::isa::Operation::add_to_accumulator_a
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0x9B
                ) == hd6301v1_adda_direct,
            "Direct ADDA operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "ADDA",
            AddressingMode::direct8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x9B)
                == nullptr,
        "Unreviewed MC6801 direct ADDA metadata was inherited"
    );
    const auto* hd6301v1_adda_indexed = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "ADDA",
        AddressingMode::indexed8
    );
    passed &= expect(
        hd6301v1_adda_indexed != nullptr,
        "HD6301V1 indexed ADDA metadata missing"
    );
    if (hd6301v1_adda_indexed != nullptr) {
        passed &= expect(
            hd6301v1_adda_indexed->opcode == 0xAB,
            "Indexed ADDA opcode mismatch"
        );
        passed &= expect(
            hd6301v1_adda_indexed->operand_bytes == 1
                && hd6301v1_adda_indexed->instruction_length == 2,
            "Indexed ADDA length mismatch"
        );
        passed &= expect(
            hd6301v1_adda_indexed->base_cycles == 4,
            "Indexed ADDA cycle mismatch"
        );
        passed &= expect(
            hd6301v1_adda_indexed->flags.read_mask == 0U
                && hd6301v1_adda_indexed->flags.written_mask
                    == mask({
                        StatusFlag::h,
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_adda_indexed->flags.preserved_mask
                    == mask({StatusFlag::i}),
            "Indexed ADDA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_adda_indexed->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_adda_indexed
                )
                && hd6301v1_adda_indexed->operation
                    == jr800::isa::Operation::add_to_accumulator_a
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xAB
                ) == hd6301v1_adda_indexed,
            "Indexed ADDA operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "ADDA",
            AddressingMode::indexed8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xAB)
                == nullptr,
        "Unreviewed MC6801 indexed ADDA metadata was inherited"
    );
    const auto* hd6301v1_adda_extended = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "ADDA",
        AddressingMode::extended16
    );
    passed &= expect(
        hd6301v1_adda_extended != nullptr,
        "HD6301V1 extended ADDA metadata missing"
    );
    if (hd6301v1_adda_extended != nullptr) {
        passed &= expect(
            hd6301v1_adda_extended->opcode == 0xBB,
            "Extended ADDA opcode mismatch"
        );
        passed &= expect(
            hd6301v1_adda_extended->operand_bytes == 2
                && hd6301v1_adda_extended->instruction_length == 3,
            "Extended ADDA length mismatch"
        );
        passed &= expect(
            hd6301v1_adda_extended->base_cycles == 4,
            "Extended ADDA cycle mismatch"
        );
        passed &= expect(
            hd6301v1_adda_extended->flags.read_mask == 0U
                && hd6301v1_adda_extended->flags.written_mask
                    == mask({
                        StatusFlag::h,
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_adda_extended->flags.preserved_mask
                    == mask({StatusFlag::i}),
            "Extended ADDA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_adda_extended->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_adda_extended
                )
                && hd6301v1_adda_extended->operation
                    == jr800::isa::Operation::add_to_accumulator_a
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xBB
                ) == hd6301v1_adda_extended,
            "Extended ADDA operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "ADDA",
            AddressingMode::extended16
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xBB)
                == nullptr,
        "Unreviewed MC6801 extended ADDA metadata was inherited"
    );

    const auto* hd6301v1_addb = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "ADDB",
        AddressingMode::immediate8
    );
    passed &= expect(hd6301v1_addb != nullptr, "HD6301V1 ADDB metadata missing");
    if (hd6301v1_addb != nullptr) {
        passed &= expect(hd6301v1_addb->opcode == 0xCB, "ADDB opcode mismatch");
        passed &= expect(
            hd6301v1_addb->operand_bytes == 1
                && hd6301v1_addb->instruction_length == 2,
            "ADDB length mismatch"
        );
        passed &= expect(hd6301v1_addb->base_cycles == 2, "ADDB cycle mismatch");
        passed &= expect(
            hd6301v1_addb->flags.read_mask == 0U
                && hd6301v1_addb->flags.written_mask
                    == mask({
                        StatusFlag::h,
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_addb->flags.preserved_mask
                    == mask({StatusFlag::i}),
            "ADDB flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_addb->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_addb),
            "ADDB debugger classification mismatch"
        );
        passed &= expect(
            hd6301v1_addb->operation
                == jr800::isa::Operation::add_to_accumulator_b,
            "ADDB operation mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "ADDB",
            AddressingMode::immediate8
        ) == nullptr,
        "Unreviewed MC6801 ADDB metadata was inherited"
    );
    const auto* hd6301v1_addb_direct = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "ADDB",
        AddressingMode::direct8
    );
    passed &= expect(
        hd6301v1_addb_direct != nullptr,
        "HD6301V1 direct ADDB metadata missing"
    );
    if (hd6301v1_addb_direct != nullptr) {
        passed &= expect(
            hd6301v1_addb_direct->opcode == 0xDB,
            "Direct ADDB opcode mismatch"
        );
        passed &= expect(
            hd6301v1_addb_direct->operand_bytes == 1
                && hd6301v1_addb_direct->instruction_length == 2,
            "Direct ADDB length mismatch"
        );
        passed &= expect(
            hd6301v1_addb_direct->base_cycles == 3,
            "Direct ADDB cycle mismatch"
        );
        passed &= expect(
            hd6301v1_addb_direct->flags.read_mask == 0U
                && hd6301v1_addb_direct->flags.written_mask
                    == mask({
                        StatusFlag::h,
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_addb_direct->flags.preserved_mask
                    == mask({StatusFlag::i}),
            "Direct ADDB flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_addb_direct->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_addb_direct
                )
                && hd6301v1_addb_direct->operation
                    == jr800::isa::Operation::add_to_accumulator_b
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xDB
                ) == hd6301v1_addb_direct,
            "Direct ADDB operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "ADDB",
            AddressingMode::direct8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xDB)
                == nullptr,
        "Unreviewed MC6801 direct ADDB metadata was inherited"
    );
    const auto* hd6301v1_addb_indexed = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "ADDB",
        AddressingMode::indexed8
    );
    passed &= expect(
        hd6301v1_addb_indexed != nullptr,
        "HD6301V1 indexed ADDB metadata missing"
    );
    if (hd6301v1_addb_indexed != nullptr) {
        passed &= expect(
            hd6301v1_addb_indexed->opcode == 0xEB,
            "Indexed ADDB opcode mismatch"
        );
        passed &= expect(
            hd6301v1_addb_indexed->operand_bytes == 1
                && hd6301v1_addb_indexed->instruction_length == 2,
            "Indexed ADDB length mismatch"
        );
        passed &= expect(
            hd6301v1_addb_indexed->base_cycles == 4,
            "Indexed ADDB cycle mismatch"
        );
        passed &= expect(
            hd6301v1_addb_indexed->flags.read_mask == 0U
                && hd6301v1_addb_indexed->flags.written_mask
                    == mask({
                        StatusFlag::h,
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_addb_indexed->flags.preserved_mask
                    == mask({StatusFlag::i}),
            "Indexed ADDB flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_addb_indexed->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_addb_indexed
                )
                && hd6301v1_addb_indexed->operation
                    == jr800::isa::Operation::add_to_accumulator_b
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xEB
                ) == hd6301v1_addb_indexed,
            "Indexed ADDB operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "ADDB",
            AddressingMode::indexed8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xEB)
                == nullptr,
        "Unreviewed MC6801 indexed ADDB metadata was inherited"
    );
    const auto* hd6301v1_addb_extended = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "ADDB",
        AddressingMode::extended16
    );
    passed &= expect(
        hd6301v1_addb_extended != nullptr,
        "HD6301V1 extended ADDB metadata missing"
    );
    if (hd6301v1_addb_extended != nullptr) {
        passed &= expect(
            hd6301v1_addb_extended->opcode == 0xFB,
            "Extended ADDB opcode mismatch"
        );
        passed &= expect(
            hd6301v1_addb_extended->operand_bytes == 2
                && hd6301v1_addb_extended->instruction_length == 3,
            "Extended ADDB length mismatch"
        );
        passed &= expect(
            hd6301v1_addb_extended->base_cycles == 4,
            "Extended ADDB cycle mismatch"
        );
        passed &= expect(
            hd6301v1_addb_extended->flags.read_mask == 0U
                && hd6301v1_addb_extended->flags.written_mask
                    == mask({
                        StatusFlag::h,
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_addb_extended->flags.preserved_mask
                    == mask({StatusFlag::i}),
            "Extended ADDB flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_addb_extended->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_addb_extended
                )
                && hd6301v1_addb_extended->operation
                    == jr800::isa::Operation::add_to_accumulator_b
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xFB
                ) == hd6301v1_addb_extended,
            "Extended ADDB operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "ADDB",
            AddressingMode::extended16
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xFB)
                == nullptr,
        "Unreviewed MC6801 extended ADDB metadata was inherited"
    );

    const auto* hd6301v1_adca = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "ADCA",
        AddressingMode::immediate8
    );
    passed &= expect(hd6301v1_adca != nullptr, "HD6301V1 ADCA metadata missing");
    if (hd6301v1_adca != nullptr) {
        passed &= expect(hd6301v1_adca->opcode == 0x89, "ADCA opcode mismatch");
        passed &= expect(
            hd6301v1_adca->operand_bytes == 1
                && hd6301v1_adca->instruction_length == 2,
            "ADCA length mismatch"
        );
        passed &= expect(hd6301v1_adca->base_cycles == 2, "ADCA cycle mismatch");
        passed &= expect(
            hd6301v1_adca->flags.read_mask == mask({StatusFlag::c})
                && hd6301v1_adca->flags.written_mask
                    == mask({
                        StatusFlag::h,
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_adca->flags.preserved_mask
                    == mask({StatusFlag::i}),
            "ADCA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_adca->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_adca)
                && hd6301v1_adca->operation
                    == jr800::isa::Operation::add_with_carry_accumulator_a
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0x89
                ) == hd6301v1_adca,
            "ADCA operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "ADCA",
            AddressingMode::immediate8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x89)
                == nullptr,
        "Unreviewed MC6801 ADCA metadata was inherited"
    );

    const auto* hd6301v1_adca_direct = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "ADCA",
        AddressingMode::direct8
    );
    passed &= expect(
        hd6301v1_adca_direct != nullptr,
        "HD6301V1 direct ADCA metadata missing"
    );
    if (hd6301v1_adca_direct != nullptr) {
        passed &= expect(
            hd6301v1_adca_direct->opcode == 0x99,
            "Direct ADCA opcode mismatch"
        );
        passed &= expect(
            hd6301v1_adca_direct->operand_bytes == 1
                && hd6301v1_adca_direct->instruction_length == 2,
            "Direct ADCA length mismatch"
        );
        passed &= expect(
            hd6301v1_adca_direct->base_cycles == 3,
            "Direct ADCA cycle mismatch"
        );
        passed &= expect(
            hd6301v1_adca_direct->flags.read_mask == mask({StatusFlag::c})
                && hd6301v1_adca_direct->flags.written_mask
                    == mask({
                        StatusFlag::h,
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_adca_direct->flags.preserved_mask
                    == mask({StatusFlag::i}),
            "Direct ADCA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_adca_direct->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_adca_direct
                )
                && hd6301v1_adca_direct->operation
                    == jr800::isa::Operation::add_with_carry_accumulator_a
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0x99
                ) == hd6301v1_adca_direct,
            "Direct ADCA operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "ADCA",
            AddressingMode::direct8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x99)
                == nullptr,
        "Unreviewed MC6801 direct ADCA metadata was inherited"
    );

    const auto* hd6301v1_adca_indexed = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "ADCA",
        AddressingMode::indexed8
    );
    passed &= expect(
        hd6301v1_adca_indexed != nullptr,
        "HD6301V1 indexed ADCA metadata missing"
    );
    if (hd6301v1_adca_indexed != nullptr) {
        passed &= expect(
            hd6301v1_adca_indexed->opcode == 0xA9,
            "Indexed ADCA opcode mismatch"
        );
        passed &= expect(
            hd6301v1_adca_indexed->operand_bytes == 1
                && hd6301v1_adca_indexed->instruction_length == 2,
            "Indexed ADCA length mismatch"
        );
        passed &= expect(
            hd6301v1_adca_indexed->base_cycles == 4,
            "Indexed ADCA cycle mismatch"
        );
        passed &= expect(
            hd6301v1_adca_indexed->flags.read_mask == mask({StatusFlag::c})
                && hd6301v1_adca_indexed->flags.written_mask
                    == mask({
                        StatusFlag::h,
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_adca_indexed->flags.preserved_mask
                    == mask({StatusFlag::i}),
            "Indexed ADCA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_adca_indexed->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_adca_indexed
                )
                && hd6301v1_adca_indexed->operation
                    == jr800::isa::Operation::add_with_carry_accumulator_a
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xA9
                ) == hd6301v1_adca_indexed,
            "Indexed ADCA operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "ADCA",
            AddressingMode::indexed8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xA9)
                == nullptr,
        "Unreviewed MC6801 indexed ADCA metadata was inherited"
    );
    const auto* hd6301v1_adca_extended = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "ADCA",
        AddressingMode::extended16
    );
    passed &= expect(
        hd6301v1_adca_extended != nullptr,
        "HD6301V1 extended ADCA metadata missing"
    );
    if (hd6301v1_adca_extended != nullptr) {
        passed &= expect(
            hd6301v1_adca_extended->opcode == 0xB9,
            "Extended ADCA opcode mismatch"
        );
        passed &= expect(
            hd6301v1_adca_extended->operand_bytes == 2
                && hd6301v1_adca_extended->instruction_length == 3,
            "Extended ADCA length mismatch"
        );
        passed &= expect(
            hd6301v1_adca_extended->base_cycles == 4,
            "Extended ADCA cycle mismatch"
        );
        passed &= expect(
            hd6301v1_adca_extended->flags.read_mask == mask({StatusFlag::c})
                && hd6301v1_adca_extended->flags.written_mask
                    == mask({
                        StatusFlag::h,
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_adca_extended->flags.preserved_mask
                    == mask({StatusFlag::i}),
            "Extended ADCA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_adca_extended->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_adca_extended
                )
                && hd6301v1_adca_extended->operation
                    == jr800::isa::Operation::add_with_carry_accumulator_a
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xB9
                ) == hd6301v1_adca_extended,
            "Extended ADCA operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "ADCA",
            AddressingMode::extended16
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xB9)
                == nullptr,
        "Unreviewed MC6801 extended ADCA metadata was inherited"
    );

    const auto* hd6301v1_adcb = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "ADCB",
        AddressingMode::immediate8
    );
    passed &= expect(hd6301v1_adcb != nullptr, "HD6301V1 ADCB metadata missing");
    if (hd6301v1_adcb != nullptr) {
        passed &= expect(hd6301v1_adcb->opcode == 0xC9, "ADCB opcode mismatch");
        passed &= expect(
            hd6301v1_adcb->operand_bytes == 1
                && hd6301v1_adcb->instruction_length == 2,
            "ADCB length mismatch"
        );
        passed &= expect(hd6301v1_adcb->base_cycles == 2, "ADCB cycle mismatch");
        passed &= expect(
            hd6301v1_adcb->flags.read_mask == mask({StatusFlag::c})
                && hd6301v1_adcb->flags.written_mask
                    == mask({
                        StatusFlag::h,
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_adcb->flags.preserved_mask
                    == mask({StatusFlag::i}),
            "ADCB flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_adcb->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_adcb)
                && hd6301v1_adcb->operation
                    == jr800::isa::Operation::add_with_carry_accumulator_b
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xC9
                ) == hd6301v1_adcb,
            "ADCB operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "ADCB",
            AddressingMode::immediate8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xC9)
                == nullptr,
        "Unreviewed MC6801 ADCB metadata was inherited"
    );
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "ADCB",
            AddressingMode::direct8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xD9)
                == nullptr,
        "Unreviewed MC6801 direct ADCB metadata was inherited"
    );

    const auto* hd6301v1_adcb_direct = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "ADCB",
        AddressingMode::direct8
    );
    passed &= expect(
        hd6301v1_adcb_direct != nullptr,
        "HD6301V1 direct ADCB metadata missing"
    );
    if (hd6301v1_adcb_direct != nullptr) {
        passed &= expect(
            hd6301v1_adcb_direct->opcode == 0xD9,
            "Direct ADCB opcode mismatch"
        );
        passed &= expect(
            hd6301v1_adcb_direct->operand_bytes == 1
                && hd6301v1_adcb_direct->instruction_length == 2,
            "Direct ADCB length mismatch"
        );
        passed &= expect(
            hd6301v1_adcb_direct->base_cycles == 3,
            "Direct ADCB cycle mismatch"
        );
        passed &= expect(
            hd6301v1_adcb_direct->flags.read_mask == mask({StatusFlag::c})
                && hd6301v1_adcb_direct->flags.written_mask
                    == mask({
                        StatusFlag::h,
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_adcb_direct->flags.preserved_mask
                    == mask({StatusFlag::i}),
            "Direct ADCB flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_adcb_direct->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_adcb_direct
                )
                && hd6301v1_adcb_direct->operation
                    == jr800::isa::Operation::add_with_carry_accumulator_b
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xD9
                ) == hd6301v1_adcb_direct,
            "Direct ADCB operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "ADCB",
            AddressingMode::indexed8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xE9)
                == nullptr,
        "Unreviewed MC6801 indexed ADCB metadata was inherited"
    );

    const auto* hd6301v1_adcb_indexed = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "ADCB",
        AddressingMode::indexed8
    );
    passed &= expect(
        hd6301v1_adcb_indexed != nullptr,
        "HD6301V1 indexed ADCB metadata missing"
    );
    if (hd6301v1_adcb_indexed != nullptr) {
        passed &= expect(
            hd6301v1_adcb_indexed->opcode == 0xE9,
            "Indexed ADCB opcode mismatch"
        );
        passed &= expect(
            hd6301v1_adcb_indexed->operand_bytes == 1
                && hd6301v1_adcb_indexed->instruction_length == 2,
            "Indexed ADCB length mismatch"
        );
        passed &= expect(
            hd6301v1_adcb_indexed->base_cycles == 4,
            "Indexed ADCB cycle mismatch"
        );
        passed &= expect(
            hd6301v1_adcb_indexed->flags.read_mask == mask({StatusFlag::c})
                && hd6301v1_adcb_indexed->flags.written_mask
                    == mask({
                        StatusFlag::h,
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_adcb_indexed->flags.preserved_mask
                    == mask({StatusFlag::i}),
            "Indexed ADCB flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_adcb_indexed->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_adcb_indexed
                )
                && hd6301v1_adcb_indexed->operation
                    == jr800::isa::Operation::add_with_carry_accumulator_b
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xE9
                ) == hd6301v1_adcb_indexed,
            "Indexed ADCB operation, classification, or decode mismatch"
        );
    }
    const auto* hd6301v1_adcb_extended = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "ADCB",
        AddressingMode::extended16
    );
    passed &= expect(
        hd6301v1_adcb_extended != nullptr,
        "HD6301V1 extended ADCB metadata missing"
    );
    if (hd6301v1_adcb_extended != nullptr) {
        passed &= expect(
            hd6301v1_adcb_extended->opcode == 0xF9,
            "Extended ADCB opcode mismatch"
        );
        passed &= expect(
            hd6301v1_adcb_extended->operand_bytes == 2
                && hd6301v1_adcb_extended->instruction_length == 3,
            "Extended ADCB length mismatch"
        );
        passed &= expect(
            hd6301v1_adcb_extended->base_cycles == 4,
            "Extended ADCB cycle mismatch"
        );
        passed &= expect(
            hd6301v1_adcb_extended->flags.read_mask == mask({StatusFlag::c})
                && hd6301v1_adcb_extended->flags.written_mask
                    == mask({
                        StatusFlag::h,
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_adcb_extended->flags.preserved_mask
                    == mask({StatusFlag::i}),
            "Extended ADCB flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_adcb_extended->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_adcb_extended
                )
                && hd6301v1_adcb_extended->operation
                    == jr800::isa::Operation::add_with_carry_accumulator_b
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xF9
                ) == hd6301v1_adcb_extended,
            "Extended ADCB operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "ADCB",
            AddressingMode::extended16
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xF9)
                == nullptr,
        "Unreviewed MC6801 extended ADCB metadata was inherited"
    );

    const auto* hd6301v1_anda = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "ANDA",
        AddressingMode::immediate8
    );
    passed &= expect(hd6301v1_anda != nullptr, "HD6301V1 ANDA metadata missing");
    if (hd6301v1_anda != nullptr) {
        passed &= expect(hd6301v1_anda->opcode == 0x84, "ANDA opcode mismatch");
        passed &= expect(
            hd6301v1_anda->operand_bytes == 1
                && hd6301v1_anda->instruction_length == 2,
            "ANDA length mismatch"
        );
        passed &= expect(hd6301v1_anda->base_cycles == 2, "ANDA cycle mismatch");
        passed &= expect(
            hd6301v1_anda->flags.read_mask == 0U
                && hd6301v1_anda->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                    })
                && hd6301v1_anda->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c}),
            "ANDA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_anda->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_anda)
                && hd6301v1_anda->operation
                    == jr800::isa::Operation::logical_and_accumulator_a
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0x84
                ) == hd6301v1_anda,
            "ANDA operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "ANDA",
            AddressingMode::immediate8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x84)
                == nullptr,
        "Unreviewed MC6801 ANDA metadata was inherited"
    );
    const auto* hd6301v1_anda_direct = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "ANDA",
        AddressingMode::direct8
    );
    passed &= expect(
        hd6301v1_anda_direct != nullptr,
        "HD6301V1 direct ANDA metadata missing"
    );
    if (hd6301v1_anda_direct != nullptr) {
        passed &= expect(
            hd6301v1_anda_direct->opcode == 0x94,
            "Direct ANDA opcode mismatch"
        );
        passed &= expect(
            hd6301v1_anda_direct->operand_bytes == 1
                && hd6301v1_anda_direct->instruction_length == 2,
            "Direct ANDA length mismatch"
        );
        passed &= expect(
            hd6301v1_anda_direct->base_cycles == 3,
            "Direct ANDA cycle mismatch"
        );
        passed &= expect(
            hd6301v1_anda_direct->flags.read_mask == 0U
                && hd6301v1_anda_direct->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_anda_direct->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c}),
            "Direct ANDA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_anda_direct->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_anda_direct
                )
                && hd6301v1_anda_direct->operation
                    == jr800::isa::Operation::logical_and_accumulator_a
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0x94
                ) == hd6301v1_anda_direct,
            "Direct ANDA operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "ANDA",
            AddressingMode::direct8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x94)
                == nullptr,
        "Unreviewed MC6801 direct ANDA metadata was inherited"
    );
    const auto* hd6301v1_anda_indexed = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "ANDA",
        AddressingMode::indexed8
    );
    passed &= expect(
        hd6301v1_anda_indexed != nullptr,
        "HD6301V1 indexed ANDA metadata missing"
    );
    if (hd6301v1_anda_indexed != nullptr) {
        passed &= expect(
            hd6301v1_anda_indexed->opcode == 0xA4,
            "Indexed ANDA opcode mismatch"
        );
        passed &= expect(
            hd6301v1_anda_indexed->operand_bytes == 1
                && hd6301v1_anda_indexed->instruction_length == 2,
            "Indexed ANDA length mismatch"
        );
        passed &= expect(
            hd6301v1_anda_indexed->base_cycles == 4,
            "Indexed ANDA cycle mismatch"
        );
        passed &= expect(
            hd6301v1_anda_indexed->flags.read_mask == 0U
                && hd6301v1_anda_indexed->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_anda_indexed->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c}),
            "Indexed ANDA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_anda_indexed->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_anda_indexed
                )
                && hd6301v1_anda_indexed->operation
                    == jr800::isa::Operation::logical_and_accumulator_a
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xA4
                ) == hd6301v1_anda_indexed,
            "Indexed ANDA operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "ANDA",
            AddressingMode::indexed8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xA4)
                == nullptr,
        "Unreviewed MC6801 indexed ANDA metadata was inherited"
    );
    const auto* hd6301v1_anda_extended = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "ANDA",
        AddressingMode::extended16
    );
    passed &= expect(
        hd6301v1_anda_extended != nullptr,
        "HD6301V1 extended ANDA metadata missing"
    );
    if (hd6301v1_anda_extended != nullptr) {
        passed &= expect(
            hd6301v1_anda_extended->opcode == 0xB4,
            "Extended ANDA opcode mismatch"
        );
        passed &= expect(
            hd6301v1_anda_extended->operand_bytes == 2
                && hd6301v1_anda_extended->instruction_length == 3,
            "Extended ANDA length mismatch"
        );
        passed &= expect(
            hd6301v1_anda_extended->base_cycles == 4,
            "Extended ANDA cycle mismatch"
        );
        passed &= expect(
            hd6301v1_anda_extended->flags.read_mask == 0U
                && hd6301v1_anda_extended->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_anda_extended->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c}),
            "Extended ANDA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_anda_extended->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_anda_extended
                )
                && hd6301v1_anda_extended->operation
                    == jr800::isa::Operation::logical_and_accumulator_a
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xB4
                ) == hd6301v1_anda_extended,
            "Extended ANDA operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "ANDA",
            AddressingMode::extended16
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xB4)
                == nullptr,
        "Unreviewed MC6801 extended ANDA metadata was inherited"
    );

    const auto* hd6301v1_andb = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "ANDB",
        AddressingMode::immediate8
    );
    passed &= expect(hd6301v1_andb != nullptr, "HD6301V1 ANDB metadata missing");
    if (hd6301v1_andb != nullptr) {
        passed &= expect(hd6301v1_andb->opcode == 0xC4, "ANDB opcode mismatch");
        passed &= expect(
            hd6301v1_andb->operand_bytes == 1
                && hd6301v1_andb->instruction_length == 2,
            "ANDB length mismatch"
        );
        passed &= expect(hd6301v1_andb->base_cycles == 2, "ANDB cycle mismatch");
        passed &= expect(
            hd6301v1_andb->flags.read_mask == 0U
                && hd6301v1_andb->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                    })
                && hd6301v1_andb->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c}),
            "ANDB flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_andb->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_andb)
                && hd6301v1_andb->operation
                    == jr800::isa::Operation::logical_and_accumulator_b
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xC4
                ) == hd6301v1_andb,
            "ANDB operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "ANDB",
            AddressingMode::immediate8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xC4)
                == nullptr,
        "Unreviewed MC6801 ANDB metadata was inherited"
    );
    const auto* hd6301v1_andb_direct = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "ANDB",
        AddressingMode::direct8
    );
    passed &= expect(
        hd6301v1_andb_direct != nullptr,
        "HD6301V1 direct ANDB metadata missing"
    );
    if (hd6301v1_andb_direct != nullptr) {
        passed &= expect(
            hd6301v1_andb_direct->opcode == 0xD4,
            "Direct ANDB opcode mismatch"
        );
        passed &= expect(
            hd6301v1_andb_direct->operand_bytes == 1
                && hd6301v1_andb_direct->instruction_length == 2,
            "Direct ANDB length mismatch"
        );
        passed &= expect(
            hd6301v1_andb_direct->base_cycles == 3,
            "Direct ANDB cycle mismatch"
        );
        passed &= expect(
            hd6301v1_andb_direct->flags.read_mask == 0U
                && hd6301v1_andb_direct->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_andb_direct->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c}),
            "Direct ANDB flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_andb_direct->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_andb_direct
                )
                && hd6301v1_andb_direct->operation
                    == jr800::isa::Operation::logical_and_accumulator_b
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xD4
                ) == hd6301v1_andb_direct,
            "Direct ANDB operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "ANDB",
            AddressingMode::direct8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xD4)
                == nullptr,
        "Unreviewed MC6801 direct ANDB metadata was inherited"
    );
    const auto* hd6301v1_andb_indexed = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "ANDB",
        AddressingMode::indexed8
    );
    passed &= expect(
        hd6301v1_andb_indexed != nullptr,
        "HD6301V1 indexed ANDB metadata missing"
    );
    if (hd6301v1_andb_indexed != nullptr) {
        passed &= expect(
            hd6301v1_andb_indexed->opcode == 0xE4,
            "Indexed ANDB opcode mismatch"
        );
        passed &= expect(
            hd6301v1_andb_indexed->operand_bytes == 1
                && hd6301v1_andb_indexed->instruction_length == 2,
            "Indexed ANDB length mismatch"
        );
        passed &= expect(
            hd6301v1_andb_indexed->base_cycles == 4,
            "Indexed ANDB cycle mismatch"
        );
        passed &= expect(
            hd6301v1_andb_indexed->flags.read_mask == 0U
                && hd6301v1_andb_indexed->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_andb_indexed->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c}),
            "Indexed ANDB flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_andb_indexed->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_andb_indexed
                )
                && hd6301v1_andb_indexed->operation
                    == jr800::isa::Operation::logical_and_accumulator_b
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xE4
                ) == hd6301v1_andb_indexed,
            "Indexed ANDB operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "ANDB",
            AddressingMode::indexed8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xE4)
                == nullptr,
        "Unreviewed MC6801 indexed ANDB metadata was inherited"
    );
    const auto* hd6301v1_andb_extended = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "ANDB",
        AddressingMode::extended16
    );
    passed &= expect(
        hd6301v1_andb_extended != nullptr,
        "HD6301V1 extended ANDB metadata missing"
    );
    if (hd6301v1_andb_extended != nullptr) {
        passed &= expect(
            hd6301v1_andb_extended->opcode == 0xF4,
            "Extended ANDB opcode mismatch"
        );
        passed &= expect(
            hd6301v1_andb_extended->operand_bytes == 2
                && hd6301v1_andb_extended->instruction_length == 3,
            "Extended ANDB length mismatch"
        );
        passed &= expect(
            hd6301v1_andb_extended->base_cycles == 4,
            "Extended ANDB cycle mismatch"
        );
        passed &= expect(
            hd6301v1_andb_extended->flags.read_mask == 0U
                && hd6301v1_andb_extended->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_andb_extended->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c}),
            "Extended ANDB flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_andb_extended->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_andb_extended
                )
                && hd6301v1_andb_extended->operation
                    == jr800::isa::Operation::logical_and_accumulator_b
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xF4
                ) == hd6301v1_andb_extended,
            "Extended ANDB operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "ANDB",
            AddressingMode::extended16
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xF4)
                == nullptr,
        "Unreviewed MC6801 extended ANDB metadata was inherited"
    );

    const auto* hd6301v1_bita = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "BITA",
        AddressingMode::immediate8
    );
    passed &= expect(
        hd6301v1_bita != nullptr,
        "HD6301V1 BITA metadata missing"
    );
    if (hd6301v1_bita != nullptr) {
        passed &= expect(
            hd6301v1_bita->opcode == 0x85,
            "BITA opcode mismatch"
        );
        passed &= expect(
            hd6301v1_bita->operand_bytes == 1
                && hd6301v1_bita->instruction_length == 2,
            "BITA length mismatch"
        );
        passed &= expect(
            hd6301v1_bita->base_cycles == 2,
            "BITA cycle mismatch"
        );
        passed &= expect(
            hd6301v1_bita->flags.read_mask == 0U
                && hd6301v1_bita->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                    })
                && hd6301v1_bita->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c}),
            "BITA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_bita->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_bita)
                && hd6301v1_bita->operation
                    == jr800::isa::Operation::bit_test_accumulator_a
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0x85
                ) == hd6301v1_bita,
            "BITA operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "BITA",
            AddressingMode::immediate8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x85)
                == nullptr,
        "Unreviewed MC6801 BITA metadata was inherited"
    );
    const auto* hd6301v1_bita_direct = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "BITA",
        AddressingMode::direct8
    );
    passed &= expect(
        hd6301v1_bita_direct != nullptr,
        "HD6301V1 direct BITA metadata missing"
    );
    if (hd6301v1_bita_direct != nullptr) {
        passed &= expect(
            hd6301v1_bita_direct->opcode == 0x95,
            "Direct BITA opcode mismatch"
        );
        passed &= expect(
            hd6301v1_bita_direct->operand_bytes == 1
                && hd6301v1_bita_direct->instruction_length == 2,
            "Direct BITA length mismatch"
        );
        passed &= expect(
            hd6301v1_bita_direct->base_cycles == 3,
            "Direct BITA cycle mismatch"
        );
        passed &= expect(
            hd6301v1_bita_direct->flags.read_mask == 0U
                && hd6301v1_bita_direct->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_bita_direct->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c}),
            "Direct BITA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_bita_direct->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_bita_direct
                )
                && hd6301v1_bita_direct->operation
                    == jr800::isa::Operation::bit_test_accumulator_a
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0x95
                ) == hd6301v1_bita_direct,
            "Direct BITA operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "BITA",
            AddressingMode::direct8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x95)
                == nullptr,
        "Unreviewed MC6801 direct BITA metadata was inherited"
    );
    const auto* hd6301v1_bita_indexed = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "BITA",
        AddressingMode::indexed8
    );
    passed &= expect(
        hd6301v1_bita_indexed != nullptr,
        "HD6301V1 indexed BITA metadata missing"
    );
    if (hd6301v1_bita_indexed != nullptr) {
        passed &= expect(
            hd6301v1_bita_indexed->opcode == 0xA5,
            "Indexed BITA opcode mismatch"
        );
        passed &= expect(
            hd6301v1_bita_indexed->operand_bytes == 1
                && hd6301v1_bita_indexed->instruction_length == 2,
            "Indexed BITA length mismatch"
        );
        passed &= expect(
            hd6301v1_bita_indexed->base_cycles == 4,
            "Indexed BITA cycle mismatch"
        );
        passed &= expect(
            hd6301v1_bita_indexed->flags.read_mask == 0U
                && hd6301v1_bita_indexed->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_bita_indexed->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c}),
            "Indexed BITA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_bita_indexed->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_bita_indexed
                )
                && hd6301v1_bita_indexed->operation
                    == jr800::isa::Operation::bit_test_accumulator_a
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xA5
                ) == hd6301v1_bita_indexed,
            "Indexed BITA operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "BITA",
            AddressingMode::indexed8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xA5)
                == nullptr,
        "Unreviewed MC6801 indexed BITA metadata was inherited"
    );
    const auto* hd6301v1_bita_extended = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "BITA",
        AddressingMode::extended16
    );
    passed &= expect(
        hd6301v1_bita_extended != nullptr,
        "HD6301V1 extended BITA metadata missing"
    );
    if (hd6301v1_bita_extended != nullptr) {
        passed &= expect(
            hd6301v1_bita_extended->opcode == 0xB5,
            "Extended BITA opcode mismatch"
        );
        passed &= expect(
            hd6301v1_bita_extended->operand_bytes == 2
                && hd6301v1_bita_extended->instruction_length == 3,
            "Extended BITA length mismatch"
        );
        passed &= expect(
            hd6301v1_bita_extended->base_cycles == 4,
            "Extended BITA cycle mismatch"
        );
        passed &= expect(
            hd6301v1_bita_extended->flags.read_mask == 0U
                && hd6301v1_bita_extended->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_bita_extended->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c}),
            "Extended BITA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_bita_extended->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_bita_extended
                )
                && hd6301v1_bita_extended->operation
                    == jr800::isa::Operation::bit_test_accumulator_a
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xB5
                ) == hd6301v1_bita_extended,
            "Extended BITA operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "BITA",
            AddressingMode::extended16
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xB5)
                == nullptr,
        "Unreviewed MC6801 extended BITA metadata was inherited"
    );

    const auto* hd6301v1_bitb = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "BITB",
        AddressingMode::immediate8
    );
    passed &= expect(hd6301v1_bitb != nullptr, "HD6301V1 BITB metadata missing");
    if (hd6301v1_bitb != nullptr) {
        passed &= expect(hd6301v1_bitb->opcode == 0xC5, "BITB opcode mismatch");
        passed &= expect(
            hd6301v1_bitb->operand_bytes == 1
                && hd6301v1_bitb->instruction_length == 2,
            "BITB length mismatch"
        );
        passed &= expect(hd6301v1_bitb->base_cycles == 2, "BITB cycle mismatch");
        passed &= expect(
            hd6301v1_bitb->flags.read_mask == 0U
                && hd6301v1_bitb->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                    })
                && hd6301v1_bitb->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c}),
            "BITB flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_bitb->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_bitb)
                && hd6301v1_bitb->operation
                    == jr800::isa::Operation::bit_test_accumulator_b
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xC5
                ) == hd6301v1_bitb,
            "BITB operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "BITB",
            AddressingMode::immediate8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xC5)
                == nullptr,
        "Unreviewed MC6801 BITB metadata was inherited"
    );
    const auto* hd6301v1_bitb_direct = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "BITB",
        AddressingMode::direct8
    );
    passed &= expect(
        hd6301v1_bitb_direct != nullptr,
        "HD6301V1 direct BITB metadata missing"
    );
    if (hd6301v1_bitb_direct != nullptr) {
        passed &= expect(
            hd6301v1_bitb_direct->opcode == 0xD5,
            "Direct BITB opcode mismatch"
        );
        passed &= expect(
            hd6301v1_bitb_direct->operand_bytes == 1
                && hd6301v1_bitb_direct->instruction_length == 2,
            "Direct BITB length mismatch"
        );
        passed &= expect(
            hd6301v1_bitb_direct->base_cycles == 3,
            "Direct BITB cycle mismatch"
        );
        passed &= expect(
            hd6301v1_bitb_direct->flags.read_mask == 0U
                && hd6301v1_bitb_direct->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_bitb_direct->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c}),
            "Direct BITB flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_bitb_direct->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_bitb_direct
                )
                && hd6301v1_bitb_direct->operation
                    == jr800::isa::Operation::bit_test_accumulator_b
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xD5
                ) == hd6301v1_bitb_direct,
            "Direct BITB operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "BITB",
            AddressingMode::direct8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xD5)
                == nullptr,
        "Unreviewed MC6801 direct BITB metadata was inherited"
    );
    const auto* hd6301v1_bitb_indexed = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "BITB",
        AddressingMode::indexed8
    );
    passed &= expect(
        hd6301v1_bitb_indexed != nullptr,
        "HD6301V1 indexed BITB metadata missing"
    );
    if (hd6301v1_bitb_indexed != nullptr) {
        passed &= expect(
            hd6301v1_bitb_indexed->opcode == 0xE5,
            "Indexed BITB opcode mismatch"
        );
        passed &= expect(
            hd6301v1_bitb_indexed->operand_bytes == 1
                && hd6301v1_bitb_indexed->instruction_length == 2,
            "Indexed BITB length mismatch"
        );
        passed &= expect(
            hd6301v1_bitb_indexed->base_cycles == 4,
            "Indexed BITB cycle mismatch"
        );
        passed &= expect(
            hd6301v1_bitb_indexed->flags.read_mask == 0U
                && hd6301v1_bitb_indexed->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_bitb_indexed->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c}),
            "Indexed BITB flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_bitb_indexed->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_bitb_indexed
                )
                && hd6301v1_bitb_indexed->operation
                    == jr800::isa::Operation::bit_test_accumulator_b
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xE5
                ) == hd6301v1_bitb_indexed,
            "Indexed BITB operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "BITB",
            AddressingMode::indexed8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xE5)
                == nullptr,
        "Unreviewed MC6801 indexed BITB metadata was inherited"
    );
    const auto* hd6301v1_bitb_extended = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "BITB",
        AddressingMode::extended16
    );
    passed &= expect(
        hd6301v1_bitb_extended != nullptr,
        "HD6301V1 extended BITB metadata missing"
    );
    if (hd6301v1_bitb_extended != nullptr) {
        passed &= expect(
            hd6301v1_bitb_extended->opcode == 0xF5,
            "Extended BITB opcode mismatch"
        );
        passed &= expect(
            hd6301v1_bitb_extended->operand_bytes == 2
                && hd6301v1_bitb_extended->instruction_length == 3,
            "Extended BITB length mismatch"
        );
        passed &= expect(
            hd6301v1_bitb_extended->base_cycles == 4,
            "Extended BITB cycle mismatch"
        );
        passed &= expect(
            hd6301v1_bitb_extended->flags.read_mask == 0U
                && hd6301v1_bitb_extended->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_bitb_extended->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c}),
            "Extended BITB flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_bitb_extended->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_bitb_extended
                )
                && hd6301v1_bitb_extended->operation
                    == jr800::isa::Operation::bit_test_accumulator_b
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xF5
                ) == hd6301v1_bitb_extended,
            "Extended BITB operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "BITB",
            AddressingMode::extended16
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xF5)
                == nullptr,
        "Unreviewed MC6801 extended BITB metadata was inherited"
    );

    const auto* hd6301v1_eora_immediate = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "EORA",
        AddressingMode::immediate8
    );
    passed &= expect(
        hd6301v1_eora_immediate != nullptr,
        "HD6301V1 immediate EORA metadata missing"
    );
    if (hd6301v1_eora_immediate != nullptr) {
        passed &= expect(
            hd6301v1_eora_immediate->opcode == 0x88,
            "Immediate EORA opcode mismatch"
        );
        passed &= expect(
            hd6301v1_eora_immediate->operand_bytes == 1
                && hd6301v1_eora_immediate->instruction_length == 2,
            "Immediate EORA length mismatch"
        );
        passed &= expect(
            hd6301v1_eora_immediate->base_cycles == 2,
            "Immediate EORA cycle mismatch"
        );
        passed &= expect(
            hd6301v1_eora_immediate->flags.read_mask == 0U
                && hd6301v1_eora_immediate->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_eora_immediate->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c})
                && hd6301v1_eora_immediate->flags.undefined_mask == 0U,
            "Immediate EORA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_eora_immediate->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_eora_immediate
                )
                && hd6301v1_eora_immediate->operation
                    == jr800::isa::Operation::exclusive_or_accumulator_a
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0x88
                ) == hd6301v1_eora_immediate,
            "Immediate EORA operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "EORA",
            AddressingMode::immediate8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x88)
                == nullptr,
        "Unreviewed MC6801 immediate EORA metadata was inherited"
    );

    const auto* hd6301v1_eora_direct = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "EORA",
        AddressingMode::direct8
    );
    passed &= expect(
        hd6301v1_eora_direct != nullptr,
        "HD6301V1 direct EORA metadata missing"
    );
    if (hd6301v1_eora_direct != nullptr) {
        passed &= expect(
            hd6301v1_eora_direct->opcode == 0x98,
            "Direct EORA opcode mismatch"
        );
        passed &= expect(
            hd6301v1_eora_direct->operand_bytes == 1
                && hd6301v1_eora_direct->instruction_length == 2,
            "Direct EORA length mismatch"
        );
        passed &= expect(
            hd6301v1_eora_direct->base_cycles == 3,
            "Direct EORA cycle mismatch"
        );
        passed &= expect(
            hd6301v1_eora_direct->flags.read_mask == 0U
                && hd6301v1_eora_direct->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_eora_direct->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c}),
            "Direct EORA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_eora_direct->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_eora_direct
                )
                && hd6301v1_eora_direct->operation
                    == jr800::isa::Operation::exclusive_or_accumulator_a
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0x98
                ) == hd6301v1_eora_direct,
            "Direct EORA operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "EORA",
            AddressingMode::direct8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x98)
                == nullptr,
        "Unreviewed MC6801 direct EORA metadata was inherited"
    );
    const auto* hd6301v1_eora_indexed = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "EORA",
        AddressingMode::indexed8
    );
    passed &= expect(
        hd6301v1_eora_indexed != nullptr,
        "HD6301V1 indexed EORA metadata missing"
    );
    if (hd6301v1_eora_indexed != nullptr) {
        passed &= expect(
            hd6301v1_eora_indexed->opcode == 0xA8,
            "Indexed EORA opcode mismatch"
        );
        passed &= expect(
            hd6301v1_eora_indexed->operand_bytes == 1
                && hd6301v1_eora_indexed->instruction_length == 2,
            "Indexed EORA length mismatch"
        );
        passed &= expect(
            hd6301v1_eora_indexed->base_cycles == 4,
            "Indexed EORA cycle mismatch"
        );
        passed &= expect(
            hd6301v1_eora_indexed->flags.read_mask == 0U
                && hd6301v1_eora_indexed->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_eora_indexed->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c}),
            "Indexed EORA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_eora_indexed->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_eora_indexed
                )
                && hd6301v1_eora_indexed->operation
                    == jr800::isa::Operation::exclusive_or_accumulator_a
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xA8
                ) == hd6301v1_eora_indexed,
            "Indexed EORA operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "EORA",
            AddressingMode::indexed8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xA8)
                == nullptr,
        "Unreviewed MC6801 indexed EORA metadata was inherited"
    );
    const auto* hd6301v1_eora_extended = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "EORA",
        AddressingMode::extended16
    );
    passed &= expect(
        hd6301v1_eora_extended != nullptr,
        "HD6301V1 extended EORA metadata missing"
    );
    if (hd6301v1_eora_extended != nullptr) {
        passed &= expect(
            hd6301v1_eora_extended->opcode == 0xB8,
            "Extended EORA opcode mismatch"
        );
        passed &= expect(
            hd6301v1_eora_extended->operand_bytes == 2
                && hd6301v1_eora_extended->instruction_length == 3,
            "Extended EORA length mismatch"
        );
        passed &= expect(
            hd6301v1_eora_extended->base_cycles == 4,
            "Extended EORA cycle mismatch"
        );
        passed &= expect(
            hd6301v1_eora_extended->flags.read_mask == 0U
                && hd6301v1_eora_extended->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_eora_extended->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c}),
            "Extended EORA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_eora_extended->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_eora_extended
                )
                && hd6301v1_eora_extended->operation
                    == jr800::isa::Operation::exclusive_or_accumulator_a
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xB8
                ) == hd6301v1_eora_extended,
            "Extended EORA operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "EORA",
            AddressingMode::extended16
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xB8)
                == nullptr,
        "Unreviewed MC6801 extended EORA metadata was inherited"
    );

    const auto* hd6301v1_eorb_immediate = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "EORB",
        AddressingMode::immediate8
    );
    passed &= expect(
        hd6301v1_eorb_immediate != nullptr,
        "HD6301V1 immediate EORB metadata missing"
    );
    if (hd6301v1_eorb_immediate != nullptr) {
        passed &= expect(
            hd6301v1_eorb_immediate->opcode == 0xC8,
            "Immediate EORB opcode mismatch"
        );
        passed &= expect(
            hd6301v1_eorb_immediate->operand_bytes == 1
                && hd6301v1_eorb_immediate->instruction_length == 2,
            "Immediate EORB length mismatch"
        );
        passed &= expect(
            hd6301v1_eorb_immediate->base_cycles == 2,
            "Immediate EORB cycle mismatch"
        );
        passed &= expect(
            hd6301v1_eorb_immediate->flags.read_mask == 0U
                && hd6301v1_eorb_immediate->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_eorb_immediate->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c})
                && hd6301v1_eorb_immediate->flags.undefined_mask == 0U,
            "Immediate EORB flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_eorb_immediate->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_eorb_immediate
                )
                && hd6301v1_eorb_immediate->operation
                    == jr800::isa::Operation::exclusive_or_accumulator_b
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xC8
                ) == hd6301v1_eorb_immediate,
            "Immediate EORB operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "EORB",
            AddressingMode::immediate8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xC8)
                == nullptr,
        "Unreviewed MC6801 immediate EORB metadata was inherited"
    );

    const auto* hd6301v1_eorb_direct = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "EORB",
        AddressingMode::direct8
    );
    passed &= expect(
        hd6301v1_eorb_direct != nullptr,
        "HD6301V1 direct EORB metadata missing"
    );
    if (hd6301v1_eorb_direct != nullptr) {
        passed &= expect(
            hd6301v1_eorb_direct->opcode == 0xD8,
            "Direct EORB opcode mismatch"
        );
        passed &= expect(
            hd6301v1_eorb_direct->operand_bytes == 1
                && hd6301v1_eorb_direct->instruction_length == 2,
            "Direct EORB length mismatch"
        );
        passed &= expect(
            hd6301v1_eorb_direct->base_cycles == 3,
            "Direct EORB cycle mismatch"
        );
        passed &= expect(
            hd6301v1_eorb_direct->flags.read_mask == 0U
                && hd6301v1_eorb_direct->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_eorb_direct->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c})
                && hd6301v1_eorb_direct->flags.undefined_mask == 0U,
            "Direct EORB flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_eorb_direct->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_eorb_direct
                )
                && hd6301v1_eorb_direct->operation
                    == jr800::isa::Operation::exclusive_or_accumulator_b
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xD8
                ) == hd6301v1_eorb_direct,
            "Direct EORB operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "EORB",
            AddressingMode::direct8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xD8)
                == nullptr,
        "Unreviewed MC6801 direct EORB metadata was inherited"
    );
    const auto* hd6301v1_eorb_indexed = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "EORB",
        AddressingMode::indexed8
    );
    passed &= expect(
        hd6301v1_eorb_indexed != nullptr,
        "HD6301V1 indexed EORB metadata missing"
    );
    if (hd6301v1_eorb_indexed != nullptr) {
        passed &= expect(
            hd6301v1_eorb_indexed->opcode == 0xE8,
            "Indexed EORB opcode mismatch"
        );
        passed &= expect(
            hd6301v1_eorb_indexed->operand_bytes == 1
                && hd6301v1_eorb_indexed->instruction_length == 2,
            "Indexed EORB length mismatch"
        );
        passed &= expect(
            hd6301v1_eorb_indexed->base_cycles == 4,
            "Indexed EORB cycle mismatch"
        );
        passed &= expect(
            hd6301v1_eorb_indexed->flags.read_mask == 0U
                && hd6301v1_eorb_indexed->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_eorb_indexed->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c})
                && hd6301v1_eorb_indexed->flags.undefined_mask == 0U,
            "Indexed EORB flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_eorb_indexed->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_eorb_indexed
                )
                && hd6301v1_eorb_indexed->operation
                    == jr800::isa::Operation::exclusive_or_accumulator_b
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xE8
                ) == hd6301v1_eorb_indexed,
            "Indexed EORB operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "EORB",
            AddressingMode::indexed8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xE8)
                == nullptr,
        "Unreviewed MC6801 indexed EORB metadata was inherited"
    );

    const auto* hd6301v1_eorb_extended = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "EORB",
        AddressingMode::extended16
    );
    passed &= expect(
        hd6301v1_eorb_extended != nullptr,
        "HD6301V1 extended EORB metadata missing"
    );
    if (hd6301v1_eorb_extended != nullptr) {
        passed &= expect(
            hd6301v1_eorb_extended->opcode == 0xF8,
            "Extended EORB opcode mismatch"
        );
        passed &= expect(
            hd6301v1_eorb_extended->operand_bytes == 2
                && hd6301v1_eorb_extended->instruction_length == 3,
            "Extended EORB length mismatch"
        );
        passed &= expect(
            hd6301v1_eorb_extended->base_cycles == 4,
            "Extended EORB cycle mismatch"
        );
        passed &= expect(
            hd6301v1_eorb_extended->flags.read_mask == 0U
                && hd6301v1_eorb_extended->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_eorb_extended->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c})
                && hd6301v1_eorb_extended->flags.undefined_mask == 0U,
            "Extended EORB flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_eorb_extended->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_eorb_extended
                )
                && hd6301v1_eorb_extended->operation
                    == jr800::isa::Operation::exclusive_or_accumulator_b
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xF8
                ) == hd6301v1_eorb_extended,
            "Extended EORB operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "EORB",
            AddressingMode::extended16
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xF8)
                == nullptr,
        "Unreviewed MC6801 extended EORB metadata was inherited"
    );

    const auto* hd6301v1_oraa_immediate = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "ORAA",
        AddressingMode::immediate8
    );
    passed &= expect(
        hd6301v1_oraa_immediate != nullptr,
        "HD6301V1 immediate ORAA metadata missing"
    );
    if (hd6301v1_oraa_immediate != nullptr) {
        passed &= expect(
            hd6301v1_oraa_immediate->opcode == 0x8A,
            "Immediate ORAA opcode mismatch"
        );
        passed &= expect(
            hd6301v1_oraa_immediate->operand_bytes == 1
                && hd6301v1_oraa_immediate->instruction_length == 2,
            "Immediate ORAA length mismatch"
        );
        passed &= expect(
            hd6301v1_oraa_immediate->base_cycles == 2,
            "Immediate ORAA cycle mismatch"
        );
        passed &= expect(
            hd6301v1_oraa_immediate->flags.read_mask == 0U
                && hd6301v1_oraa_immediate->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_oraa_immediate->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c}),
            "Immediate ORAA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_oraa_immediate->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_oraa_immediate
                )
                && hd6301v1_oraa_immediate->operation
                    == jr800::isa::Operation::logical_or_accumulator_a
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0x8A
                ) == hd6301v1_oraa_immediate,
            "Immediate ORAA operation, classification, or decode mismatch"
        );
    }

    const auto* hd6301v1_oraa_direct = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "ORAA",
        AddressingMode::direct8
    );
    passed &= expect(
        hd6301v1_oraa_direct != nullptr,
        "HD6301V1 direct ORAA metadata missing"
    );
    if (hd6301v1_oraa_direct != nullptr) {
        passed &= expect(
            hd6301v1_oraa_direct->opcode == 0x9A,
            "Direct ORAA opcode mismatch"
        );
        passed &= expect(
            hd6301v1_oraa_direct->operand_bytes == 1
                && hd6301v1_oraa_direct->instruction_length == 2,
            "Direct ORAA length mismatch"
        );
        passed &= expect(
            hd6301v1_oraa_direct->base_cycles == 3,
            "Direct ORAA cycle mismatch"
        );
        passed &= expect(
            hd6301v1_oraa_direct->flags.read_mask == 0U
                && hd6301v1_oraa_direct->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_oraa_direct->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c}),
            "Direct ORAA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_oraa_direct->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_oraa_direct
                )
                && hd6301v1_oraa_direct->operation
                    == jr800::isa::Operation::logical_or_accumulator_a
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0x9A
                ) == hd6301v1_oraa_direct,
            "Direct ORAA operation, classification, or decode mismatch"
        );
    }

    const auto* hd6301v1_oraa_indexed = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "ORAA",
        AddressingMode::indexed8
    );
    passed &= expect(
        hd6301v1_oraa_indexed != nullptr,
        "HD6301V1 indexed ORAA metadata missing"
    );
    if (hd6301v1_oraa_indexed != nullptr) {
        passed &= expect(
            hd6301v1_oraa_indexed->opcode == 0xAA,
            "Indexed ORAA opcode mismatch"
        );
        passed &= expect(
            hd6301v1_oraa_indexed->operand_bytes == 1
                && hd6301v1_oraa_indexed->instruction_length == 2,
            "Indexed ORAA length mismatch"
        );
        passed &= expect(
            hd6301v1_oraa_indexed->base_cycles == 4,
            "Indexed ORAA cycle mismatch"
        );
        passed &= expect(
            hd6301v1_oraa_indexed->flags.read_mask == 0U
                && hd6301v1_oraa_indexed->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_oraa_indexed->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c})
                && hd6301v1_oraa_indexed->flags.undefined_mask == 0U,
            "Indexed ORAA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_oraa_indexed->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_oraa_indexed
                )
                && hd6301v1_oraa_indexed->operation
                    == jr800::isa::Operation::logical_or_accumulator_a
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xAA
                ) == hd6301v1_oraa_indexed,
            "Indexed ORAA operation, classification, or decode mismatch"
        );
    }

    const auto* hd6301v1_oraa_extended = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "ORAA",
        AddressingMode::extended16
    );
    passed &= expect(
        hd6301v1_oraa_extended != nullptr,
        "HD6301V1 extended ORAA metadata missing"
    );
    if (hd6301v1_oraa_extended != nullptr) {
        passed &= expect(
            hd6301v1_oraa_extended->opcode == 0xBA,
            "Extended ORAA opcode mismatch"
        );
        passed &= expect(
            hd6301v1_oraa_extended->operand_bytes == 2
                && hd6301v1_oraa_extended->instruction_length == 3,
            "Extended ORAA length mismatch"
        );
        passed &= expect(
            hd6301v1_oraa_extended->base_cycles == 4,
            "Extended ORAA cycle mismatch"
        );
        passed &= expect(
            hd6301v1_oraa_extended->flags.read_mask == 0U
                && hd6301v1_oraa_extended->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_oraa_extended->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c})
                && hd6301v1_oraa_extended->flags.undefined_mask == 0U,
            "Extended ORAA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_oraa_extended->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_oraa_extended
                )
                && hd6301v1_oraa_extended->operation
                    == jr800::isa::Operation::logical_or_accumulator_a
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xBA
                ) == hd6301v1_oraa_extended,
            "Extended ORAA operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "ORAA",
            AddressingMode::immediate8
        ) == nullptr
            && jr800::isa::find_encoding(
                CpuProfile::mc6801,
                "ORAA",
                AddressingMode::direct8
            ) == nullptr
            && jr800::isa::find_encoding(
                CpuProfile::mc6801,
                "ORAA",
                AddressingMode::indexed8
            ) == nullptr
            && jr800::isa::find_encoding(
                CpuProfile::mc6801,
                "ORAA",
                AddressingMode::extended16
            ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x8A)
                == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x9A)
                == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xAA)
                == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xBA)
                == nullptr,
        "Unreviewed MC6801 ORAA metadata was inherited"
    );
    const auto* hd6301v1_orab_immediate = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "ORAB",
        AddressingMode::immediate8
    );
    passed &= expect(
        hd6301v1_orab_immediate != nullptr,
        "HD6301V1 immediate ORAB metadata missing"
    );
    if (hd6301v1_orab_immediate != nullptr) {
        passed &= expect(
            hd6301v1_orab_immediate->opcode == 0xCA,
            "Immediate ORAB opcode mismatch"
        );
        passed &= expect(
            hd6301v1_orab_immediate->operand_bytes == 1
                && hd6301v1_orab_immediate->instruction_length == 2,
            "Immediate ORAB length mismatch"
        );
        passed &= expect(
            hd6301v1_orab_immediate->base_cycles == 2,
            "Immediate ORAB cycle mismatch"
        );
        passed &= expect(
            hd6301v1_orab_immediate->flags.read_mask == 0U
                && hd6301v1_orab_immediate->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_orab_immediate->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c})
                && hd6301v1_orab_immediate->flags.undefined_mask == 0U,
            "Immediate ORAB flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_orab_immediate->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_orab_immediate
                )
                && hd6301v1_orab_immediate->operation
                    == jr800::isa::Operation::logical_or_accumulator_b
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xCA
                ) == hd6301v1_orab_immediate,
            "Immediate ORAB operation, classification, or decode mismatch"
        );
    }
    const auto* hd6301v1_orab_direct = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "ORAB",
        AddressingMode::direct8
    );
    passed &= expect(
        hd6301v1_orab_direct != nullptr,
        "HD6301V1 direct ORAB metadata missing"
    );
    if (hd6301v1_orab_direct != nullptr) {
        passed &= expect(
            hd6301v1_orab_direct->opcode == 0xDA,
            "Direct ORAB opcode mismatch"
        );
        passed &= expect(
            hd6301v1_orab_direct->operand_bytes == 1
                && hd6301v1_orab_direct->instruction_length == 2,
            "Direct ORAB length mismatch"
        );
        passed &= expect(
            hd6301v1_orab_direct->base_cycles == 3,
            "Direct ORAB cycle mismatch"
        );
        passed &= expect(
            hd6301v1_orab_direct->flags.read_mask == 0U
                && hd6301v1_orab_direct->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_orab_direct->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c})
                && hd6301v1_orab_direct->flags.undefined_mask == 0U,
            "Direct ORAB flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_orab_direct->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_orab_direct
                )
                && hd6301v1_orab_direct->operation
                    == jr800::isa::Operation::logical_or_accumulator_b
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xDA
                ) == hd6301v1_orab_direct,
            "Direct ORAB operation, classification, or decode mismatch"
        );
    }
    const auto* hd6301v1_orab_indexed = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "ORAB",
        AddressingMode::indexed8
    );
    passed &= expect(
        hd6301v1_orab_indexed != nullptr,
        "HD6301V1 indexed ORAB metadata missing"
    );
    if (hd6301v1_orab_indexed != nullptr) {
        passed &= expect(
            hd6301v1_orab_indexed->opcode == 0xEA,
            "Indexed ORAB opcode mismatch"
        );
        passed &= expect(
            hd6301v1_orab_indexed->operand_bytes == 1
                && hd6301v1_orab_indexed->instruction_length == 2,
            "Indexed ORAB length mismatch"
        );
        passed &= expect(
            hd6301v1_orab_indexed->base_cycles == 4,
            "Indexed ORAB cycle mismatch"
        );
        passed &= expect(
            hd6301v1_orab_indexed->flags.read_mask == 0U
                && hd6301v1_orab_indexed->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_orab_indexed->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c})
                && hd6301v1_orab_indexed->flags.undefined_mask == 0U,
            "Indexed ORAB flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_orab_indexed->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_orab_indexed
                )
                && hd6301v1_orab_indexed->operation
                    == jr800::isa::Operation::logical_or_accumulator_b
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xEA
                ) == hd6301v1_orab_indexed,
            "Indexed ORAB operation, classification, or decode mismatch"
        );
    }
    const auto* hd6301v1_orab_extended = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "ORAB",
        AddressingMode::extended16
    );
    passed &= expect(
        hd6301v1_orab_extended != nullptr,
        "HD6301V1 extended ORAB metadata missing"
    );
    if (hd6301v1_orab_extended != nullptr) {
        passed &= expect(
            hd6301v1_orab_extended->opcode == 0xFA,
            "Extended ORAB opcode mismatch"
        );
        passed &= expect(
            hd6301v1_orab_extended->operand_bytes == 2
                && hd6301v1_orab_extended->instruction_length == 3,
            "Extended ORAB length mismatch"
        );
        passed &= expect(
            hd6301v1_orab_extended->base_cycles == 4,
            "Extended ORAB cycle mismatch"
        );
        passed &= expect(
            hd6301v1_orab_extended->flags.read_mask == 0U
                && hd6301v1_orab_extended->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_orab_extended->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c})
                && hd6301v1_orab_extended->flags.undefined_mask == 0U,
            "Extended ORAB flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_orab_extended->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_orab_extended
                )
                && hd6301v1_orab_extended->operation
                    == jr800::isa::Operation::logical_or_accumulator_b
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xFA
                ) == hd6301v1_orab_extended,
            "Extended ORAB operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "ORAB",
            AddressingMode::immediate8
        ) == nullptr
            && jr800::isa::find_encoding(
                CpuProfile::mc6801,
                "ORAB",
                AddressingMode::direct8
            ) == nullptr
            && jr800::isa::find_encoding(
                CpuProfile::mc6801,
                "ORAB",
                AddressingMode::indexed8
            ) == nullptr
            && jr800::isa::find_encoding(
                CpuProfile::mc6801,
                "ORAB",
                AddressingMode::extended16
            ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xCA)
                == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xDA)
                == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xEA)
                == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xFA)
                == nullptr,
        "Unreviewed MC6801 ORAB metadata was inherited"
    );
    const auto* hd6301v1_suba = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "SUBA",
        AddressingMode::immediate8
    );
    passed &= expect(hd6301v1_suba != nullptr, "HD6301V1 SUBA encoding missing");
    if (hd6301v1_suba != nullptr) {
        passed &= expect(hd6301v1_suba->opcode == 0x80, "SUBA opcode mismatch");
        passed &= expect(
            hd6301v1_suba->operand_bytes == 1
                && hd6301v1_suba->instruction_length == 2,
            "SUBA length mismatch"
        );
        passed &= expect(hd6301v1_suba->base_cycles == 2, "SUBA cycle mismatch");
        passed &= expect(
            hd6301v1_suba->flags.read_mask == 0U
                && hd6301v1_suba->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_suba->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i}),
            "SUBA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_suba->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_suba)
                && hd6301v1_suba->operation
                    == jr800::isa::Operation::subtract_from_accumulator_a
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0x80
                ) == hd6301v1_suba,
            "SUBA operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "SUBA",
            AddressingMode::immediate8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x80)
                == nullptr,
        "Unreviewed MC6801 SUBA metadata was inherited"
    );

    const auto* hd6301v1_suba_direct = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "SUBA",
        AddressingMode::direct8
    );
    passed &= expect(
        hd6301v1_suba_direct != nullptr,
        "HD6301V1 direct SUBA encoding missing"
    );
    if (hd6301v1_suba_direct != nullptr) {
        passed &= expect(
            hd6301v1_suba_direct->opcode == 0x90,
            "Direct SUBA opcode mismatch"
        );
        passed &= expect(
            hd6301v1_suba_direct->operand_bytes == 1
                && hd6301v1_suba_direct->instruction_length == 2,
            "Direct SUBA length mismatch"
        );
        passed &= expect(
            hd6301v1_suba_direct->base_cycles == 3,
            "Direct SUBA cycle mismatch"
        );
        passed &= expect(
            hd6301v1_suba_direct->flags.read_mask == 0U
                && hd6301v1_suba_direct->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_suba_direct->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i}),
            "Direct SUBA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_suba_direct->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_suba_direct
                )
                && hd6301v1_suba_direct->operation
                    == jr800::isa::Operation::subtract_from_accumulator_a
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0x90
                ) == hd6301v1_suba_direct,
            "Direct SUBA operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "SUBA",
            AddressingMode::direct8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x90)
                == nullptr,
        "Unreviewed MC6801 direct SUBA metadata was inherited"
    );

    const auto* hd6301v1_suba_indexed = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "SUBA",
        AddressingMode::indexed8
    );
    passed &= expect(
        hd6301v1_suba_indexed != nullptr,
        "HD6301V1 indexed SUBA encoding missing"
    );
    if (hd6301v1_suba_indexed != nullptr) {
        passed &= expect(
            hd6301v1_suba_indexed->opcode == 0xA0,
            "Indexed SUBA opcode mismatch"
        );
        passed &= expect(
            hd6301v1_suba_indexed->operand_bytes == 1
                && hd6301v1_suba_indexed->instruction_length == 2,
            "Indexed SUBA length mismatch"
        );
        passed &= expect(
            hd6301v1_suba_indexed->base_cycles == 4,
            "Indexed SUBA cycle mismatch"
        );
        passed &= expect(
            hd6301v1_suba_indexed->flags.read_mask == 0U
                && hd6301v1_suba_indexed->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_suba_indexed->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i})
                && hd6301v1_suba_indexed->flags.undefined_mask == 0U,
            "Indexed SUBA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_suba_indexed->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_suba_indexed
                )
                && hd6301v1_suba_indexed->operation
                    == jr800::isa::Operation::subtract_from_accumulator_a
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xA0
                ) == hd6301v1_suba_indexed,
            "Indexed SUBA operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "SUBA",
            AddressingMode::indexed8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xA0)
                == nullptr,
        "Unreviewed MC6801 indexed SUBA metadata was inherited"
    );
    const auto* hd6301v1_suba_extended = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "SUBA",
        AddressingMode::extended16
    );
    passed &= expect(
        hd6301v1_suba_extended != nullptr,
        "HD6301V1 extended SUBA encoding missing"
    );
    if (hd6301v1_suba_extended != nullptr) {
        passed &= expect(
            hd6301v1_suba_extended->opcode == 0xB0,
            "Extended SUBA opcode mismatch"
        );
        passed &= expect(
            hd6301v1_suba_extended->operand_bytes == 2
                && hd6301v1_suba_extended->instruction_length == 3,
            "Extended SUBA length mismatch"
        );
        passed &= expect(
            hd6301v1_suba_extended->base_cycles == 4,
            "Extended SUBA cycle mismatch"
        );
        passed &= expect(
            hd6301v1_suba_extended->flags.read_mask == 0U
                && hd6301v1_suba_extended->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_suba_extended->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i})
                && hd6301v1_suba_extended->flags.undefined_mask == 0U,
            "Extended SUBA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_suba_extended->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_suba_extended
                )
                && hd6301v1_suba_extended->operation
                    == jr800::isa::Operation::subtract_from_accumulator_a
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xB0
                ) == hd6301v1_suba_extended,
            "Extended SUBA operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
                CpuProfile::mc6801,
                "SUBA",
                AddressingMode::extended16
            ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xB0)
                == nullptr,
        "Unreviewed MC6801 extended SUBA metadata was inherited"
    );

    const auto* hd6301v1_subb = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "SUBB",
        AddressingMode::immediate8
    );
    passed &= expect(hd6301v1_subb != nullptr, "HD6301V1 SUBB encoding missing");
    if (hd6301v1_subb != nullptr) {
        passed &= expect(hd6301v1_subb->opcode == 0xC0, "SUBB opcode mismatch");
        passed &= expect(
            hd6301v1_subb->operand_bytes == 1
                && hd6301v1_subb->instruction_length == 2,
            "SUBB length mismatch"
        );
        passed &= expect(hd6301v1_subb->base_cycles == 2, "SUBB cycle mismatch");
        passed &= expect(
            hd6301v1_subb->flags.read_mask == 0U
                && hd6301v1_subb->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_subb->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i}),
            "SUBB flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_subb->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_subb)
                && hd6301v1_subb->operation
                    == jr800::isa::Operation::subtract_from_accumulator_b
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xC0
                ) == hd6301v1_subb,
            "SUBB operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "SUBB",
            AddressingMode::immediate8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xC0)
                == nullptr,
        "Unreviewed MC6801 SUBB metadata was inherited"
    );

    const auto* hd6301v1_subb_direct = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "SUBB",
        AddressingMode::direct8
    );
    passed &= expect(
        hd6301v1_subb_direct != nullptr,
        "HD6301V1 direct SUBB encoding missing"
    );
    if (hd6301v1_subb_direct != nullptr) {
        passed &= expect(
            hd6301v1_subb_direct->opcode == 0xD0,
            "Direct SUBB opcode mismatch"
        );
        passed &= expect(
            hd6301v1_subb_direct->operand_bytes == 1
                && hd6301v1_subb_direct->instruction_length == 2,
            "Direct SUBB length mismatch"
        );
        passed &= expect(
            hd6301v1_subb_direct->base_cycles == 3,
            "Direct SUBB cycle mismatch"
        );
        passed &= expect(
            hd6301v1_subb_direct->flags.read_mask == 0U
                && hd6301v1_subb_direct->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_subb_direct->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i}),
            "Direct SUBB flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_subb_direct->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_subb_direct
                )
                && hd6301v1_subb_direct->operation
                    == jr800::isa::Operation::subtract_from_accumulator_b
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xD0
                ) == hd6301v1_subb_direct,
            "Direct SUBB operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "SUBB",
            AddressingMode::direct8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xD0)
                == nullptr,
        "Unreviewed MC6801 direct SUBB metadata was inherited"
    );
    const auto* hd6301v1_subb_indexed = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "SUBB",
        AddressingMode::indexed8
    );
    passed &= expect(
        hd6301v1_subb_indexed != nullptr,
        "HD6301V1 indexed SUBB encoding missing"
    );
    if (hd6301v1_subb_indexed != nullptr) {
        passed &= expect(
            hd6301v1_subb_indexed->opcode == 0xE0,
            "Indexed SUBB opcode mismatch"
        );
        passed &= expect(
            hd6301v1_subb_indexed->operand_bytes == 1
                && hd6301v1_subb_indexed->instruction_length == 2,
            "Indexed SUBB length mismatch"
        );
        passed &= expect(
            hd6301v1_subb_indexed->base_cycles == 4,
            "Indexed SUBB cycle mismatch"
        );
        passed &= expect(
            hd6301v1_subb_indexed->flags.read_mask == 0U
                && hd6301v1_subb_indexed->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_subb_indexed->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i})
                && hd6301v1_subb_indexed->flags.undefined_mask == 0U,
            "Indexed SUBB flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_subb_indexed->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_subb_indexed
                )
                && hd6301v1_subb_indexed->operation
                    == jr800::isa::Operation::subtract_from_accumulator_b
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xE0
                ) == hd6301v1_subb_indexed,
            "Indexed SUBB operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
                CpuProfile::mc6801,
                "SUBB",
                AddressingMode::indexed8
            ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xE0)
                == nullptr,
        "Unreviewed MC6801 indexed SUBB metadata was inherited"
    );
    const auto* hd6301v1_subb_extended = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "SUBB",
        AddressingMode::extended16
    );
    passed &= expect(
        hd6301v1_subb_extended != nullptr,
        "HD6301V1 extended SUBB encoding missing"
    );
    if (hd6301v1_subb_extended != nullptr) {
        passed &= expect(
            hd6301v1_subb_extended->opcode == 0xF0,
            "Extended SUBB opcode mismatch"
        );
        passed &= expect(
            hd6301v1_subb_extended->operand_bytes == 2
                && hd6301v1_subb_extended->instruction_length == 3,
            "Extended SUBB length mismatch"
        );
        passed &= expect(
            hd6301v1_subb_extended->base_cycles == 4,
            "Extended SUBB cycle mismatch"
        );
        passed &= expect(
            hd6301v1_subb_extended->flags.read_mask == 0U
                && hd6301v1_subb_extended->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_subb_extended->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i})
                && hd6301v1_subb_extended->flags.undefined_mask == 0U,
            "Extended SUBB flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_subb_extended->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_subb_extended
                )
                && hd6301v1_subb_extended->operation
                    == jr800::isa::Operation::subtract_from_accumulator_b
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xF0
                ) == hd6301v1_subb_extended,
            "Extended SUBB operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
                CpuProfile::mc6801,
                "SUBB",
                AddressingMode::extended16
            ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xF0)
                == nullptr,
        "Unreviewed MC6801 extended SUBB metadata was inherited"
    );


    const auto* hd6301v1_cmpa = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "CMPA",
        AddressingMode::immediate8
    );
    passed &= expect(hd6301v1_cmpa != nullptr, "HD6301V1 CMPA encoding missing");
    if (hd6301v1_cmpa != nullptr) {
        passed &= expect(hd6301v1_cmpa->opcode == 0x81, "CMPA opcode mismatch");
        passed &= expect(
            hd6301v1_cmpa->operand_bytes == 1
                && hd6301v1_cmpa->instruction_length == 2,
            "CMPA length mismatch"
        );
        passed &= expect(hd6301v1_cmpa->base_cycles == 2, "CMPA cycle mismatch");
        passed &= expect(
            hd6301v1_cmpa->flags.read_mask == 0U
                && hd6301v1_cmpa->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_cmpa->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i}),
            "CMPA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_cmpa->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_cmpa)
                && hd6301v1_cmpa->operation
                    == jr800::isa::Operation::compare_accumulator_a,
            "CMPA operation or debugger classification mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "CMPA",
            AddressingMode::immediate8
        ) == nullptr,
        "Unreviewed MC6801 CMPA metadata was inherited"
    );

    const auto* hd6301v1_cmpa_direct = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "CMPA",
        AddressingMode::direct8
    );
    passed &= expect(
        hd6301v1_cmpa_direct != nullptr,
        "HD6301V1 direct CMPA encoding missing"
    );
    if (hd6301v1_cmpa_direct != nullptr) {
        passed &= expect(
            hd6301v1_cmpa_direct->opcode == 0x91,
            "Direct CMPA opcode mismatch"
        );
        passed &= expect(
            hd6301v1_cmpa_direct->operand_bytes == 1
                && hd6301v1_cmpa_direct->instruction_length == 2,
            "Direct CMPA length mismatch"
        );
        passed &= expect(
            hd6301v1_cmpa_direct->base_cycles == 3,
            "Direct CMPA cycle mismatch"
        );
        passed &= expect(
            hd6301v1_cmpa_direct->flags.read_mask == 0U
                && hd6301v1_cmpa_direct->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_cmpa_direct->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i}),
            "Direct CMPA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_cmpa_direct->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_cmpa_direct
                )
                && hd6301v1_cmpa_direct->operation
                    == jr800::isa::Operation::compare_accumulator_a,
            "Direct CMPA operation or debugger classification mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "CMPA",
            AddressingMode::direct8
        ) == nullptr,
        "Unreviewed MC6801 direct CMPA metadata was inherited"
    );
    const auto* hd6301v1_cmpa_indexed = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "CMPA",
        AddressingMode::indexed8
    );
    passed &= expect(
        hd6301v1_cmpa_indexed != nullptr,
        "HD6301V1 indexed CMPA encoding missing"
    );
    if (hd6301v1_cmpa_indexed != nullptr) {
        passed &= expect(
            hd6301v1_cmpa_indexed->opcode == 0xA1,
            "Indexed CMPA opcode mismatch"
        );
        passed &= expect(
            hd6301v1_cmpa_indexed->operand_bytes == 1
                && hd6301v1_cmpa_indexed->instruction_length == 2,
            "Indexed CMPA length mismatch"
        );
        passed &= expect(
            hd6301v1_cmpa_indexed->base_cycles == 4,
            "Indexed CMPA cycle mismatch"
        );
        passed &= expect(
            hd6301v1_cmpa_indexed->flags.read_mask == 0U
                && hd6301v1_cmpa_indexed->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_cmpa_indexed->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i})
                && hd6301v1_cmpa_indexed->flags.undefined_mask == 0U,
            "Indexed CMPA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_cmpa_indexed->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_cmpa_indexed
                )
                && hd6301v1_cmpa_indexed->operation
                    == jr800::isa::Operation::compare_accumulator_a
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xA1
                ) == hd6301v1_cmpa_indexed,
            "Indexed CMPA operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "CMPA",
            AddressingMode::indexed8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xA1)
                == nullptr,
        "Unreviewed MC6801 indexed CMPA metadata was inherited"
    );
    const auto* hd6301v1_cmpa_extended = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "CMPA",
        AddressingMode::extended16
    );
    passed &= expect(
        hd6301v1_cmpa_extended != nullptr,
        "HD6301V1 extended CMPA encoding missing"
    );
    if (hd6301v1_cmpa_extended != nullptr) {
        passed &= expect(
            hd6301v1_cmpa_extended->opcode == 0xB1,
            "Extended CMPA opcode mismatch"
        );
        passed &= expect(
            hd6301v1_cmpa_extended->operand_bytes == 2
                && hd6301v1_cmpa_extended->instruction_length == 3,
            "Extended CMPA length mismatch"
        );
        passed &= expect(
            hd6301v1_cmpa_extended->base_cycles == 4,
            "Extended CMPA cycle mismatch"
        );
        passed &= expect(
            hd6301v1_cmpa_extended->flags.read_mask == 0U
                && hd6301v1_cmpa_extended->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_cmpa_extended->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i})
                && hd6301v1_cmpa_extended->flags.undefined_mask == 0U,
            "Extended CMPA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_cmpa_extended->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_cmpa_extended
                )
                && hd6301v1_cmpa_extended->operation
                    == jr800::isa::Operation::compare_accumulator_a
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xB1
                ) == hd6301v1_cmpa_extended,
            "Extended CMPA operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "CMPA",
            AddressingMode::extended16
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xB1)
                == nullptr,
        "Unreviewed MC6801 extended CMPA metadata was inherited"
    );

    const auto* hd6301v1_cmpb = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "CMPB",
        AddressingMode::immediate8
    );
    passed &= expect(hd6301v1_cmpb != nullptr, "HD6301V1 CMPB encoding missing");
    if (hd6301v1_cmpb != nullptr) {
        passed &= expect(hd6301v1_cmpb->opcode == 0xC1, "CMPB opcode mismatch");
        passed &= expect(
            hd6301v1_cmpb->operand_bytes == 1
                && hd6301v1_cmpb->instruction_length == 2,
            "CMPB length mismatch"
        );
        passed &= expect(hd6301v1_cmpb->base_cycles == 2, "CMPB cycle mismatch");
        passed &= expect(
            hd6301v1_cmpb->flags.read_mask == 0U
                && hd6301v1_cmpb->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_cmpb->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i}),
            "CMPB flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_cmpb->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_cmpb)
                && hd6301v1_cmpb->operation
                    == jr800::isa::Operation::compare_accumulator_b,
            "CMPB operation or debugger classification mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "CMPB",
            AddressingMode::immediate8
        ) == nullptr,
        "Unreviewed MC6801 CMPB metadata was inherited"
    );

    const auto* hd6301v1_cmpb_direct = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "CMPB",
        AddressingMode::direct8
    );
    passed &= expect(
        hd6301v1_cmpb_direct != nullptr,
        "HD6301V1 direct CMPB encoding missing"
    );
    if (hd6301v1_cmpb_direct != nullptr) {
        passed &= expect(
            hd6301v1_cmpb_direct->opcode == 0xD1,
            "Direct CMPB opcode mismatch"
        );
        passed &= expect(
            hd6301v1_cmpb_direct->operand_bytes == 1
                && hd6301v1_cmpb_direct->instruction_length == 2,
            "Direct CMPB length mismatch"
        );
        passed &= expect(
            hd6301v1_cmpb_direct->base_cycles == 3,
            "Direct CMPB cycle mismatch"
        );
        passed &= expect(
            hd6301v1_cmpb_direct->flags.read_mask == 0U
                && hd6301v1_cmpb_direct->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_cmpb_direct->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i}),
            "Direct CMPB flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_cmpb_direct->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_cmpb_direct
                )
                && hd6301v1_cmpb_direct->operation
                    == jr800::isa::Operation::compare_accumulator_b,
            "Direct CMPB operation or debugger classification mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "CMPB",
            AddressingMode::direct8
        ) == nullptr,
        "Unreviewed MC6801 direct CMPB metadata was inherited"
    );

    const auto* hd6301v1_cmpb_indexed = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "CMPB",
        AddressingMode::indexed8
    );
    passed &= expect(
        hd6301v1_cmpb_indexed != nullptr,
        "HD6301V1 indexed CMPB encoding missing"
    );
    if (hd6301v1_cmpb_indexed != nullptr) {
        passed &= expect(
            hd6301v1_cmpb_indexed->opcode == 0xE1,
            "Indexed CMPB opcode mismatch"
        );
        passed &= expect(
            hd6301v1_cmpb_indexed->operand_bytes == 1
                && hd6301v1_cmpb_indexed->instruction_length == 2,
            "Indexed CMPB length mismatch"
        );
        passed &= expect(
            hd6301v1_cmpb_indexed->base_cycles == 4,
            "Indexed CMPB cycle mismatch"
        );
        passed &= expect(
            hd6301v1_cmpb_indexed->flags.read_mask == 0U
                && hd6301v1_cmpb_indexed->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_cmpb_indexed->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i}),
            "Indexed CMPB flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_cmpb_indexed->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_cmpb_indexed
                )
                && hd6301v1_cmpb_indexed->operation
                    == jr800::isa::Operation::compare_accumulator_b,
            "Indexed CMPB operation or debugger classification mismatch"
        );
        passed &= expect(
            jr800::isa::decode_instruction(CpuProfile::hd6301v1, 0xE1)
                == hd6301v1_cmpb_indexed,
            "Indexed CMPB opcode decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "CMPB",
            AddressingMode::indexed8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xE1)
                == nullptr,
        "Unreviewed MC6801 indexed CMPB metadata was inherited"
    );
    const auto* hd6301v1_cmpb_extended = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "CMPB",
        AddressingMode::extended16
    );
    passed &= expect(
        hd6301v1_cmpb_extended != nullptr,
        "HD6301V1 extended CMPB encoding missing"
    );
    if (hd6301v1_cmpb_extended != nullptr) {
        passed &= expect(
            hd6301v1_cmpb_extended->opcode == 0xF1,
            "Extended CMPB opcode mismatch"
        );
        passed &= expect(
            hd6301v1_cmpb_extended->operand_bytes == 2
                && hd6301v1_cmpb_extended->instruction_length == 3,
            "Extended CMPB length mismatch"
        );
        passed &= expect(
            hd6301v1_cmpb_extended->base_cycles == 4,
            "Extended CMPB cycle mismatch"
        );
        passed &= expect(
            hd6301v1_cmpb_extended->flags.read_mask == 0U
                && hd6301v1_cmpb_extended->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_cmpb_extended->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i}),
            "Extended CMPB flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_cmpb_extended->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_cmpb_extended
                )
                && hd6301v1_cmpb_extended->operation
                    == jr800::isa::Operation::compare_accumulator_b,
            "Extended CMPB operation or debugger classification mismatch"
        );
        passed &= expect(
            jr800::isa::decode_instruction(CpuProfile::hd6301v1, 0xF1)
                == hd6301v1_cmpb_extended,
            "Extended CMPB opcode decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "CMPB",
            AddressingMode::extended16
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xF1)
                == nullptr,
        "Unreviewed MC6801 extended CMPB metadata was inherited"
    );

    const auto* hd6301v1_sbca = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "SBCA",
        AddressingMode::immediate8
    );
    passed &= expect(hd6301v1_sbca != nullptr, "HD6301V1 SBCA encoding missing");
    if (hd6301v1_sbca != nullptr) {
        passed &= expect(hd6301v1_sbca->opcode == 0x82, "SBCA opcode mismatch");
        passed &= expect(
            hd6301v1_sbca->operand_bytes == 1
                && hd6301v1_sbca->instruction_length == 2,
            "SBCA length mismatch"
        );
        passed &= expect(hd6301v1_sbca->base_cycles == 2, "SBCA cycle mismatch");
        passed &= expect(
            hd6301v1_sbca->flags.read_mask == mask({StatusFlag::c})
                && hd6301v1_sbca->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_sbca->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i}),
            "SBCA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_sbca->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_sbca)
                && hd6301v1_sbca->operation
                    == jr800::isa::Operation::subtract_with_carry_accumulator_a,
            "SBCA operation or debugger classification mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "SBCA",
            AddressingMode::immediate8
        ) == nullptr,
        "Unreviewed MC6801 SBCA metadata was inherited"
    );
    const auto* hd6301v1_sbca_direct = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "SBCA",
        AddressingMode::direct8
    );
    passed &= expect(
        hd6301v1_sbca_direct != nullptr,
        "HD6301V1 direct SBCA encoding missing"
    );
    if (hd6301v1_sbca_direct != nullptr) {
        passed &= expect(
            hd6301v1_sbca_direct->opcode == 0x92,
            "Direct SBCA opcode mismatch"
        );
        passed &= expect(
            hd6301v1_sbca_direct->operand_bytes == 1
                && hd6301v1_sbca_direct->instruction_length == 2,
            "Direct SBCA length mismatch"
        );
        passed &= expect(
            hd6301v1_sbca_direct->base_cycles == 3,
            "Direct SBCA cycle mismatch"
        );
        passed &= expect(
            hd6301v1_sbca_direct->flags.read_mask
                    == mask({StatusFlag::c})
                && hd6301v1_sbca_direct->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_sbca_direct->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i}),
            "Direct SBCA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_sbca_direct->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_sbca_direct
                )
                && hd6301v1_sbca_direct->operation
                    == jr800::isa::Operation::subtract_with_carry_accumulator_a,
            "Direct SBCA operation or debugger classification mismatch"
        );
        passed &= expect(
            jr800::isa::decode_instruction(CpuProfile::hd6301v1, 0x92)
                == hd6301v1_sbca_direct,
            "Direct SBCA opcode decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "SBCA",
            AddressingMode::direct8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x92)
                == nullptr,
        "Unreviewed MC6801 direct SBCA metadata was inherited"
    );
    const auto* hd6301v1_sbca_indexed = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "SBCA",
        AddressingMode::indexed8
    );
    passed &= expect(
        hd6301v1_sbca_indexed != nullptr,
        "HD6301V1 indexed SBCA encoding missing"
    );
    if (hd6301v1_sbca_indexed != nullptr) {
        passed &= expect(
            hd6301v1_sbca_indexed->opcode == 0xA2,
            "Indexed SBCA opcode mismatch"
        );
        passed &= expect(
            hd6301v1_sbca_indexed->operand_bytes == 1
                && hd6301v1_sbca_indexed->instruction_length == 2,
            "Indexed SBCA length mismatch"
        );
        passed &= expect(
            hd6301v1_sbca_indexed->base_cycles == 4,
            "Indexed SBCA cycle mismatch"
        );
        passed &= expect(
            hd6301v1_sbca_indexed->flags.read_mask
                    == mask({StatusFlag::c})
                && hd6301v1_sbca_indexed->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_sbca_indexed->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i}),
            "Indexed SBCA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_sbca_indexed->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_sbca_indexed
                )
                && hd6301v1_sbca_indexed->operation
                    == jr800::isa::Operation::subtract_with_carry_accumulator_a,
            "Indexed SBCA operation or debugger classification mismatch"
        );
        passed &= expect(
            jr800::isa::decode_instruction(CpuProfile::hd6301v1, 0xA2)
                == hd6301v1_sbca_indexed,
            "Indexed SBCA opcode decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "SBCA",
            AddressingMode::indexed8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xA2)
                == nullptr,
        "Unreviewed MC6801 indexed SBCA metadata was inherited"
    );
    const auto* hd6301v1_sbca_extended = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "SBCA",
        AddressingMode::extended16
    );
    passed &= expect(
        hd6301v1_sbca_extended != nullptr,
        "HD6301V1 extended SBCA encoding missing"
    );
    if (hd6301v1_sbca_extended != nullptr) {
        passed &= expect(
            hd6301v1_sbca_extended->opcode == 0xB2,
            "Extended SBCA opcode mismatch"
        );
        passed &= expect(
            hd6301v1_sbca_extended->operand_bytes == 2
                && hd6301v1_sbca_extended->instruction_length == 3,
            "Extended SBCA length mismatch"
        );
        passed &= expect(
            hd6301v1_sbca_extended->base_cycles == 4,
            "Extended SBCA cycle mismatch"
        );
        passed &= expect(
            hd6301v1_sbca_extended->flags.read_mask
                    == mask({StatusFlag::c})
                && hd6301v1_sbca_extended->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_sbca_extended->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i}),
            "Extended SBCA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_sbca_extended->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_sbca_extended
                )
                && hd6301v1_sbca_extended->operation
                    == jr800::isa::Operation::subtract_with_carry_accumulator_a,
            "Extended SBCA operation or debugger classification mismatch"
        );
        passed &= expect(
            jr800::isa::decode_instruction(CpuProfile::hd6301v1, 0xB2)
                == hd6301v1_sbca_extended,
            "Extended SBCA opcode decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "SBCA",
            AddressingMode::extended16
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xB2)
                == nullptr,
        "Unreviewed MC6801 extended SBCA metadata was inherited"
    );

    const auto* hd6301v1_sbcb_immediate = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "SBCB",
        AddressingMode::immediate8
    );
    passed &= expect(
        hd6301v1_sbcb_immediate != nullptr,
        "HD6301V1 immediate SBCB encoding missing"
    );
    if (hd6301v1_sbcb_immediate != nullptr) {
        passed &= expect(
            hd6301v1_sbcb_immediate->opcode == 0xC2,
            "Immediate SBCB opcode mismatch"
        );
        passed &= expect(
            hd6301v1_sbcb_immediate->operand_bytes == 1
                && hd6301v1_sbcb_immediate->instruction_length == 2,
            "Immediate SBCB length mismatch"
        );
        passed &= expect(
            hd6301v1_sbcb_immediate->base_cycles == 2,
            "Immediate SBCB cycle mismatch"
        );
        passed &= expect(
            hd6301v1_sbcb_immediate->flags.read_mask
                    == mask({StatusFlag::c})
                && hd6301v1_sbcb_immediate->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_sbcb_immediate->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i}),
            "Immediate SBCB flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_sbcb_immediate->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_sbcb_immediate
                )
                && hd6301v1_sbcb_immediate->operation
                    == jr800::isa::Operation::subtract_with_carry_accumulator_b,
            "Immediate SBCB operation or debugger classification mismatch"
        );
        passed &= expect(
            jr800::isa::decode_instruction(CpuProfile::hd6301v1, 0xC2)
                == hd6301v1_sbcb_immediate,
            "Immediate SBCB opcode decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "SBCB",
            AddressingMode::immediate8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xC2)
                == nullptr,
        "Unreviewed MC6801 immediate SBCB metadata was inherited"
    );

    const auto* hd6301v1_sbcb_direct = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "SBCB",
        AddressingMode::direct8
    );
    passed &= expect(
        hd6301v1_sbcb_direct != nullptr,
        "HD6301V1 direct SBCB encoding missing"
    );
    if (hd6301v1_sbcb_direct != nullptr) {
        passed &= expect(
            hd6301v1_sbcb_direct->opcode == 0xD2,
            "Direct SBCB opcode mismatch"
        );
        passed &= expect(
            hd6301v1_sbcb_direct->operand_bytes == 1
                && hd6301v1_sbcb_direct->instruction_length == 2,
            "Direct SBCB length mismatch"
        );
        passed &= expect(
            hd6301v1_sbcb_direct->base_cycles == 3,
            "Direct SBCB cycle mismatch"
        );
        passed &= expect(
            hd6301v1_sbcb_direct->flags.read_mask
                    == mask({StatusFlag::c})
                && hd6301v1_sbcb_direct->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_sbcb_direct->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i}),
            "Direct SBCB flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_sbcb_direct->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_sbcb_direct
                )
                && hd6301v1_sbcb_direct->operation
                    == jr800::isa::Operation::subtract_with_carry_accumulator_b,
            "Direct SBCB operation or debugger classification mismatch"
        );
        passed &= expect(
            jr800::isa::decode_instruction(CpuProfile::hd6301v1, 0xD2)
                == hd6301v1_sbcb_direct,
            "Direct SBCB opcode decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "SBCB",
            AddressingMode::direct8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xD2)
                == nullptr,
        "Unreviewed MC6801 direct SBCB metadata was inherited"
    );

    const auto* hd6301v1_sbcb_indexed = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "SBCB",
        AddressingMode::indexed8
    );
    passed &= expect(
        hd6301v1_sbcb_indexed != nullptr,
        "HD6301V1 indexed SBCB encoding missing"
    );
    if (hd6301v1_sbcb_indexed != nullptr) {
        passed &= expect(
            hd6301v1_sbcb_indexed->opcode == 0xE2,
            "Indexed SBCB opcode mismatch"
        );
        passed &= expect(
            hd6301v1_sbcb_indexed->operand_bytes == 1
                && hd6301v1_sbcb_indexed->instruction_length == 2,
            "Indexed SBCB length mismatch"
        );
        passed &= expect(
            hd6301v1_sbcb_indexed->base_cycles == 4,
            "Indexed SBCB cycle mismatch"
        );
        passed &= expect(
            hd6301v1_sbcb_indexed->flags.read_mask
                    == mask({StatusFlag::c})
                && hd6301v1_sbcb_indexed->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_sbcb_indexed->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i}),
            "Indexed SBCB flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_sbcb_indexed->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_sbcb_indexed
                )
                && hd6301v1_sbcb_indexed->operation
                    == jr800::isa::Operation::subtract_with_carry_accumulator_b,
            "Indexed SBCB operation or debugger classification mismatch"
        );
        passed &= expect(
            jr800::isa::decode_instruction(CpuProfile::hd6301v1, 0xE2)
                == hd6301v1_sbcb_indexed,
            "Indexed SBCB opcode decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "SBCB",
            AddressingMode::indexed8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xE2)
                == nullptr,
        "Unreviewed MC6801 indexed SBCB metadata was inherited"
    );

    const auto* hd6301v1_sbcb_extended = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "SBCB",
        AddressingMode::extended16
    );
    passed &= expect(
        hd6301v1_sbcb_extended != nullptr,
        "HD6301V1 extended SBCB encoding missing"
    );
    if (hd6301v1_sbcb_extended != nullptr) {
        passed &= expect(
            hd6301v1_sbcb_extended->opcode == 0xF2,
            "Extended SBCB opcode mismatch"
        );
        passed &= expect(
            hd6301v1_sbcb_extended->operand_bytes == 2
                && hd6301v1_sbcb_extended->instruction_length == 3,
            "Extended SBCB length mismatch"
        );
        passed &= expect(
            hd6301v1_sbcb_extended->base_cycles == 4,
            "Extended SBCB cycle mismatch"
        );
        passed &= expect(
            hd6301v1_sbcb_extended->flags.read_mask
                    == mask({StatusFlag::c})
                && hd6301v1_sbcb_extended->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_sbcb_extended->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i}),
            "Extended SBCB flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_sbcb_extended->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_sbcb_extended
                )
                && hd6301v1_sbcb_extended->operation
                    == jr800::isa::Operation::subtract_with_carry_accumulator_b,
            "Extended SBCB operation or debugger classification mismatch"
        );
        passed &= expect(
            jr800::isa::decode_instruction(CpuProfile::hd6301v1, 0xF2)
                == hd6301v1_sbcb_extended,
            "Extended SBCB opcode decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
                CpuProfile::mc6801,
                "SBCB",
                AddressingMode::extended16
            ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xF2)
                == nullptr,
        "Unreviewed MC6801 extended SBCB metadata was inherited"
    );

    const auto* hd6301v1_cpx = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "CPX",
        AddressingMode::immediate16
    );
    passed &= expect(hd6301v1_cpx != nullptr, "HD6301V1 CPX encoding missing");
    if (hd6301v1_cpx != nullptr) {
        passed &= expect(hd6301v1_cpx->opcode == 0x8C, "CPX opcode mismatch");
        passed &= expect(hd6301v1_cpx->instruction_length == 3, "CPX length mismatch");
        passed &= expect(hd6301v1_cpx->base_cycles == 3, "CPX cycle mismatch");
        passed &= expect(
            hd6301v1_cpx->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_cpx->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i}),
            "CPX flag metadata mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "CPX",
            AddressingMode::immediate16
        ) == nullptr,
        "Unreviewed MC6801 CPX metadata was inherited"
    );
    const auto* hd6301v1_cpx_direct = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "CPX",
        AddressingMode::direct8
    );
    passed &= expect(
        hd6301v1_cpx_direct != nullptr,
        "HD6301V1 direct CPX encoding missing"
    );
    if (hd6301v1_cpx_direct != nullptr) {
        passed &= expect(
            hd6301v1_cpx_direct->opcode == 0x9C
                && hd6301v1_cpx_direct->operand_bytes == 1
                && hd6301v1_cpx_direct->instruction_length == 2
                && hd6301v1_cpx_direct->base_cycles == 4,
            "Direct CPX encoding, length, or cycle mismatch"
        );
        passed &= expect(
            hd6301v1_cpx_direct->flags.read_mask == 0U
                && hd6301v1_cpx_direct->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_cpx_direct->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i})
                && hd6301v1_cpx_direct->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_cpx_direct)
                && hd6301v1_cpx_direct->operation
                    == jr800::isa::Operation::compare_index_register,
            "Direct CPX flags, operation, or classification mismatch"
        );
        passed &= expect(
            jr800::isa::decode_instruction(CpuProfile::hd6301v1, 0x9C)
                == hd6301v1_cpx_direct,
            "Direct CPX opcode decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
                CpuProfile::mc6801,
                "CPX",
                AddressingMode::direct8
            ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x9C)
                == nullptr,
        "Unreviewed MC6801 direct CPX metadata was inherited"
    );
    const auto* hd6301v1_cpx_indexed = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "CPX",
        AddressingMode::indexed8
    );
    passed &= expect(
        hd6301v1_cpx_indexed != nullptr,
        "HD6301V1 indexed CPX encoding missing"
    );
    if (hd6301v1_cpx_indexed != nullptr) {
        passed &= expect(
            hd6301v1_cpx_indexed->opcode == 0xAC
                && hd6301v1_cpx_indexed->operand_bytes == 1
                && hd6301v1_cpx_indexed->instruction_length == 2
                && hd6301v1_cpx_indexed->base_cycles == 5,
            "Indexed CPX encoding, length, or cycle mismatch"
        );
        passed &= expect(
            hd6301v1_cpx_indexed->flags.read_mask == 0U
                && hd6301v1_cpx_indexed->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_cpx_indexed->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i})
                && hd6301v1_cpx_indexed->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_cpx_indexed)
                && hd6301v1_cpx_indexed->operation
                    == jr800::isa::Operation::compare_index_register,
            "Indexed CPX flags, operation, or classification mismatch"
        );
        passed &= expect(
            jr800::isa::decode_instruction(CpuProfile::hd6301v1, 0xAC)
                == hd6301v1_cpx_indexed,
            "Indexed CPX opcode decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
                CpuProfile::mc6801,
                "CPX",
                AddressingMode::indexed8
            ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xAC)
                == nullptr,
        "Unreviewed MC6801 indexed CPX metadata was inherited"
    );
    const auto* hd6301v1_cpx_extended = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "CPX",
        AddressingMode::extended16
    );
    passed &= expect(
        hd6301v1_cpx_extended != nullptr,
        "HD6301V1 extended CPX encoding missing"
    );
    if (hd6301v1_cpx_extended != nullptr) {
        passed &= expect(
            hd6301v1_cpx_extended->opcode == 0xBC
                && hd6301v1_cpx_extended->operand_bytes == 2
                && hd6301v1_cpx_extended->instruction_length == 3
                && hd6301v1_cpx_extended->base_cycles == 5,
            "Extended CPX encoding, length, or cycle mismatch"
        );
        passed &= expect(
            hd6301v1_cpx_extended->flags.read_mask == 0U
                && hd6301v1_cpx_extended->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_cpx_extended->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i})
                && hd6301v1_cpx_extended->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_cpx_extended)
                && hd6301v1_cpx_extended->operation
                    == jr800::isa::Operation::compare_index_register,
            "Extended CPX flags, operation, or classification mismatch"
        );
        passed &= expect(
            jr800::isa::decode_instruction(CpuProfile::hd6301v1, 0xBC)
                == hd6301v1_cpx_extended,
            "Extended CPX opcode decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
                CpuProfile::mc6801,
                "CPX",
                AddressingMode::extended16
            ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xBC)
                == nullptr,
        "Unreviewed MC6801 extended CPX metadata was inherited"
    );

    const auto* hd6301v1_clv = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "CLV",
        AddressingMode::implied
    );
    passed &= expect(hd6301v1_clv != nullptr, "HD6301V1 CLV encoding missing");
    if (hd6301v1_clv != nullptr) {
        passed &= expect(hd6301v1_clv->opcode == 0x0A, "CLV opcode mismatch");
        passed &= expect(
            hd6301v1_clv->operand_bytes == 0
                && hd6301v1_clv->instruction_length == 1,
            "CLV length mismatch"
        );
        passed &= expect(hd6301v1_clv->base_cycles == 1, "CLV cycle mismatch");
        passed &= expect(
            hd6301v1_clv->flags.read_mask == 0U
                && hd6301v1_clv->flags.written_mask == mask({StatusFlag::v})
                && hd6301v1_clv->flags.preserved_mask
                    == mask({
                        StatusFlag::h,
                        StatusFlag::i,
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::c,
                    }),
            "CLV flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_clv->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_clv),
            "CLV debugger classification mismatch"
        );
        passed &= expect(
            hd6301v1_clv->operation == jr800::isa::Operation::clear_overflow,
            "CLV operation mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "CLV",
            AddressingMode::implied
        ) == nullptr,
        "Unreviewed MC6801 CLV metadata was inherited"
    );

    const auto* hd6301v1_sev = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "SEV",
        AddressingMode::implied
    );
    passed &= expect(hd6301v1_sev != nullptr, "HD6301V1 SEV encoding missing");
    if (hd6301v1_sev != nullptr) {
        passed &= expect(hd6301v1_sev->opcode == 0x0B, "SEV opcode mismatch");
        passed &= expect(
            hd6301v1_sev->operand_bytes == 0
                && hd6301v1_sev->instruction_length == 1,
            "SEV length mismatch"
        );
        passed &= expect(hd6301v1_sev->base_cycles == 1, "SEV cycle mismatch");
        passed &= expect(
            hd6301v1_sev->flags.read_mask == 0U
                && hd6301v1_sev->flags.written_mask == mask({StatusFlag::v})
                && hd6301v1_sev->flags.preserved_mask
                    == mask({
                        StatusFlag::h,
                        StatusFlag::i,
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::c,
                    }),
            "SEV flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_sev->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_sev),
            "SEV debugger classification mismatch"
        );
        passed &= expect(
            hd6301v1_sev->operation == jr800::isa::Operation::set_overflow,
            "SEV operation mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "SEV",
            AddressingMode::implied
        ) == nullptr,
        "Unreviewed MC6801 SEV metadata was inherited"
    );

    const auto* hd6301v1_clc = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "CLC",
        AddressingMode::implied
    );
    passed &= expect(hd6301v1_clc != nullptr, "HD6301V1 CLC encoding missing");
    if (hd6301v1_clc != nullptr) {
        passed &= expect(hd6301v1_clc->opcode == 0x0C, "CLC opcode mismatch");
        passed &= expect(
            hd6301v1_clc->operand_bytes == 0
                && hd6301v1_clc->instruction_length == 1,
            "CLC length mismatch"
        );
        passed &= expect(hd6301v1_clc->base_cycles == 1, "CLC cycle mismatch");
        passed &= expect(
            hd6301v1_clc->flags.read_mask == 0U
                && hd6301v1_clc->flags.written_mask == mask({StatusFlag::c})
                && hd6301v1_clc->flags.preserved_mask
                    == mask({
                        StatusFlag::h,
                        StatusFlag::i,
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                    }),
            "CLC flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_clc->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_clc),
            "CLC debugger classification mismatch"
        );
        passed &= expect(
            hd6301v1_clc->operation == jr800::isa::Operation::clear_carry,
            "CLC operation mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "CLC",
            AddressingMode::implied
        ) == nullptr,
        "Unreviewed MC6801 CLC metadata was inherited"
    );

    const auto* hd6301v1_sec = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "SEC",
        AddressingMode::implied
    );
    passed &= expect(hd6301v1_sec != nullptr, "HD6301V1 SEC encoding missing");
    if (hd6301v1_sec != nullptr) {
        passed &= expect(hd6301v1_sec->opcode == 0x0D, "SEC opcode mismatch");
        passed &= expect(
            hd6301v1_sec->operand_bytes == 0
                && hd6301v1_sec->instruction_length == 1,
            "SEC length mismatch"
        );
        passed &= expect(hd6301v1_sec->base_cycles == 1, "SEC cycle mismatch");
        passed &= expect(
            hd6301v1_sec->flags.read_mask == 0U
                && hd6301v1_sec->flags.written_mask == mask({StatusFlag::c})
                && hd6301v1_sec->flags.preserved_mask
                    == mask({
                        StatusFlag::h,
                        StatusFlag::i,
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                    }),
            "SEC flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_sec->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_sec),
            "SEC debugger classification mismatch"
        );
        passed &= expect(
            hd6301v1_sec->operation == jr800::isa::Operation::set_carry,
            "SEC operation mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "SEC",
            AddressingMode::implied
        ) == nullptr,
        "Unreviewed MC6801 SEC metadata was inherited"
    );

    const auto* hd6301v1_cli = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "CLI",
        AddressingMode::implied
    );
    passed &= expect(hd6301v1_cli != nullptr, "HD6301V1 CLI encoding missing");
    if (hd6301v1_cli != nullptr) {
        passed &= expect(hd6301v1_cli->opcode == 0x0E, "CLI opcode mismatch");
        passed &= expect(
            hd6301v1_cli->operand_bytes == 0
                && hd6301v1_cli->instruction_length == 1,
            "CLI length mismatch"
        );
        passed &= expect(hd6301v1_cli->base_cycles == 1, "CLI cycle mismatch");
        passed &= expect(
            hd6301v1_cli->flags.read_mask == 0U
                && hd6301v1_cli->flags.written_mask == mask({StatusFlag::i})
                && hd6301v1_cli->flags.preserved_mask
                    == mask({
                        StatusFlag::h,
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    }),
            "CLI flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_cli->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_cli),
            "CLI debugger classification mismatch"
        );
        passed &= expect(
            hd6301v1_cli->operation
                == jr800::isa::Operation::clear_interrupt_mask,
            "CLI operation mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "CLI",
            AddressingMode::implied
        ) == nullptr,
        "Unreviewed MC6801 CLI metadata was inherited"
    );

    const auto* hd6301v1_sei = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "SEI",
        AddressingMode::implied
    );
    passed &= expect(hd6301v1_sei != nullptr, "HD6301V1 SEI encoding missing");
    if (hd6301v1_sei != nullptr) {
        passed &= expect(hd6301v1_sei->opcode == 0x0F, "SEI opcode mismatch");
        passed &= expect(hd6301v1_sei->instruction_length == 1, "SEI length mismatch");
        passed &= expect(hd6301v1_sei->base_cycles == 1, "SEI cycle mismatch");
        passed &= expect(
            hd6301v1_sei->flags.written_mask == mask({StatusFlag::i})
                && hd6301v1_sei->flags.preserved_mask
                    == mask({
                        StatusFlag::h,
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    }),
            "SEI flag metadata mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "SEI",
            AddressingMode::implied
        ) == nullptr,
        "Unreviewed MC6801 SEI metadata was inherited"
    );

    const auto* hd6301v1_clra = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "CLRA",
        AddressingMode::implied
    );
    passed &= expect(hd6301v1_clra != nullptr, "HD6301V1 CLRA encoding missing");
    if (hd6301v1_clra != nullptr) {
        passed &= expect(hd6301v1_clra->opcode == 0x4F, "CLRA opcode mismatch");
        passed &= expect(hd6301v1_clra->instruction_length == 1, "CLRA length mismatch");
        passed &= expect(hd6301v1_clra->base_cycles == 1, "CLRA cycle mismatch");
        passed &= expect(
            hd6301v1_clra->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_clra->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i}),
            "CLRA flag metadata mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "CLRA",
            AddressingMode::implied
        ) == nullptr,
        "Unreviewed MC6801 CLRA metadata was inherited"
    );

    const auto* hd6301v1_clrb = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "CLRB",
        AddressingMode::implied
    );
    passed &= expect(hd6301v1_clrb != nullptr, "HD6301V1 CLRB encoding missing");
    if (hd6301v1_clrb != nullptr) {
        passed &= expect(hd6301v1_clrb->opcode == 0x5F, "CLRB opcode mismatch");
        passed &= expect(
            hd6301v1_clrb->operand_bytes == 0
                && hd6301v1_clrb->instruction_length == 1,
            "CLRB length mismatch"
        );
        passed &= expect(hd6301v1_clrb->base_cycles == 1, "CLRB cycle mismatch");
        passed &= expect(
            hd6301v1_clrb->flags.read_mask == 0U
                && hd6301v1_clrb->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_clrb->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i})
                && hd6301v1_clrb->flags.undefined_mask == 0U,
            "CLRB flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_clrb->classification == InstructionClass::linear
                && hd6301v1_clrb->operation
                    == jr800::isa::Operation::clear_accumulator_b
                && !jr800::isa::is_step_over_candidate(*hd6301v1_clrb),
            "CLRB operation or debugger classification mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "CLRB",
            AddressingMode::implied
        ) == nullptr,
        "Unreviewed MC6801 CLRB metadata was inherited"
    );

    const auto* hd6301v1_deca = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "DECA",
        AddressingMode::implied
    );
    passed &= expect(hd6301v1_deca != nullptr, "HD6301V1 DECA encoding missing");
    if (hd6301v1_deca != nullptr) {
        passed &= expect(hd6301v1_deca->opcode == 0x4A, "DECA opcode mismatch");
        passed &= expect(hd6301v1_deca->instruction_length == 1, "DECA length mismatch");
        passed &= expect(hd6301v1_deca->base_cycles == 1, "DECA cycle mismatch");
        passed &= expect(
            hd6301v1_deca->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_deca->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c}),
            "DECA flag metadata mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "DECA",
            AddressingMode::implied
        ) == nullptr,
        "Unreviewed MC6801 DECA metadata was inherited"
    );

    const auto* hd6301v1_decb = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "DECB",
        AddressingMode::implied
    );
    passed &= expect(
        hd6301v1_decb != nullptr,
        "HD6301V1 DECB encoding missing"
    );
    if (hd6301v1_decb != nullptr) {
        passed &= expect(hd6301v1_decb->opcode == 0x5A, "DECB opcode mismatch");
        passed &= expect(
            hd6301v1_decb->instruction_length == 1,
            "DECB length mismatch"
        );
        passed &= expect(
            hd6301v1_decb->base_cycles == 1,
            "DECB cycle mismatch"
        );
        passed &= expect(
            hd6301v1_decb->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_decb->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c}),
            "DECB flag metadata mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "DECB",
            AddressingMode::implied
        ) == nullptr,
        "Unreviewed MC6801 DECB metadata was inherited"
    );

    const auto* hd6301v1_inca = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "INCA",
        AddressingMode::implied
    );
    passed &= expect(hd6301v1_inca != nullptr, "HD6301V1 INCA encoding missing");
    if (hd6301v1_inca != nullptr) {
        passed &= expect(hd6301v1_inca->opcode == 0x4C, "INCA opcode mismatch");
        passed &= expect(hd6301v1_inca->instruction_length == 1, "INCA length mismatch");
        passed &= expect(hd6301v1_inca->base_cycles == 1, "INCA cycle mismatch");
        passed &= expect(
            hd6301v1_inca->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_inca->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c}),
            "INCA flag metadata mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "INCA",
            AddressingMode::implied
        ) == nullptr,
        "Unreviewed MC6801 INCA metadata was inherited"
    );

    const auto* hd6301v1_incb = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "INCB",
        AddressingMode::implied
    );
    passed &= expect(hd6301v1_incb != nullptr, "HD6301V1 INCB metadata missing");
    if (hd6301v1_incb != nullptr) {
        passed &= expect(hd6301v1_incb->opcode == 0x5C, "INCB opcode mismatch");
        passed &= expect(
            hd6301v1_incb->operand_bytes == 0
                && hd6301v1_incb->instruction_length == 1,
            "INCB length mismatch"
        );
        passed &= expect(hd6301v1_incb->base_cycles == 1, "INCB cycle mismatch");
        passed &= expect(
            hd6301v1_incb->flags.read_mask == 0U
                && hd6301v1_incb->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_incb->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c}),
            "INCB flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_incb->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_incb)
                && hd6301v1_incb->operation
                    == jr800::isa::Operation::increment_accumulator_b
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0x5C
                ) == hd6301v1_incb,
            "INCB operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "INCB",
            AddressingMode::implied
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x5C)
                == nullptr,
        "Unreviewed MC6801 INCB metadata was inherited"
    );
    const auto* hd6301v1_inc_extended = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "INC",
        AddressingMode::extended16
    );
    passed &= expect(
        hd6301v1_inc_extended != nullptr,
        "HD6301V1 extended INC metadata missing"
    );
    if (hd6301v1_inc_extended != nullptr) {
        passed &= expect(
            hd6301v1_inc_extended->opcode == 0x7C,
            "Extended INC opcode mismatch"
        );
        passed &= expect(
            hd6301v1_inc_extended->operand_bytes == 2
                && hd6301v1_inc_extended->instruction_length == 3,
            "Extended INC length mismatch"
        );
        passed &= expect(
            hd6301v1_inc_extended->base_cycles == 6,
            "Extended INC cycle mismatch"
        );
        passed &= expect(
            hd6301v1_inc_extended->flags.read_mask == 0U
                && hd6301v1_inc_extended->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_inc_extended->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c}),
            "Extended INC flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_inc_extended->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_inc_extended
                )
                && hd6301v1_inc_extended->operation
                    == jr800::isa::Operation::increment_memory
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0x7C
                ) == hd6301v1_inc_extended,
            "Extended INC operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "INC",
            AddressingMode::extended16
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x7C)
                == nullptr,
        "Unreviewed MC6801 extended INC metadata was inherited"
    );
    const auto* hd6301v1_inc_indexed = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "INC",
        AddressingMode::indexed8
    );
    passed &= expect(
        hd6301v1_inc_indexed != nullptr,
        "HD6301V1 indexed INC metadata missing"
    );
    if (hd6301v1_inc_indexed != nullptr) {
        passed &= expect(
            hd6301v1_inc_indexed->opcode == 0x6C,
            "Indexed INC opcode mismatch"
        );
        passed &= expect(
            hd6301v1_inc_indexed->operand_bytes == 1
                && hd6301v1_inc_indexed->instruction_length == 2,
            "Indexed INC length mismatch"
        );
        passed &= expect(
            hd6301v1_inc_indexed->base_cycles == 6,
            "Indexed INC cycle mismatch"
        );
        passed &= expect(
            hd6301v1_inc_indexed->flags.read_mask == 0U
                && hd6301v1_inc_indexed->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_inc_indexed->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c}),
            "Indexed INC flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_inc_indexed->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_inc_indexed
                )
                && hd6301v1_inc_indexed->operation
                    == jr800::isa::Operation::increment_memory
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0x6C
                ) == hd6301v1_inc_indexed,
            "Indexed INC operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "INC",
            AddressingMode::indexed8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x6C)
                == nullptr,
        "Unreviewed MC6801 indexed INC metadata was inherited"
    );

    const auto* hd6301v1_clr_indexed = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "CLR",
        AddressingMode::indexed8
    );
    passed &= expect(
        hd6301v1_clr_indexed != nullptr,
        "HD6301V1 indexed CLR encoding missing"
    );
    if (hd6301v1_clr_indexed != nullptr) {
        passed &= expect(
            hd6301v1_clr_indexed->opcode == 0x6F,
            "Indexed CLR opcode mismatch"
        );
        passed &= expect(
            hd6301v1_clr_indexed->instruction_length == 2,
            "Indexed CLR length mismatch"
        );
        passed &= expect(
            hd6301v1_clr_indexed->base_cycles == 5,
            "Indexed CLR cycle mismatch"
        );
        passed &= expect(
            hd6301v1_clr_indexed->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_clr_indexed->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i}),
            "Indexed CLR flag metadata mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "CLR",
            AddressingMode::indexed8
        ) == nullptr,
        "Unreviewed MC6801 indexed CLR metadata was inherited"
    );

    const auto* hd6301v1_clr_extended = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "CLR",
        AddressingMode::extended16
    );
    passed &= expect(
        hd6301v1_clr_extended != nullptr,
        "HD6301V1 extended CLR encoding missing"
    );
    if (hd6301v1_clr_extended != nullptr) {
        passed &= expect(
            hd6301v1_clr_extended->opcode == 0x7F,
            "Extended CLR opcode mismatch"
        );
        passed &= expect(
            hd6301v1_clr_extended->operand_bytes == 2
                && hd6301v1_clr_extended->instruction_length == 3,
            "Extended CLR length mismatch"
        );
        passed &= expect(
            hd6301v1_clr_extended->base_cycles == 5,
            "Extended CLR cycle mismatch"
        );
        passed &= expect(
            hd6301v1_clr_extended->flags.read_mask == 0U
                && hd6301v1_clr_extended->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_clr_extended->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i})
                && hd6301v1_clr_extended->flags.undefined_mask == 0U,
            "Extended CLR flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_clr_extended->classification
                    == InstructionClass::linear
                && hd6301v1_clr_extended->operation
                    == jr800::isa::Operation::clear_memory
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_clr_extended
                )
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0x7F
                ) == hd6301v1_clr_extended,
            "Extended CLR operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "CLR",
            AddressingMode::extended16
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x7F)
                == nullptr,
        "Unreviewed MC6801 extended CLR metadata was inherited"
    );
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::hd6301v1,
            "CLR",
            AddressingMode::direct8
        ) == nullptr,
        "Unreviewed direct CLR metadata was staged"
    );

    const auto* ldaa = jr800::isa::find_encoding(
        CpuProfile::mc6801,
        "LDAA",
        AddressingMode::immediate8
    );
    passed &= expect(ldaa != nullptr, "LDAA immediate encoding missing");
    if (ldaa != nullptr) {
        passed &= expect(ldaa->opcode == 0x86, "LDAA immediate opcode mismatch");
        passed &= expect(ldaa->operand_bytes == 1, "LDAA operand length mismatch");
        passed &= expect(ldaa->instruction_length == 2, "LDAA total length mismatch");
        passed &= expect(ldaa->base_cycles == 2, "LDAA cycle mismatch");
        passed &= expect(
            ldaa->flags.written_mask
                == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v}),
            "LDAA written flags mismatch"
        );
        passed &= expect(
            ldaa->flags.preserved_mask
                == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c}),
            "LDAA preserved flags mismatch"
        );
    }

    const auto* hd6301v1_ldab_immediate = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "LDAB",
        AddressingMode::immediate8
    );
    passed &= expect(
        hd6301v1_ldab_immediate != nullptr,
        "HD6301V1 immediate LDAB encoding missing"
    );
    if (hd6301v1_ldab_immediate != nullptr) {
        passed &= expect(
            hd6301v1_ldab_immediate->opcode == 0xC6,
            "Immediate LDAB opcode mismatch"
        );
        passed &= expect(
            hd6301v1_ldab_immediate->operand_bytes == 1
                && hd6301v1_ldab_immediate->instruction_length == 2,
            "Immediate LDAB length mismatch"
        );
        passed &= expect(
            hd6301v1_ldab_immediate->base_cycles == 2,
            "Immediate LDAB cycle mismatch"
        );
        passed &= expect(
            hd6301v1_ldab_immediate->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_ldab_immediate->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c}),
            "Immediate LDAB flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_ldab_immediate->classification
                == InstructionClass::linear,
            "Immediate LDAB debugger classification mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "LDAB",
            AddressingMode::immediate8
        ) == nullptr,
        "Unreviewed MC6801 immediate LDAB metadata was inherited"
    );

    const auto* hd6301v1_ldab_direct = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "LDAB",
        AddressingMode::direct8
    );
    passed &= expect(
        hd6301v1_ldab_direct != nullptr,
        "HD6301V1 direct LDAB encoding missing"
    );
    if (hd6301v1_ldab_direct != nullptr) {
        passed &= expect(
            hd6301v1_ldab_direct->opcode == 0xD6,
            "Direct LDAB opcode mismatch"
        );
        passed &= expect(
            hd6301v1_ldab_direct->operand_bytes == 1
                && hd6301v1_ldab_direct->instruction_length == 2,
            "Direct LDAB length mismatch"
        );
        passed &= expect(
            hd6301v1_ldab_direct->base_cycles == 3,
            "Direct LDAB cycle mismatch"
        );
        passed &= expect(
            hd6301v1_ldab_direct->flags.read_mask == 0U
                && hd6301v1_ldab_direct->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_ldab_direct->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c}),
            "Direct LDAB flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_ldab_direct->classification
                    == InstructionClass::linear
                && hd6301v1_ldab_direct->operation
                    == jr800::isa::Operation::load_accumulator_b,
            "Direct LDAB operation or debugger classification mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "LDAB",
            AddressingMode::direct8
        ) == nullptr,
        "Unreviewed MC6801 direct LDAB metadata was inherited"
    );

    const auto* hd6301v1_ldab_extended = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "LDAB",
        AddressingMode::extended16
    );
    passed &= expect(
        hd6301v1_ldab_extended != nullptr,
        "HD6301V1 extended LDAB encoding missing"
    );
    if (hd6301v1_ldab_extended != nullptr) {
        passed &= expect(
            hd6301v1_ldab_extended->opcode == 0xF6,
            "Extended LDAB opcode mismatch"
        );
        passed &= expect(
            hd6301v1_ldab_extended->operand_bytes == 2
                && hd6301v1_ldab_extended->instruction_length == 3,
            "Extended LDAB length mismatch"
        );
        passed &= expect(
            hd6301v1_ldab_extended->base_cycles == 4,
            "Extended LDAB cycle mismatch"
        );
        passed &= expect(
            hd6301v1_ldab_extended->flags.read_mask == 0U
                && hd6301v1_ldab_extended->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_ldab_extended->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c}),
            "Extended LDAB flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_ldab_extended->classification
                    == InstructionClass::linear
                && hd6301v1_ldab_extended->operation
                    == jr800::isa::Operation::load_accumulator_b,
            "Extended LDAB operation or classification mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "LDAB",
            AddressingMode::extended16
        ) == nullptr,
        "Unreviewed MC6801 extended LDAB metadata was inherited"
    );
    const auto* hd6301v1_ldab_indexed = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "LDAB",
        AddressingMode::indexed8
    );
    passed &= expect(
        hd6301v1_ldab_indexed != nullptr,
        "HD6301V1 indexed LDAB encoding missing"
    );
    if (hd6301v1_ldab_indexed != nullptr) {
        passed &= expect(
            hd6301v1_ldab_indexed->opcode == 0xE6,
            "Indexed LDAB opcode mismatch"
        );
        passed &= expect(
            hd6301v1_ldab_indexed->operand_bytes == 1
                && hd6301v1_ldab_indexed->instruction_length == 2,
            "Indexed LDAB length mismatch"
        );
        passed &= expect(
            hd6301v1_ldab_indexed->base_cycles == 4,
            "Indexed LDAB cycle mismatch"
        );
        passed &= expect(
            hd6301v1_ldab_indexed->flags.read_mask == 0U
                && hd6301v1_ldab_indexed->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_ldab_indexed->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c})
                && hd6301v1_ldab_indexed->flags.undefined_mask == 0U,
            "Indexed LDAB flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_ldab_indexed->classification
                    == InstructionClass::linear
                && hd6301v1_ldab_indexed->operation
                    == jr800::isa::Operation::load_accumulator_b
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_ldab_indexed
                )
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xE6
                ) == hd6301v1_ldab_indexed,
            "Indexed LDAB operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "LDAB",
            AddressingMode::indexed8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xE6)
                == nullptr,
        "Unreviewed MC6801 indexed LDAB metadata was inherited"
    );

    const auto* hd6301v1_ldd_immediate = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "LDD",
        AddressingMode::immediate16
    );
    passed &= expect(
        hd6301v1_ldd_immediate != nullptr,
        "HD6301V1 immediate LDD encoding missing"
    );
    if (hd6301v1_ldd_immediate != nullptr) {
        passed &= expect(
            hd6301v1_ldd_immediate->opcode == 0xCC,
            "Immediate LDD opcode mismatch"
        );
        passed &= expect(
            hd6301v1_ldd_immediate->operand_bytes == 2
                && hd6301v1_ldd_immediate->instruction_length == 3,
            "Immediate LDD length mismatch"
        );
        passed &= expect(
            hd6301v1_ldd_immediate->base_cycles == 3,
            "Immediate LDD cycle mismatch"
        );
        passed &= expect(
            hd6301v1_ldd_immediate->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_ldd_immediate->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c}),
            "Immediate LDD flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_ldd_immediate->classification
                == InstructionClass::linear,
            "Immediate LDD debugger classification mismatch"
        );
        passed &= expect(
            hd6301v1_ldd_immediate->operation
                == jr800::isa::Operation::load_double_accumulator,
            "Immediate LDD operation mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "LDD",
            AddressingMode::immediate16
        ) == nullptr,
        "Unreviewed MC6801 immediate LDD metadata was inherited"
    );

    const auto* hd6301v1_ldd_direct = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "LDD",
        AddressingMode::direct8
    );
    passed &= expect(
        hd6301v1_ldd_direct != nullptr,
        "HD6301V1 direct LDD encoding missing"
    );
    if (hd6301v1_ldd_direct != nullptr) {
        passed &= expect(
            hd6301v1_ldd_direct->opcode == 0xDC,
            "Direct LDD opcode mismatch"
        );
        passed &= expect(
            hd6301v1_ldd_direct->operand_bytes == 1
                && hd6301v1_ldd_direct->instruction_length == 2,
            "Direct LDD length mismatch"
        );
        passed &= expect(
            hd6301v1_ldd_direct->base_cycles == 4,
            "Direct LDD cycle mismatch"
        );
        passed &= expect(
            hd6301v1_ldd_direct->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_ldd_direct->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c}),
            "Direct LDD flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_ldd_direct->classification
                    == InstructionClass::linear
                && hd6301v1_ldd_direct->operation
                    == jr800::isa::Operation::load_double_accumulator
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_ldd_direct
                )
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xDC
                ) == hd6301v1_ldd_direct,
            "Direct LDD operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "LDD",
            AddressingMode::direct8
        ) == nullptr
            && jr800::isa::decode_instruction(
                CpuProfile::mc6801,
                0xDC
            ) == nullptr,
        "Unreviewed MC6801 direct LDD metadata was inherited"
    );

    const auto* hd6301v1_ldd_indexed = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "LDD",
        AddressingMode::indexed8
    );
    passed &= expect(
        hd6301v1_ldd_indexed != nullptr,
        "HD6301V1 indexed LDD encoding missing"
    );
    if (hd6301v1_ldd_indexed != nullptr) {
        passed &= expect(
            hd6301v1_ldd_indexed->opcode == 0xEC,
            "Indexed LDD opcode mismatch"
        );
        passed &= expect(
            hd6301v1_ldd_indexed->operand_bytes == 1
                && hd6301v1_ldd_indexed->instruction_length == 2,
            "Indexed LDD length mismatch"
        );
        passed &= expect(
            hd6301v1_ldd_indexed->base_cycles == 5,
            "Indexed LDD cycle mismatch"
        );
        passed &= expect(
            hd6301v1_ldd_indexed->flags.read_mask == 0U
                && hd6301v1_ldd_indexed->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_ldd_indexed->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c})
                && hd6301v1_ldd_indexed->flags.undefined_mask == 0U,
            "Indexed LDD flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_ldd_indexed->classification
                    == InstructionClass::linear
                && hd6301v1_ldd_indexed->operation
                    == jr800::isa::Operation::load_double_accumulator
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_ldd_indexed
                )
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xEC
                ) == hd6301v1_ldd_indexed,
            "Indexed LDD operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "LDD",
            AddressingMode::indexed8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xEC)
                == nullptr,
        "Unreviewed MC6801 indexed LDD metadata was inherited"
    );
    const auto* hd6301v1_ldd_extended = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "LDD",
        AddressingMode::extended16
    );
    passed &= expect(
        hd6301v1_ldd_extended != nullptr,
        "HD6301V1 extended LDD encoding missing"
    );
    if (hd6301v1_ldd_extended != nullptr) {
        passed &= expect(
            hd6301v1_ldd_extended->opcode == 0xFCU
                && hd6301v1_ldd_extended->operand_bytes == 2U
                && hd6301v1_ldd_extended->instruction_length == 3U
                && hd6301v1_ldd_extended->base_cycles == 5U,
            "Extended LDD opcode, length, or cycle metadata mismatch"
        );
        passed &= expect(
            hd6301v1_ldd_extended->flags.read_mask == 0U
                && hd6301v1_ldd_extended->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_ldd_extended->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c})
                && hd6301v1_ldd_extended->flags.undefined_mask == 0U,
            "Extended LDD flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_ldd_extended->classification
                    == InstructionClass::linear
                && hd6301v1_ldd_extended->operation
                    == jr800::isa::Operation::load_double_accumulator
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_ldd_extended
                )
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xFCU
                ) == hd6301v1_ldd_extended,
            "Extended LDD operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "LDD",
            AddressingMode::extended16
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xFCU)
                == nullptr,
        "Unreviewed MC6801 extended LDD metadata was inherited"
    );

    const auto* hd6301v1_mul = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "MUL",
        AddressingMode::implied
    );
    passed &= expect(hd6301v1_mul != nullptr, "HD6301V1 MUL encoding missing");
    if (hd6301v1_mul != nullptr) {
        passed &= expect(hd6301v1_mul->opcode == 0x3D, "MUL opcode mismatch");
        passed &= expect(
            hd6301v1_mul->operand_bytes == 0
                && hd6301v1_mul->instruction_length == 1,
            "MUL length mismatch"
        );
        passed &= expect(hd6301v1_mul->base_cycles == 7, "MUL cycle mismatch");
        passed &= expect(
            hd6301v1_mul->flags.written_mask == mask({StatusFlag::c})
                && hd6301v1_mul->flags.preserved_mask
                    == mask({
                        StatusFlag::h,
                        StatusFlag::i,
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                    }),
            "MUL flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_mul->classification == InstructionClass::linear,
            "MUL debugger classification mismatch"
        );
        passed &= expect(
            hd6301v1_mul->operation
                == jr800::isa::Operation::multiply_unsigned_accumulators,
            "MUL operation mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "MUL",
            AddressingMode::implied
        ) == nullptr,
        "Unreviewed MC6801 MUL metadata was inherited"
    );

    const auto* hd6301v1_nega = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "NEGA",
        AddressingMode::implied
    );
    passed &= expect(hd6301v1_nega != nullptr, "HD6301V1 NEGA metadata missing");
    if (hd6301v1_nega != nullptr) {
        passed &= expect(hd6301v1_nega->opcode == 0x40, "NEGA opcode mismatch");
        passed &= expect(
            hd6301v1_nega->operand_bytes == 0
                && hd6301v1_nega->instruction_length == 1,
            "NEGA length mismatch"
        );
        passed &= expect(hd6301v1_nega->base_cycles == 1, "NEGA cycle mismatch");
        passed &= expect(
            hd6301v1_nega->flags.read_mask == 0U
                && hd6301v1_nega->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_nega->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i}),
            "NEGA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_nega->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_nega)
                && hd6301v1_nega->operation
                    == jr800::isa::Operation::negate_accumulator_a
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0x40
                ) == hd6301v1_nega,
            "NEGA operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "NEGA",
            AddressingMode::implied
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x40)
                == nullptr,
        "Unreviewed MC6801 NEGA metadata was inherited"
    );
    const auto* hd6301v1_negb = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "NEGB",
        AddressingMode::implied
    );
    passed &= expect(hd6301v1_negb != nullptr, "HD6301V1 NEGB metadata missing");
    if (hd6301v1_negb != nullptr) {
        passed &= expect(hd6301v1_negb->opcode == 0x50, "NEGB opcode mismatch");
        passed &= expect(
            hd6301v1_negb->operand_bytes == 0
                && hd6301v1_negb->instruction_length == 1,
            "NEGB length mismatch"
        );
        passed &= expect(hd6301v1_negb->base_cycles == 1, "NEGB cycle mismatch");
        passed &= expect(
            hd6301v1_negb->flags.read_mask == 0U
                && hd6301v1_negb->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_negb->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i}),
            "NEGB flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_negb->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_negb)
                && hd6301v1_negb->operation
                    == jr800::isa::Operation::negate_accumulator_b
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0x50
                ) == hd6301v1_negb,
            "NEGB operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "NEGB",
            AddressingMode::implied
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x50)
                == nullptr,
        "Unreviewed MC6801 NEGB metadata was inherited"
    );
    struct MemoryUnaryCase {
        const char* mnemonic;
        AddressingMode mode;
        std::uint8_t opcode;
        std::uint8_t operand_bytes;
        bool reads_carry;
        jr800::isa::Operation operation;
        std::uint8_t written_mask = static_cast<std::uint8_t>(
            jr800::isa::flag_mask(jr800::isa::StatusFlag::n)
            | jr800::isa::flag_mask(jr800::isa::StatusFlag::z)
            | jr800::isa::flag_mask(jr800::isa::StatusFlag::v)
            | jr800::isa::flag_mask(jr800::isa::StatusFlag::c)
        );
        std::uint8_t preserved_mask = static_cast<std::uint8_t>(
            jr800::isa::flag_mask(jr800::isa::StatusFlag::h)
            | jr800::isa::flag_mask(jr800::isa::StatusFlag::i)
        );
    };
    constexpr std::array memory_unary_cases{
        MemoryUnaryCase{
            "NEG",
            AddressingMode::indexed8,
            0x60U,
            1U,
            false,
            jr800::isa::Operation::negate_memory,
        },
        MemoryUnaryCase{
            "NEG",
            AddressingMode::extended16,
            0x70U,
            2U,
            false,
            jr800::isa::Operation::negate_memory,
        },
        MemoryUnaryCase{
            "COM",
            AddressingMode::indexed8,
            0x63U,
            1U,
            false,
            jr800::isa::Operation::complement_memory,
        },
        MemoryUnaryCase{
            "COM",
            AddressingMode::extended16,
            0x73U,
            2U,
            false,
            jr800::isa::Operation::complement_memory,
        },
        MemoryUnaryCase{
            "LSR",
            AddressingMode::indexed8,
            0x64U,
            1U,
            false,
            jr800::isa::Operation::logical_shift_right_memory,
        },
        MemoryUnaryCase{
            "LSR",
            AddressingMode::extended16,
            0x74U,
            2U,
            false,
            jr800::isa::Operation::logical_shift_right_memory,
        },
        MemoryUnaryCase{
            "ROR",
            AddressingMode::indexed8,
            0x66U,
            1U,
            true,
            jr800::isa::Operation::rotate_right_memory,
        },
        MemoryUnaryCase{
            "ROR",
            AddressingMode::extended16,
            0x76U,
            2U,
            true,
            jr800::isa::Operation::rotate_right_memory,
        },
        MemoryUnaryCase{
            "ASR",
            AddressingMode::indexed8,
            0x67U,
            1U,
            false,
            jr800::isa::Operation::arithmetic_shift_right_memory,
        },
        MemoryUnaryCase{
            "ASR",
            AddressingMode::extended16,
            0x77U,
            2U,
            false,
            jr800::isa::Operation::arithmetic_shift_right_memory,
        },
        MemoryUnaryCase{
            "ASL",
            AddressingMode::indexed8,
            0x68U,
            1U,
            false,
            jr800::isa::Operation::arithmetic_shift_left_memory,
        },
        MemoryUnaryCase{
            "ASL",
            AddressingMode::extended16,
            0x78U,
            2U,
            false,
            jr800::isa::Operation::arithmetic_shift_left_memory,
        },
        MemoryUnaryCase{
            "ROL",
            AddressingMode::indexed8,
            0x69U,
            1U,
            true,
            jr800::isa::Operation::rotate_left_memory,
        },
        MemoryUnaryCase{
            "ROL",
            AddressingMode::extended16,
            0x79U,
            2U,
            true,
            jr800::isa::Operation::rotate_left_memory,
        },
        MemoryUnaryCase{
            "DEC",
            AddressingMode::indexed8,
            0x6AU,
            1U,
            false,
            jr800::isa::Operation::decrement_memory,
            mask({StatusFlag::n, StatusFlag::z, StatusFlag::v}),
            mask({StatusFlag::h, StatusFlag::i, StatusFlag::c}),
        },
        MemoryUnaryCase{
            "DEC",
            AddressingMode::extended16,
            0x7AU,
            2U,
            false,
            jr800::isa::Operation::decrement_memory,
            mask({StatusFlag::n, StatusFlag::z, StatusFlag::v}),
            mask({StatusFlag::h, StatusFlag::i, StatusFlag::c}),
        },
    };
    for (const auto& test_case : memory_unary_cases) {
        const auto* hd6301v1_memory_unary = jr800::isa::find_encoding(
            CpuProfile::hd6301v1,
            test_case.mnemonic,
            test_case.mode
        );
        passed &= expect(
            hd6301v1_memory_unary != nullptr,
            "HD6301V1 memory-unary metadata missing"
        );
        if (hd6301v1_memory_unary != nullptr) {
            passed &= expect(
                hd6301v1_memory_unary->opcode == test_case.opcode
                    && hd6301v1_memory_unary->operand_bytes
                        == test_case.operand_bytes
                    && hd6301v1_memory_unary->instruction_length
                        == static_cast<std::uint8_t>(
                            test_case.operand_bytes + 1U
                        )
                    && hd6301v1_memory_unary->base_cycles == 6U,
                "Memory-unary opcode, length, or cycle metadata mismatch"
            );
            passed &= expect(
                hd6301v1_memory_unary->flags.read_mask
                        == (test_case.reads_carry
                            ? mask({StatusFlag::c})
                            : 0U)
                    && hd6301v1_memory_unary->flags.written_mask
                        == test_case.written_mask
                    && hd6301v1_memory_unary->flags.preserved_mask
                        == test_case.preserved_mask
                    && hd6301v1_memory_unary->flags.undefined_mask == 0U,
                "Memory-unary flag metadata mismatch"
            );
            passed &= expect(
                hd6301v1_memory_unary->classification
                    == InstructionClass::linear
                    && !jr800::isa::is_step_over_candidate(
                        *hd6301v1_memory_unary
                    )
                    && hd6301v1_memory_unary->operation
                        == test_case.operation
                    && jr800::isa::decode_instruction(
                        CpuProfile::hd6301v1,
                        test_case.opcode
                    ) == hd6301v1_memory_unary,
                "Memory-unary operation, classification, or decode mismatch"
            );
        }
        passed &= expect(
            jr800::isa::find_encoding(
                CpuProfile::mc6801,
                test_case.mnemonic,
                test_case.mode
            ) == nullptr
                && jr800::isa::decode_instruction(
                    CpuProfile::mc6801,
                    test_case.opcode
                ) == nullptr,
            "Unreviewed MC6801 memory-unary metadata was inherited"
        );
    }

    const auto* hd6301v1_coma = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "COMA",
        AddressingMode::implied
    );
    passed &= expect(hd6301v1_coma != nullptr, "HD6301V1 COMA encoding missing");
    if (hd6301v1_coma != nullptr) {
        passed &= expect(hd6301v1_coma->opcode == 0x43, "COMA opcode mismatch");
        passed &= expect(
            hd6301v1_coma->operand_bytes == 0
                && hd6301v1_coma->instruction_length == 1,
            "COMA length mismatch"
        );
        passed &= expect(hd6301v1_coma->base_cycles == 1, "COMA cycle mismatch");
        passed &= expect(
            hd6301v1_coma->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_coma->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i}),
            "COMA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_coma->classification == InstructionClass::linear,
            "COMA debugger classification mismatch"
        );
        passed &= expect(
            hd6301v1_coma->operation
                == jr800::isa::Operation::complement_accumulator_a,
            "COMA operation mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "COMA",
            AddressingMode::implied
        ) == nullptr,
        "Unreviewed MC6801 COMA metadata was inherited"
    );

    const auto* hd6301v1_comb = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "COMB",
        AddressingMode::implied
    );
    passed &= expect(hd6301v1_comb != nullptr, "HD6301V1 COMB encoding missing");
    if (hd6301v1_comb != nullptr) {
        passed &= expect(hd6301v1_comb->opcode == 0x53, "COMB opcode mismatch");
        passed &= expect(
            hd6301v1_comb->operand_bytes == 0
                && hd6301v1_comb->instruction_length == 1,
            "COMB length mismatch"
        );
        passed &= expect(hd6301v1_comb->base_cycles == 1, "COMB cycle mismatch");
        passed &= expect(
            hd6301v1_comb->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_comb->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i}),
            "COMB flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_comb->classification == InstructionClass::linear,
            "COMB debugger classification mismatch"
        );
        passed &= expect(
            hd6301v1_comb->operation
                == jr800::isa::Operation::complement_accumulator_b,
            "COMB operation mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "COMB",
            AddressingMode::implied
        ) == nullptr,
        "Unreviewed MC6801 COMB metadata was inherited"
    );

    const auto* hd6301v1_lsra = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "LSRA",
        AddressingMode::implied
    );
    passed &= expect(hd6301v1_lsra != nullptr, "HD6301V1 LSRA encoding missing");
    if (hd6301v1_lsra != nullptr) {
        passed &= expect(hd6301v1_lsra->opcode == 0x44, "LSRA opcode mismatch");
        passed &= expect(
            hd6301v1_lsra->operand_bytes == 0
                && hd6301v1_lsra->instruction_length == 1,
            "LSRA length mismatch"
        );
        passed &= expect(
            hd6301v1_lsra->base_cycles == 1,
            "LSRA cycle mismatch"
        );
        passed &= expect(
            hd6301v1_lsra->flags.read_mask == 0U
                && hd6301v1_lsra->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_lsra->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i}),
            "LSRA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_lsra->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_lsra)
                && hd6301v1_lsra->operation
                    == jr800::isa::Operation::logical_shift_right_accumulator_a
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0x44
                ) == hd6301v1_lsra,
            "LSRA operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "LSRA",
            AddressingMode::implied
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x44)
                == nullptr,
        "Unreviewed MC6801 LSRA metadata was inherited"
    );
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::hd6301v1,
            "LSRA",
            AddressingMode::direct8
        ) == nullptr
            && jr800::isa::find_encoding(
                CpuProfile::hd6301v1,
                "LSRA",
                AddressingMode::indexed8
            ) == nullptr
            && jr800::isa::find_encoding(
                CpuProfile::hd6301v1,
                "LSRA",
                AddressingMode::extended16
            ) == nullptr,
        "Non-implied LSRA metadata was staged"
    );
    const auto* hd6301v1_lsrb = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "LSRB",
        AddressingMode::implied
    );
    passed &= expect(hd6301v1_lsrb != nullptr, "HD6301V1 LSRB encoding missing");
    if (hd6301v1_lsrb != nullptr) {
        passed &= expect(hd6301v1_lsrb->opcode == 0x54, "LSRB opcode mismatch");
        passed &= expect(
            hd6301v1_lsrb->operand_bytes == 0
                && hd6301v1_lsrb->instruction_length == 1,
            "LSRB length mismatch"
        );
        passed &= expect(
            hd6301v1_lsrb->base_cycles == 1,
            "LSRB cycle mismatch"
        );
        passed &= expect(
            hd6301v1_lsrb->flags.read_mask == 0U
                && hd6301v1_lsrb->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_lsrb->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i}),
            "LSRB flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_lsrb->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_lsrb)
                && hd6301v1_lsrb->operation
                    == jr800::isa::Operation::logical_shift_right_accumulator_b
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0x54
                ) == hd6301v1_lsrb,
            "LSRB operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "LSRB",
            AddressingMode::implied
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x54)
                == nullptr,
        "Unreviewed MC6801 LSRB metadata was inherited"
    );
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::hd6301v1,
            "LSRB",
            AddressingMode::direct8
        ) == nullptr
            && jr800::isa::find_encoding(
                CpuProfile::hd6301v1,
                "LSRB",
                AddressingMode::indexed8
            ) == nullptr
            && jr800::isa::find_encoding(
                CpuProfile::hd6301v1,
                "LSRB",
                AddressingMode::extended16
            ) == nullptr,
        "Non-implied LSRB metadata was staged"
    );

    const auto* hd6301v1_rola = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "ROLA",
        AddressingMode::implied
    );
    passed &= expect(hd6301v1_rola != nullptr, "HD6301V1 ROLA metadata missing");
    if (hd6301v1_rola != nullptr) {
        passed &= expect(hd6301v1_rola->opcode == 0x49, "ROLA opcode mismatch");
        passed &= expect(
            hd6301v1_rola->operand_bytes == 0
                && hd6301v1_rola->instruction_length == 1,
            "ROLA length mismatch"
        );
        passed &= expect(hd6301v1_rola->base_cycles == 1, "ROLA cycle mismatch");
        passed &= expect(
            hd6301v1_rola->flags.read_mask == mask({StatusFlag::c})
                && hd6301v1_rola->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_rola->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i}),
            "ROLA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_rola->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_rola)
                && hd6301v1_rola->operation
                    == jr800::isa::Operation::rotate_left_accumulator_a
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0x49
                ) == hd6301v1_rola,
            "ROLA operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "ROLA",
            AddressingMode::implied
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x49)
                == nullptr,
        "Unreviewed MC6801 ROLA metadata was inherited"
    );
    const auto* hd6301v1_rolb = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "ROLB",
        AddressingMode::implied
    );
    passed &= expect(
        hd6301v1_rolb != nullptr,
        "HD6301V1 ROLB encoding missing"
    );
    if (hd6301v1_rolb != nullptr) {
        passed &= expect(
            hd6301v1_rolb->opcode == 0x59,
            "ROLB opcode mismatch"
        );
        passed &= expect(
            hd6301v1_rolb->operand_bytes == 0
                && hd6301v1_rolb->instruction_length == 1,
            "ROLB length mismatch"
        );
        passed &= expect(
            hd6301v1_rolb->base_cycles == 1,
            "ROLB cycle mismatch"
        );
        passed &= expect(
            hd6301v1_rolb->flags.read_mask == mask({StatusFlag::c})
                && hd6301v1_rolb->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_rolb->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i}),
            "ROLB flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_rolb->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_rolb)
                && hd6301v1_rolb->operation
                    == jr800::isa::Operation::rotate_left_accumulator_b
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0x59
                ) == hd6301v1_rolb,
            "ROLB operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "ROLB",
            AddressingMode::implied
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x59)
                == nullptr,
        "Unreviewed MC6801 ROLB metadata was inherited"
    );

    const auto* hd6301v1_rora = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "RORA",
        AddressingMode::implied
    );
    passed &= expect(hd6301v1_rora != nullptr, "HD6301V1 RORA encoding missing");
    if (hd6301v1_rora != nullptr) {
        passed &= expect(hd6301v1_rora->opcode == 0x46, "RORA opcode mismatch");
        passed &= expect(
            hd6301v1_rora->operand_bytes == 0
                && hd6301v1_rora->instruction_length == 1,
            "RORA length mismatch"
        );
        passed &= expect(
            hd6301v1_rora->base_cycles == 1,
            "RORA cycle mismatch"
        );
        passed &= expect(
            hd6301v1_rora->flags.read_mask == mask({StatusFlag::c})
                && hd6301v1_rora->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_rora->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i}),
            "RORA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_rora->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_rora)
                && hd6301v1_rora->operation
                    == jr800::isa::Operation::rotate_right_accumulator_a
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0x46
                ) == hd6301v1_rora,
            "RORA operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "RORA",
            AddressingMode::implied
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x46)
                == nullptr,
        "Unreviewed MC6801 RORA metadata was inherited"
    );
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::hd6301v1,
            "RORA",
            AddressingMode::direct8
        ) == nullptr
            && jr800::isa::find_encoding(
                CpuProfile::hd6301v1,
                "RORA",
                AddressingMode::indexed8
            ) == nullptr
            && jr800::isa::find_encoding(
                CpuProfile::hd6301v1,
                "RORA",
                AddressingMode::extended16
            ) == nullptr,
        "Non-implied RORA metadata was staged"
    );
    const auto* hd6301v1_rorb = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "RORB",
        AddressingMode::implied
    );
    passed &= expect(hd6301v1_rorb != nullptr, "HD6301V1 RORB encoding missing");
    if (hd6301v1_rorb != nullptr) {
        passed &= expect(hd6301v1_rorb->opcode == 0x56, "RORB opcode mismatch");
        passed &= expect(
            hd6301v1_rorb->operand_bytes == 0
                && hd6301v1_rorb->instruction_length == 1,
            "RORB length mismatch"
        );
        passed &= expect(
            hd6301v1_rorb->base_cycles == 1,
            "RORB cycle mismatch"
        );
        passed &= expect(
            hd6301v1_rorb->flags.read_mask == mask({StatusFlag::c})
                && hd6301v1_rorb->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_rorb->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i}),
            "RORB flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_rorb->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_rorb)
                && hd6301v1_rorb->operation
                    == jr800::isa::Operation::rotate_right_accumulator_b
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0x56
                ) == hd6301v1_rorb,
            "RORB operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "RORB",
            AddressingMode::implied
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x56)
                == nullptr,
        "Unreviewed MC6801 RORB metadata was inherited"
    );
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::hd6301v1,
            "RORB",
            AddressingMode::direct8
        ) == nullptr
            && jr800::isa::find_encoding(
                CpuProfile::hd6301v1,
                "RORB",
                AddressingMode::indexed8
            ) == nullptr
            && jr800::isa::find_encoding(
                CpuProfile::hd6301v1,
                "RORB",
                AddressingMode::extended16
            ) == nullptr,
        "Non-implied RORB metadata was staged"
    );

    const auto* hd6301v1_asla = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "ASLA",
        AddressingMode::implied
    );
    passed &= expect(hd6301v1_asla != nullptr, "HD6301V1 ASLA encoding missing");
    if (hd6301v1_asla != nullptr) {
        passed &= expect(hd6301v1_asla->opcode == 0x48, "ASLA opcode mismatch");
        passed &= expect(
            hd6301v1_asla->operand_bytes == 0
                && hd6301v1_asla->instruction_length == 1,
            "ASLA length mismatch"
        );
        passed &= expect(
            hd6301v1_asla->base_cycles == 1,
            "ASLA cycle mismatch"
        );
        passed &= expect(
            hd6301v1_asla->flags.read_mask == 0U
                && hd6301v1_asla->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_asla->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i}),
            "ASLA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_asla->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_asla)
                && hd6301v1_asla->operation
                    == jr800::isa::Operation::arithmetic_shift_left_accumulator_a
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0x48
                ) == hd6301v1_asla,
            "ASLA operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "ASLA",
            AddressingMode::implied
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x48)
                == nullptr,
        "Unreviewed MC6801 ASLA metadata was inherited"
    );
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::hd6301v1,
            "ASLA",
            AddressingMode::direct8
        ) == nullptr
            && jr800::isa::find_encoding(
                CpuProfile::hd6301v1,
                "ASLA",
                AddressingMode::indexed8
            ) == nullptr
            && jr800::isa::find_encoding(
                CpuProfile::hd6301v1,
                "ASLA",
                AddressingMode::extended16
            ) == nullptr
            && jr800::isa::find_encoding(
                CpuProfile::hd6301v1,
                "ASL",
                AddressingMode::direct8
            ) == nullptr,
        "Non-implied ASLA or direct ASL metadata was staged"
    );

    const auto* hd6301v1_aslb = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "ASLB",
        AddressingMode::implied
    );
    passed &= expect(hd6301v1_aslb != nullptr, "HD6301V1 ASLB encoding missing");
    if (hd6301v1_aslb != nullptr) {
        passed &= expect(hd6301v1_aslb->opcode == 0x58, "ASLB opcode mismatch");
        passed &= expect(
            hd6301v1_aslb->operand_bytes == 0
                && hd6301v1_aslb->instruction_length == 1,
            "ASLB length mismatch"
        );
        passed &= expect(
            hd6301v1_aslb->base_cycles == 1,
            "ASLB cycle mismatch"
        );
        passed &= expect(
            hd6301v1_aslb->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_aslb->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i}),
            "ASLB flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_aslb->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_aslb),
            "ASLB debugger classification mismatch"
        );
        passed &= expect(
            hd6301v1_aslb->operation
                == jr800::isa::Operation::arithmetic_shift_left_accumulator_b,
            "ASLB operation mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "ASLB",
            AddressingMode::implied
        ) == nullptr,
        "Unreviewed MC6801 ASLB metadata was inherited"
    );

    const auto* hd6301v1_asra = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "ASRA",
        AddressingMode::implied
    );
    passed &= expect(
        hd6301v1_asra != nullptr,
        "HD6301V1 ASRA encoding missing"
    );
    if (hd6301v1_asra != nullptr) {
        passed &= expect(hd6301v1_asra->opcode == 0x47, "ASRA opcode mismatch");
        passed &= expect(
            hd6301v1_asra->operand_bytes == 0
                && hd6301v1_asra->instruction_length == 1,
            "ASRA length mismatch"
        );
        passed &= expect(
            hd6301v1_asra->base_cycles == 1,
            "ASRA cycle mismatch"
        );
        passed &= expect(
            hd6301v1_asra->flags.read_mask == 0U
                && hd6301v1_asra->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_asra->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i}),
            "ASRA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_asra->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_asra)
                && hd6301v1_asra->operation
                    == jr800::isa::Operation::
                        arithmetic_shift_right_accumulator_a
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0x47
                ) == hd6301v1_asra,
            "ASRA operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "ASRA",
            AddressingMode::implied
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x47)
                == nullptr,
        "Unreviewed MC6801 ASRA metadata was inherited"
    );
    const auto* hd6301v1_asrb = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "ASRB",
        AddressingMode::implied
    );
    passed &= expect(
        hd6301v1_asrb != nullptr,
        "HD6301V1 ASRB encoding missing"
    );
    if (hd6301v1_asrb != nullptr) {
        passed &= expect(hd6301v1_asrb->opcode == 0x57, "ASRB opcode mismatch");
        passed &= expect(
            hd6301v1_asrb->operand_bytes == 0
                && hd6301v1_asrb->instruction_length == 1,
            "ASRB length mismatch"
        );
        passed &= expect(
            hd6301v1_asrb->base_cycles == 1,
            "ASRB cycle mismatch"
        );
        passed &= expect(
            hd6301v1_asrb->flags.read_mask == 0U
                && hd6301v1_asrb->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_asrb->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i}),
            "ASRB flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_asrb->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_asrb)
                && hd6301v1_asrb->operation
                    == jr800::isa::Operation::
                        arithmetic_shift_right_accumulator_b
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0x57
                ) == hd6301v1_asrb,
            "ASRB operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "ASRB",
            AddressingMode::implied
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x57)
                == nullptr,
        "Unreviewed MC6801 ASRB metadata was inherited"
    );
    const auto* hd6301v1_subd_direct = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "SUBD",
        AddressingMode::direct8
    );
    passed &= expect(
        hd6301v1_subd_direct != nullptr,
        "HD6301V1 direct SUBD encoding missing"
    );
    if (hd6301v1_subd_direct != nullptr) {
        passed &= expect(
            hd6301v1_subd_direct->opcode == 0x93,
            "Direct SUBD opcode mismatch"
        );
        passed &= expect(
            hd6301v1_subd_direct->operand_bytes == 1
                && hd6301v1_subd_direct->instruction_length == 2,
            "Direct SUBD length mismatch"
        );
        passed &= expect(
            hd6301v1_subd_direct->base_cycles == 4,
            "Direct SUBD cycle mismatch"
        );
        passed &= expect(
            hd6301v1_subd_direct->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_subd_direct->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i}),
            "Direct SUBD flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_subd_direct->classification
                == InstructionClass::linear,
            "Direct SUBD debugger classification mismatch"
        );
        passed &= expect(
            hd6301v1_subd_direct->operation
                == jr800::isa::Operation::subtract_from_double_accumulator,
            "Direct SUBD operation mismatch"
        );
    }

    const auto* hd6301v1_subd_indexed = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "SUBD",
        AddressingMode::indexed8
    );
    passed &= expect(
        hd6301v1_subd_indexed != nullptr,
        "HD6301V1 indexed SUBD encoding missing"
    );
    if (hd6301v1_subd_indexed != nullptr) {
        passed &= expect(
            hd6301v1_subd_indexed->opcode == 0xA3,
            "Indexed SUBD opcode mismatch"
        );
        passed &= expect(
            hd6301v1_subd_indexed->operand_bytes == 1
                && hd6301v1_subd_indexed->instruction_length == 2,
            "Indexed SUBD length mismatch"
        );
        passed &= expect(
            hd6301v1_subd_indexed->base_cycles == 5,
            "Indexed SUBD cycle mismatch"
        );
        passed &= expect(
            hd6301v1_subd_indexed->flags.read_mask == 0U
                && hd6301v1_subd_indexed->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_subd_indexed->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i}),
            "Indexed SUBD flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_subd_indexed->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_subd_indexed
                )
                && hd6301v1_subd_indexed->operation
                    == jr800::isa::Operation::subtract_from_double_accumulator
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xA3
                ) == hd6301v1_subd_indexed,
            "Indexed SUBD operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "SUBD",
            AddressingMode::indexed8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xA3)
                == nullptr,
        "Unreviewed MC6801 indexed SUBD metadata was inherited"
    );

    const auto* hd6301v1_subd_immediate = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "SUBD",
        AddressingMode::immediate16
    );
    passed &= expect(
        hd6301v1_subd_immediate != nullptr,
        "HD6301V1 immediate SUBD encoding missing"
    );
    if (hd6301v1_subd_immediate != nullptr) {
        passed &= expect(
            hd6301v1_subd_immediate->opcode == 0x83,
            "Immediate SUBD opcode mismatch"
        );
        passed &= expect(
            hd6301v1_subd_immediate->operand_bytes == 2
                && hd6301v1_subd_immediate->instruction_length == 3,
            "Immediate SUBD length mismatch"
        );
        passed &= expect(
            hd6301v1_subd_immediate->base_cycles == 3,
            "Immediate SUBD cycle mismatch"
        );
        passed &= expect(
            hd6301v1_subd_immediate->flags.read_mask == 0U
                && hd6301v1_subd_immediate->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_subd_immediate->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i}),
            "Immediate SUBD flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_subd_immediate->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_subd_immediate
                )
                && hd6301v1_subd_immediate->operation
                    == jr800::isa::Operation::subtract_from_double_accumulator
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0x83
                ) == hd6301v1_subd_immediate,
            "Immediate SUBD operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "SUBD",
            AddressingMode::immediate16
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x83)
                == nullptr,
        "Unreviewed MC6801 immediate SUBD metadata was inherited"
    );

    const auto* hd6301v1_subd_extended = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "SUBD",
        AddressingMode::extended16
    );
    passed &= expect(
        hd6301v1_subd_extended != nullptr,
        "HD6301V1 extended SUBD encoding missing"
    );
    if (hd6301v1_subd_extended != nullptr) {
        passed &= expect(
            hd6301v1_subd_extended->opcode == 0xB3,
            "Extended SUBD opcode mismatch"
        );
        passed &= expect(
            hd6301v1_subd_extended->operand_bytes == 2
                && hd6301v1_subd_extended->instruction_length == 3,
            "Extended SUBD length mismatch"
        );
        passed &= expect(
            hd6301v1_subd_extended->base_cycles == 5,
            "Extended SUBD cycle mismatch"
        );
        passed &= expect(
            hd6301v1_subd_extended->flags.read_mask == 0U
                && hd6301v1_subd_extended->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_subd_extended->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i}),
            "Extended SUBD flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_subd_extended->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_subd_extended
                )
                && hd6301v1_subd_extended->operation
                    == jr800::isa::Operation::subtract_from_double_accumulator
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xB3
                ) == hd6301v1_subd_extended,
            "Extended SUBD operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "SUBD",
            AddressingMode::extended16
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xB3)
                == nullptr,
        "Unreviewed MC6801 extended SUBD metadata was inherited"
    );
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "SUBD",
            AddressingMode::direct8
        ) == nullptr,
        "Unreviewed MC6801 direct SUBD metadata was inherited"
    );

    const auto* hd6301v1_addd_immediate = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "ADDD",
        AddressingMode::immediate16
    );
    passed &= expect(
        hd6301v1_addd_immediate != nullptr,
        "HD6301V1 immediate ADDD encoding missing"
    );
    if (hd6301v1_addd_immediate != nullptr) {
        passed &= expect(
            hd6301v1_addd_immediate->opcode == 0xC3,
            "Immediate ADDD opcode mismatch"
        );
        passed &= expect(
            hd6301v1_addd_immediate->operand_bytes == 2
                && hd6301v1_addd_immediate->instruction_length == 3,
            "Immediate ADDD length mismatch"
        );
        passed &= expect(
            hd6301v1_addd_immediate->base_cycles == 3,
            "Immediate ADDD cycle mismatch"
        );
        passed &= expect(
            hd6301v1_addd_immediate->flags.read_mask == 0U
                && hd6301v1_addd_immediate->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_addd_immediate->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i}),
            "Immediate ADDD flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_addd_immediate->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_addd_immediate
                )
                && hd6301v1_addd_immediate->operation
                    == jr800::isa::Operation::add_to_double_accumulator
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xC3
                ) == hd6301v1_addd_immediate,
            "Immediate ADDD operation, classification, or decode mismatch"
        );
    }

    const auto* hd6301v1_addd_direct = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "ADDD",
        AddressingMode::direct8
    );
    passed &= expect(
        hd6301v1_addd_direct != nullptr,
        "HD6301V1 direct ADDD encoding missing"
    );
    if (hd6301v1_addd_direct != nullptr) {
        passed &= expect(
            hd6301v1_addd_direct->opcode == 0xD3,
            "Direct ADDD opcode mismatch"
        );
        passed &= expect(
            hd6301v1_addd_direct->operand_bytes == 1
                && hd6301v1_addd_direct->instruction_length == 2,
            "Direct ADDD length mismatch"
        );
        passed &= expect(
            hd6301v1_addd_direct->base_cycles == 4,
            "Direct ADDD cycle mismatch"
        );
        passed &= expect(
            hd6301v1_addd_direct->flags.read_mask == 0U
                && hd6301v1_addd_direct->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_addd_direct->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i}),
            "Direct ADDD flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_addd_direct->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_addd_direct
                )
                && hd6301v1_addd_direct->operation
                    == jr800::isa::Operation::add_to_double_accumulator
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xD3
                ) == hd6301v1_addd_direct,
            "Direct ADDD operation, classification, or decode mismatch"
        );
    }

    const auto* hd6301v1_addd_indexed = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "ADDD",
        AddressingMode::indexed8
    );
    passed &= expect(
        hd6301v1_addd_indexed != nullptr,
        "HD6301V1 indexed ADDD encoding missing"
    );
    if (hd6301v1_addd_indexed != nullptr) {
        passed &= expect(
            hd6301v1_addd_indexed->opcode == 0xE3,
            "Indexed ADDD opcode mismatch"
        );
        passed &= expect(
            hd6301v1_addd_indexed->operand_bytes == 1
                && hd6301v1_addd_indexed->instruction_length == 2,
            "Indexed ADDD length mismatch"
        );
        passed &= expect(
            hd6301v1_addd_indexed->base_cycles == 5,
            "Indexed ADDD cycle mismatch"
        );
        passed &= expect(
            hd6301v1_addd_indexed->flags.read_mask == 0U
                && hd6301v1_addd_indexed->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_addd_indexed->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i}),
            "Indexed ADDD flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_addd_indexed->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_addd_indexed
                )
                && hd6301v1_addd_indexed->operation
                    == jr800::isa::Operation::add_to_double_accumulator
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xE3
                ) == hd6301v1_addd_indexed,
            "Indexed ADDD operation, classification, or decode mismatch"
        );
    }

    const auto* hd6301v1_addd_extended = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "ADDD",
        AddressingMode::extended16
    );
    passed &= expect(
        hd6301v1_addd_extended != nullptr,
        "HD6301V1 extended ADDD encoding missing"
    );
    if (hd6301v1_addd_extended != nullptr) {
        passed &= expect(
            hd6301v1_addd_extended->opcode == 0xF3,
            "Extended ADDD opcode mismatch"
        );
        passed &= expect(
            hd6301v1_addd_extended->operand_bytes == 2
                && hd6301v1_addd_extended->instruction_length == 3,
            "Extended ADDD length mismatch"
        );
        passed &= expect(
            hd6301v1_addd_extended->base_cycles == 5,
            "Extended ADDD cycle mismatch"
        );
        passed &= expect(
            hd6301v1_addd_extended->flags.read_mask == 0U
                && hd6301v1_addd_extended->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_addd_extended->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i}),
            "Extended ADDD flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_addd_extended->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_addd_extended
                )
                && hd6301v1_addd_extended->operation
                    == jr800::isa::Operation::add_to_double_accumulator
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xF3
                ) == hd6301v1_addd_extended,
            "Extended ADDD operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "ADDD",
            AddressingMode::immediate16
        ) == nullptr
            && jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "ADDD",
            AddressingMode::direct8
        ) == nullptr
            && jr800::isa::find_encoding(
                CpuProfile::mc6801,
                "ADDD",
                AddressingMode::indexed8
            ) == nullptr
            && jr800::isa::find_encoding(
                CpuProfile::mc6801,
                "ADDD",
                AddressingMode::extended16
            ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xC3)
                == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xD3)
                == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xE3)
                == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xF3)
                == nullptr,
        "Unreviewed MC6801 ADDD metadata was inherited"
    );

    const auto* hd6301v1_ldaa_extended = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "LDAA",
        AddressingMode::extended16
    );
    passed &= expect(
        hd6301v1_ldaa_extended != nullptr,
        "HD6301V1 extended LDAA encoding missing"
    );
    if (hd6301v1_ldaa_extended != nullptr) {
        passed &= expect(
            hd6301v1_ldaa_extended->opcode == 0xB6,
            "Extended LDAA opcode mismatch"
        );
        passed &= expect(
            hd6301v1_ldaa_extended->operand_bytes == 2
                && hd6301v1_ldaa_extended->instruction_length == 3,
            "Extended LDAA length mismatch"
        );
        passed &= expect(
            hd6301v1_ldaa_extended->base_cycles == 4,
            "Extended LDAA cycle mismatch"
        );
        passed &= expect(
            hd6301v1_ldaa_extended->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_ldaa_extended->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c}),
            "Extended LDAA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_ldaa_extended->classification
                == InstructionClass::linear,
            "Extended LDAA debugger classification mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "LDAA",
            AddressingMode::extended16
        ) == nullptr,
        "Unreviewed MC6801 extended LDAA metadata was inherited"
    );

    const auto* hd6301v1_ldaa_direct = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "LDAA",
        AddressingMode::direct8
    );
    passed &= expect(
        hd6301v1_ldaa_direct != nullptr,
        "HD6301V1 direct LDAA encoding missing"
    );
    if (hd6301v1_ldaa_direct != nullptr) {
        passed &= expect(
            hd6301v1_ldaa_direct->opcode == 0x96,
            "Direct LDAA opcode mismatch"
        );
        passed &= expect(
            hd6301v1_ldaa_direct->operand_bytes == 1
                && hd6301v1_ldaa_direct->instruction_length == 2,
            "Direct LDAA length mismatch"
        );
        passed &= expect(
            hd6301v1_ldaa_direct->base_cycles == 3,
            "Direct LDAA cycle mismatch"
        );
        passed &= expect(
            hd6301v1_ldaa_direct->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_ldaa_direct->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c}),
            "Direct LDAA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_ldaa_direct->classification
                == InstructionClass::linear,
            "Direct LDAA debugger classification mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "LDAA",
            AddressingMode::direct8
        ) == nullptr,
        "Unreviewed MC6801 direct LDAA metadata was inherited"
    );

    const auto* hd6301v1_ldaa_indexed = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "LDAA",
        AddressingMode::indexed8
    );
    passed &= expect(
        hd6301v1_ldaa_indexed != nullptr,
        "HD6301V1 indexed LDAA encoding missing"
    );
    if (hd6301v1_ldaa_indexed != nullptr) {
        passed &= expect(
            hd6301v1_ldaa_indexed->opcode == 0xA6,
            "Indexed LDAA opcode mismatch"
        );
        passed &= expect(
            hd6301v1_ldaa_indexed->operand_bytes == 1
                && hd6301v1_ldaa_indexed->instruction_length == 2,
            "Indexed LDAA length mismatch"
        );
        passed &= expect(
            hd6301v1_ldaa_indexed->base_cycles == 4,
            "Indexed LDAA cycle mismatch"
        );
        passed &= expect(
            hd6301v1_ldaa_indexed->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_ldaa_indexed->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c}),
            "Indexed LDAA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_ldaa_indexed->classification
                == InstructionClass::linear,
            "Indexed LDAA debugger classification mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "LDAA",
            AddressingMode::indexed8
        ) == nullptr,
        "Unreviewed MC6801 indexed LDAA metadata was inherited"
    );

    const auto* staa = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "STAA",
        AddressingMode::direct8
    );
    passed &= expect(staa != nullptr, "STAA direct encoding missing");
    if (staa != nullptr) {
        passed &= expect(staa->opcode == 0x97, "STAA direct opcode mismatch");
        passed &= expect(staa->instruction_length == 2, "STAA direct length mismatch");
        passed &= expect(staa->base_cycles == 3, "STAA direct cycle mismatch");
    }

    const auto* hd6301v1_staa_extended = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "STAA",
        AddressingMode::extended16
    );
    passed &= expect(
        hd6301v1_staa_extended != nullptr,
        "HD6301V1 extended STAA encoding missing"
    );
    if (hd6301v1_staa_extended != nullptr) {
        passed &= expect(
            hd6301v1_staa_extended->opcode == 0xB7,
            "Extended STAA opcode mismatch"
        );
        passed &= expect(
            hd6301v1_staa_extended->operand_bytes == 2
                && hd6301v1_staa_extended->instruction_length == 3,
            "Extended STAA length mismatch"
        );
        passed &= expect(
            hd6301v1_staa_extended->base_cycles == 4,
            "Extended STAA cycle mismatch"
        );
        passed &= expect(
            hd6301v1_staa_extended->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_staa_extended->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c}),
            "Extended STAA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_staa_extended->classification
                == InstructionClass::linear,
            "Extended STAA debugger classification mismatch"
        );
        passed &= expect(
            jr800::isa::decode_instruction(CpuProfile::hd6301v1, 0xB7)
                == hd6301v1_staa_extended,
            "Extended STAA decode metadata mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "STAA",
            AddressingMode::extended16
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xB7)
                == nullptr,
        "Unreviewed MC6801 extended STAA metadata was inherited"
    );

    const auto* hd6301v1_staa_indexed = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "STAA",
        AddressingMode::indexed8
    );
    passed &= expect(
        hd6301v1_staa_indexed != nullptr,
        "HD6301V1 indexed STAA encoding missing"
    );
    if (hd6301v1_staa_indexed != nullptr) {
        passed &= expect(
            hd6301v1_staa_indexed->opcode == 0xA7,
            "Indexed STAA opcode mismatch"
        );
        passed &= expect(
            hd6301v1_staa_indexed->operand_bytes == 1
                && hd6301v1_staa_indexed->instruction_length == 2,
            "Indexed STAA length mismatch"
        );
        passed &= expect(
            hd6301v1_staa_indexed->base_cycles == 4,
            "Indexed STAA cycle mismatch"
        );
        passed &= expect(
            hd6301v1_staa_indexed->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_staa_indexed->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c}),
            "Indexed STAA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_staa_indexed->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_staa_indexed
                ),
            "Indexed STAA debugger classification mismatch"
        );
        passed &= expect(
            jr800::isa::decode_instruction(CpuProfile::hd6301v1, 0xA7)
                == hd6301v1_staa_indexed,
            "Indexed STAA decode metadata mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "STAA",
            AddressingMode::indexed8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xA7)
                == nullptr,
        "Unreviewed MC6801 indexed STAA metadata was inherited"
    );

    const auto* hd6301v1_stab_direct = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "STAB",
        AddressingMode::direct8
    );
    passed &= expect(
        hd6301v1_stab_direct != nullptr,
        "HD6301V1 direct STAB encoding missing"
    );
    if (hd6301v1_stab_direct != nullptr) {
        passed &= expect(
            hd6301v1_stab_direct->opcode == 0xD7,
            "Direct STAB opcode mismatch"
        );
        passed &= expect(
            hd6301v1_stab_direct->operand_bytes == 1
                && hd6301v1_stab_direct->instruction_length == 2,
            "Direct STAB length mismatch"
        );
        passed &= expect(
            hd6301v1_stab_direct->base_cycles == 3,
            "Direct STAB cycle mismatch"
        );
        passed &= expect(
            hd6301v1_stab_direct->flags.read_mask == 0U
                && hd6301v1_stab_direct->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_stab_direct->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c}),
            "Direct STAB flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_stab_direct->classification
                    == InstructionClass::linear
                && hd6301v1_stab_direct->operation
                    == jr800::isa::Operation::store_accumulator_b
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xD7
                ) == hd6301v1_stab_direct,
            "Direct STAB operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "STAB",
            AddressingMode::direct8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xD7)
                == nullptr,
        "Unreviewed MC6801 direct STAB metadata was inherited"
    );

    const auto* hd6301v1_stab_indexed = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "STAB",
        AddressingMode::indexed8
    );
    passed &= expect(
        hd6301v1_stab_indexed != nullptr,
        "HD6301V1 indexed STAB encoding missing"
    );
    if (hd6301v1_stab_indexed != nullptr) {
        passed &= expect(
            hd6301v1_stab_indexed->opcode == 0xE7,
            "Indexed STAB opcode mismatch"
        );
        passed &= expect(
            hd6301v1_stab_indexed->operand_bytes == 1
                && hd6301v1_stab_indexed->instruction_length == 2,
            "Indexed STAB length mismatch"
        );
        passed &= expect(
            hd6301v1_stab_indexed->base_cycles == 4,
            "Indexed STAB cycle mismatch"
        );
        passed &= expect(
            hd6301v1_stab_indexed->flags.read_mask == 0U
                && hd6301v1_stab_indexed->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_stab_indexed->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c}),
            "Indexed STAB flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_stab_indexed->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_stab_indexed
                )
                && hd6301v1_stab_indexed->operation
                    == jr800::isa::Operation::store_accumulator_b
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xE7
                ) == hd6301v1_stab_indexed,
            "Indexed STAB operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "STAB",
            AddressingMode::indexed8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xE7)
                == nullptr,
        "Unreviewed MC6801 indexed STAB metadata was inherited"
    );
    const auto* hd6301v1_stab_extended = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "STAB",
        AddressingMode::extended16
    );
    passed &= expect(
        hd6301v1_stab_extended != nullptr,
        "HD6301V1 extended STAB encoding missing"
    );
    if (hd6301v1_stab_extended != nullptr) {
        passed &= expect(
            hd6301v1_stab_extended->opcode == 0xF7U
                && hd6301v1_stab_extended->operand_bytes == 2U
                && hd6301v1_stab_extended->instruction_length == 3U
                && hd6301v1_stab_extended->base_cycles == 4U,
            "Extended STAB opcode, length, or cycle metadata mismatch"
        );
        passed &= expect(
            hd6301v1_stab_extended->flags.read_mask == 0U
                && hd6301v1_stab_extended->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_stab_extended->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c})
                && hd6301v1_stab_extended->flags.undefined_mask == 0U,
            "Extended STAB flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_stab_extended->classification
                    == InstructionClass::linear
                && hd6301v1_stab_extended->operation
                    == jr800::isa::Operation::store_accumulator_b
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_stab_extended
                )
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xF7U
                ) == hd6301v1_stab_extended,
            "Extended STAB operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "STAB",
            AddressingMode::extended16
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xF7U)
                == nullptr,
        "Unreviewed MC6801 extended STAB metadata was inherited"
    );

    const auto* bra = jr800::isa::find_encoding(
        CpuProfile::mc6801,
        "BRA",
        AddressingMode::relative8
    );
    passed &= expect(bra != nullptr, "BRA relative encoding missing");
    if (bra != nullptr) {
        passed &= expect(bra->opcode == 0x20, "BRA opcode mismatch");
        passed &= expect(bra->instruction_length == 2, "BRA length mismatch");
        passed &= expect(bra->base_cycles == 3, "BRA cycle mismatch");
        passed &= expect(
            bra->classification == InstructionClass::branch,
            "BRA debugger classification mismatch"
        );
    }

    const auto* hd6301v1_brn = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "BRN",
        AddressingMode::relative8
    );
    passed &= expect(hd6301v1_brn != nullptr, "HD6301V1 BRN encoding missing");
    if (hd6301v1_brn != nullptr) {
        const auto all_flags = mask({
            StatusFlag::h,
            StatusFlag::i,
            StatusFlag::n,
            StatusFlag::z,
            StatusFlag::v,
            StatusFlag::c,
        });
        passed &= expect(hd6301v1_brn->opcode == 0x21, "BRN opcode mismatch");
        passed &= expect(
            hd6301v1_brn->operand_bytes == 1
                && hd6301v1_brn->instruction_length == 2,
            "BRN length mismatch"
        );
        passed &= expect(hd6301v1_brn->base_cycles == 3, "BRN cycle mismatch");
        passed &= expect(
            hd6301v1_brn->flags.read_mask == 0U
                && hd6301v1_brn->flags.written_mask == 0U
                && hd6301v1_brn->flags.preserved_mask == all_flags
                && hd6301v1_brn->flags.undefined_mask == 0U,
            "BRN flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_brn->classification == InstructionClass::branch
                && !jr800::isa::is_step_over_candidate(*hd6301v1_brn)
                && hd6301v1_brn->operation
                    == jr800::isa::Operation::branch_never
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0x21
                ) == hd6301v1_brn,
            "BRN operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "BRN",
            AddressingMode::relative8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x21)
                == nullptr,
        "Unreviewed MC6801 BRN metadata was inherited"
    );

    const auto* hd6301v1_bhi = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "BHI",
        AddressingMode::relative8
    );
    passed &= expect(hd6301v1_bhi != nullptr, "HD6301V1 BHI encoding missing");
    if (hd6301v1_bhi != nullptr) {
        const auto all_flags = mask({
            StatusFlag::h,
            StatusFlag::i,
            StatusFlag::n,
            StatusFlag::z,
            StatusFlag::v,
            StatusFlag::c,
        });
        passed &= expect(hd6301v1_bhi->opcode == 0x22, "BHI opcode mismatch");
        passed &= expect(
            hd6301v1_bhi->operand_bytes == 1
                && hd6301v1_bhi->instruction_length == 2,
            "BHI length mismatch"
        );
        passed &= expect(hd6301v1_bhi->base_cycles == 3, "BHI cycle mismatch");
        passed &= expect(
            hd6301v1_bhi->flags.read_mask
                    == mask({StatusFlag::c, StatusFlag::z})
                && hd6301v1_bhi->flags.written_mask == 0U
                && hd6301v1_bhi->flags.preserved_mask == all_flags
                && hd6301v1_bhi->flags.undefined_mask == 0U,
            "BHI flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_bhi->classification == InstructionClass::branch
                && !jr800::isa::is_step_over_candidate(*hd6301v1_bhi)
                && hd6301v1_bhi->operation
                    == jr800::isa::Operation::branch_if_higher
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0x22
                ) == hd6301v1_bhi,
            "BHI operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "BHI",
            AddressingMode::relative8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x22)
                == nullptr,
        "Unreviewed MC6801 BHI metadata was inherited"
    );
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::hd6301v1,
            "BHI",
            AddressingMode::direct8
        ) == nullptr
            && jr800::isa::find_encoding(
                CpuProfile::hd6301v1,
                "BHI",
                AddressingMode::indexed8
            ) == nullptr
            && jr800::isa::find_encoding(
                CpuProfile::hd6301v1,
                "BHI",
                AddressingMode::extended16
            ) == nullptr,
        "Non-relative BHI metadata was staged"
    );

    const auto* hd6301v1_bls = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "BLS",
        AddressingMode::relative8
    );
    passed &= expect(hd6301v1_bls != nullptr, "HD6301V1 BLS encoding missing");
    if (hd6301v1_bls != nullptr) {
        const auto all_flags = mask({
            StatusFlag::h,
            StatusFlag::i,
            StatusFlag::n,
            StatusFlag::z,
            StatusFlag::v,
            StatusFlag::c,
        });
        passed &= expect(hd6301v1_bls->opcode == 0x23, "BLS opcode mismatch");
        passed &= expect(
            hd6301v1_bls->operand_bytes == 1
                && hd6301v1_bls->instruction_length == 2,
            "BLS length mismatch"
        );
        passed &= expect(hd6301v1_bls->base_cycles == 3, "BLS cycle mismatch");
        passed &= expect(
            hd6301v1_bls->flags.read_mask
                    == mask({StatusFlag::c, StatusFlag::z})
                && hd6301v1_bls->flags.written_mask == 0U
                && hd6301v1_bls->flags.preserved_mask == all_flags
                && hd6301v1_bls->flags.undefined_mask == 0U,
            "BLS flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_bls->classification == InstructionClass::branch
                && !jr800::isa::is_step_over_candidate(*hd6301v1_bls)
                && hd6301v1_bls->operation
                    == jr800::isa::Operation::branch_if_lower_or_same
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0x23
                ) == hd6301v1_bls,
            "BLS operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "BLS",
            AddressingMode::relative8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x23)
                == nullptr,
        "Unreviewed MC6801 BLS metadata was inherited"
    );
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::hd6301v1,
            "BLS",
            AddressingMode::direct8
        ) == nullptr
            && jr800::isa::find_encoding(
                CpuProfile::hd6301v1,
                "BLS",
                AddressingMode::indexed8
            ) == nullptr
            && jr800::isa::find_encoding(
                CpuProfile::hd6301v1,
                "BLS",
                AddressingMode::extended16
            ) == nullptr,
        "Non-relative BLS metadata was staged"
    );

    const auto* hd6301v1_bcc = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "BCC",
        AddressingMode::relative8
    );
    passed &= expect(hd6301v1_bcc != nullptr, "HD6301V1 BCC encoding missing");
    if (hd6301v1_bcc != nullptr) {
        passed &= expect(hd6301v1_bcc->opcode == 0x24, "BCC opcode mismatch");
        passed &= expect(
            hd6301v1_bcc->operand_bytes == 1
                && hd6301v1_bcc->instruction_length == 2,
            "BCC length mismatch"
        );
        passed &= expect(hd6301v1_bcc->base_cycles == 3, "BCC cycle mismatch");
        passed &= expect(
            hd6301v1_bcc->flags.read_mask == mask({StatusFlag::c})
                && hd6301v1_bcc->flags.preserved_mask
                    == mask({
                        StatusFlag::h,
                        StatusFlag::i,
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    }),
            "BCC flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_bcc->classification == InstructionClass::branch,
            "BCC debugger classification mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "BCC",
            AddressingMode::relative8
        ) == nullptr,
        "Unreviewed MC6801 BCC metadata was inherited"
    );

    const auto* hd6301v1_bcs = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "BCS",
        AddressingMode::relative8
    );
    passed &= expect(hd6301v1_bcs != nullptr, "HD6301V1 BCS encoding missing");
    if (hd6301v1_bcs != nullptr) {
        passed &= expect(hd6301v1_bcs->opcode == 0x25, "BCS opcode mismatch");
        passed &= expect(
            hd6301v1_bcs->operand_bytes == 1
                && hd6301v1_bcs->instruction_length == 2,
            "BCS length mismatch"
        );
        passed &= expect(hd6301v1_bcs->base_cycles == 3, "BCS cycle mismatch");
        passed &= expect(
            hd6301v1_bcs->flags.read_mask == mask({StatusFlag::c})
                && hd6301v1_bcs->flags.written_mask == 0U
                && hd6301v1_bcs->flags.preserved_mask
                    == mask({
                        StatusFlag::h,
                        StatusFlag::i,
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    }),
            "BCS flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_bcs->classification == InstructionClass::branch
                && hd6301v1_bcs->operation
                    == jr800::isa::Operation::branch_if_carry_set
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0x25
                ) == hd6301v1_bcs,
            "BCS operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "BCS",
            AddressingMode::relative8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x25)
                == nullptr,
        "Unreviewed MC6801 BCS metadata was inherited"
    );
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::hd6301v1,
            "BCS",
            AddressingMode::direct8
        ) == nullptr
            && jr800::isa::find_encoding(
                CpuProfile::hd6301v1,
                "BCS",
                AddressingMode::indexed8
            ) == nullptr
            && jr800::isa::find_encoding(
                CpuProfile::hd6301v1,
                "BCS",
                AddressingMode::extended16
            ) == nullptr,
        "Non-relative BCS metadata was staged"
    );

    const auto* hd6301v1_bne = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "BNE",
        AddressingMode::relative8
    );
    passed &= expect(hd6301v1_bne != nullptr, "HD6301V1 BNE encoding missing");
    if (hd6301v1_bne != nullptr) {
        passed &= expect(hd6301v1_bne->opcode == 0x26, "BNE opcode mismatch");
        passed &= expect(
            hd6301v1_bne->operand_bytes == 1
                && hd6301v1_bne->instruction_length == 2,
            "BNE length mismatch"
        );
        passed &= expect(hd6301v1_bne->base_cycles == 3, "BNE cycle mismatch");
        passed &= expect(
            hd6301v1_bne->flags.read_mask == mask({StatusFlag::z})
                && hd6301v1_bne->flags.preserved_mask
                    == mask({
                        StatusFlag::h,
                        StatusFlag::i,
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    }),
            "BNE flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_bne->classification == InstructionClass::branch,
            "BNE debugger classification mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "BNE",
            AddressingMode::relative8
        ) == nullptr,
        "Unreviewed MC6801 BNE metadata was inherited"
    );

    const auto* hd6301v1_beq = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "BEQ",
        AddressingMode::relative8
    );
    passed &= expect(hd6301v1_beq != nullptr, "HD6301V1 BEQ encoding missing");
    if (hd6301v1_beq != nullptr) {
        passed &= expect(hd6301v1_beq->opcode == 0x27, "BEQ opcode mismatch");
        passed &= expect(
            hd6301v1_beq->operand_bytes == 1
                && hd6301v1_beq->instruction_length == 2,
            "BEQ length mismatch"
        );
        passed &= expect(hd6301v1_beq->base_cycles == 3, "BEQ cycle mismatch");
        passed &= expect(
            hd6301v1_beq->flags.read_mask == mask({StatusFlag::z})
                && hd6301v1_beq->flags.written_mask == 0U
                && hd6301v1_beq->flags.preserved_mask
                    == mask({
                        StatusFlag::h,
                        StatusFlag::i,
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    }),
            "BEQ flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_beq->classification == InstructionClass::branch
                && hd6301v1_beq->operation
                    == jr800::isa::Operation::branch_if_equal
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0x27
                ) == hd6301v1_beq,
            "BEQ operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "BEQ",
            AddressingMode::relative8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x27)
                == nullptr,
        "Unreviewed MC6801 BEQ metadata was inherited"
    );
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::hd6301v1,
            "BEQ",
            AddressingMode::direct8
        ) == nullptr
            && jr800::isa::find_encoding(
                CpuProfile::hd6301v1,
                "BEQ",
                AddressingMode::indexed8
            ) == nullptr
            && jr800::isa::find_encoding(
                CpuProfile::hd6301v1,
                "BEQ",
                AddressingMode::extended16
            ) == nullptr,
        "Non-relative BEQ metadata was staged"
    );

    const auto* hd6301v1_bvc = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "BVC",
        AddressingMode::relative8
    );
    passed &= expect(hd6301v1_bvc != nullptr, "HD6301V1 BVC encoding missing");
    if (hd6301v1_bvc != nullptr) {
        const auto all_flags = mask({
            StatusFlag::h,
            StatusFlag::i,
            StatusFlag::n,
            StatusFlag::z,
            StatusFlag::v,
            StatusFlag::c,
        });
        passed &= expect(hd6301v1_bvc->opcode == 0x28, "BVC opcode mismatch");
        passed &= expect(
            hd6301v1_bvc->operand_bytes == 1
                && hd6301v1_bvc->instruction_length == 2,
            "BVC length mismatch"
        );
        passed &= expect(hd6301v1_bvc->base_cycles == 3, "BVC cycle mismatch");
        passed &= expect(
            hd6301v1_bvc->flags.read_mask == mask({StatusFlag::v})
                && hd6301v1_bvc->flags.written_mask == 0U
                && hd6301v1_bvc->flags.preserved_mask == all_flags
                && hd6301v1_bvc->flags.undefined_mask == 0U,
            "BVC flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_bvc->classification == InstructionClass::branch
                && !jr800::isa::is_step_over_candidate(*hd6301v1_bvc)
                && hd6301v1_bvc->operation
                    == jr800::isa::Operation::branch_if_overflow_clear
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0x28
                ) == hd6301v1_bvc,
            "BVC operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "BVC",
            AddressingMode::relative8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x28)
                == nullptr,
        "Unreviewed MC6801 BVC metadata was inherited"
    );
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::hd6301v1,
            "BVC",
            AddressingMode::direct8
        ) == nullptr
            && jr800::isa::find_encoding(
                CpuProfile::hd6301v1,
                "BVC",
                AddressingMode::indexed8
            ) == nullptr
            && jr800::isa::find_encoding(
                CpuProfile::hd6301v1,
                "BVC",
                AddressingMode::extended16
            ) == nullptr,
        "Non-relative BVC metadata was staged"
    );

    const auto* hd6301v1_bvs = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "BVS",
        AddressingMode::relative8
    );
    passed &= expect(hd6301v1_bvs != nullptr, "HD6301V1 BVS encoding missing");
    if (hd6301v1_bvs != nullptr) {
        const auto all_flags = mask({
            StatusFlag::h,
            StatusFlag::i,
            StatusFlag::n,
            StatusFlag::z,
            StatusFlag::v,
            StatusFlag::c,
        });
        passed &= expect(hd6301v1_bvs->opcode == 0x29, "BVS opcode mismatch");
        passed &= expect(
            hd6301v1_bvs->operand_bytes == 1
                && hd6301v1_bvs->instruction_length == 2,
            "BVS length mismatch"
        );
        passed &= expect(hd6301v1_bvs->base_cycles == 3, "BVS cycle mismatch");
        passed &= expect(
            hd6301v1_bvs->flags.read_mask == mask({StatusFlag::v})
                && hd6301v1_bvs->flags.written_mask == 0U
                && hd6301v1_bvs->flags.preserved_mask == all_flags
                && hd6301v1_bvs->flags.undefined_mask == 0U,
            "BVS flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_bvs->classification == InstructionClass::branch
                && !jr800::isa::is_step_over_candidate(*hd6301v1_bvs)
                && hd6301v1_bvs->operation
                    == jr800::isa::Operation::branch_if_overflow_set
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0x29
                ) == hd6301v1_bvs,
            "BVS operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "BVS",
            AddressingMode::relative8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x29)
                == nullptr,
        "Unreviewed MC6801 BVS metadata was inherited"
    );
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::hd6301v1,
            "BVS",
            AddressingMode::direct8
        ) == nullptr
            && jr800::isa::find_encoding(
                CpuProfile::hd6301v1,
                "BVS",
                AddressingMode::indexed8
            ) == nullptr
            && jr800::isa::find_encoding(
                CpuProfile::hd6301v1,
                "BVS",
                AddressingMode::extended16
            ) == nullptr,
        "Non-relative BVS metadata was staged"
    );

    const auto* hd6301v1_bpl = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "BPL",
        AddressingMode::relative8
    );
    passed &= expect(hd6301v1_bpl != nullptr, "HD6301V1 BPL encoding missing");
    if (hd6301v1_bpl != nullptr) {
        passed &= expect(hd6301v1_bpl->opcode == 0x2A, "BPL opcode mismatch");
        passed &= expect(
            hd6301v1_bpl->operand_bytes == 1
                && hd6301v1_bpl->instruction_length == 2,
            "BPL length mismatch"
        );
        passed &= expect(hd6301v1_bpl->base_cycles == 3, "BPL cycle mismatch");
        passed &= expect(
            hd6301v1_bpl->flags.read_mask == mask({StatusFlag::n})
                && hd6301v1_bpl->flags.preserved_mask
                    == mask({
                        StatusFlag::h,
                        StatusFlag::i,
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    }),
            "BPL flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_bpl->classification == InstructionClass::branch,
            "BPL debugger classification mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "BPL",
            AddressingMode::relative8
        ) == nullptr,
        "Unreviewed MC6801 BPL metadata was inherited"
    );

    const auto* hd6301v1_bmi = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "BMI",
        AddressingMode::relative8
    );
    passed &= expect(hd6301v1_bmi != nullptr, "HD6301V1 BMI encoding missing");
    if (hd6301v1_bmi != nullptr) {
        passed &= expect(hd6301v1_bmi->opcode == 0x2B, "BMI opcode mismatch");
        passed &= expect(
            hd6301v1_bmi->operand_bytes == 1
                && hd6301v1_bmi->instruction_length == 2,
            "BMI length mismatch"
        );
        passed &= expect(hd6301v1_bmi->base_cycles == 3, "BMI cycle mismatch");
        passed &= expect(
            hd6301v1_bmi->flags.read_mask == mask({StatusFlag::n})
                && hd6301v1_bmi->flags.written_mask == 0U
                && hd6301v1_bmi->flags.preserved_mask
                    == mask({
                        StatusFlag::h,
                        StatusFlag::i,
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    }),
            "BMI flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_bmi->classification == InstructionClass::branch
                && !jr800::isa::is_step_over_candidate(*hd6301v1_bmi)
                && hd6301v1_bmi->operation
                    == jr800::isa::Operation::branch_if_minus
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0x2B
                ) == hd6301v1_bmi,
            "BMI operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "BMI",
            AddressingMode::relative8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x2B)
                == nullptr,
        "Unreviewed MC6801 BMI metadata was inherited"
    );
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::hd6301v1,
            "BMI",
            AddressingMode::direct8
        ) == nullptr
            && jr800::isa::find_encoding(
                CpuProfile::hd6301v1,
                "BMI",
                AddressingMode::indexed8
            ) == nullptr
            && jr800::isa::find_encoding(
                CpuProfile::hd6301v1,
                "BMI",
                AddressingMode::extended16
            ) == nullptr,
        "Non-relative BMI metadata was staged"
    );

    const auto* hd6301v1_bge = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "BGE",
        AddressingMode::relative8
    );
    passed &= expect(hd6301v1_bge != nullptr, "HD6301V1 BGE encoding missing");
    if (hd6301v1_bge != nullptr) {
        const auto all_flags = mask({
            StatusFlag::h,
            StatusFlag::i,
            StatusFlag::n,
            StatusFlag::z,
            StatusFlag::v,
            StatusFlag::c,
        });
        passed &= expect(hd6301v1_bge->opcode == 0x2C, "BGE opcode mismatch");
        passed &= expect(
            hd6301v1_bge->operand_bytes == 1
                && hd6301v1_bge->instruction_length == 2,
            "BGE length mismatch"
        );
        passed &= expect(hd6301v1_bge->base_cycles == 3, "BGE cycle mismatch");
        passed &= expect(
            hd6301v1_bge->flags.read_mask
                    == mask({StatusFlag::n, StatusFlag::v})
                && hd6301v1_bge->flags.written_mask == 0U
                && hd6301v1_bge->flags.preserved_mask == all_flags
                && hd6301v1_bge->flags.undefined_mask == 0U,
            "BGE flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_bge->classification == InstructionClass::branch
                && !jr800::isa::is_step_over_candidate(*hd6301v1_bge)
                && hd6301v1_bge->operation
                    == jr800::isa::Operation::branch_if_greater_or_equal
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0x2C
                ) == hd6301v1_bge,
            "BGE operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "BGE",
            AddressingMode::relative8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x2C)
                == nullptr,
        "Unreviewed MC6801 BGE metadata was inherited"
    );
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::hd6301v1,
            "BGE",
            AddressingMode::direct8
        ) == nullptr
            && jr800::isa::find_encoding(
                CpuProfile::hd6301v1,
                "BGE",
                AddressingMode::indexed8
            ) == nullptr
            && jr800::isa::find_encoding(
                CpuProfile::hd6301v1,
                "BGE",
                AddressingMode::extended16
            ) == nullptr,
        "Non-relative BGE metadata was staged"
    );

    const auto* hd6301v1_blt = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "BLT",
        AddressingMode::relative8
    );
    passed &= expect(hd6301v1_blt != nullptr, "HD6301V1 BLT encoding missing");
    if (hd6301v1_blt != nullptr) {
        const auto all_flags = mask({
            StatusFlag::h,
            StatusFlag::i,
            StatusFlag::n,
            StatusFlag::z,
            StatusFlag::v,
            StatusFlag::c,
        });
        passed &= expect(hd6301v1_blt->opcode == 0x2D, "BLT opcode mismatch");
        passed &= expect(
            hd6301v1_blt->operand_bytes == 1
                && hd6301v1_blt->instruction_length == 2,
            "BLT length mismatch"
        );
        passed &= expect(hd6301v1_blt->base_cycles == 3, "BLT cycle mismatch");
        passed &= expect(
            hd6301v1_blt->flags.read_mask
                    == mask({StatusFlag::n, StatusFlag::v})
                && hd6301v1_blt->flags.written_mask == 0U
                && hd6301v1_blt->flags.preserved_mask == all_flags
                && hd6301v1_blt->flags.undefined_mask == 0U,
            "BLT flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_blt->classification == InstructionClass::branch
                && !jr800::isa::is_step_over_candidate(*hd6301v1_blt)
                && hd6301v1_blt->operation
                    == jr800::isa::Operation::branch_if_less
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0x2D
                ) == hd6301v1_blt,
            "BLT operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "BLT",
            AddressingMode::relative8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x2D)
                == nullptr,
        "Unreviewed MC6801 BLT metadata was inherited"
    );
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::hd6301v1,
            "BLT",
            AddressingMode::direct8
        ) == nullptr
            && jr800::isa::find_encoding(
                CpuProfile::hd6301v1,
                "BLT",
                AddressingMode::indexed8
            ) == nullptr
            && jr800::isa::find_encoding(
                CpuProfile::hd6301v1,
                "BLT",
                AddressingMode::extended16
            ) == nullptr,
        "Non-relative BLT metadata was staged"
    );

    const auto* hd6301v1_bgt = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "BGT",
        AddressingMode::relative8
    );
    passed &= expect(hd6301v1_bgt != nullptr, "HD6301V1 BGT encoding missing");
    if (hd6301v1_bgt != nullptr) {
        const auto all_flags = mask({
            StatusFlag::h,
            StatusFlag::i,
            StatusFlag::n,
            StatusFlag::z,
            StatusFlag::v,
            StatusFlag::c,
        });
        passed &= expect(hd6301v1_bgt->opcode == 0x2E, "BGT opcode mismatch");
        passed &= expect(
            hd6301v1_bgt->operand_bytes == 1
                && hd6301v1_bgt->instruction_length == 2,
            "BGT length mismatch"
        );
        passed &= expect(hd6301v1_bgt->base_cycles == 3, "BGT cycle mismatch");
        passed &= expect(
            hd6301v1_bgt->flags.read_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_bgt->flags.written_mask == 0U
                && hd6301v1_bgt->flags.preserved_mask == all_flags
                && hd6301v1_bgt->flags.undefined_mask == 0U,
            "BGT flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_bgt->classification == InstructionClass::branch
                && !jr800::isa::is_step_over_candidate(*hd6301v1_bgt)
                && hd6301v1_bgt->operation
                    == jr800::isa::Operation::branch_if_greater
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0x2E
                ) == hd6301v1_bgt,
            "BGT operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "BGT",
            AddressingMode::relative8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x2E)
                == nullptr,
        "Unreviewed MC6801 BGT metadata was inherited"
    );
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::hd6301v1,
            "BGT",
            AddressingMode::direct8
        ) == nullptr
            && jr800::isa::find_encoding(
                CpuProfile::hd6301v1,
                "BGT",
                AddressingMode::indexed8
            ) == nullptr
            && jr800::isa::find_encoding(
                CpuProfile::hd6301v1,
                "BGT",
                AddressingMode::extended16
            ) == nullptr,
        "Non-relative BGT metadata was staged"
    );

    const auto* hd6301v1_ble = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "BLE",
        AddressingMode::relative8
    );
    passed &= expect(hd6301v1_ble != nullptr, "HD6301V1 BLE encoding missing");
    if (hd6301v1_ble != nullptr) {
        const auto all_flags = mask({
            StatusFlag::h,
            StatusFlag::i,
            StatusFlag::n,
            StatusFlag::z,
            StatusFlag::v,
            StatusFlag::c,
        });
        passed &= expect(hd6301v1_ble->opcode == 0x2F, "BLE opcode mismatch");
        passed &= expect(
            hd6301v1_ble->operand_bytes == 1
                && hd6301v1_ble->instruction_length == 2,
            "BLE length mismatch"
        );
        passed &= expect(hd6301v1_ble->base_cycles == 3, "BLE cycle mismatch");
        passed &= expect(
            hd6301v1_ble->flags.read_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_ble->flags.written_mask == 0U
                && hd6301v1_ble->flags.preserved_mask == all_flags
                && hd6301v1_ble->flags.undefined_mask == 0U,
            "BLE flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_ble->classification == InstructionClass::branch
                && !jr800::isa::is_step_over_candidate(*hd6301v1_ble)
                && hd6301v1_ble->operation
                    == jr800::isa::Operation::branch_if_less_or_equal
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0x2F
                ) == hd6301v1_ble,
            "BLE operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "BLE",
            AddressingMode::relative8
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x2F)
                == nullptr,
        "Unreviewed MC6801 BLE metadata was inherited"
    );
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::hd6301v1,
            "BLE",
            AddressingMode::direct8
        ) == nullptr
            && jr800::isa::find_encoding(
                CpuProfile::hd6301v1,
                "BLE",
                AddressingMode::indexed8
            ) == nullptr
            && jr800::isa::find_encoding(
                CpuProfile::hd6301v1,
                "BLE",
                AddressingMode::extended16
            ) == nullptr,
        "Non-relative BLE metadata was staged"
    );

    const auto* mc6801_bsr = jr800::isa::decode_instruction(CpuProfile::mc6801, 0x8D);
    const auto* hd6301v1_bsr =
        jr800::isa::decode_instruction(CpuProfile::hd6301v1, 0x8D);
    passed &= expect(mc6801_bsr != nullptr, "MC6801 BSR metadata missing");
    passed &= expect(hd6301v1_bsr != nullptr, "HD6301V1 BSR metadata missing");
    if (mc6801_bsr != nullptr && hd6301v1_bsr != nullptr) {
        passed &= expect(mc6801_bsr->base_cycles == 6, "MC6801 BSR cycle mismatch");
        passed &= expect(hd6301v1_bsr->base_cycles == 5, "HD6301V1 BSR cycle mismatch");
        passed &= expect(
            mc6801_bsr->classification == InstructionClass::call,
            "BSR debugger classification mismatch"
        );
        passed &= expect(
            jr800::isa::is_step_over_candidate(*mc6801_bsr),
            "BSR must be a step-over candidate"
        );
    }

    const auto* hd6301v1_jsr = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "JSR",
        AddressingMode::extended16
    );
    passed &= expect(
        hd6301v1_jsr != nullptr,
        "HD6301V1 extended JSR metadata missing"
    );
    if (hd6301v1_jsr != nullptr) {
        passed &= expect(hd6301v1_jsr->opcode == 0xBD, "JSR opcode mismatch");
        passed &= expect(
            hd6301v1_jsr->operand_bytes == 2
                && hd6301v1_jsr->instruction_length == 3,
            "JSR length mismatch"
        );
        passed &= expect(hd6301v1_jsr->base_cycles == 6, "JSR cycle mismatch");
        passed &= expect(
            hd6301v1_jsr->flags.read_mask == 0U
                && hd6301v1_jsr->flags.written_mask == 0U
                && hd6301v1_jsr->flags.preserved_mask
                    == mask({
                        StatusFlag::h,
                        StatusFlag::i,
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    }),
            "JSR flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_jsr->classification == InstructionClass::call
                && jr800::isa::is_step_over_candidate(*hd6301v1_jsr),
            "JSR debugger classification mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "JSR",
            AddressingMode::extended16
        ) == nullptr,
        "Unreviewed MC6801 extended JSR metadata was inherited"
    );
    const auto* hd6301v1_jsr_direct = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "JSR",
        AddressingMode::direct8
    );
    const auto* hd6301v1_jsr_indexed = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "JSR",
        AddressingMode::indexed8
    );
    passed &= expect(
        hd6301v1_jsr_direct != nullptr && hd6301v1_jsr_indexed != nullptr,
        "HD6301V1 direct or indexed JSR metadata missing"
    );
    if (hd6301v1_jsr_direct != nullptr && hd6301v1_jsr_indexed != nullptr) {
        passed &= expect(
            hd6301v1_jsr_direct->opcode == 0x9D
                && hd6301v1_jsr_indexed->opcode == 0xAD
                && hd6301v1_jsr_direct->operand_bytes == 1
                && hd6301v1_jsr_indexed->operand_bytes == 1
                && hd6301v1_jsr_direct->instruction_length == 2
                && hd6301v1_jsr_indexed->instruction_length == 2
                && hd6301v1_jsr_direct->base_cycles == 5
                && hd6301v1_jsr_indexed->base_cycles == 5,
            "Direct or indexed JSR encoding or timing metadata mismatch"
        );
        passed &= expect(
            hd6301v1_jsr_direct->flags.read_mask == 0U
                && hd6301v1_jsr_direct->flags.written_mask == 0U
                && hd6301v1_jsr_direct->flags.preserved_mask
                    == hd6301v1_jsr->flags.preserved_mask
                && hd6301v1_jsr_indexed->flags.read_mask == 0U
                && hd6301v1_jsr_indexed->flags.written_mask == 0U
                && hd6301v1_jsr_indexed->flags.preserved_mask
                    == hd6301v1_jsr_direct->flags.preserved_mask
                && hd6301v1_jsr_direct->flags.undefined_mask == 0U
                && hd6301v1_jsr_indexed->flags.undefined_mask == 0U
                && hd6301v1_jsr_direct->classification
                    == InstructionClass::call
                && hd6301v1_jsr_indexed->classification
                    == InstructionClass::call
                && hd6301v1_jsr_direct->operation
                    == jr800::isa::Operation::branch_to_subroutine
                && hd6301v1_jsr_indexed->operation
                    == jr800::isa::Operation::branch_to_subroutine
                && jr800::isa::is_step_over_candidate(*hd6301v1_jsr_direct)
                && jr800::isa::is_step_over_candidate(*hd6301v1_jsr_indexed),
            "Direct or indexed JSR flags, operation, or class mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "JSR",
            AddressingMode::direct8
        ) == nullptr
            && jr800::isa::find_encoding(
                CpuProfile::mc6801,
                "JSR",
                AddressingMode::indexed8
            ) == nullptr,
        "Unreviewed MC6801 direct or indexed JSR metadata was inherited"
    );

    const auto* rts = jr800::isa::decode_instruction(CpuProfile::mc6801, 0x39);
    passed &= expect(rts != nullptr, "RTS metadata missing");
    if (rts != nullptr) {
        passed &= expect(rts->instruction_length == 1, "RTS length mismatch");
        passed &= expect(rts->base_cycles == 5, "RTS cycle mismatch");
        passed &= expect(
            rts->classification == InstructionClass::subroutine_return,
            "RTS debugger classification mismatch"
        );
        passed &= expect(!jr800::isa::is_step_over_candidate(*rts), "RTS step-over mismatch");
    }

    const auto* hd6301v1_rti = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "RTI",
        AddressingMode::implied
    );
    passed &= expect(hd6301v1_rti != nullptr, "HD6301V1 RTI metadata missing");
    if (hd6301v1_rti != nullptr) {
        passed &= expect(hd6301v1_rti->opcode == 0x3B, "RTI opcode mismatch");
        passed &= expect(
            hd6301v1_rti->operand_bytes == 0
                && hd6301v1_rti->instruction_length == 1,
            "RTI length mismatch"
        );
        passed &= expect(
            hd6301v1_rti->base_cycles == 10,
            "RTI cycle mismatch"
        );
        passed &= expect(
            hd6301v1_rti->flags.read_mask == 0U
                && hd6301v1_rti->flags.written_mask
                    == mask({
                        StatusFlag::h,
                        StatusFlag::i,
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_rti->flags.preserved_mask == 0U
                && hd6301v1_rti->flags.undefined_mask == 0U,
            "RTI flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_rti->classification
                    == InstructionClass::interrupt_return
                && !jr800::isa::is_step_over_candidate(*hd6301v1_rti)
                && hd6301v1_rti->operation
                    == jr800::isa::Operation::return_from_interrupt
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0x3B
                ) == hd6301v1_rti,
            "RTI operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "RTI",
            AddressingMode::implied
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x3B)
                == nullptr,
        "Unreviewed MC6801 RTI metadata was inherited"
    );
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::hd6301v1,
            "RTI",
            AddressingMode::immediate8
        ) == nullptr
            && jr800::isa::find_encoding(
                CpuProfile::hd6301v1,
                "RTI",
                AddressingMode::direct8
            ) == nullptr
            && jr800::isa::find_encoding(
                CpuProfile::hd6301v1,
                "RTI",
                AddressingMode::indexed8
            ) == nullptr
            && jr800::isa::find_encoding(
                CpuProfile::hd6301v1,
                "RTI",
                AddressingMode::extended16
            ) == nullptr,
        "Non-implied RTI metadata was staged"
    );

    const auto* hd6301v1_tsx = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "TSX",
        AddressingMode::implied
    );
    passed &= expect(hd6301v1_tsx != nullptr, "HD6301V1 TSX metadata missing");
    if (hd6301v1_tsx != nullptr) {
        passed &= expect(hd6301v1_tsx->opcode == 0x30, "TSX opcode mismatch");
        passed &= expect(
            hd6301v1_tsx->operand_bytes == 0
                && hd6301v1_tsx->instruction_length == 1,
            "TSX length mismatch"
        );
        passed &= expect(hd6301v1_tsx->base_cycles == 1, "TSX cycle mismatch");
        passed &= expect(
            hd6301v1_tsx->flags.read_mask == 0U
                && hd6301v1_tsx->flags.written_mask == 0U
                && hd6301v1_tsx->flags.preserved_mask
                    == mask({
                        StatusFlag::h,
                        StatusFlag::i,
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    }),
            "TSX flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_tsx->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_tsx),
            "TSX debugger classification mismatch"
        );
        passed &= expect(
            hd6301v1_tsx->operation
                == jr800::isa::Operation::transfer_stack_pointer_to_index_register,
            "TSX operation mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "TSX",
            AddressingMode::implied
        ) == nullptr,
        "Unreviewed MC6801 TSX metadata was inherited"
    );

    const auto* hd6301v1_ins = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "INS",
        AddressingMode::implied
    );
    passed &= expect(hd6301v1_ins != nullptr, "HD6301V1 INS metadata missing");
    if (hd6301v1_ins != nullptr) {
        passed &= expect(hd6301v1_ins->opcode == 0x31, "INS opcode mismatch");
        passed &= expect(
            hd6301v1_ins->operand_bytes == 0
                && hd6301v1_ins->instruction_length == 1,
            "INS length mismatch"
        );
        passed &= expect(hd6301v1_ins->base_cycles == 1, "INS cycle mismatch");
        passed &= expect(
            hd6301v1_ins->flags.read_mask == 0U
                && hd6301v1_ins->flags.written_mask == 0U
                && hd6301v1_ins->flags.preserved_mask
                    == mask({
                        StatusFlag::h,
                        StatusFlag::i,
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    }),
            "INS flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_ins->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_ins),
            "INS debugger classification mismatch"
        );
        passed &= expect(
            hd6301v1_ins->operation
                == jr800::isa::Operation::increment_stack_pointer,
            "INS operation mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "INS",
            AddressingMode::implied
        ) == nullptr,
        "Unreviewed MC6801 INS metadata was inherited"
    );

    const auto* hd6301v1_pula = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "PULA",
        AddressingMode::implied
    );
    passed &= expect(hd6301v1_pula != nullptr, "HD6301V1 PULA metadata missing");
    if (hd6301v1_pula != nullptr) {
        passed &= expect(hd6301v1_pula->opcode == 0x32, "PULA opcode mismatch");
        passed &= expect(
            hd6301v1_pula->operand_bytes == 0
                && hd6301v1_pula->instruction_length == 1,
            "PULA length mismatch"
        );
        passed &= expect(hd6301v1_pula->base_cycles == 3, "PULA cycle mismatch");
        passed &= expect(
            hd6301v1_pula->flags.read_mask == 0U
                && hd6301v1_pula->flags.written_mask == 0U
                && hd6301v1_pula->flags.preserved_mask
                    == mask({
                        StatusFlag::h,
                        StatusFlag::i,
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    }),
            "PULA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_pula->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_pula),
            "PULA debugger classification mismatch"
        );
        passed &= expect(
            hd6301v1_pula->operation
                == jr800::isa::Operation::pull_accumulator_a,
            "PULA operation mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "PULA",
            AddressingMode::implied
        ) == nullptr,
        "Unreviewed MC6801 PULA metadata was inherited"
    );

    const auto* hd6301v1_pulb = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "PULB",
        AddressingMode::implied
    );
    passed &= expect(hd6301v1_pulb != nullptr, "HD6301V1 PULB metadata missing");
    if (hd6301v1_pulb != nullptr) {
        passed &= expect(hd6301v1_pulb->opcode == 0x33, "PULB opcode mismatch");
        passed &= expect(
            hd6301v1_pulb->operand_bytes == 0
                && hd6301v1_pulb->instruction_length == 1,
            "PULB length mismatch"
        );
        passed &= expect(hd6301v1_pulb->base_cycles == 3, "PULB cycle mismatch");
        passed &= expect(
            hd6301v1_pulb->flags.read_mask == 0U
                && hd6301v1_pulb->flags.written_mask == 0U
                && hd6301v1_pulb->flags.preserved_mask
                    == mask({
                        StatusFlag::h,
                        StatusFlag::i,
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    }),
            "PULB flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_pulb->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_pulb),
            "PULB debugger classification mismatch"
        );
        passed &= expect(
            hd6301v1_pulb->operation
                == jr800::isa::Operation::pull_accumulator_b,
            "PULB operation mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "PULB",
            AddressingMode::implied
        ) == nullptr,
        "Unreviewed MC6801 PULB metadata was inherited"
    );

    const auto* hd6301v1_des = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "DES",
        AddressingMode::implied
    );
    passed &= expect(hd6301v1_des != nullptr, "HD6301V1 DES metadata missing");
    if (hd6301v1_des != nullptr) {
        passed &= expect(hd6301v1_des->opcode == 0x34, "DES opcode mismatch");
        passed &= expect(
            hd6301v1_des->operand_bytes == 0
                && hd6301v1_des->instruction_length == 1,
            "DES length mismatch"
        );
        passed &= expect(hd6301v1_des->base_cycles == 1, "DES cycle mismatch");
        passed &= expect(
            hd6301v1_des->flags.read_mask == 0U
                && hd6301v1_des->flags.written_mask == 0U
                && hd6301v1_des->flags.preserved_mask
                    == mask({
                        StatusFlag::h,
                        StatusFlag::i,
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    }),
            "DES flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_des->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_des),
            "DES debugger classification mismatch"
        );
        passed &= expect(
            hd6301v1_des->operation
                == jr800::isa::Operation::decrement_stack_pointer,
            "DES operation mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "DES",
            AddressingMode::implied
        ) == nullptr,
        "Unreviewed MC6801 DES metadata was inherited"
    );

    const auto* hd6301v1_txs = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "TXS",
        AddressingMode::implied
    );
    passed &= expect(hd6301v1_txs != nullptr, "HD6301V1 TXS metadata missing");
    if (hd6301v1_txs != nullptr) {
        passed &= expect(hd6301v1_txs->opcode == 0x35, "TXS opcode mismatch");
        passed &= expect(
            hd6301v1_txs->operand_bytes == 0
                && hd6301v1_txs->instruction_length == 1,
            "TXS length mismatch"
        );
        passed &= expect(hd6301v1_txs->base_cycles == 1, "TXS cycle mismatch");
        passed &= expect(
            hd6301v1_txs->flags.read_mask == 0U
                && hd6301v1_txs->flags.written_mask == 0U
                && hd6301v1_txs->flags.preserved_mask
                    == mask({
                        StatusFlag::h,
                        StatusFlag::i,
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    }),
            "TXS flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_txs->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_txs),
            "TXS debugger classification mismatch"
        );
        passed &= expect(
            hd6301v1_txs->operation
                == jr800::isa::Operation::transfer_index_register_to_stack_pointer,
            "TXS operation mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "TXS",
            AddressingMode::implied
        ) == nullptr,
        "Unreviewed MC6801 TXS metadata was inherited"
    );

    const auto* hd6301v1_psha = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "PSHA",
        AddressingMode::implied
    );
    passed &= expect(hd6301v1_psha != nullptr, "HD6301V1 PSHA metadata missing");
    if (hd6301v1_psha != nullptr) {
        passed &= expect(hd6301v1_psha->opcode == 0x36, "PSHA opcode mismatch");
        passed &= expect(
            hd6301v1_psha->operand_bytes == 0
                && hd6301v1_psha->instruction_length == 1,
            "PSHA length mismatch"
        );
        passed &= expect(hd6301v1_psha->base_cycles == 4, "PSHA cycle mismatch");
        passed &= expect(
            hd6301v1_psha->flags.read_mask == 0U
                && hd6301v1_psha->flags.written_mask == 0U
                && hd6301v1_psha->flags.preserved_mask
                    == mask({
                        StatusFlag::h,
                        StatusFlag::i,
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    }),
            "PSHA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_psha->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_psha),
            "PSHA debugger classification mismatch"
        );
        passed &= expect(
            hd6301v1_psha->operation
                == jr800::isa::Operation::push_accumulator_a,
            "PSHA operation mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "PSHA",
            AddressingMode::implied
        ) == nullptr,
        "Unreviewed MC6801 PSHA metadata was inherited"
    );

    const auto* hd6301v1_pshb = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "PSHB",
        AddressingMode::implied
    );
    passed &= expect(hd6301v1_pshb != nullptr, "HD6301V1 PSHB metadata missing");
    if (hd6301v1_pshb != nullptr) {
        passed &= expect(hd6301v1_pshb->opcode == 0x37, "PSHB opcode mismatch");
        passed &= expect(
            hd6301v1_pshb->operand_bytes == 0
                && hd6301v1_pshb->instruction_length == 1,
            "PSHB length mismatch"
        );
        passed &= expect(hd6301v1_pshb->base_cycles == 4, "PSHB cycle mismatch");
        passed &= expect(
            hd6301v1_pshb->flags.read_mask == 0U
                && hd6301v1_pshb->flags.written_mask == 0U
                && hd6301v1_pshb->flags.preserved_mask
                    == mask({
                        StatusFlag::h,
                        StatusFlag::i,
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    }),
            "PSHB flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_pshb->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_pshb),
            "PSHB debugger classification mismatch"
        );
        passed &= expect(
            hd6301v1_pshb->operation
                == jr800::isa::Operation::push_accumulator_b,
            "PSHB operation mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "PSHB",
            AddressingMode::implied
        ) == nullptr,
        "Unreviewed MC6801 PSHB metadata was inherited"
    );

    const auto* hd6301v1_pshx = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "PSHX",
        AddressingMode::implied
    );
    passed &= expect(hd6301v1_pshx != nullptr, "HD6301V1 PSHX metadata missing");
    if (hd6301v1_pshx != nullptr) {
        passed &= expect(hd6301v1_pshx->opcode == 0x3C, "PSHX opcode mismatch");
        passed &= expect(
            hd6301v1_pshx->operand_bytes == 0
                && hd6301v1_pshx->instruction_length == 1,
            "PSHX length mismatch"
        );
        passed &= expect(hd6301v1_pshx->base_cycles == 5, "PSHX cycle mismatch");
        passed &= expect(
            hd6301v1_pshx->flags.read_mask == 0U
                && hd6301v1_pshx->flags.written_mask == 0U
                && hd6301v1_pshx->flags.preserved_mask
                    == mask({
                        StatusFlag::h,
                        StatusFlag::i,
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    }),
            "PSHX flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_pshx->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_pshx),
            "PSHX debugger classification mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "PSHX",
            AddressingMode::implied
        ) == nullptr,
        "Unreviewed MC6801 PSHX metadata was inherited"
    );

    const auto* hd6301v1_pulx = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "PULX",
        AddressingMode::implied
    );
    passed &= expect(hd6301v1_pulx != nullptr, "HD6301V1 PULX metadata missing");
    if (hd6301v1_pulx != nullptr) {
        passed &= expect(hd6301v1_pulx->opcode == 0x38, "PULX opcode mismatch");
        passed &= expect(
            hd6301v1_pulx->operand_bytes == 0
                && hd6301v1_pulx->instruction_length == 1,
            "PULX length mismatch"
        );
        passed &= expect(hd6301v1_pulx->base_cycles == 4, "PULX cycle mismatch");
        passed &= expect(
            hd6301v1_pulx->flags.read_mask == 0U
                && hd6301v1_pulx->flags.written_mask == 0U
                && hd6301v1_pulx->flags.preserved_mask
                    == mask({
                        StatusFlag::h,
                        StatusFlag::i,
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    }),
            "PULX flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_pulx->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_pulx),
            "PULX debugger classification mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "PULX",
            AddressingMode::implied
        ) == nullptr,
        "Unreviewed MC6801 PULX metadata was inherited"
    );

    const auto* hd6301v1_aim = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "AIM",
        AddressingMode::immediate8_direct8
    );
    passed &= expect(hd6301v1_aim != nullptr, "HD6301V1 AIM metadata missing");
    if (hd6301v1_aim != nullptr) {
        passed &= expect(hd6301v1_aim->opcode == 0x71, "AIM opcode mismatch");
        passed &= expect(hd6301v1_aim->operand_bytes == 2, "AIM operand length mismatch");
        passed &= expect(hd6301v1_aim->instruction_length == 3, "AIM total length mismatch");
        passed &= expect(hd6301v1_aim->base_cycles == 6, "AIM cycle mismatch");
        passed &= expect(
            hd6301v1_aim->flags.written_mask
                == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v}),
            "AIM written flags mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "AIM",
            AddressingMode::immediate8_direct8
        ) == nullptr,
        "AIM must remain unavailable for MC6801"
    );
    const auto* hd6301v1_aim_indexed = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "AIM",
        AddressingMode::immediate8_indexed8
    );
    passed &= expect(
        hd6301v1_aim_indexed != nullptr,
        "HD6301V1 indexed AIM metadata missing"
    );
    if (hd6301v1_aim_indexed != nullptr) {
        passed &= expect(
            hd6301v1_aim_indexed->opcode == 0x61
                && hd6301v1_aim_indexed->operand_bytes == 2
                && hd6301v1_aim_indexed->instruction_length == 3
                && hd6301v1_aim_indexed->base_cycles == 7,
            "Indexed AIM encoding or timing metadata mismatch"
        );
        passed &= expect(
            hd6301v1_aim_indexed->flags.read_mask == 0U
                && hd6301v1_aim_indexed->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_aim_indexed->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c})
                && hd6301v1_aim_indexed->flags.undefined_mask == 0U
                && hd6301v1_aim_indexed->classification
                    == InstructionClass::linear
                && hd6301v1_aim_indexed->operation
                    == jr800::isa::Operation::and_immediate_with_memory,
            "Indexed AIM flag, class, or operation metadata mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "AIM",
            AddressingMode::immediate8_indexed8
        ) == nullptr,
        "Indexed AIM must remain unavailable for MC6801"
    );

    const auto* hd6301v1_oim = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "OIM",
        AddressingMode::immediate8_direct8
    );
    passed &= expect(hd6301v1_oim != nullptr, "HD6301V1 OIM metadata missing");
    if (hd6301v1_oim != nullptr) {
        passed &= expect(hd6301v1_oim->opcode == 0x72, "OIM opcode mismatch");
        passed &= expect(hd6301v1_oim->operand_bytes == 2, "OIM operand length mismatch");
        passed &= expect(hd6301v1_oim->instruction_length == 3, "OIM total length mismatch");
        passed &= expect(hd6301v1_oim->base_cycles == 6, "OIM cycle mismatch");
        passed &= expect(
            hd6301v1_oim->flags.written_mask
                == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_oim->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c}),
            "OIM flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_oim->classification == InstructionClass::linear,
            "OIM debugger classification mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "OIM",
            AddressingMode::immediate8_direct8
        ) == nullptr,
        "OIM must remain unavailable for MC6801"
    );

    const auto* hd6301v1_eim = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "EIM",
        AddressingMode::immediate8_direct8
    );
    passed &= expect(hd6301v1_eim != nullptr, "HD6301V1 EIM metadata missing");
    if (hd6301v1_eim != nullptr) {
        passed &= expect(hd6301v1_eim->opcode == 0x75, "EIM opcode mismatch");
        passed &= expect(
            hd6301v1_eim->operand_bytes == 2
                && hd6301v1_eim->instruction_length == 3,
            "EIM length mismatch"
        );
        passed &= expect(hd6301v1_eim->base_cycles == 6, "EIM cycle mismatch");
        passed &= expect(
            hd6301v1_eim->flags.read_mask == 0U
                && hd6301v1_eim->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_eim->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c})
                && hd6301v1_eim->flags.undefined_mask == 0U,
            "EIM flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_eim->classification == InstructionClass::linear
                && hd6301v1_eim->operation
                    == jr800::isa::Operation::exclusive_or_immediate_with_memory
                && !jr800::isa::is_step_over_candidate(*hd6301v1_eim),
            "EIM operation or debugger classification mismatch"
        );
    }
    const auto* hd6301v1_eim_indexed = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "EIM",
        AddressingMode::immediate8_indexed8
    );
    passed &= expect(
        hd6301v1_eim_indexed != nullptr,
        "HD6301V1 indexed EIM metadata missing"
    );
    if (hd6301v1_eim_indexed != nullptr) {
        passed &= expect(
            hd6301v1_eim_indexed->opcode == 0x65
                && hd6301v1_eim_indexed->operand_bytes == 2
                && hd6301v1_eim_indexed->instruction_length == 3
                && hd6301v1_eim_indexed->base_cycles == 7,
            "Indexed EIM encoding or timing metadata mismatch"
        );
        passed &= expect(
            hd6301v1_eim_indexed->flags.read_mask == 0U
                && hd6301v1_eim_indexed->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_eim_indexed->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c})
                && hd6301v1_eim_indexed->flags.undefined_mask == 0U
                && hd6301v1_eim_indexed->classification
                    == InstructionClass::linear
                && hd6301v1_eim_indexed->operation
                    == jr800::isa::Operation::exclusive_or_immediate_with_memory,
            "Indexed EIM flag, class, or operation metadata mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "EIM",
            AddressingMode::immediate8_indexed8
        ) == nullptr,
        "Indexed EIM must remain unavailable for MC6801"
    );
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "EIM",
            AddressingMode::immediate8_direct8
        ) == nullptr,
        "EIM must remain unavailable for MC6801"
    );

    const auto* hd6301v1_oim_indexed = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "OIM",
        AddressingMode::immediate8_indexed8
    );
    passed &= expect(
        hd6301v1_oim_indexed != nullptr,
        "HD6301V1 indexed OIM metadata missing"
    );
    if (hd6301v1_oim_indexed != nullptr) {
        passed &= expect(
            hd6301v1_oim_indexed->opcode == 0x62,
            "Indexed OIM opcode mismatch"
        );
        passed &= expect(
            hd6301v1_oim_indexed->operand_bytes == 2
                && hd6301v1_oim_indexed->instruction_length == 3,
            "Indexed OIM length mismatch"
        );
        passed &= expect(
            hd6301v1_oim_indexed->base_cycles == 7,
            "Indexed OIM cycle mismatch"
        );
        passed &= expect(
            hd6301v1_oim_indexed->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_oim_indexed->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c}),
            "Indexed OIM flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_oim_indexed->classification == InstructionClass::linear,
            "Indexed OIM debugger classification mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "OIM",
            AddressingMode::immediate8_indexed8
        ) == nullptr,
        "Indexed OIM must remain unavailable for MC6801"
    );

    const auto* hd6301v1_tim_indexed = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "TIM",
        AddressingMode::immediate8_indexed8
    );
    passed &= expect(
        hd6301v1_tim_indexed != nullptr,
        "HD6301V1 indexed TIM metadata missing"
    );
    if (hd6301v1_tim_indexed != nullptr) {
        passed &= expect(
            hd6301v1_tim_indexed->opcode == 0x6B,
            "Indexed TIM opcode mismatch"
        );
        passed &= expect(
            hd6301v1_tim_indexed->operand_bytes == 2
                && hd6301v1_tim_indexed->instruction_length == 3,
            "Indexed TIM length mismatch"
        );
        passed &= expect(
            hd6301v1_tim_indexed->base_cycles == 5,
            "Indexed TIM cycle mismatch"
        );
        passed &= expect(
            hd6301v1_tim_indexed->flags.read_mask == 0U
                && hd6301v1_tim_indexed->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_tim_indexed->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c})
                && hd6301v1_tim_indexed->flags.undefined_mask == 0U,
            "Indexed TIM flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_tim_indexed->classification == InstructionClass::linear
                && hd6301v1_tim_indexed->operation
                    == jr800::isa::Operation::test_immediate_with_memory
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_tim_indexed
                ),
            "Indexed TIM operation or debugger classification mismatch"
        );
    }
    const auto* hd6301v1_tim_direct = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "TIM",
        AddressingMode::immediate8_direct8
    );
    passed &= expect(
        hd6301v1_tim_direct != nullptr,
        "HD6301V1 direct TIM metadata missing"
    );
    if (hd6301v1_tim_direct != nullptr) {
        passed &= expect(
            hd6301v1_tim_direct->opcode == 0x7B,
            "Direct TIM opcode mismatch"
        );
        passed &= expect(
            hd6301v1_tim_direct->operand_bytes == 2
                && hd6301v1_tim_direct->instruction_length == 3,
            "Direct TIM length mismatch"
        );
        passed &= expect(
            hd6301v1_tim_direct->base_cycles == 4,
            "Direct TIM cycle mismatch"
        );
        passed &= expect(
            hd6301v1_tim_direct->flags.read_mask == 0U
                && hd6301v1_tim_direct->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_tim_direct->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c})
                && hd6301v1_tim_direct->flags.undefined_mask == 0U,
            "Direct TIM flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_tim_direct->classification == InstructionClass::linear
                && hd6301v1_tim_direct->operation
                    == jr800::isa::Operation::test_immediate_with_memory
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_tim_direct
                ),
            "Direct TIM operation or debugger classification mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "TIM",
            AddressingMode::immediate8_indexed8
        ) == nullptr,
        "Indexed TIM must remain unavailable for MC6801"
    );
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "TIM",
            AddressingMode::immediate8_direct8
        ) == nullptr,
        "Direct TIM must remain unavailable for MC6801"
    );

    const auto* hd6301v1_tst_extended = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "TST",
        AddressingMode::extended16
    );
    passed &= expect(
        hd6301v1_tst_extended != nullptr,
        "HD6301V1 extended TST metadata missing"
    );
    if (hd6301v1_tst_extended != nullptr) {
        passed &= expect(
            hd6301v1_tst_extended->opcode == 0x7D,
            "Extended TST opcode mismatch"
        );
        passed &= expect(
            hd6301v1_tst_extended->operand_bytes == 2
                && hd6301v1_tst_extended->instruction_length == 3,
            "Extended TST length mismatch"
        );
        passed &= expect(
            hd6301v1_tst_extended->base_cycles == 4,
            "Extended TST cycle mismatch"
        );
        passed &= expect(
            hd6301v1_tst_extended->flags.read_mask == 0U
                && hd6301v1_tst_extended->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_tst_extended->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i})
                && hd6301v1_tst_extended->flags.undefined_mask == 0U,
            "Extended TST flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_tst_extended->classification
                    == InstructionClass::linear
                && hd6301v1_tst_extended->operation
                    == jr800::isa::Operation::test_memory
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_tst_extended
                )
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0x7D
                ) == hd6301v1_tst_extended,
            "Extended TST operation, classification, or decode mismatch"
        );
    }
    const auto* hd6301v1_tst_indexed = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "TST",
        AddressingMode::indexed8
    );
    passed &= expect(
        hd6301v1_tst_indexed != nullptr,
        "HD6301V1 indexed TST metadata missing"
    );
    if (hd6301v1_tst_indexed != nullptr) {
        passed &= expect(
            hd6301v1_tst_indexed->opcode == 0x6D,
            "Indexed TST opcode mismatch"
        );
        passed &= expect(
            hd6301v1_tst_indexed->operand_bytes == 1
                && hd6301v1_tst_indexed->instruction_length == 2,
            "Indexed TST length mismatch"
        );
        passed &= expect(
            hd6301v1_tst_indexed->base_cycles == 4,
            "Indexed TST cycle mismatch"
        );
        passed &= expect(
            hd6301v1_tst_indexed->flags.read_mask == 0U
                && hd6301v1_tst_indexed->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_tst_indexed->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i})
                && hd6301v1_tst_indexed->flags.undefined_mask == 0U,
            "Indexed TST flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_tst_indexed->classification
                    == InstructionClass::linear
                && hd6301v1_tst_indexed->operation
                    == jr800::isa::Operation::test_memory
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_tst_indexed
                )
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0x6D
                ) == hd6301v1_tst_indexed,
            "Indexed TST operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "TST",
            AddressingMode::extended16
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x7D)
                == nullptr
            && jr800::isa::find_encoding(
                CpuProfile::mc6801,
                "TST",
                AddressingMode::indexed8
            ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x6D)
                == nullptr,
        "Unreviewed MC6801 TST metadata was inherited"
    );

    const auto* hd6301v1_tsta = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "TSTA",
        AddressingMode::implied
    );
    passed &= expect(
        hd6301v1_tsta != nullptr,
        "HD6301V1 TSTA metadata missing"
    );
    if (hd6301v1_tsta != nullptr) {
        passed &= expect(
            hd6301v1_tsta->opcode == 0x4D,
            "TSTA opcode mismatch"
        );
        passed &= expect(
            hd6301v1_tsta->operand_bytes == 0
                && hd6301v1_tsta->instruction_length == 1,
            "TSTA length mismatch"
        );
        passed &= expect(
            hd6301v1_tsta->base_cycles == 1,
            "TSTA cycle mismatch"
        );
        passed &= expect(
            hd6301v1_tsta->flags.read_mask == 0U
                && hd6301v1_tsta->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_tsta->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i})
                && hd6301v1_tsta->flags.undefined_mask == 0U,
            "TSTA flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_tsta->operation
                    == jr800::isa::Operation::test_accumulator_a
                && hd6301v1_tsta->classification
                    == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_tsta)
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0x4D
                ) == hd6301v1_tsta,
            "TSTA operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "TSTA",
            AddressingMode::implied
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x4D)
                == nullptr,
        "Unreviewed MC6801 TSTA metadata was inherited"
    );

    const auto* hd6301v1_tstb = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "TSTB",
        AddressingMode::implied
    );
    passed &= expect(
        hd6301v1_tstb != nullptr,
        "HD6301V1 TSTB metadata missing"
    );
    if (hd6301v1_tstb != nullptr) {
        passed &= expect(
            hd6301v1_tstb->opcode == 0x5D,
            "TSTB opcode mismatch"
        );
        passed &= expect(
            hd6301v1_tstb->operand_bytes == 0
                && hd6301v1_tstb->instruction_length == 1,
            "TSTB length mismatch"
        );
        passed &= expect(
            hd6301v1_tstb->base_cycles == 1,
            "TSTB cycle mismatch"
        );
        passed &= expect(
            hd6301v1_tstb->flags.read_mask == 0U
                && hd6301v1_tstb->flags.written_mask
                    == mask({
                        StatusFlag::n,
                        StatusFlag::z,
                        StatusFlag::v,
                        StatusFlag::c,
                    })
                && hd6301v1_tstb->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i})
                && hd6301v1_tstb->flags.undefined_mask == 0U,
            "TSTB flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_tstb->classification == InstructionClass::linear
                && hd6301v1_tstb->operation
                    == jr800::isa::Operation::test_accumulator_b
                && !jr800::isa::is_step_over_candidate(*hd6301v1_tstb)
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0x5D
                ) == hd6301v1_tstb,
            "TSTB operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "TSTB",
            AddressingMode::implied
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0x5D)
                == nullptr,
        "Unreviewed MC6801 TSTB metadata was inherited"
    );

    const auto* hd6301v1_jmp_indexed = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "JMP",
        AddressingMode::indexed8
    );
    passed &= expect(
        hd6301v1_jmp_indexed != nullptr,
        "HD6301V1 indexed JMP encoding missing"
    );
    if (hd6301v1_jmp_indexed != nullptr) {
        passed &= expect(
            hd6301v1_jmp_indexed->opcode == 0x6E,
            "Indexed JMP opcode mismatch"
        );
        passed &= expect(
            hd6301v1_jmp_indexed->operand_bytes == 1
                && hd6301v1_jmp_indexed->instruction_length == 2,
            "Indexed JMP length mismatch"
        );
        passed &= expect(
            hd6301v1_jmp_indexed->base_cycles == 3,
            "Indexed JMP cycle mismatch"
        );
        passed &= expect(
            hd6301v1_jmp_indexed->flags.preserved_mask
                == mask({
                    StatusFlag::h,
                    StatusFlag::i,
                    StatusFlag::n,
                    StatusFlag::z,
                    StatusFlag::v,
                    StatusFlag::c,
                }),
            "Indexed JMP preserved flags mismatch"
        );
        passed &= expect(
            hd6301v1_jmp_indexed->classification == InstructionClass::branch
                && !jr800::isa::is_step_over_candidate(*hd6301v1_jmp_indexed),
            "Indexed JMP debugger classification mismatch"
        );
        passed &= expect(
            hd6301v1_jmp_indexed->operation == jr800::isa::Operation::jump,
            "Indexed JMP operation mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "JMP",
            AddressingMode::indexed8
        ) == nullptr,
        "Unreviewed MC6801 indexed JMP metadata was inherited"
    );

    const auto* hd6301v1_jmp = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "JMP",
        AddressingMode::extended16
    );
    passed &= expect(hd6301v1_jmp != nullptr, "HD6301V1 JMP encoding missing");
    if (hd6301v1_jmp != nullptr) {
        passed &= expect(hd6301v1_jmp->opcode == 0x7E, "JMP opcode mismatch");
        passed &= expect(hd6301v1_jmp->operand_bytes == 2, "JMP operand length mismatch");
        passed &= expect(hd6301v1_jmp->instruction_length == 3, "JMP total length mismatch");
        passed &= expect(hd6301v1_jmp->base_cycles == 3, "JMP cycle mismatch");
        passed &= expect(
            hd6301v1_jmp->flags.preserved_mask
                == mask({
                    StatusFlag::h,
                    StatusFlag::i,
                    StatusFlag::n,
                    StatusFlag::z,
                    StatusFlag::v,
                    StatusFlag::c,
                }),
            "JMP preserved flags mismatch"
        );
        passed &= expect(
            hd6301v1_jmp->classification == InstructionClass::branch,
            "JMP debugger classification mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "JMP",
            AddressingMode::extended16
        ) == nullptr,
        "Unreviewed MC6801 JMP metadata was inherited"
    );

    const auto* hd6301v1_lds = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "LDS",
        AddressingMode::immediate16
    );
    passed &= expect(hd6301v1_lds != nullptr, "HD6301V1 LDS encoding missing");
    if (hd6301v1_lds != nullptr) {
        passed &= expect(hd6301v1_lds->opcode == 0x8E, "LDS opcode mismatch");
        passed &= expect(hd6301v1_lds->operand_bytes == 2, "LDS operand length mismatch");
        passed &= expect(hd6301v1_lds->instruction_length == 3, "LDS total length mismatch");
        passed &= expect(hd6301v1_lds->base_cycles == 3, "LDS cycle mismatch");
        passed &= expect(
            hd6301v1_lds->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_lds->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c}),
            "LDS flag metadata mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "LDS",
            AddressingMode::immediate16
        ) == nullptr,
        "Unreviewed MC6801 LDS metadata was inherited"
    );

    const auto* hd6301v1_lds_direct = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "LDS",
        AddressingMode::direct8
    );
    passed &= expect(
        hd6301v1_lds_direct != nullptr,
        "HD6301V1 direct LDS encoding missing"
    );
    if (hd6301v1_lds_direct != nullptr) {
        passed &= expect(
            hd6301v1_lds_direct->opcode == 0x9E,
            "Direct LDS opcode mismatch"
        );
        passed &= expect(
            hd6301v1_lds_direct->operand_bytes == 1
                && hd6301v1_lds_direct->instruction_length == 2,
            "Direct LDS length mismatch"
        );
        passed &= expect(
            hd6301v1_lds_direct->base_cycles == 4,
            "Direct LDS cycle mismatch"
        );
        passed &= expect(
            hd6301v1_lds_direct->flags.read_mask == 0U
                && hd6301v1_lds_direct->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_lds_direct->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c})
                && hd6301v1_lds_direct->flags.undefined_mask == 0U,
            "Direct LDS flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_lds_direct->classification == InstructionClass::linear
                && hd6301v1_lds_direct->operation
                    == jr800::isa::Operation::load_stack_pointer
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_lds_direct
                ),
            "Direct LDS operation or debugger classification mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "LDS",
            AddressingMode::direct8
        ) == nullptr,
        "Unreviewed MC6801 direct LDS metadata was inherited"
    );
    const auto* hd6301v1_lds_indexed = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "LDS",
        AddressingMode::indexed8
    );
    const auto* hd6301v1_lds_extended = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "LDS",
        AddressingMode::extended16
    );
    passed &= expect(
        hd6301v1_lds_indexed != nullptr && hd6301v1_lds_extended != nullptr,
        "HD6301V1 indexed or extended LDS metadata missing"
    );
    if (hd6301v1_lds_indexed != nullptr
        && hd6301v1_lds_extended != nullptr) {
        passed &= expect(
            hd6301v1_lds_indexed->opcode == 0xAE
                && hd6301v1_lds_extended->opcode == 0xBE
                && hd6301v1_lds_indexed->operand_bytes == 1
                && hd6301v1_lds_indexed->instruction_length == 2
                && hd6301v1_lds_extended->operand_bytes == 2
                && hd6301v1_lds_extended->instruction_length == 3
                && hd6301v1_lds_indexed->base_cycles == 5
                && hd6301v1_lds_extended->base_cycles == 5,
            "Indexed or extended LDS encoding or timing metadata mismatch"
        );
        passed &= expect(
            hd6301v1_lds_indexed->flags.read_mask == 0U
                && hd6301v1_lds_indexed->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_lds_indexed->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c})
                && hd6301v1_lds_indexed->flags.undefined_mask == 0U
                && hd6301v1_lds_extended->flags.read_mask == 0U
                && hd6301v1_lds_extended->flags.written_mask
                    == hd6301v1_lds_indexed->flags.written_mask
                && hd6301v1_lds_extended->flags.preserved_mask
                    == hd6301v1_lds_indexed->flags.preserved_mask
                && hd6301v1_lds_extended->flags.undefined_mask == 0U
                && hd6301v1_lds_indexed->classification
                    == InstructionClass::linear
                && hd6301v1_lds_extended->classification
                    == InstructionClass::linear
                && hd6301v1_lds_indexed->operation
                    == jr800::isa::Operation::load_stack_pointer
                && hd6301v1_lds_extended->operation
                    == jr800::isa::Operation::load_stack_pointer,
            "Indexed or extended LDS flags, class, or operation mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "LDS",
            AddressingMode::indexed8
        ) == nullptr
            && jr800::isa::find_encoding(
                CpuProfile::mc6801,
                "LDS",
                AddressingMode::extended16
            ) == nullptr,
        "Unreviewed MC6801 indexed or extended LDS metadata was inherited"
    );

    const auto* hd6301v1_ldx = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "LDX",
        AddressingMode::immediate16
    );
    passed &= expect(hd6301v1_ldx != nullptr, "HD6301V1 LDX encoding missing");
    if (hd6301v1_ldx != nullptr) {
        passed &= expect(hd6301v1_ldx->opcode == 0xCE, "LDX opcode mismatch");
        passed &= expect(hd6301v1_ldx->operand_bytes == 2, "LDX operand length mismatch");
        passed &= expect(hd6301v1_ldx->instruction_length == 3, "LDX total length mismatch");
        passed &= expect(hd6301v1_ldx->base_cycles == 3, "LDX cycle mismatch");
        passed &= expect(
            hd6301v1_ldx->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_ldx->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c}),
            "LDX flag metadata mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "LDX",
            AddressingMode::immediate16
        ) == nullptr,
        "Unreviewed MC6801 LDX metadata was inherited"
    );

    const auto* hd6301v1_ldx_direct = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "LDX",
        AddressingMode::direct8
    );
    passed &= expect(
        hd6301v1_ldx_direct != nullptr,
        "HD6301V1 direct LDX encoding missing"
    );
    if (hd6301v1_ldx_direct != nullptr) {
        passed &= expect(
            hd6301v1_ldx_direct->opcode == 0xDE,
            "Direct LDX opcode mismatch"
        );
        passed &= expect(
            hd6301v1_ldx_direct->operand_bytes == 1
                && hd6301v1_ldx_direct->instruction_length == 2,
            "Direct LDX length mismatch"
        );
        passed &= expect(
            hd6301v1_ldx_direct->base_cycles == 4,
            "Direct LDX cycle mismatch"
        );
        passed &= expect(
            hd6301v1_ldx_direct->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_ldx_direct->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c}),
            "Direct LDX flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_ldx_direct->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_ldx_direct),
            "Direct LDX debugger classification mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "LDX",
            AddressingMode::direct8
        ) == nullptr,
        "Unreviewed MC6801 direct LDX metadata was inherited"
    );

    const auto* hd6301v1_ldx_indexed = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "LDX",
        AddressingMode::indexed8
    );
    passed &= expect(
        hd6301v1_ldx_indexed != nullptr,
        "HD6301V1 indexed LDX encoding missing"
    );
    if (hd6301v1_ldx_indexed != nullptr) {
        passed &= expect(
            hd6301v1_ldx_indexed->opcode == 0xEE,
            "Indexed LDX opcode mismatch"
        );
        passed &= expect(
            hd6301v1_ldx_indexed->operand_bytes == 1
                && hd6301v1_ldx_indexed->instruction_length == 2,
            "Indexed LDX length mismatch"
        );
        passed &= expect(
            hd6301v1_ldx_indexed->base_cycles == 5,
            "Indexed LDX cycle mismatch"
        );
        passed &= expect(
            hd6301v1_ldx_indexed->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_ldx_indexed->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c}),
            "Indexed LDX flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_ldx_indexed->classification == InstructionClass::linear
                && !jr800::isa::is_step_over_candidate(*hd6301v1_ldx_indexed),
            "Indexed LDX debugger classification mismatch"
        );
        passed &= expect(
            hd6301v1_ldx_indexed->operation
                == jr800::isa::Operation::load_index_register,
            "Indexed LDX operation mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "LDX",
            AddressingMode::indexed8
        ) == nullptr,
        "Unreviewed MC6801 indexed LDX metadata was inherited"
    );
    const auto* hd6301v1_ldx_extended = jr800::isa::find_encoding(
        CpuProfile::hd6301v1,
        "LDX",
        AddressingMode::extended16
    );
    passed &= expect(
        hd6301v1_ldx_extended != nullptr,
        "HD6301V1 extended LDX encoding missing"
    );
    if (hd6301v1_ldx_extended != nullptr) {
        passed &= expect(
            hd6301v1_ldx_extended->opcode == 0xFE,
            "Extended LDX opcode mismatch"
        );
        passed &= expect(
            hd6301v1_ldx_extended->operand_bytes == 2
                && hd6301v1_ldx_extended->instruction_length == 3,
            "Extended LDX length mismatch"
        );
        passed &= expect(
            hd6301v1_ldx_extended->base_cycles == 5,
            "Extended LDX cycle mismatch"
        );
        passed &= expect(
            hd6301v1_ldx_extended->flags.read_mask == 0U
                && hd6301v1_ldx_extended->flags.written_mask
                    == mask({StatusFlag::n, StatusFlag::z, StatusFlag::v})
                && hd6301v1_ldx_extended->flags.preserved_mask
                    == mask({StatusFlag::h, StatusFlag::i, StatusFlag::c})
                && hd6301v1_ldx_extended->flags.undefined_mask == 0U,
            "Extended LDX flag metadata mismatch"
        );
        passed &= expect(
            hd6301v1_ldx_extended->classification
                    == InstructionClass::linear
                && hd6301v1_ldx_extended->operation
                    == jr800::isa::Operation::load_index_register
                && !jr800::isa::is_step_over_candidate(
                    *hd6301v1_ldx_extended
                )
                && jr800::isa::decode_instruction(
                    CpuProfile::hd6301v1,
                    0xFE
                ) == hd6301v1_ldx_extended,
            "Extended LDX operation, classification, or decode mismatch"
        );
    }
    passed &= expect(
        jr800::isa::find_encoding(
            CpuProfile::mc6801,
            "LDX",
            AddressingMode::extended16
        ) == nullptr
            && jr800::isa::decode_instruction(CpuProfile::mc6801, 0xFE)
                == nullptr,
        "Unreviewed MC6801 extended LDX metadata was inherited"
    );

    passed &= expect(
        jr800::isa::decode_instruction(CpuProfile::jr800_unresolved, 0x01) == nullptr,
        "Unresolved JR-800 profile must not inherit NOP metadata"
    );
    passed &= expect(
        jr800::isa::decode_instruction(CpuProfile::jr800_unresolved, 0x71) == nullptr,
        "Unresolved JR-800 profile must not inherit extension metadata"
    );

    return passed ? 0 : 1;
}
