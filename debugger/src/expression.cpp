// SPDX-License-Identifier: MIT

#include "jr800/debugger/expression.hpp"

#include <charconv>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace jr800::debugger {
namespace {

constexpr std::size_t kMaximumExpressionLength = 256U;
constexpr std::size_t kMaximumExpressionNodes = 128U;
constexpr std::size_t kMaximumExpressionDepth = 32U;

enum class TokenKind : std::uint8_t {
    end,
    invalid,
    number,
    identifier,
    string,
    left_parenthesis,
    right_parenthesis,
    left_bracket,
    right_bracket,
    plus,
    minus,
    multiply,
    divide,
    remainder,
    bitwise_not,
    logical_not,
    shift_left,
    shift_right,
    less,
    less_equal,
    greater,
    greater_equal,
    equal,
    not_equal,
    bitwise_and,
    bitwise_xor,
    bitwise_or,
    logical_and,
    logical_or,
};

struct Token {
    TokenKind kind{TokenKind::invalid};
    std::uint64_t number{};
    std::string text;
    std::size_t offset{};
};

class Lexer final {
public:
    explicit Lexer(std::string_view text) noexcept : text_(text) {}

    [[nodiscard]] Token next() {
        while (peek() == ' ' || peek() == '\t') {
            ++index_;
        }
        const auto offset = index_;
        const auto first = peek();
        if (first == '\0') {
            return Token{TokenKind::end, 0U, {}, offset};
        }
        if (first == '$') {
            ++index_;
            return number(16, offset);
        }
        if (first == '0' && (peek(1U) == 'x' || peek(1U) == 'X')) {
            index_ += 2U;
            return number(16, offset);
        }
        if (is_decimal_digit(first)) {
            return number(10, offset);
        }
        if (first == '"') {
            return string(offset);
        }
        if (is_ascii_letter(first) || first == '_') {
            std::string value;
            while (is_ascii_letter(peek()) || is_decimal_digit(peek())
                   || peek() == '_') {
                const auto character = peek();
                value.push_back(character >= 'A' && character <= 'Z'
                    ? static_cast<char>(character - 'A' + 'a')
                    : character);
                ++index_;
            }
            return Token{TokenKind::identifier, 0U, std::move(value), offset};
        }

        ++index_;
        switch (first) {
        case '(':
            return Token{TokenKind::left_parenthesis, 0U, {}, offset};
        case ')':
            return Token{TokenKind::right_parenthesis, 0U, {}, offset};
        case '[':
            return Token{TokenKind::left_bracket, 0U, {}, offset};
        case ']':
            return Token{TokenKind::right_bracket, 0U, {}, offset};
        case '+':
            return Token{TokenKind::plus, 0U, {}, offset};
        case '-':
            return Token{TokenKind::minus, 0U, {}, offset};
        case '*':
            return Token{TokenKind::multiply, 0U, {}, offset};
        case '/':
            return Token{TokenKind::divide, 0U, {}, offset};
        case '%':
            return Token{TokenKind::remainder, 0U, {}, offset};
        case '~':
            return Token{TokenKind::bitwise_not, 0U, {}, offset};
        case '^':
            return Token{TokenKind::bitwise_xor, 0U, {}, offset};
        case '!':
            if (take('=')) {
                return Token{TokenKind::not_equal, 0U, {}, offset};
            }
            return Token{TokenKind::logical_not, 0U, {}, offset};
        case '<':
            if (take('<')) {
                return Token{TokenKind::shift_left, 0U, {}, offset};
            }
            if (take('=')) {
                return Token{TokenKind::less_equal, 0U, {}, offset};
            }
            return Token{TokenKind::less, 0U, {}, offset};
        case '>':
            if (take('>')) {
                return Token{TokenKind::shift_right, 0U, {}, offset};
            }
            if (take('=')) {
                return Token{TokenKind::greater_equal, 0U, {}, offset};
            }
            return Token{TokenKind::greater, 0U, {}, offset};
        case '=':
            if (take('=')) {
                return Token{TokenKind::equal, 0U, {}, offset};
            }
            break;
        case '&':
            if (take('&')) {
                return Token{TokenKind::logical_and, 0U, {}, offset};
            }
            return Token{TokenKind::bitwise_and, 0U, {}, offset};
        case '|':
            if (take('|')) {
                return Token{TokenKind::logical_or, 0U, {}, offset};
            }
            return Token{TokenKind::bitwise_or, 0U, {}, offset};
        default:
            break;
        }
        return Token{TokenKind::invalid, 0U, {}, offset};
    }

private:
    [[nodiscard]] static constexpr bool is_ascii_letter(char value) noexcept {
        return (value >= 'a' && value <= 'z')
            || (value >= 'A' && value <= 'Z');
    }

