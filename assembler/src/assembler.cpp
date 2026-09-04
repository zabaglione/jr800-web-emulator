// SPDX-License-Identifier: MIT

#include "jr800/assembler/assembler.hpp"

#include <algorithm>
#include <bit>
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "jr800/isa/instruction_metadata.hpp"
#include "jr800/formats/sha256.hpp"

namespace jr800::assembler {
namespace {

struct Location {
    std::size_t line{1};
    std::size_t column{1};
};

enum class TokenKind {
    end,
    line_end,
    invalid,
    identifier,
    number,
    comma,
    colon,
    hash,
    left_parenthesis,
    right_parenthesis,
    plus,
    minus,
    star,
    slash,
    ampersand,
    pipe,
    caret,
    tilde,
    shift_left,
    shift_right,
};

struct Token {
    TokenKind kind{TokenKind::invalid};
    std::string text;
    std::int64_t number{};
    Location location;
};

void add_diagnostic(
    std::vector<Diagnostic>& diagnostics,
    const Source& source,
    std::string code,
    std::string message,
    Location location
) {
    diagnostics.push_back(Diagnostic{
        std::move(code),
        std::move(message),
        source.logical_path,
        location.line,
        location.column,
    });
}

class Lexer {
public:
    Lexer(const Source& source, std::vector<Diagnostic>& diagnostics)
        : source_(source), diagnostics_(diagnostics) {}

    [[nodiscard]] std::vector<Token> tokenize() {
        std::vector<Token> tokens;
        while (true) {
            auto token = next();
            const auto kind = token.kind;
            tokens.push_back(std::move(token));
            if (kind == TokenKind::end) {
                break;
            }
        }
        return tokens;
    }

private:
    [[nodiscard]] char peek(std::size_t lookahead = 0) const noexcept {
        const auto position = index_ + lookahead;
        return position < source_.text.size() ? source_.text[position] : '\0';
    }

    [[nodiscard]] Location location() const noexcept {
        return Location{line_, column_};
    }

    char advance() {
        const auto value = peek();
        if (value != '\0') {
            ++index_;
            ++column_;
        }
        return value;
    }

    void consume_newline() {
        if (peek() == '\r') {
            ++index_;
            if (peek() == '\n') {
                ++index_;
            }
        } else {
            ++index_;
        }
        ++line_;
        column_ = 1;
    }

    static bool is_identifier_start(char value) {
        return std::isalpha(static_cast<unsigned char>(value)) != 0 || value == '_'
            || value == '.';
    }

    static bool is_identifier_continue(char value) {
        return std::isalnum(static_cast<unsigned char>(value)) != 0 || value == '_'
            || value == '.';
    }

    static int digit_value(char value) {
        if (value >= '0' && value <= '9') {
            return value - '0';
        }
        if (value >= 'a' && value <= 'f') {
            return value - 'a' + 10;
        }
        if (value >= 'A' && value <= 'F') {
            return value - 'A' + 10;
        }
        return -1;
    }

    Token number(int base, std::size_t prefix_length) {
        const auto start = location();
        const auto start_index = index_;
        for (std::size_t count = 0; count < prefix_length; ++count) {
            advance();
        }

        std::uint64_t value = 0;
        std::size_t digits = 0;
        while (true) {
            const auto digit = digit_value(peek());
            if (digit < 0 || digit >= base) {
                break;
            }
            if (value
                > (static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())
                   - static_cast<std::uint64_t>(digit))
                    / static_cast<std::uint64_t>(base)) {
                while (digit_value(peek()) >= 0) {
                    advance();
                }
                add_diagnostic(
                    diagnostics_,
                    source_,
                    "E1002",
                    "numeric literal exceeds signed 64-bit range",
                    start
                );
                return Token{
                    TokenKind::invalid,
                    source_.text.substr(start_index, index_ - start_index),
                    0,
                    start,
                };
            }
            value = value * static_cast<std::uint64_t>(base)
                + static_cast<std::uint64_t>(digit);
            ++digits;
            advance();
        }
        if (digits == 0) {
            add_diagnostic(
                diagnostics_,
                source_,
                "E1003",
                "numeric prefix requires at least one digit",
                start
            );
            return Token{
                TokenKind::invalid,
                source_.text.substr(start_index, index_ - start_index),
                0,
                start,
            };
        }
        return Token{
            TokenKind::number,
            source_.text.substr(start_index, index_ - start_index),
            static_cast<std::int64_t>(value),
            start,
        };
    }

    Token next() {
        while (peek() == ' ' || peek() == '\t' || peek() == '\f') {
            advance();
        }
        if (peek() == ';') {
            while (peek() != '\0' && peek() != '\n' && peek() != '\r') {
                advance();
            }
        }

        const auto start = location();
        const auto value = peek();
        if (value == '\0') {
            return Token{TokenKind::end, {}, 0, start};
        }
        if (value == '\n' || value == '\r') {
            consume_newline();
            return Token{TokenKind::line_end, "\n", 0, start};
        }
        if (is_identifier_start(value)) {
            const auto start_index = index_;
            while (is_identifier_continue(peek())) {
                advance();
            }
            return Token{
                TokenKind::identifier,
                source_.text.substr(start_index, index_ - start_index),
                0,
                start,
            };
        }
        if (std::isdigit(static_cast<unsigned char>(value)) != 0) {
            return number(10, 0);
        }
        if (value == '$') {
            return number(16, 1);
        }
        if (value == '%' && (peek(1) == '0' || peek(1) == '1')) {
            return number(2, 1);
        }

        advance();
        switch (value) {
        case ',':
            return Token{TokenKind::comma, ",", 0, start};
        case ':':
            return Token{TokenKind::colon, ":", 0, start};
        case '#':
            return Token{TokenKind::hash, "#", 0, start};
        case '(':
            return Token{TokenKind::left_parenthesis, "(", 0, start};
        case ')':
            return Token{TokenKind::right_parenthesis, ")", 0, start};
        case '+':
            return Token{TokenKind::plus, "+", 0, start};
        case '-':
            return Token{TokenKind::minus, "-", 0, start};
        case '*':
            return Token{TokenKind::star, "*", 0, start};
        case '/':
            return Token{TokenKind::slash, "/", 0, start};
        case '&':
            return Token{TokenKind::ampersand, "&", 0, start};
        case '|':
            return Token{TokenKind::pipe, "|", 0, start};
        case '^':
            return Token{TokenKind::caret, "^", 0, start};
        case '~':
            return Token{TokenKind::tilde, "~", 0, start};
        case '<':
            if (peek() == '<') {
                advance();
                return Token{TokenKind::shift_left, "<<", 0, start};
            }
            break;
        case '>':
            if (peek() == '>') {
                advance();
                return Token{TokenKind::shift_right, ">>", 0, start};
            }
            break;
        default:
            break;
        }

        add_diagnostic(
            diagnostics_,
            source_,
            "E1001",
            "unexpected character",
            start
        );
        return Token{TokenKind::invalid, std::string(1, value), 0, start};
    }

