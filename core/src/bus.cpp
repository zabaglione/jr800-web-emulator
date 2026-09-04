// SPDX-License-Identifier: MIT

#include "jr800/core/bus.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace jr800::core {

BusObserver::~BusObserver() {
    if (bus_ != nullptr) {
        bus_->release_destroying_observer(*this);
    }
}

Bus::~Bus() {
    static_cast<void>(set_observer(nullptr));
}

bool Bus::set_observer(BusObserver* observer) noexcept {
    if (observer_ == observer) {
        return true;
    }
    if (observer != nullptr && observer->bus_ != nullptr
        && observer->bus_ != this) {
        return false;
    }
    auto* const detached = observer_;
    observer_ = observer;
    if (observer != nullptr) {
        observer->bus_ = this;
    }
    if (detached != nullptr) {
        detached->bus_ = nullptr;
    }
    return true;
}

void Bus::release_destroying_observer(BusObserver& observer) noexcept {
    if (observer_ == &observer) {
        observer_ = nullptr;
    }
    observer.bus_ = nullptr;
}

void Bus::set_instruction_context(
    std::uint64_t cycle,
    std::uint16_t pc
) noexcept {
    instruction_cycle_ = cycle;
    instruction_pc_ = pc;
}

void Bus::notify_read(
    std::uint16_t address,
    std::optional<std::uint8_t> value,
    AccessKind kind
) noexcept {
    ++access_sequence_;
    if (observer_ != nullptr) {
        const auto recorded_value = value.value_or(0U);
        const auto value_known = value.has_value();
        observer_->on_bus_access(BusAccessEvent{
            access_sequence_,
            instruction_cycle_,
            instruction_pc_,
            address,
            recorded_value,
            recorded_value,
            kind,
            value_known,
            value_known,
        });
    }
}

void Bus::notify_write(
    std::uint16_t address,
    std::uint8_t value,
    std::optional<std::uint8_t> previous_value
) noexcept {
    ++access_sequence_;
    if (observer_ != nullptr) {
        observer_->on_bus_access(BusAccessEvent{
            access_sequence_,
            instruction_cycle_,
            instruction_pc_,
            address,
            value,
            previous_value.value_or(0U),
            AccessKind::data_write,
            true,
            previous_value.has_value(),
        });
    }
}

BusFault RamBus::advance_cycles(std::uint32_t cycles) noexcept {
    static_cast<void>(cycles);
    return BusFault::none;
}

InterruptRequest RamBus::maskable_interrupt_request() const noexcept {
    return interrupt_request_;
}

BusReadResult RamBus::read8(
    std::uint16_t address,
    AccessKind kind
) noexcept {
    const auto value = memory_[address];
    notify_read(address, value, kind);
    return {BusFault::none, value};
}

BusDiscardedReadResult RamBus::read8_discard(
    std::uint16_t address
) noexcept {
    notify_read(address, memory_[address], AccessKind::data_read);
    return {BusFault::none};
}

BusReadResult RamBus::inspect8(std::uint16_t address) const noexcept {
    return {BusFault::none, memory_[address]};
}

BusWriteResult RamBus::write8(
    std::uint16_t address,
    std::uint8_t value
) noexcept {
    const auto previous = memory_[address];
    memory_[address] = value;
    notify_write(address, value, previous);
    return {BusFault::none, previous, true};
}

void RamBus::clear() noexcept {
    memory_.fill(0U);
    interrupt_request_ = {};
}

bool RamBus::load(
    std::uint16_t address,
    std::span<const std::uint8_t> bytes
) noexcept {
    const auto begin = static_cast<std::size_t>(address);
    if (bytes.size() > memory_.size() - begin) {
        return false;
    }
    std::copy(bytes.begin(), bytes.end(), memory_.begin() + begin);
    return true;
}

bool RamBus::fill(
    std::uint16_t address,
    std::uint32_t size,
    std::uint8_t value
) noexcept {
    const auto begin = static_cast<std::size_t>(address);
    if (size > memory_.size() - begin) {
        return false;
    }
    std::fill_n(memory_.begin() + begin, size, value);
    return true;
}

std::uint8_t RamBus::peek8(std::uint16_t address) const noexcept {
    return memory_[address];
}

void RamBus::poke8(std::uint16_t address, std::uint8_t value) noexcept {
    memory_[address] = value;
}

void RamBus::set_maskable_interrupt_request(
    InterruptRequest request
) noexcept {
    interrupt_request_ = request;
}

}  // namespace jr800::core