    [[nodiscard]] static constexpr bool is_decimal_digit(char value) noexcept {
        return value >= '0' && value <= '9';
    }

    [[nodiscard]] char peek(std::size_t lookahead = 0U) const noexcept {
        const auto position = index_ + lookahead;
        return position < text_.size() ? text_[position] : '\0';
    }

    bool take(char expected) noexcept {
        if (peek() != expected) {
            return false;
        }
        ++index_;
        return true;
    }

    Token number(int base, std::size_t offset) {
        const auto begin = index_;
        while (true) {
            const auto character = peek();
            const auto decimal = character >= '0' && character <= '9';
            const auto lower_hex = character >= 'a' && character <= 'f';
            const auto upper_hex = character >= 'A' && character <= 'F';
            if (!decimal && !(base == 16 && (lower_hex || upper_hex))) {
                break;
            }
            ++index_;
        }
        if (begin == index_) {
            return Token{TokenKind::invalid, 0U, {}, offset};
        }
        std::uint64_t value{};
        const auto digits = text_.substr(begin, index_ - begin);
        const auto [end, error] = std::from_chars(
            digits.data(),
            digits.data() + digits.size(),
            value,
            base
        );
        if (error != std::errc{} || end != digits.data() + digits.size()) {
            return Token{TokenKind::invalid, 0U, {}, offset};
        }
        return Token{TokenKind::number, value, {}, offset};
    }

    Token string(std::size_t offset) {
        ++index_;
        std::string value;
        while (true) {
            const auto character = peek();
            if (character == '"') {
                ++index_;
                return Token{
                    TokenKind::string,
                    0U,
                    std::move(value),
                    offset,
                };
            }
            if (character == '\0' || character == '\r' || character == '\n'
                || character == '\\') {
                return Token{TokenKind::invalid, 0U, {}, offset};
            }
            value.push_back(character);
            ++index_;
        }
    }

    std::string_view text_;
    std::size_t index_{};
};

enum class NodeKind : std::uint8_t {
    literal,
    symbol,
    debug_symbol,
    memory,
    unary,
    binary,
};

enum class Symbol : std::uint8_t {
    pc,
    sp,
    x,
    a,
    b,
    ccr,
    cycles,
    h,
    i,
    n,
    z,
    v,
    c,
};

struct Node {
    NodeKind kind{NodeKind::literal};
    TokenKind operation{TokenKind::end};
    Symbol symbol{Symbol::pc};
    std::uint64_t literal{};
    std::string debug_symbol;
    std::unique_ptr<Node> left;
    std::unique_ptr<Node> right;
};

class Parser final {
public:
    Parser(std::string_view text, ExpressionCompileDiagnostic& diagnostic)
        : lexer_(text), diagnostic_(diagnostic), current_(lexer_.next()) {}

    [[nodiscard]] std::unique_ptr<Node> parse() {
        auto root = parse_binary(0, 0U);
        if (root != nullptr && current_.kind != TokenKind::end) {
            fail(
                current_.kind == TokenKind::invalid
                    ? ExpressionCompileError::invalid_token
                    : ExpressionCompileError::invalid_syntax,
                current_.offset
            );
            return nullptr;
        }
        return root;
    }

private:
    static int precedence(TokenKind kind) noexcept {
        switch (kind) {
        case TokenKind::logical_or:
            return 10;
        case TokenKind::logical_and:
            return 20;
        case TokenKind::bitwise_or:
            return 30;
        case TokenKind::bitwise_xor:
            return 40;
        case TokenKind::bitwise_and:
            return 50;
        case TokenKind::equal:
        case TokenKind::not_equal:
            return 60;
        case TokenKind::less:
        case TokenKind::less_equal:
        case TokenKind::greater:
        case TokenKind::greater_equal:
            return 70;
        case TokenKind::shift_left:
        case TokenKind::shift_right:
            return 80;
        case TokenKind::plus:
        case TokenKind::minus:
            return 90;
        case TokenKind::multiply:
        case TokenKind::divide:
        case TokenKind::remainder:
            return 100;
        default:
            return -1;
        }
    }