    const Source& source_;
    std::vector<Diagnostic>& diagnostics_;
    std::size_t index_{};
    std::size_t line_{1};
    std::size_t column_{1};
};

struct Expression {
    enum class Kind {
        literal,
        symbol,
        unary,
        binary,
    };

    Kind kind{Kind::literal};
    Location location;
    std::int64_t literal{};
    std::string symbol;
    TokenKind operation{TokenKind::invalid};
    std::unique_ptr<Expression> left;
    std::unique_ptr<Expression> right;
};

enum class StatementKind {
    section,
    binding_local,
    binding_global,
    external,
    absolute_symbol,
    label,
    bytes,
    words,
    space,
    instruction,
};

enum class OperandShape {
    none,
    immediate,
    plain,
    indexed,
    immediate_then_plain,
    immediate_then_indexed,
};

enum class ValueConstraint {
    relocation_default,
    unsigned8,
};

struct Statement {
    StatementKind kind{StatementKind::label};
    Location location;
    std::string source_text;
    std::string name;
    std::string qualifier;
    OperandShape operand_shape{OperandShape::none};
    std::vector<std::unique_ptr<Expression>> expressions;
};

class AbortLine final : public std::exception {};

std::string lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

std::string upper_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::toupper(character));
    });
    return value;
}

std::vector<std::string> split_source_lines(std::string_view source) {
    std::vector<std::string> lines;
    std::size_t begin = 0;
    for (std::size_t index = 0; index < source.size(); ++index) {
        if (source[index] == '\n' || source[index] == '\r') {
            lines.emplace_back(source.substr(begin, index - begin));
            if (source[index] == '\r' && index + 1U < source.size()
                && source[index + 1U] == '\n') {
                ++index;
            }
            begin = index + 1U;
        }
    }
    if (begin < source.size() || source.empty()) {
        lines.emplace_back(source.substr(begin));
    }
    return lines;
}

class Parser {
public:
    Parser(
        const Source& source,
        std::vector<Token> tokens,
        std::vector<Diagnostic>& diagnostics
    )
        : source_(source),
          tokens_(std::move(tokens)),
          diagnostics_(diagnostics),
          source_lines_(split_source_lines(source.text)) {}

    [[nodiscard]] std::vector<Statement> parse() {
        std::vector<Statement> statements;
        while (current().kind != TokenKind::end) {
            if (match(TokenKind::line_end)) {
                continue;
            }
            try {
                parse_line(statements);
            } catch (const AbortLine&) {
                synchronize_line();
            }
        }
        return statements;
    }

private:
    static constexpr std::size_t kMaxExpressionDepth = 128U;
    static constexpr std::size_t kMaxExpressionNodes = 256U;

    [[nodiscard]] const Token& current(std::size_t lookahead = 0) const {
        const auto position = std::min(index_ + lookahead, tokens_.size() - 1U);
        return tokens_[position];
    }

    bool match(TokenKind kind) {
        if (current().kind != kind) {
            return false;
        }
        ++index_;
        return true;
    }

    const Token& consume(TokenKind kind, std::string message) {
        if (current().kind != kind) {
            error(current().location, std::move(message));
        }
        return tokens_[index_++];
    }

    [[noreturn]] void error(
        Location location,
        std::string code,
        std::string message
    ) {
        add_diagnostic(
            diagnostics_,
            source_,
            std::move(code),
            std::move(message),
            location
        );
        throw AbortLine{};
    }

    [[noreturn]] void error(Location location, std::string message) {
        error(location, "E2001", std::move(message));
    }

    void require_expression_depth(Location location, std::size_t depth) {
        if (depth > kMaxExpressionDepth) {
            error(location, "E2002", "expression nesting exceeds the parser limit");
        }
    }

    void add_expression_node(Location location, std::size_t& node_count) {
        if (node_count >= kMaxExpressionNodes) {
            error(location, "E2002", "expression size exceeds the parser limit");
        }
        ++node_count;
    }

    void synchronize_line() {
        while (current().kind != TokenKind::line_end && current().kind != TokenKind::end) {
            ++index_;
        }
        match(TokenKind::line_end);
    }

    [[nodiscard]] std::string source_line(std::size_t line) const {
        return line > 0U && line <= source_lines_.size() ? source_lines_[line - 1U] : "";
    }

    void require_line_end() {
        if (current().kind != TokenKind::line_end && current().kind != TokenKind::end) {
            error(current().location, "unexpected token after statement");
        }
        match(TokenKind::line_end);
    }

    void parse_line(std::vector<Statement>& statements) {
        if (current().kind == TokenKind::invalid) {
            throw AbortLine{};
        }
        if (current().kind == TokenKind::identifier
            && current(1).kind == TokenKind::colon) {
            const auto label = current();
            index_ += 2U;
            Statement label_statement;
            label_statement.kind = StatementKind::label;
            label_statement.location = label.location;
            label_statement.source_text = source_line(label.location.line);
            label_statement.name = label.text;
            statements.push_back(std::move(label_statement));
            if (current().kind == TokenKind::line_end || current().kind == TokenKind::end) {
                require_line_end();
                return;
            }
        }

        const auto head = consume(TokenKind::identifier, "expected a directive or instruction");
        Statement statement;
        statement.location = head.location;
        statement.source_text = source_line(head.location.line);
        if (!head.text.empty() && head.text.front() == '.') {
            parse_directive(lower_ascii(head.text), statement);
        } else {
            parse_instruction(upper_ascii(head.text), statement);
        }
        require_line_end();
        statements.push_back(std::move(statement));
    }

