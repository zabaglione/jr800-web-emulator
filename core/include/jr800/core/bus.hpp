// SPDX-License-Identifier: MIT

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>

namespace jr800::core {

class Bus;

enum class AccessKind : std::uint8_t {
    instruction_fetch,
    data_read,
    data_write,
};

enum class BusFault : std::uint8_t {
    none,
    backing_store_unavailable,
    uninitialized_read,
    unsupported_access,
    read_only_write,
    device_state_unknown,
    device_state_unsupported,
};

enum class InterruptSource : std::uint8_t {
    none,
    timer_input_capture,
    timer_output_compare,
    timer_overflow,
    serial,
};

struct InterruptRequest {
    InterruptSource source{InterruptSource::none};
    bool known{true};

    [[nodiscard]] bool asserted() const noexcept {
        return known && source != InterruptSource::none;
    }

    bool operator==(const InterruptRequest&) const = default;
};

struct BusReadResult {
    BusFault fault{BusFault::none};
    std::optional<std::uint8_t> value;

    [[nodiscard]] bool succeeded() const noexcept {
        return fault == BusFault::none && value.has_value();
    }
};

struct BusDiscardedReadResult {
    BusFault fault{BusFault::none};

    [[nodiscard]] bool succeeded() const noexcept {
        return fault == BusFault::none;
    }
};

struct BusWriteResult {
    BusFault fault{BusFault::none};
    std::uint8_t previous_value{};
    bool previous_value_known{};

    [[nodiscard]] bool succeeded() const noexcept {
        return fault == BusFault::none;
    }
};

struct BusAccessEvent {
    std::uint64_t sequence{};
    std::uint64_t instruction_cycle{};
    std::uint16_t instruction_pc{};
    std::uint16_t address{};
    std::uint8_t value{};
    std::uint8_t previous_value{};
    AccessKind kind{AccessKind::data_read};
    bool value_known{true};
    bool previous_value_known{true};

    bool operator==(const BusAccessEvent&) const = default;
};

class BusObserver {
public:
    BusObserver() = default;
    virtual ~BusObserver();

    BusObserver(const BusObserver&) = delete;
    BusObserver& operator=(const BusObserver&) = delete;
    BusObserver(BusObserver&&) = delete;
    BusObserver& operator=(BusObserver&&) = delete;

    virtual void on_bus_access(const BusAccessEvent& event) noexcept = 0;

private:
    friend class Bus;
    Bus* bus_{};
};

class Bus {
public:
    Bus() = default;
    virtual ~Bus();

    Bus(const Bus&) = delete;
    Bus& operator=(const Bus&) = delete;
    Bus(Bus&&) = delete;
    Bus& operator=(Bus&&) = delete;

    [[nodiscard]] virtual BusFault advance_cycles(
        std::uint32_t cycles
    ) noexcept = 0;
    [[nodiscard]] virtual InterruptRequest maskable_interrupt_request()
        const noexcept = 0;

    [[nodiscard]] virtual BusReadResult read8(
        std::uint16_t address,
        AccessKind kind
    ) noexcept = 0;
    [[nodiscard]] virtual BusDiscardedReadResult read8_discard(
        std::uint16_t address
    ) noexcept = 0;
    [[nodiscard]] virtual BusReadResult inspect8(
        std::uint16_t address
    ) const noexcept = 0;
    [[nodiscard]] virtual BusWriteResult write8(
        std::uint16_t address,
        std::uint8_t value
    ) noexcept = 0;

    [[nodiscard]] bool set_observer(BusObserver* observer) noexcept;
    void set_instruction_context(
        std::uint64_t cycle,
        std::uint16_t pc
    ) noexcept;

protected:
    // Copy trace counters for an isolated machine transaction, never observers.
    void copy_execution_context(const Bus& source) noexcept {
        access_sequence_ = source.access_sequence_;
        instruction_cycle_ = source.instruction_cycle_;
        instruction_pc_ = source.instruction_pc_;
    }

    void notify_read(
        std::uint16_t address,
        std::optional<std::uint8_t> value,
        AccessKind kind
    ) noexcept;
    void notify_write(
        std::uint16_t address,
        std::uint8_t value,
        std::optional<std::uint8_t> previous_value
    ) noexcept;

private:
    friend class BusObserver;
    void release_destroying_observer(BusObserver& observer) noexcept;

    BusObserver* observer_{};
    std::uint64_t access_sequence_{};
    std::uint64_t instruction_cycle_{};
    std::uint16_t instruction_pc_{};
};

class RamBus final : public Bus {
public:
    RamBus() = default;
    ~RamBus() override = default;

    RamBus(const RamBus&) = delete;
    RamBus& operator=(const RamBus&) = delete;
    RamBus(RamBus&&) = delete;
    RamBus& operator=(RamBus&&) = delete;

    [[nodiscard]] BusFault advance_cycles(
        std::uint32_t cycles
    ) noexcept override;
    [[nodiscard]] InterruptRequest maskable_interrupt_request()
        const noexcept override;

    [[nodiscard]] BusReadResult read8(
        std::uint16_t address,
        AccessKind kind
    ) noexcept override;
    [[nodiscard]] BusDiscardedReadResult read8_discard(
        std::uint16_t address
    ) noexcept override;
    [[nodiscard]] BusReadResult inspect8(
        std::uint16_t address
    ) const noexcept override;
    [[nodiscard]] BusWriteResult write8(
        std::uint16_t address,
        std::uint8_t value
    ) noexcept override;

    void clear() noexcept;
    [[nodiscard]] bool load(
        std::uint16_t address,
        std::span<const std::uint8_t> bytes
    ) noexcept;
    [[nodiscard]] bool fill(
        std::uint16_t address,
        std::uint32_t size,
        std::uint8_t value
    ) noexcept;
    [[nodiscard]] std::uint8_t peek8(std::uint16_t address) const noexcept;
    void poke8(std::uint16_t address, std::uint8_t value) noexcept;
    void set_maskable_interrupt_request(InterruptRequest request) noexcept;

private:
    std::array<std::uint8_t, 65'536> memory_{};
    InterruptRequest interrupt_request_{};
};

}  // namespace jr800::core