    void advance() {
        current_ = lexer_.next();
    }

    void fail(ExpressionCompileError error, std::size_t offset) noexcept {
        if (diagnostic_.succeeded()) {
            diagnostic_ = ExpressionCompileDiagnostic{error, offset};
        }
    }

    bool add_node(std::size_t offset, std::size_t depth) noexcept {
        if (depth > kMaximumExpressionDepth
            || node_count_ >= kMaximumExpressionNodes) {
            fail(ExpressionCompileError::too_complex, offset);
            return false;
        }
        ++node_count_;
        return true;
    }

    [[nodiscard]] std::unique_ptr<Node> parse_binary(
        int minimum_precedence,
        std::size_t depth
    ) {
        if (depth > kMaximumExpressionDepth) {
            fail(ExpressionCompileError::too_complex, current_.offset);
            return nullptr;
        }
        auto left = parse_unary(depth);
        while (left != nullptr) {
            const auto operation = current_;
            const auto operation_precedence = precedence(operation.kind);
            if (operation_precedence < minimum_precedence) {
                break;
            }
            advance();
            auto right = parse_binary(operation_precedence + 1, depth + 1U);
            if (right == nullptr || !add_node(operation.offset, depth)) {
                return nullptr;
            }
            auto parent = std::make_unique<Node>();
            parent->kind = NodeKind::binary;
            parent->operation = operation.kind;
            parent->left = std::move(left);
            parent->right = std::move(right);
            left = std::move(parent);
        }
        return left;
    }

    [[nodiscard]] std::unique_ptr<Node> parse_unary(std::size_t depth) {
        if (current_.kind == TokenKind::plus
            || current_.kind == TokenKind::minus
            || current_.kind == TokenKind::bitwise_not
            || current_.kind == TokenKind::logical_not) {
            const auto operation = current_;
            advance();
            auto operand = parse_unary(depth + 1U);
            if (operand == nullptr || !add_node(operation.offset, depth)) {
                return nullptr;
            }
            auto node = std::make_unique<Node>();
            node->kind = NodeKind::unary;
            node->operation = operation.kind;
            node->right = std::move(operand);
            return node;
        }
        return parse_primary(depth);
    }

    [[nodiscard]] static bool symbol_from_name(
        std::string_view name,
        Symbol& symbol
    ) noexcept {
        if (name == "pc") {
            symbol = Symbol::pc;
        } else if (name == "sp") {
            symbol = Symbol::sp;
        } else if (name == "x") {
            symbol = Symbol::x;
        } else if (name == "a") {
            symbol = Symbol::a;
        } else if (name == "b") {
            symbol = Symbol::b;
        } else if (name == "ccr") {
            symbol = Symbol::ccr;
        } else if (name == "cycles") {
            symbol = Symbol::cycles;
        } else if (name == "h") {
            symbol = Symbol::h;
        } else if (name == "i") {
            symbol = Symbol::i;
        } else if (name == "n") {
            symbol = Symbol::n;
        } else if (name == "z") {
            symbol = Symbol::z;
        } else if (name == "v") {
            symbol = Symbol::v;
        } else if (name == "c") {
            symbol = Symbol::c;
        } else {
            return false;
        }
        return true;
    }