    void parse_directive(const std::string& directive, Statement& statement) {
        if (directive == ".section") {
            statement.kind = StatementKind::section;
            statement.name = consume(TokenKind::identifier, "expected section name").text;
            consume(TokenKind::comma, "expected ',' before section class");
            statement.qualifier = lower_ascii(
                consume(TokenKind::identifier, "expected section class").text
            );
            return;
        }
        if (directive == ".local" || directive == ".global" || directive == ".extern") {
            statement.kind = directive == ".local" ? StatementKind::binding_local
                : directive == ".global"           ? StatementKind::binding_global
                                                    : StatementKind::external;
            statement.name = consume(TokenKind::identifier, "expected symbol name").text;
            return;
        }
        if (directive == ".equ") {
            statement.kind = StatementKind::absolute_symbol;
            statement.name = consume(TokenKind::identifier, "expected symbol name").text;
            consume(TokenKind::comma, "expected ',' before expression");
            statement.expressions.push_back(parse_expression());
            return;
        }
        if (directive == ".byte" || directive == ".word") {
            statement.kind = directive == ".byte" ? StatementKind::bytes : StatementKind::words;
            statement.expressions.push_back(parse_expression());
            while (match(TokenKind::comma)) {
                statement.expressions.push_back(parse_expression());
            }
            return;
        }
        if (directive == ".space") {
            statement.kind = StatementKind::space;
            statement.expressions.push_back(parse_expression());
            return;
        }
        error(statement.location, "unknown directive: " + directive);
    }

    void parse_instruction(std::string mnemonic, Statement& statement) {
        statement.kind = StatementKind::instruction;
        statement.name = std::move(mnemonic);
        if (current().kind == TokenKind::line_end || current().kind == TokenKind::end) {
            statement.operand_shape = OperandShape::none;
            return;
        }
        if (match(TokenKind::hash)) {
            statement.operand_shape = OperandShape::immediate;
            statement.expressions.push_back(parse_expression());
            if (match(TokenKind::comma)) {
                statement.operand_shape = OperandShape::immediate_then_plain;
                statement.expressions.push_back(parse_expression());
                if (match(TokenKind::comma)) {
                    const auto index = consume(
                        TokenKind::identifier,
                        "expected X after indexed displacement"
                    );
                    if (upper_ascii(index.text) != "X") {
                        error(index.location, "indexed operand must end with X");
                    }
                    statement.operand_shape = OperandShape::immediate_then_indexed;
                }
            }
            return;
        }
        statement.operand_shape = OperandShape::plain;
        statement.expressions.push_back(parse_expression());
        if (match(TokenKind::comma)) {
            const auto index = consume(
                TokenKind::identifier,
                "expected X after indexed displacement"
            );
            if (upper_ascii(index.text) != "X") {
                error(index.location, "indexed operand must end with X");
            }
            statement.operand_shape = OperandShape::indexed;
        }
    }

    static int precedence(TokenKind kind) {
        switch (kind) {
        case TokenKind::star:
        case TokenKind::slash:
            return 70;
        case TokenKind::plus:
        case TokenKind::minus:
            return 60;
        case TokenKind::shift_left:
        case TokenKind::shift_right:
            return 50;
        case TokenKind::ampersand:
            return 40;
        case TokenKind::caret:
            return 30;
        case TokenKind::pipe:
            return 20;
        default:
            return -1;
        }
    }

    std::unique_ptr<Expression> parse_expression() {
        std::size_t node_count = 0U;
        return parse_binary_expression(0, 0U, node_count);
    }

    std::unique_ptr<Expression> parse_binary_expression(
        int minimum_precedence,
        std::size_t depth,
        std::size_t& node_count
    ) {
        require_expression_depth(current().location, depth);
        auto left = parse_unary(depth, node_count);
        while (true) {
            const auto operator_precedence = precedence(current().kind);
            if (operator_precedence < minimum_precedence) {
                break;
            }
            const auto operation = current();
            ++index_;
            auto right = parse_binary_expression(
                operator_precedence + 1,
                depth + 1U,
                node_count
            );
            add_expression_node(operation.location, node_count);
            auto expression = std::make_unique<Expression>();
            expression->kind = Expression::Kind::binary;
            expression->location = operation.location;
            expression->operation = operation.kind;
            expression->left = std::move(left);
            expression->right = std::move(right);
            left = std::move(expression);
        }
        return left;
    }

    std::unique_ptr<Expression> parse_unary(
        std::size_t depth,
        std::size_t& node_count
    ) {
        require_expression_depth(current().location, depth);
        if (current().kind == TokenKind::plus || current().kind == TokenKind::minus
            || current().kind == TokenKind::tilde) {
            const auto operation = current();
            ++index_;
            add_expression_node(operation.location, node_count);
            auto expression = std::make_unique<Expression>();
            expression->kind = Expression::Kind::unary;
            expression->location = operation.location;
            expression->operation = operation.kind;
            expression->right = parse_unary(depth + 1U, node_count);
            return expression;
        }
        return parse_primary(depth, node_count);
    }

    std::unique_ptr<Expression> parse_primary(
        std::size_t depth,
        std::size_t& node_count
    ) {
        require_expression_depth(current().location, depth);
        const auto token = current();
        if (match(TokenKind::number)) {
            add_expression_node(token.location, node_count);
            auto expression = std::make_unique<Expression>();
            expression->kind = Expression::Kind::literal;
            expression->location = token.location;
            expression->literal = token.number;
            return expression;
        }
        if (match(TokenKind::identifier)) {
            add_expression_node(token.location, node_count);
            auto expression = std::make_unique<Expression>();
            expression->kind = Expression::Kind::symbol;
            expression->location = token.location;
            expression->symbol = token.text;
            return expression;
        }
        if (match(TokenKind::left_parenthesis)) {
            auto expression = parse_binary_expression(0, depth + 1U, node_count);
            consume(TokenKind::right_parenthesis, "expected ')'");
            return expression;
        }
        error(token.location, "expected expression");
    }

    const Source& source_;
    std::vector<Token> tokens_;
    std::vector<Diagnostic>& diagnostics_;
    std::vector<std::string> source_lines_;
    std::size_t index_{};
};

struct ExpressionValue {
    std::optional<std::string> symbol;
    std::int64_t addend{};
};

class EvaluationError final : public std::runtime_error {
public:
    EvaluationError(Location location, std::string message)
        : std::runtime_error(std::move(message)), location_(location) {}

