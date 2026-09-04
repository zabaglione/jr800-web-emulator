// SPDX-License-Identifier: MIT

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "jr800/formats/jr8app.hpp"
#include "jr800/formats/jr8dbg.hpp"
#include "jr800/formats/linked_error.hpp"

namespace {

using jr800::formats::linked::ErrorCode;

bool expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

template <typename Callable>
bool expect_error(Callable&& callable, ErrorCode expected, std::string_view message) {
    try {
        std::forward<Callable>(callable)();
    } catch (const jr800::formats::linked::Error& error) {
        if (error.code() == expected) {
            return true;
        }
        std::cerr << message << ": unexpected error code\n";
        return false;
    }
    std::cerr << message << ": no error\n";
    return false;
}

jr800::formats::Sha256Digest digest(std::uint8_t seed) {
    jr800::formats::Sha256Digest result{};
    for (std::size_t index = 0; index < result.size(); ++index) {
        result[index] = static_cast<std::uint8_t>(seed + index);
    }
    return result;
}

jr800::formats::jr8app::Application make_application() {
    using namespace jr800::formats::jr8app;

    Application application;
    application.target_profile = "hd6301v1";
    application.entry_point = 0x0200;
    application.segments = {
        Segment{SegmentKind::data, 0x0200, 4, {0x86, 0x2A, 0x20, 0xFC}},
        Segment{SegmentKind::zero_fill, 0x2000, 16, {}},
    };
    application.integrity_sha256 = compute_integrity(application);
    return application;
}

jr800::formats::jr8dbg::DebugInfo make_debug_info(
    const jr800::formats::Sha256Digest& application_integrity
) {
    using namespace jr800::formats::jr8dbg;

    DebugInfo debug_info;
    debug_info.target_profile = "hd6301v1";
    debug_info.application_integrity_sha256 = application_integrity;
    debug_info.source_files = {
        SourceFile{"src/main.s", digest(0x20)},
        SourceFile{"src/lib.s", digest(0x40)},
    };
    debug_info.symbols = {
        Symbol{"constant", SymbolBinding::global, SymbolKind::absolute, 0x002A, 0, 1},
        Symbol{"entry", SymbolBinding::global, SymbolKind::address, 0x0200, 4, 0},
        Symbol{"loop", SymbolBinding::local, SymbolKind::address, 0x0202, 0, 0},
    };
    debug_info.line_mappings = {
        LineMapping{0x0200, 2, 0, 5, 5},
        LineMapping{0x0202, 2, 1, 4, 5},
    };
    return debug_info;
}

}  // namespace