    [[nodiscard]] std::unique_ptr<Node> parse_primary(std::size_t depth) {
        const auto token = current_;
        if (token.kind == TokenKind::invalid) {
            fail(ExpressionCompileError::invalid_token, token.offset);
            return nullptr;
        }
        if (token.kind == TokenKind::number) {
            advance();
            if (!add_node(token.offset, depth)) {
                return nullptr;
            }
            auto node = std::make_unique<Node>();
            node->literal = token.number;
            return node;
        }
        if (token.kind == TokenKind::identifier) {
            advance();
            if (token.text == "mem8") {
                if (current_.kind != TokenKind::left_bracket) {
                    fail(ExpressionCompileError::invalid_syntax, current_.offset);
                    return nullptr;
                }
                advance();
                auto address = parse_binary(0, depth + 1U);
                if (address == nullptr
                    || current_.kind != TokenKind::right_bracket) {
                    if (address != nullptr) {
                        fail(ExpressionCompileError::invalid_syntax, current_.offset);
                    }
                    return nullptr;
                }
                advance();
                if (!add_node(token.offset, depth)) {
                    return nullptr;
                }
                auto node = std::make_unique<Node>();
                node->kind = NodeKind::memory;
                node->right = std::move(address);
                return node;
            }
            if (token.text == "symbol") {
                if (current_.kind != TokenKind::left_parenthesis) {
                    fail(ExpressionCompileError::invalid_syntax, current_.offset);
                    return nullptr;
                }
                advance();
                if (current_.kind != TokenKind::string) {
                    fail(
                        current_.kind == TokenKind::invalid
                            ? ExpressionCompileError::invalid_token
                            : ExpressionCompileError::invalid_syntax,
                        current_.offset
                    );
                    return nullptr;
                }
                auto name = std::move(current_.text);
                advance();
                if (current_.kind != TokenKind::right_parenthesis) {
                    fail(ExpressionCompileError::invalid_syntax, current_.offset);
                    return nullptr;
                }
                advance();
                if (!add_node(token.offset, depth)) {
                    return nullptr;
                }
                auto node = std::make_unique<Node>();
                node->kind = NodeKind::debug_symbol;
                node->debug_symbol = std::move(name);
                return node;
            }
            Symbol symbol;
            if (!symbol_from_name(token.text, symbol)) {
                fail(ExpressionCompileError::unknown_identifier, token.offset);
                return nullptr;
            }
            if (!add_node(token.offset, depth)) {
                return nullptr;
            }
            auto node = std::make_unique<Node>();
            node->kind = NodeKind::symbol;
            node->symbol = symbol;
            return node;
        }
        if (token.kind == TokenKind::left_parenthesis) {
            advance();
            auto node = parse_binary(0, depth + 1U);
            if (node == nullptr || current_.kind != TokenKind::right_parenthesis) {
                if (node != nullptr) {
                    fail(ExpressionCompileError::invalid_syntax, current_.offset);
                }
                return nullptr;
            }
            advance();
            return node;
        }
        fail(ExpressionCompileError::invalid_syntax, token.offset);
        return nullptr;
    }