    [[nodiscard]] Location location() const noexcept {
        return location_;
    }

private:
    Location location_;
};

std::int64_t checked_add(std::int64_t left, std::int64_t right, Location location) {
    if ((right > 0 && left > std::numeric_limits<std::int64_t>::max() - right)
        || (right < 0 && left < std::numeric_limits<std::int64_t>::min() - right)) {
        throw EvaluationError(location, "expression addition overflow");
    }
    return left + right;
}

std::int64_t checked_subtract(std::int64_t left, std::int64_t right, Location location) {
    if ((right < 0 && left > std::numeric_limits<std::int64_t>::max() + right)
        || (right > 0 && left < std::numeric_limits<std::int64_t>::min() + right)) {
        throw EvaluationError(location, "expression subtraction overflow");
    }
    return left - right;
}

std::int64_t checked_multiply(std::int64_t left, std::int64_t right, Location location) {
    if (left == 0 || right == 0) {
        return 0;
    }
    if ((left == -1 && right == std::numeric_limits<std::int64_t>::min())
        || (right == -1 && left == std::numeric_limits<std::int64_t>::min())) {
        throw EvaluationError(location, "expression multiplication overflow");
    }
    if (left > 0) {
        if ((right > 0 && left > std::numeric_limits<std::int64_t>::max() / right)
            || (right < 0 && right < std::numeric_limits<std::int64_t>::min() / left)) {
            throw EvaluationError(location, "expression multiplication overflow");
        }
    } else if ((right > 0 && left < std::numeric_limits<std::int64_t>::min() / right)
               || (right < 0 && left < std::numeric_limits<std::int64_t>::max() / right)) {
        throw EvaluationError(location, "expression multiplication overflow");
    }
    return left * right;
}

template <typename SymbolResolver>
ExpressionValue evaluate_expression(const Expression& expression, SymbolResolver&& resolve_symbol) {
    switch (expression.kind) {
    case Expression::Kind::literal:
        return ExpressionValue{std::nullopt, expression.literal};
    case Expression::Kind::symbol:
        return resolve_symbol(expression.symbol, expression.location);
    case Expression::Kind::unary: {
        auto value = evaluate_expression(*expression.right, resolve_symbol);
        if (value.symbol.has_value() && expression.operation != TokenKind::plus) {
            throw EvaluationError(
                expression.location,
                "only unary '+' is valid for a relocatable expression"
            );
        }
        switch (expression.operation) {
        case TokenKind::plus:
            return value;
        case TokenKind::minus:
            if (value.addend == std::numeric_limits<std::int64_t>::min()) {
                throw EvaluationError(expression.location, "expression negation overflow");
            }
            value.addend = -value.addend;
            return value;
        case TokenKind::tilde:
            value.addend = ~value.addend;
            return value;
        default:
            throw EvaluationError(expression.location, "invalid unary operator");
        }
    }
    case Expression::Kind::binary:
        break;
    }

    auto left = evaluate_expression(*expression.left, resolve_symbol);
    auto right = evaluate_expression(*expression.right, resolve_symbol);
    if (left.symbol.has_value() || right.symbol.has_value()) {
        if (expression.operation == TokenKind::plus) {
            if (left.symbol.has_value() && !right.symbol.has_value()) {
                left.addend = checked_add(left.addend, right.addend, expression.location);
                return left;
            }
            if (!left.symbol.has_value() && right.symbol.has_value()) {
                right.addend = checked_add(right.addend, left.addend, expression.location);
                return right;
            }
        }
        if (expression.operation == TokenKind::minus && left.symbol.has_value()
            && !right.symbol.has_value()) {
            left.addend = checked_subtract(left.addend, right.addend, expression.location);
            return left;
        }
        throw EvaluationError(
            expression.location,
            "relocatable expression must be one symbol plus or minus a constant"
        );
    }

    switch (expression.operation) {
    case TokenKind::plus:
        return {std::nullopt, checked_add(left.addend, right.addend, expression.location)};
    case TokenKind::minus:
        return {
            std::nullopt,
            checked_subtract(left.addend, right.addend, expression.location),
        };
    case TokenKind::star:
        return {
            std::nullopt,
            checked_multiply(left.addend, right.addend, expression.location),
        };
    case TokenKind::slash:
        if (right.addend == 0) {
            throw EvaluationError(expression.location, "division by zero");
        }
        if (left.addend == std::numeric_limits<std::int64_t>::min()
            && right.addend == -1) {
            throw EvaluationError(expression.location, "expression division overflow");
        }
        return {std::nullopt, left.addend / right.addend};
    case TokenKind::shift_left:
    case TokenKind::shift_right: {
        if (right.addend < 0 || right.addend > 63) {
            throw EvaluationError(expression.location, "shift count must be from 0 through 63");
        }
        const auto shift = static_cast<unsigned int>(right.addend);
        if (expression.operation == TokenKind::shift_left) {
            const auto bits = std::bit_cast<std::uint64_t>(left.addend) << shift;
            return {std::nullopt, std::bit_cast<std::int64_t>(bits)};
        }
        return {std::nullopt, left.addend >> shift};
    }
    case TokenKind::ampersand:
        return {std::nullopt, left.addend & right.addend};
    case TokenKind::caret:
        return {std::nullopt, left.addend ^ right.addend};
    case TokenKind::pipe:
        return {std::nullopt, left.addend | right.addend};
    default:
        throw EvaluationError(expression.location, "invalid binary operator");
    }
}

enum class SymbolState {
    declared,
    section,
    absolute,
    external,
};

struct SymbolBuilder {
    std::string name;
    formats::jro::SymbolBinding binding{formats::jro::SymbolBinding::local};
    bool binding_explicit{};
    SymbolState state{SymbolState::declared};
    std::optional<std::uint32_t> section_index;
    std::uint32_t value{};
    Location location;
};

struct SectionBuilder {
    std::string name;
    std::string section_class;
    formats::jro::SectionType type{formats::jro::SectionType::program_bits};
    formats::jro::SectionAttributes attributes{formats::jro::SectionAttributes::none};
    std::uint32_t logical_size{};
};

struct PassInfo {
    std::optional<std::uint32_t> section_index;
    std::uint32_t offset{};
    std::uint32_t size{};
    const isa::InstructionMetadata* instruction{};
};

