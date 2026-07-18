#pragma once

#include "types.h"

#include <cstdint>
#include <string>
#include <type_traits>

namespace Eunshin {

enum class MoveType : std::uint8_t {
    Normal = 0,
    Castle = 1,
    EnPassant = 2,
    PromotionKnight = 3,
    PromotionBishop = 4,
    PromotionRook = 5,
    PromotionQueen = 6,
    None = 15
};

class Move final {
public:
    constexpr Move() noexcept : value_(NONE_VALUE) {}

    constexpr Move(Square from, Square to, MoveType type = MoveType::Normal) noexcept
        : value_(encode(from, to, type)) {}

    [[nodiscard]] static constexpr Move none() noexcept {
        return Move(NONE_VALUE, RawTag{});
    }

    [[nodiscard]] static constexpr Move fromRaw(std::uint16_t value) noexcept {
        return Move(value, RawTag{});
    }

    [[nodiscard]] constexpr Square from() const noexcept {
        return static_cast<Square>(value_ & FROM_MASK);
    }

    [[nodiscard]] constexpr Square to() const noexcept {
        return static_cast<Square>((value_ >> TO_SHIFT) & TO_MASK);
    }

    [[nodiscard]] constexpr MoveType type() const noexcept {
        return static_cast<MoveType>((value_ >> TYPE_SHIFT) & TYPE_MASK);
    }

    [[nodiscard]] constexpr bool isNone() const noexcept {
        return type() == MoveType::None;
    }

    [[nodiscard]] constexpr bool isCastle() const noexcept {
        return type() == MoveType::Castle;
    }

    [[nodiscard]] constexpr bool isEnPassant() const noexcept {
        return type() == MoveType::EnPassant;
    }

    [[nodiscard]] constexpr bool isPromotion() const noexcept {
        return type() >= MoveType::PromotionKnight
            && type() <= MoveType::PromotionQueen;
    }

    [[nodiscard]] constexpr PieceType promotionType() const noexcept {
        switch (type()) {
        case MoveType::PromotionKnight: return PieceType::Knight;
        case MoveType::PromotionBishop: return PieceType::Bishop;
        case MoveType::PromotionRook:   return PieceType::Rook;
        case MoveType::PromotionQueen:  return PieceType::Queen;
        default:                        return PieceType::None;
        }
    }

    [[nodiscard]] constexpr std::uint16_t raw() const noexcept {
        return value_;
    }

    [[nodiscard]] friend constexpr bool operator==(Move lhs, Move rhs) noexcept {
        return lhs.value_ == rhs.value_;
    }

    [[nodiscard]] friend constexpr bool operator!=(Move lhs, Move rhs) noexcept {
        return !(lhs == rhs);
    }

private:
    struct RawTag {};

    static constexpr unsigned TO_SHIFT = 6;
    static constexpr unsigned TYPE_SHIFT = 12;
    static constexpr std::uint16_t FROM_MASK = 0x003FU;
    static constexpr std::uint16_t TO_MASK = 0x003FU;
    static constexpr std::uint16_t TYPE_MASK = 0x000FU;
    static constexpr std::uint16_t NONE_VALUE = 0xF000U;

    constexpr Move(std::uint16_t value, RawTag) noexcept : value_(value) {}

    [[nodiscard]] static constexpr std::uint16_t encode(
        Square from, Square to, MoveType type) noexcept {
        if (!isValid(from) || !isValid(to) || !isSupportedType(type))
            return NONE_VALUE;

        return static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(index(from)) & FROM_MASK)
            | ((static_cast<std::uint16_t>(index(to)) & TO_MASK) << TO_SHIFT)
            | ((static_cast<std::uint16_t>(index(type)) & TYPE_MASK) << TYPE_SHIFT));
    }

    [[nodiscard]] static constexpr bool isSupportedType(MoveType type) noexcept {
        return type == MoveType::Normal || type == MoveType::Castle ||
               type == MoveType::EnPassant ||
               (type >= MoveType::PromotionKnight &&
                type <= MoveType::PromotionQueen);
    }

    std::uint16_t value_;
};

[[nodiscard]] inline std::string moveToUci(Move move) {
    if (move.isNone())
        return "0000";

    std::string result(4, ' ');
    result[0] = static_cast<char>('a' + fileOf(move.from()));
    result[1] = static_cast<char>('1' + rankOf(move.from()));
    result[2] = static_cast<char>('a' + fileOf(move.to()));
    result[3] = static_cast<char>('1' + rankOf(move.to()));

    if (move.isPromotion()) {
        char suffix = 'q';
        switch (move.type()) {
        case MoveType::PromotionKnight: suffix = 'n'; break;
        case MoveType::PromotionBishop: suffix = 'b'; break;
        case MoveType::PromotionRook:   suffix = 'r'; break;
        case MoveType::PromotionQueen:  suffix = 'q'; break;
        default: break;
        }
        result.push_back(suffix);
    }

    return result;
}

static_assert(sizeof(Move) == sizeof(std::uint16_t), "Move must remain exactly 16 bits");
static_assert(std::is_trivially_copyable<Move>::value, "Move must remain trivially copyable");
static_assert(Move::fromRaw(Move(Square::E2, Square::E4).raw())
                  == Move(Square::E2, Square::E4),
              "Move raw round-trip must be lossless");

} // namespace Eunshin