    Lexer lexer_;
    ExpressionCompileDiagnostic& diagnostic_;
    Token current_;
    std::size_t node_count_{};
};

ExpressionEvaluationResult evaluation_error(
    ExpressionEvaluationError error
) noexcept {
    ExpressionEvaluationResult result;
    result.error = error;
    return result;
}

ExpressionEvaluationResult state_error(core::CpuStatePart part) noexcept {
    auto result = evaluation_error(ExpressionEvaluationError::unknown_state);
    result.state_fault = part;
    return result;
}

ExpressionEvaluationResult value_result(std::uint64_t value) noexcept {
    ExpressionEvaluationResult result;
    result.value = value;
    return result;
}

ExpressionEvaluationResult evaluate_symbol(
    Symbol symbol,
    const core::CpuState& state
) noexcept {
    using core::ConditionCode;
    using core::CpuRegister;
    switch (symbol) {
    case Symbol::pc:
        return state.knowledge.knows(CpuRegister::program_counter)
            ? value_result(state.pc)
            : state_error(core::CpuStatePart::program_counter);
    case Symbol::sp:
        return state.knowledge.knows(CpuRegister::stack_pointer)
            ? value_result(state.sp)
            : state_error(core::CpuStatePart::stack_pointer);
    case Symbol::x:
        return state.knowledge.knows(CpuRegister::index_register)
            ? value_result(state.x)
            : state_error(core::CpuStatePart::index_register);
    case Symbol::a:
        return state.knowledge.knows(CpuRegister::accumulator_a)
            ? value_result(state.a)
            : state_error(core::CpuStatePart::accumulator_a);
    case Symbol::b:
        return state.knowledge.knows(CpuRegister::accumulator_b)
            ? value_result(state.b)
            : state_error(core::CpuStatePart::accumulator_b);
    case Symbol::ccr:
        return (state.knowledge.condition_code & 0x3FU) == 0x3FU
            ? value_result(state.condition_code)
            : state_error(core::CpuStatePart::condition_code);
    case Symbol::cycles:
        return value_result(state.cycle_count);
    case Symbol::h:
    case Symbol::i:
    case Symbol::n:
    case Symbol::z:
    case Symbol::v:
    case Symbol::c:
        break;
    }
    const auto flag = symbol == Symbol::h ? ConditionCode::half_carry
        : symbol == Symbol::i          ? ConditionCode::interrupt_mask
        : symbol == Symbol::n          ? ConditionCode::negative
        : symbol == Symbol::z          ? ConditionCode::zero
        : symbol == Symbol::v          ? ConditionCode::overflow
                                       : ConditionCode::carry;
    const auto mask = core::condition_mask(flag);
    if ((state.knowledge.condition_code & mask) == 0U) {
        return state_error(core::CpuStatePart::condition_code);
    }
    return value_result((state.condition_code & mask) == 0U ? 0U : 1U);
}

ExpressionEvaluationResult evaluate_node(
    const Node& node,
    const core::Machine& machine,
    const ExpressionSymbolResolver* symbol_resolver
) noexcept {
    switch (node.kind) {
    case NodeKind::literal:
        return value_result(node.literal);
    case NodeKind::symbol:
        return evaluate_symbol(node.symbol, machine.cpu().state());
    case NodeKind::debug_symbol: {
        if (symbol_resolver == nullptr) {
            return evaluation_error(ExpressionEvaluationError::symbol_not_found);
        }
        const auto resolved = symbol_resolver->resolve(node.debug_symbol);
        switch (resolved.status) {
        case ExpressionSymbolLookupStatus::found:
            return value_result(resolved.value);
        case ExpressionSymbolLookupStatus::not_found:
            return evaluation_error(ExpressionEvaluationError::symbol_not_found);
        case ExpressionSymbolLookupStatus::ambiguous:
            return evaluation_error(ExpressionEvaluationError::ambiguous_symbol);
        }
        return evaluation_error(ExpressionEvaluationError::symbol_not_found);
    }
    case NodeKind::memory: {
        const auto address = evaluate_node(
            *node.right,
            machine,
            symbol_resolver
        );
        if (!address.succeeded()) {
            return address;
        }
        if (address.value > 0xFFFFU) {
            return evaluation_error(
                ExpressionEvaluationError::address_out_of_range
            );
        }
        const auto narrowed = static_cast<std::uint16_t>(address.value);
        const auto read = machine.inspect8(narrowed);
        if (!read.succeeded()) {
            auto result = evaluation_error(
                ExpressionEvaluationError::memory_access
            );
            result.bus_fault = read.fault;
            result.fault_address = narrowed;
            return result;
        }
        return value_result(*read.value);
    }
    case NodeKind::unary: {
        const auto operand = evaluate_node(
            *node.right,
            machine,
            symbol_resolver
        );
        if (!operand.succeeded()) {
            return operand;
        }
        switch (node.operation) {
        case TokenKind::plus:
            return operand;
        case TokenKind::minus:
            return value_result(0U - operand.value);
        case TokenKind::bitwise_not:
            return value_result(~operand.value);
        case TokenKind::logical_not:
            return value_result(operand.value == 0U ? 1U : 0U);
        default:
            return evaluation_error(ExpressionEvaluationError::invalid_shift);
        }
    }
    case NodeKind::binary:
        break;
    }

    const auto left = evaluate_node(*node.left, machine, symbol_resolver);
    if (!left.succeeded()) {
        return left;
    }
    if (node.operation == TokenKind::logical_and && left.value == 0U) {
        return value_result(0U);
    }
    if (node.operation == TokenKind::logical_or && left.value != 0U) {
        return value_result(1U);
    }
    const auto right = evaluate_node(*node.right, machine, symbol_resolver);
    if (!right.succeeded()) {
        return right;
    }
    switch (node.operation) {
    case TokenKind::plus:
        return value_result(left.value + right.value);
    case TokenKind::minus:
        return value_result(left.value - right.value);
    case TokenKind::multiply:
        return value_result(left.value * right.value);
    case TokenKind::divide:
        return right.value == 0U
            ? evaluation_error(ExpressionEvaluationError::division_by_zero)
            : value_result(left.value / right.value);
    case TokenKind::remainder:
        return right.value == 0U
            ? evaluation_error(ExpressionEvaluationError::division_by_zero)
            : value_result(left.value % right.value);
    case TokenKind::shift_left:
    case TokenKind::shift_right:
        if (right.value >= std::numeric_limits<std::uint64_t>::digits) {
            return evaluation_error(ExpressionEvaluationError::invalid_shift);
        }
        return value_result(node.operation == TokenKind::shift_left
            ? left.value << right.value
            : left.value >> right.value);
    case TokenKind::less:
        return value_result(left.value < right.value ? 1U : 0U);
    case TokenKind::less_equal:
        return value_result(left.value <= right.value ? 1U : 0U);
    case TokenKind::greater:
        return value_result(left.value > right.value ? 1U : 0U);
    case TokenKind::greater_equal:
        return value_result(left.value >= right.value ? 1U : 0U);
    case TokenKind::equal:
        return value_result(left.value == right.value ? 1U : 0U);
    case TokenKind::not_equal:
        return value_result(left.value != right.value ? 1U : 0U);
    case TokenKind::bitwise_and:
        return value_result(left.value & right.value);
    case TokenKind::bitwise_xor:
        return value_result(left.value ^ right.value);
    case TokenKind::bitwise_or:
        return value_result(left.value | right.value);
    case TokenKind::logical_and:
        return value_result(right.value == 0U ? 0U : 1U);
    case TokenKind::logical_or:
        return value_result(right.value == 0U ? 0U : 1U);
    default:
        return evaluation_error(ExpressionEvaluationError::invalid_shift);
    }
}

}  // namespace