class AssemblerEngine {
public:
    AssemblerEngine(
        const Source& source,
        const Options& options,
        std::vector<Diagnostic>& diagnostics
    )
        : source_(source), options_(options), diagnostics_(diagnostics) {}

    [[nodiscard]] std::optional<Output> run(std::vector<Statement> statements) {
        statements_ = std::move(statements);
        if (!select_profile()) {
            return std::nullopt;
        }
        first_pass();
        if (!diagnostics_.empty()) {
            return std::nullopt;
        }
        auto output = second_pass();
        if (!diagnostics_.empty()) {
            return std::nullopt;
        }
        return output;
    }

private:
    bool select_profile() {
        const auto selected = isa::find_profile(options_.target_profile);
        if (!selected.has_value()) {
            diagnose(
                Location{1, 1},
                "E3001",
                "unknown target profile: " + options_.target_profile
            );
            return false;
        }
        profile_ = *selected;
        const auto has_instructions = std::any_of(
            isa::all_instructions().begin(),
            isa::all_instructions().end(),
            [&](const auto& instruction) {
                return isa::instruction_applies_to(instruction, *profile_);
            }
        );
        if (!has_instructions) {
            diagnose(
                Location{1, 1},
                "E3002",
                "target profile has no reviewed instruction metadata: "
                    + options_.target_profile
            );
            return false;
        }
        if (source_.logical_path.empty()) {
            diagnose(Location{1, 1}, "E3003", "source logical path must not be empty");
            return false;
        }
        if (options_.producer_version.empty()) {
            diagnose(Location{1, 1}, "E3004", "producer version must not be empty");
            return false;
        }
        return true;
    }

    void diagnose(Location location, std::string code, std::string message) {
        add_diagnostic(
            diagnostics_,
            source_,
            std::move(code),
            std::move(message),
            location
        );
    }

    std::uint32_t touch_symbol(const std::string& name, Location location) {
        const auto found = symbol_indexes_.find(name);
        if (found != symbol_indexes_.end()) {
            return found->second;
        }
        const auto index = static_cast<std::uint32_t>(symbols_.size());
        symbol_indexes_.emplace(name, index);
        symbols_.push_back(SymbolBuilder{
            name,
            formats::jro::SymbolBinding::local,
            false,
            SymbolState::declared,
            std::nullopt,
            0,
            location,
        });
        return index;
    }

    ExpressionValue resolve_symbol(const std::string& name, Location) const {
        const auto found = symbol_indexes_.find(name);
        if (found == symbol_indexes_.end()) {
            return ExpressionValue{name, 0};
        }
        const auto& symbol = symbols_[found->second];
        if (symbol.state == SymbolState::absolute) {
            return ExpressionValue{std::nullopt, symbol.value};
        }
        return ExpressionValue{name, 0};
    }

    std::optional<std::int64_t> absolute_value(const Expression& expression) {
        try {
            const auto value = evaluate_expression(
                expression,
                [&](const std::string& name, Location location) {
                    return resolve_symbol(name, location);
                }
            );
            if (value.symbol.has_value()) {
                throw EvaluationError(
                    expression.location,
                    "expression must be absolute at this point"
                );
            }
            return value.addend;
        } catch (const EvaluationError& error) {
            diagnose(error.location(), "E3101", error.what());
            return std::nullopt;
        }
    }

    std::optional<std::int64_t> tentative_absolute_value(
        const Expression& expression
    ) const {
        try {
            const auto value = evaluate_expression(
                expression,
                [&](const std::string& name, Location location) {
                    return resolve_symbol(name, location);
                }
            );
            if (!value.symbol.has_value()) {
                return value.addend;
            }
        } catch (const EvaluationError&) {
        }
        return std::nullopt;
    }

    std::optional<std::uint32_t> require_current_section(const Statement& statement) {
        if (!current_section_.has_value()) {
            diagnose(
                statement.location,
                "E3201",
                "statement requires a preceding .section directive"
            );
            return std::nullopt;
        }
        return current_section_;
    }

    void add_size(std::uint32_t section_index, std::uint64_t amount, Location location) {
        auto& section = sections_[section_index];
        const auto result = static_cast<std::uint64_t>(section.logical_size) + amount;
        if (result > 65'536U) {
            diagnose(location, "E3202", "section exceeds 65,536 bytes");
            return;
        }
        section.logical_size = static_cast<std::uint32_t>(result);
    }

    void select_section(const Statement& statement) {
        formats::jro::SectionType type;
        formats::jro::SectionAttributes attributes;
        if (statement.qualifier == "code") {
            type = formats::jro::SectionType::program_bits;
            attributes = formats::jro::SectionAttributes::allocate
                | formats::jro::SectionAttributes::execute;
        } else if (statement.qualifier == "data") {
            type = formats::jro::SectionType::program_bits;
            attributes = formats::jro::SectionAttributes::allocate
                | formats::jro::SectionAttributes::write;
        } else if (statement.qualifier == "bss") {
            type = formats::jro::SectionType::no_bits;
            attributes = formats::jro::SectionAttributes::allocate
                | formats::jro::SectionAttributes::write;
        } else {
            diagnose(statement.location, "E3203", "unknown section class: " + statement.qualifier);
            return;
        }

        const auto found = section_indexes_.find(statement.name);
        if (found != section_indexes_.end()) {
            auto& section = sections_[found->second];
            if (section.section_class != statement.qualifier) {
                diagnose(
                    statement.location,
                    "E3204",
                    "section cannot be reopened with a different class"
                );
                return;
            }
            current_section_ = found->second;
            return;
        }
        const auto index = static_cast<std::uint32_t>(sections_.size());
        section_indexes_.emplace(statement.name, index);
        sections_.push_back(SectionBuilder{
            statement.name,
            statement.qualifier,
            type,
            attributes,
            0,
        });
        current_section_ = index;
    }

    void declare_binding(
        const Statement& statement,
        formats::jro::SymbolBinding binding
    ) {
        auto& symbol = symbols_[touch_symbol(statement.name, statement.location)];
        if (symbol.state == SymbolState::external && binding == formats::jro::SymbolBinding::local) {
            diagnose(statement.location, "E3301", "external symbol cannot be local");
            return;
        }
        if (symbol.binding_explicit && symbol.binding != binding) {
            diagnose(statement.location, "E3302", "conflicting symbol binding declaration");
            return;
        }
        symbol.binding = binding;
        symbol.binding_explicit = true;
    }