int main() {
    using namespace jr800::formats;

    bool passed = true;
    const auto application = make_application();
    const auto application_bytes = jr8app::write(application);
    passed &= expect(
        jr8app::read(application_bytes) == application,
        "JR8APP semantic round trip failed"
    );
    passed &= expect(
        application_bytes == jr8app::write(application),
        "JR8APP repeated serialization differs"
    );

    auto reordered_application = application;
    std::reverse(
        reordered_application.segments.begin(),
        reordered_application.segments.end()
    );
    reordered_application.integrity_sha256 = jr8app::compute_integrity(reordered_application);
    passed &= expect(
        jr8app::write(reordered_application) == application_bytes,
        "JR8APP segment serialization is not canonical"
    );

    auto tampered_application = application_bytes;
    tampered_application[80] ^= 0x01U;
    passed &= expect_error(
        [&] { static_cast<void>(jr8app::read(tampered_application)); },
        linked::ErrorCode::integrity_mismatch,
        "JR8APP integrity tamper was not detected"
    );
    for (std::size_t length = 0; length < application_bytes.size(); ++length) {
        passed &= expect_error(
            [&] {
                static_cast<void>(jr8app::read(std::span{application_bytes}.first(length)));
            },
            linked::ErrorCode::truncated,
            "JR8APP truncation was not rejected"
        );
    }

    auto bad_magic = application_bytes;
    bad_magic.front() ^= 0xFFU;
    passed &= expect_error(
        [&] { static_cast<void>(jr8app::read(bad_magic)); },
        linked::ErrorCode::invalid_magic,
        "JR8APP bad magic was not rejected"
    );

    const auto debug_info = make_debug_info(application.integrity_sha256);
    const auto debug_bytes = jr8dbg::write(debug_info);
    passed &= expect(
        jr8dbg::read(debug_bytes) == debug_info,
        "JR8DBG semantic round trip failed"
    );
    passed &= expect(
        debug_bytes == jr8dbg::write(debug_info),
        "JR8DBG repeated serialization differs"
    );

    auto reordered_debug = debug_info;
    std::reverse(reordered_debug.symbols.begin(), reordered_debug.symbols.end());
    std::reverse(reordered_debug.line_mappings.begin(), reordered_debug.line_mappings.end());
    passed &= expect(
        jr8dbg::write(reordered_debug) == debug_bytes,
        "JR8DBG order-insensitive tables are not canonical"
    );

    const auto* line = jr8dbg::find_line(debug_info, 0x0201);
    passed &= expect(line != nullptr, "JR8DBG line lookup failed");
    if (line != nullptr) {
        passed &= expect(
            line->source_file_index == 0U && line->line == 5U,
            "JR8DBG line lookup returned the wrong mapping"
        );
    }
    passed &= expect(
        jr8dbg::find_line(debug_info, 0x0300) == nullptr,
        "JR8DBG unmapped address lookup mismatch"
    );
    const auto* source_line = jr8dbg::find_source_line(
        debug_info,
        "src/main.s",
        5U
    );
    passed &= expect(
        source_line != nullptr && source_line->address == 0x0200U,
        "JR8DBG source-line reverse lookup failed"
    );
    auto repeated_source_line = debug_info;
    repeated_source_line.line_mappings.insert(
        repeated_source_line.line_mappings.begin(),
        jr8dbg::LineMapping{0x0300U, 1U, 0U, 5U, 1U}
    );
    const auto* earliest_source_line = jr8dbg::find_source_line(
        repeated_source_line,
        "src/main.s",
        5U
    );
    passed &= expect(
        earliest_source_line != nullptr
            && earliest_source_line->address == 0x0200U,
        "JR8DBG source-line lookup depends on mapping table order"
    );
    passed &= expect(
        jr8dbg::find_source_line(debug_info, "src/missing.s", 5U) == nullptr
            && jr8dbg::find_source_line(debug_info, "src/main.s", 0U)
                == nullptr
            && jr8dbg::find_source_line(debug_info, "src/main.s", 99U)
                == nullptr,
        "JR8DBG source-line reverse lookup guessed a missing location"
    );
    const auto symbols = jr8dbg::find_symbols(debug_info, 0x0202);
    passed &= expect(symbols.size() == 2U, "JR8DBG symbol lookup count mismatch");
    passed &= expect(
        std::none_of(symbols.begin(), symbols.end(), [](const auto* symbol) {
            return symbol->kind == jr8dbg::SymbolKind::absolute;
        }),
        "JR8DBG address lookup included an absolute symbol"
    );

    auto invalid_debug = debug_info;
    invalid_debug.line_mappings.front().source_file_index = 99U;
    passed &= expect_error(
        [&] { static_cast<void>(jr8dbg::write(invalid_debug)); },
        linked::ErrorCode::invalid_reference,
        "JR8DBG invalid source reference was not rejected"
    );
    for (std::size_t length = 0; length < debug_bytes.size(); ++length) {
        passed &= expect_error(
            [&] {
                static_cast<void>(jr8dbg::read(std::span{debug_bytes}.first(length)));
            },
            linked::ErrorCode::truncated,
            "JR8DBG truncation was not rejected"
        );
    }

    return passed ? 0 : 1;
}