class CompiledExpression::Impl final {
public:
    explicit Impl(std::unique_ptr<Node> root) noexcept
        : root_(std::move(root)) {}

    [[nodiscard]] ExpressionEvaluationResult evaluate(
        const core::Machine& machine,
        const ExpressionSymbolResolver* symbol_resolver
    ) const noexcept {
        return evaluate_node(*root_, machine, symbol_resolver);
    }

private:
    std::unique_ptr<Node> root_;
};

CompiledExpression::CompiledExpression(std::unique_ptr<Impl> impl) noexcept
    : impl_(std::move(impl)) {}

CompiledExpression::~CompiledExpression() = default;
CompiledExpression::CompiledExpression(CompiledExpression&&) noexcept = default;
CompiledExpression& CompiledExpression::operator=(CompiledExpression&&) noexcept = default;

ExpressionEvaluationResult CompiledExpression::evaluate(
    const core::Machine& machine,
    const ExpressionSymbolResolver* symbol_resolver
) const noexcept {
    return impl_->evaluate(machine, symbol_resolver);
}

std::unique_ptr<CompiledExpression> compile_expression(
    std::string_view text,
    ExpressionCompileDiagnostic& diagnostic
) {
    diagnostic = {};
    if (text.empty()) {
        diagnostic.error = ExpressionCompileError::empty;
        return nullptr;
    }
    if (text.size() > kMaximumExpressionLength) {
        diagnostic = {
            ExpressionCompileError::too_long,
            kMaximumExpressionLength,
        };
        return nullptr;
    }
    if (const auto nul = text.find('\0'); nul != std::string_view::npos) {
        diagnostic = {ExpressionCompileError::invalid_token, nul};
        return nullptr;
    }
    Parser parser{text, diagnostic};
    auto root = parser.parse();
    if (root == nullptr) {
        if (diagnostic.succeeded()) {
            diagnostic.error = ExpressionCompileError::invalid_syntax;
        }
        return nullptr;
    }
    return std::unique_ptr<CompiledExpression>{new CompiledExpression{
        std::make_unique<CompiledExpression::Impl>(std::move(root)),
    }};
}

}  // namespace jr800::debugger