    void declare_external(const Statement& statement) {
        auto& symbol = symbols_[touch_symbol(statement.name, statement.location)];
        if (symbol.binding_explicit && symbol.binding == formats::jro::SymbolBinding::local) {
            diagnose(statement.location, "E3301", "external symbol cannot be local");
            return;
        }
        if (symbol.state != SymbolState::declared && symbol.state != SymbolState::external) {
            diagnose(statement.location, "E3303", "defined symbol cannot be redeclared external");
            return;
        }
        symbol.binding = formats::jro::SymbolBinding::global;
        symbol.binding_explicit = true;
        symbol.state = SymbolState::external;
    }

    void define_label(const Statement& statement) {
        const auto section_index = require_current_section(statement);
        if (!section_index.has_value()) {
            return;
        }
        auto& symbol = symbols_[touch_symbol(statement.name, statement.location)];
        if (symbol.state != SymbolState::declared) {
            diagnose(statement.location, "E3304", "duplicate or conflicting symbol definition");
            return;
        }
        symbol.state = SymbolState::section;
        symbol.section_index = *section_index;
        symbol.value = sections_[*section_index].logical_size;
    }

    void define_absolute(const Statement& statement) {
        auto& symbol = symbols_[touch_symbol(statement.name, statement.location)];
        if (symbol.state != SymbolState::declared) {
            diagnose(statement.location, "E3304", "duplicate or conflicting symbol definition");
            return;
        }
        const auto value = absolute_value(*statement.expressions.front());
        if (!value.has_value()) {
            return;
        }
        if (*value < 0 || *value > 0xFFFF) {
            diagnose(statement.location, "E3305", "absolute symbol must fit 16 bits");
            return;
        }
        symbol.state = SymbolState::absolute;
        symbol.value = static_cast<std::uint32_t>(*value);
    }

    static bool shape_matches(OperandShape shape, isa::AddressingMode mode) {
        switch (shape) {
        case OperandShape::none:
            return mode == isa::AddressingMode::implied;
        case OperandShape::immediate:
            return mode == isa::AddressingMode::immediate8
                || mode == isa::AddressingMode::immediate16;
        case OperandShape::plain:
            return mode == isa::AddressingMode::direct8
                || mode == isa::AddressingMode::extended16
                || mode == isa::AddressingMode::relative8;
        case OperandShape::indexed:
            return mode == isa::AddressingMode::indexed8;
        case OperandShape::immediate_then_plain:
            return mode == isa::AddressingMode::immediate8_direct8;
        case OperandShape::immediate_then_indexed:
            return mode == isa::AddressingMode::immediate8_indexed8;
        }
        return false;
    }

    const isa::InstructionMetadata* select_instruction(const Statement& statement) {
        std::vector<const isa::InstructionMetadata*> matches;
        for (const auto& instruction : isa::all_instructions()) {
            if (instruction.mnemonic == statement.name
                && isa::instruction_applies_to(instruction, *profile_)
                && shape_matches(statement.operand_shape, instruction.addressing_mode)) {
                matches.push_back(&instruction);
            }
        }
        if (matches.empty()) {
            diagnose(
                statement.location,
                "E3401",
                "instruction form is unavailable for target profile: " + statement.name
            );
            return nullptr;
        }
        if (matches.size() == 2U && statement.expressions.size() == 1U) {
            const isa::InstructionMetadata* direct = nullptr;
            const isa::InstructionMetadata* extended = nullptr;
            for (const auto* instruction : matches) {
                if (instruction->addressing_mode == isa::AddressingMode::direct8) {
                    direct = instruction;
                } else if (
                    instruction->addressing_mode == isa::AddressingMode::extended16
                ) {
                    extended = instruction;
                }
            }
            if (direct != nullptr && extended != nullptr) {
                const auto absolute = tentative_absolute_value(
                    *statement.expressions.front()
                );
                return absolute.has_value() && *absolute >= 0 && *absolute <= 0xFF
                    ? direct
                    : extended;
            }
        }
        if (matches.size() != 1U) {
            diagnose(statement.location, "E3402", "instruction operand form is ambiguous");
            return nullptr;
        }
        return matches.front();
    }

    void first_pass() {
        pass_info_.resize(statements_.size());
        for (std::size_t index = 0; index < statements_.size(); ++index) {
            const auto& statement = statements_[index];
            auto& info = pass_info_[index];
            switch (statement.kind) {
            case StatementKind::section:
                select_section(statement);
                break;
            case StatementKind::binding_local:
                declare_binding(statement, formats::jro::SymbolBinding::local);
                break;
            case StatementKind::binding_global:
                declare_binding(statement, formats::jro::SymbolBinding::global);
                break;
            case StatementKind::external:
                declare_external(statement);
                break;
            case StatementKind::absolute_symbol:
                define_absolute(statement);
                break;
            case StatementKind::label:
                define_label(statement);
                break;
            case StatementKind::bytes:
            case StatementKind::words: {
                const auto section_index = require_current_section(statement);
                if (!section_index.has_value()) {
                    break;
                }
                info.section_index = section_index;
                info.offset = sections_[*section_index].logical_size;
                const auto width = statement.kind == StatementKind::bytes ? 1U : 2U;
                info.size = static_cast<std::uint32_t>(statement.expressions.size() * width);
                add_size(*section_index, info.size, statement.location);
                break;
            }
            case StatementKind::space: {
                const auto section_index = require_current_section(statement);
                if (!section_index.has_value()) {
                    break;
                }
                const auto amount = absolute_value(*statement.expressions.front());
                if (!amount.has_value()) {
                    break;
                }
                if (*amount < 0 || *amount > 65'536) {
                    diagnose(statement.location, "E3205", ".space size is out of range");
                    break;
                }
                info.section_index = section_index;
                info.offset = sections_[*section_index].logical_size;
                info.size = static_cast<std::uint32_t>(*amount);
                add_size(*section_index, info.size, statement.location);
                break;
            }
            case StatementKind::instruction: {
                const auto section_index = require_current_section(statement);
                if (!section_index.has_value()) {
                    break;
                }
                const auto* instruction = select_instruction(statement);
                if (instruction == nullptr) {
                    break;
                }
                info.section_index = section_index;
                info.offset = sections_[*section_index].logical_size;
                info.size = instruction->instruction_length;
                info.instruction = instruction;
                add_size(*section_index, info.size, statement.location);
                break;
            }
            }
        }

        for (const auto& symbol : symbols_) {
            if (symbol.state == SymbolState::declared) {
                diagnose(symbol.location, "E3306", "declared symbol was never defined");
            }
        }
    }

    ExpressionValue evaluate_final(const Expression& expression) {
        return evaluate_expression(
            expression,
            [&](const std::string& name, Location location) {
                return resolve_symbol(name, location);
            }
        );
    }

    void emit_expression(
        formats::jro::ObjectFile& object,
        const Expression& expression,
        std::uint32_t section_index,
        std::uint32_t offset,
        formats::jro::RelocationType relocation_type,
        ValueConstraint value_constraint = ValueConstraint::relocation_default
    ) {
        ExpressionValue value;
        try {
            value = evaluate_final(expression);
        } catch (const EvaluationError& error) {
            diagnose(error.location(), "E3101", error.what());
            return;
        }

        auto& data = object.sections[section_index].data;
        const auto emit_byte = [&](std::uint32_t byte_offset, std::uint8_t byte) {
            data[byte_offset] = byte;
        };

        if (value.symbol.has_value()) {
            const auto symbol = symbol_indexes_.find(*value.symbol);
            if (symbol == symbol_indexes_.end()) {
                diagnose(expression.location, "E3307", "unknown symbol; declare extern explicitly");
                return;
            }
            if (value.addend < std::numeric_limits<std::int32_t>::min()
                || value.addend > std::numeric_limits<std::int32_t>::max()) {
                diagnose(expression.location, "E3501", "relocation addend exceeds 32 bits");
                return;
            }
            object.relocations.push_back(formats::jro::Relocation{
                section_index,
                offset,
                relocation_type,
                symbol->second,
                static_cast<std::int32_t>(value.addend),
            });
            return;
        }

        const auto absolute = value.addend;
        if (value_constraint == ValueConstraint::unsigned8
            && (absolute < 0 || absolute > 255)) {
            diagnose(
                expression.location,
                "E3507",
                "unsigned byte value is out of range"
            );
            return;
        }
        switch (relocation_type) {
        case formats::jro::RelocationType::abs8:
            if (absolute < -128 || absolute > 255) {
                diagnose(expression.location, "E3502", "byte value is out of range");
                return;
            }
            emit_byte(offset, static_cast<std::uint8_t>(absolute & 0xFF));
            break;
        case formats::jro::RelocationType::direct8:
            if (absolute < 0 || absolute > 255) {
                diagnose(expression.location, "E3503", "direct address is out of range");
                return;
            }
            emit_byte(offset, static_cast<std::uint8_t>(absolute));
            break;
        case formats::jro::RelocationType::abs16_be:
            if (absolute < -32'768 || absolute > 65'535) {
                diagnose(expression.location, "E3504", "word value is out of range");
                return;
            }
            emit_byte(offset, static_cast<std::uint8_t>((absolute >> 8) & 0xFF));
            emit_byte(offset + 1U, static_cast<std::uint8_t>(absolute & 0xFF));
            break;
        case formats::jro::RelocationType::rel8:
            diagnose(
                expression.location,
                "E3505",
                "relative operand must reference a symbol"
            );
            break;
        }
    }

    void emit_statement(
        formats::jro::ObjectFile& object,
        const Statement& statement,
        const PassInfo& info
    ) {
        if (!info.section_index.has_value() || info.size == 0U) {
            return;
        }
        const auto section_index = *info.section_index;
        if (object.sections[section_index].type == formats::jro::SectionType::no_bits) {
            if (statement.kind != StatementKind::space) {
                diagnose(statement.location, "E3506", "only .space may emit into a bss section");
            }
            return;
        }

        if (statement.kind == StatementKind::bytes || statement.kind == StatementKind::words) {
            const auto width = statement.kind == StatementKind::bytes ? 1U : 2U;
            const auto relocation_type = statement.kind == StatementKind::bytes
                ? formats::jro::RelocationType::abs8
                : formats::jro::RelocationType::abs16_be;
            for (std::size_t index = 0; index < statement.expressions.size(); ++index) {
                emit_expression(
                    object,
                    *statement.expressions[index],
                    section_index,
                    info.offset + static_cast<std::uint32_t>(index * width),
                    relocation_type
                );
            }
            return;
        }
        if (statement.kind != StatementKind::instruction || info.instruction == nullptr) {
            return;
        }

        auto& data = object.sections[section_index].data;
        data[info.offset] = info.instruction->opcode;
        switch (info.instruction->addressing_mode) {
        case isa::AddressingMode::implied:
            break;
        case isa::AddressingMode::immediate8:
            emit_expression(
                object,
                *statement.expressions[0],
                section_index,
                info.offset + 1U,
                formats::jro::RelocationType::abs8
            );
            break;
        case isa::AddressingMode::immediate16:
            emit_expression(
                object,
                *statement.expressions[0],
                section_index,
                info.offset + 1U,
                formats::jro::RelocationType::abs16_be
            );
            break;
        case isa::AddressingMode::direct8:
            emit_expression(
                object,
                *statement.expressions[0],
                section_index,
                info.offset + 1U,
                formats::jro::RelocationType::direct8
            );
            break;
        case isa::AddressingMode::extended16:
            emit_expression(
                object,
                *statement.expressions[0],
                section_index,
                info.offset + 1U,
                formats::jro::RelocationType::abs16_be
            );
            break;
        case isa::AddressingMode::relative8:
            emit_expression(
                object,
                *statement.expressions[0],
                section_index,
                info.offset + 1U,
                formats::jro::RelocationType::rel8
            );
            break;
        case isa::AddressingMode::indexed8:
            emit_expression(
                object,
                *statement.expressions[0],
                section_index,
                info.offset + 1U,
                formats::jro::RelocationType::abs8,
                ValueConstraint::unsigned8
            );
            break;
        case isa::AddressingMode::immediate8_direct8:
            emit_expression(
                object,
                *statement.expressions[0],
                section_index,
                info.offset + 1U,
                formats::jro::RelocationType::abs8
            );
            emit_expression(
                object,
                *statement.expressions[1],
                section_index,
                info.offset + 2U,
                formats::jro::RelocationType::direct8
            );
            break;
        case isa::AddressingMode::immediate8_indexed8:
            emit_expression(
                object,
                *statement.expressions[0],
                section_index,
                info.offset + 1U,
                formats::jro::RelocationType::abs8
            );
            emit_expression(
                object,
                *statement.expressions[1],
                section_index,
                info.offset + 2U,
                formats::jro::RelocationType::abs8,
                ValueConstraint::unsigned8
            );
            break;
        }
    }

    formats::Sha256Digest source_digest() const {
        return formats::sha256(std::span{
            reinterpret_cast<const std::uint8_t*>(source_.text.data()),
            source_.text.size(),
        });
    }

    formats::Sha256Digest build_digest() const {
        std::vector<std::uint8_t> identity;
        const auto append = [&](std::string_view value) {
            identity.insert(identity.end(), value.begin(), value.end());
            identity.push_back(0U);
        };
        append("JR8AS-BUILD-ID-V1");
        append(options_.target_profile);
        append(options_.producer_version);
        append(source_.logical_path);
        identity.insert(identity.end(), source_.text.begin(), source_.text.end());
        return formats::sha256(identity);
    }

    std::string make_listing(const formats::jro::ObjectFile& object) const {
        std::ostringstream listing;
        listing << "JR8AS LISTING\n"
                << "Target: " << options_.target_profile << '\n'
                << "Source: " << source_.logical_path << "\n\n";
        for (std::size_t index = 0; index < statements_.size(); ++index) {
            const auto& info = pass_info_[index];
            if (!info.section_index.has_value() || info.size == 0U) {
                continue;
            }
            const auto& statement = statements_[index];
            const auto& section = object.sections[*info.section_index];
            listing << std::dec << std::setw(5) << statement.location.line << "  "
                    << section.name << ':' << std::uppercase << std::hex << std::setw(4)
                    << std::setfill('0') << info.offset << std::setfill(' ') << "  ";
            if (section.type == formats::jro::SectionType::no_bits) {
                listing << "<NO_BITS " << std::dec << info.size << '>';
            } else {
                const auto displayed = std::min<std::uint32_t>(info.size, 8U);
                for (std::uint32_t byte_index = 0; byte_index < displayed; ++byte_index) {
                    listing << std::hex << std::setw(2) << std::setfill('0')
                            << static_cast<unsigned int>(section.data[info.offset + byte_index])
                            << ' ';
                }
                listing << std::setfill(' ');
                if (displayed < info.size) {
                    listing << "...";
                }
            }
            listing << "  " << statement.source_text << '\n';
        }
        return listing.str();
    }

    std::optional<Output> second_pass() {
        formats::jro::ObjectFile object;
        object.target_profile = options_.target_profile;
        object.build = formats::jro::BuildIdentity{
            "jr8as",
            options_.producer_version,
            build_digest(),
        };
        object.source_files.push_back(formats::jro::SourceFile{
            source_.logical_path,
            source_digest(),
        });
        for (const auto& section : sections_) {
            object.sections.push_back(formats::jro::Section{
                section.name,
                section.type,
                section.attributes,
                1,
                std::nullopt,
                section.logical_size,
                section.type == formats::jro::SectionType::program_bits
                    ? std::vector<std::uint8_t>(section.logical_size, 0U)
                    : std::vector<std::uint8_t>{},
            });
        }
        for (const auto& symbol : symbols_) {
            formats::jro::SymbolDefinition definition{
                formats::jro::SymbolDefinition::undefined};
            switch (symbol.state) {
            case SymbolState::section:
                definition = formats::jro::SymbolDefinition::section;
                break;
            case SymbolState::absolute:
                definition = formats::jro::SymbolDefinition::absolute;
                break;
            case SymbolState::external:
                definition = formats::jro::SymbolDefinition::undefined;
                break;
            case SymbolState::declared:
                continue;
            }
            object.symbols.push_back(formats::jro::Symbol{
                symbol.name,
                symbol.binding,
                definition,
                symbol.section_index,
                symbol.value,
                0,
            });
        }

        for (std::size_t index = 0; index < statements_.size(); ++index) {
            emit_statement(object, statements_[index], pass_info_[index]);
            const auto& info = pass_info_[index];
            if (info.section_index.has_value() && info.size > 0U) {
                object.source_lines.push_back(formats::jro::SourceLineMapping{
                    *info.section_index,
                    info.offset,
                    info.size,
                    0,
                    static_cast<std::uint32_t>(statements_[index].location.line),
                    static_cast<std::uint32_t>(statements_[index].location.column),
                });
            }
        }
        if (!diagnostics_.empty()) {
            return std::nullopt;
        }

        try {
            static_cast<void>(formats::jro::write(object));
        } catch (const formats::jro::Error& error) {
            diagnose(Location{1, 1}, "E3999", std::string("invalid JRO output: ") + error.what());
            return std::nullopt;
        }
        return Output{object, make_listing(object)};
    }

    const Source& source_;
    const Options& options_;
    std::vector<Diagnostic>& diagnostics_;
    std::optional<isa::CpuProfile> profile_;
    std::vector<Statement> statements_;
    std::vector<PassInfo> pass_info_;
    std::vector<SectionBuilder> sections_;
    std::unordered_map<std::string, std::uint32_t> section_indexes_;
    std::optional<std::uint32_t> current_section_;
    std::vector<SymbolBuilder> symbols_;
    std::unordered_map<std::string, std::uint32_t> symbol_indexes_;
};

}  // namespace

Result assemble(const Source& source, const Options& options) {
    Result result;
    Lexer lexer(source, result.diagnostics);
    auto tokens = lexer.tokenize();
    Parser parser(source, std::move(tokens), result.diagnostics);
    auto statements = parser.parse();
    if (!result.diagnostics.empty()) {
        return result;
    }

    AssemblerEngine engine(source, options, result.diagnostics);
    try {
        result.output = engine.run(std::move(statements));
    } catch (const std::exception& error) {
        add_diagnostic(
            result.diagnostics,
            source,
            "E3998",
            std::string("internal assembly failure: ") + error.what(),
            Location{1, 1}
        );
        result.output.reset();
    }
    if (!result.diagnostics.empty()) {
        result.output.reset();
    }
    return result;
}

}  // namespace jr800::assembler
