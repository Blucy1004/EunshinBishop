#include "core/attacks.h"
#include "core/bitboard.h"
#include "core/move.h"
#include "core/random.h"
#include "core/types.h"
#include "core/zobrist.h"
#include "engine/engine.h"
#include "engine/options.h"
#include "position/fen.h"
#include "position/movegen.h"
#include "position/position.h"
#include "search/history.h"
#include "search/move_picker.h"
#include "search/search_stack.h"
#include "search/see.h"
#include "search/tt.h"
#include "search/worker.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace {

using namespace Eunshin;

int failures = 0;
int checks = 0;

void expect(bool condition, std::string_view message) {
    ++checks;
    if (condition) return;
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
}

bool equalState(const StateInfo& lhs, const StateInfo& rhs) {
    return lhs.previous == rhs.previous &&
           lhs.positionKey == rhs.positionKey &&
           lhs.pawnKey == rhs.pawnKey &&
           lhs.materialKey == rhs.materialKey &&
           lhs.rule50 == rhs.rule50 &&
           lhs.pliesFromNull == rhs.pliesFromNull &&
           lhs.fullmoveNumber == rhs.fullmoveNumber &&
           lhs.castlingRights == rhs.castlingRights &&
           lhs.epSquare == rhs.epSquare &&
           lhs.move == rhs.move &&
           lhs.movedPiece == rhs.movedPiece &&
           lhs.capturedPiece == rhs.capturedPiece &&
           lhs.capturedSquare == rhs.capturedSquare &&
           lhs.checkers == rhs.checkers &&
           lhs.nullMove == rhs.nullMove &&
           lhs.accumulator == rhs.accumulator;
}

struct PositionSnapshot {
    std::array<Piece, SQUARE_NB> board{};
    std::array<Bitboard, COLOR_NB> byColor{};
    std::array<Bitboard, PIECE_TYPE_NB> byType{};
    Bitboard occupied = EMPTY_BB;
    Color side = Color::None;
    const StateInfo* stateAddress = nullptr;
    StateInfo state{};
    std::string fen;
};

PositionSnapshot snapshot(const Position& position) {
    PositionSnapshot result;
    for (int square = 0; square < SQUARE_NB; ++square)
        result.board[static_cast<std::size_t>(square)] =
            position.pieceOn(static_cast<Square>(square));
    for (int color = 0; color < COLOR_NB; ++color)
        result.byColor[static_cast<std::size_t>(color)] =
            position.pieces(static_cast<Color>(color));
    for (int type = 0; type < PIECE_TYPE_NB; ++type)
        result.byType[static_cast<std::size_t>(type)] =
            position.pieces(static_cast<PieceType>(type + 1));
    result.occupied = position.occupied();
    result.side = position.sideToMove();
    result.stateAddress = position.state();
    if (position.state()) result.state = *position.state();
    result.fen = toFen(position);
    return result;
}

bool matches(const Position& position, const PositionSnapshot& expected) {
    if (position.state() != expected.stateAddress ||
        position.sideToMove() != expected.side ||
        position.occupied() != expected.occupied ||
        toFen(position) != expected.fen)
        return false;
    for (int square = 0; square < SQUARE_NB; ++square) {
        if (position.pieceOn(static_cast<Square>(square)) !=
            expected.board[static_cast<std::size_t>(square)])
            return false;
    }
    for (int color = 0; color < COLOR_NB; ++color) {
        if (position.pieces(static_cast<Color>(color)) !=
            expected.byColor[static_cast<std::size_t>(color)])
            return false;
    }
    for (int type = 0; type < PIECE_TYPE_NB; ++type) {
        if (position.pieces(static_cast<PieceType>(type + 1)) !=
            expected.byType[static_cast<std::size_t>(type)])
            return false;
    }
    return position.state() && equalState(*position.state(), expected.state);
}

bool load(Position& position, StateInfo& root, std::string_view fen) {
    std::string error;
    const bool ok = setFromFen(position, fen, root, &error);
    if (!ok) std::cerr << "FEN error: " << error << " [" << fen << "]\n";
    return ok;
}

void seedAccumulator(StateInfo& state, std::uint64_t generation) {
    for (std::size_t perspective = 0;
         perspective < NNUE::kPerspectiveCount; ++perspective) {
        for (std::size_t neuron = 0;
             neuron < NNUE::kAccumulatorWidth; ++neuron) {
            state.accumulator.values[perspective][neuron] =
                static_cast<std::int32_t>(
                    (perspective + 1U) * 1000U + neuron);
        }
    }
    state.accumulator.validMask = 3;
    state.accumulator.generation = generation;
}

Bitboard slowSlider(Square square,
                    Bitboard occupied,
                    const int directions[][2],
                    int directionCount) {
    Bitboard result = EMPTY_BB;
    for (int direction = 0; direction < directionCount; ++direction) {
        int file = fileOf(square) + directions[direction][0];
        int rank = rankOf(square) + directions[direction][1];
        while (file >= 0 && file < 8 && rank >= 0 && rank < 8) {
            const Square target = makeSquare(file, rank);
            result |= bit(target);
            if (occupied & bit(target)) break;
            file += directions[direction][0];
            rank += directions[direction][1];
        }
    }
    return result;
}

Bitboard slowRook(Square square, Bitboard occupied) {
    constexpr int directions[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    return slowSlider(square, occupied, directions, 4);
}

Bitboard slowBishop(Square square, Bitboard occupied) {
    constexpr int directions[4][2] = {{1, 1}, {-1, 1}, {1, -1}, {-1, -1}};
    return slowSlider(square, occupied, directions, 4);
}

void testTypesAndMoves() {
    static_assert(std::is_enum<Color>::value, "Color must be an enum");
    static_assert(std::is_enum<PieceType>::value, "PieceType must be an enum");
    static_assert(std::is_enum<Piece>::value, "Piece must be an enum");
    static_assert(std::is_enum<Square>::value, "Square must be an enum");
    static_assert(std::is_enum<MoveType>::value, "MoveType must be an enum");
    static_assert(!std::is_convertible<Color, int>::value,
                  "Color must not implicitly convert to int");
    static_assert(sizeof(Move) == 2, "Move must be exactly 16 bits");
    static_assert(std::is_trivially_copyable<Move>::value,
                  "Move must be trivially copyable");
    static_assert(std::is_standard_layout<Move>::value,
                  "Move must have standard layout");

    expect(index(Square::A1) == 0 && index(Square::H8) == 63 &&
               index(Square::None) == 64,
           "Square numbering");
    expect(fileOf(Square::A1) == 0 && rankOf(Square::A1) == 0 &&
               fileOf(Square::H8) == 7 && rankOf(Square::H8) == 7,
           "Square file/rank helpers");
    expect(fileOf(Square::None) == -1 && rankOf(Square::None) == -1 &&
               bit(Square::None) == EMPTY_BB,
           "invalid square helpers are safe");

    for (Color color : {Color::White, Color::Black}) {
        for (int typeIndex = 1; typeIndex <= PIECE_TYPE_NB; ++typeIndex) {
            const PieceType type = static_cast<PieceType>(typeIndex);
            const Piece piece = makePiece(color, type);
            expect(colorOf(piece) == color && typeOf(piece) == type,
                   "Piece construction round-trip");
        }
    }

    struct RawCase {
        Move move;
        std::uint16_t raw;
        const char* uci;
    };
    const std::array<RawCase, 10> cases{{
        {Move::none(), 0xF000U, "0000"},
        {Move(Square::E2, Square::E4), 0x070CU, "e2e4"},
        {Move(Square::E1, Square::G1, MoveType::Castle), 0x1184U, "e1g1"},
        {Move(Square::E1, Square::C1, MoveType::Castle), 0x1084U, "e1c1"},
        {Move(Square::E8, Square::G8, MoveType::Castle), 0x1FBCU, "e8g8"},
        {Move(Square::E5, Square::D6, MoveType::EnPassant), 0x2AE4U, "e5d6"},
        {Move(Square::A7, Square::A8, MoveType::PromotionKnight), 0x3E30U, "a7a8n"},
        {Move(Square::A7, Square::A8, MoveType::PromotionBishop), 0x4E30U, "a7a8b"},
        {Move(Square::A7, Square::A8, MoveType::PromotionRook), 0x5E30U, "a7a8r"},
        {Move(Square::A7, Square::A8, MoveType::PromotionQueen), 0x6E30U, "a7a8q"}
    }};
    for (const RawCase& test : cases) {
        expect(test.move.raw() == test.raw, "fixed Move raw encoding");
        expect(Move::fromRaw(test.raw) == test.move, "Move raw round-trip");
        expect(moveToUci(test.move) == test.uci, "Move UCI rendering");
    }
    expect(Move(Square::B7, Square::A8, MoveType::PromotionQueen).raw() ==
               0x6E31U,
           "capture-promotion raw encoding");
    expect(Move(Square::None, Square::A2).isNone() &&
               Move(Square::A2, Square::None).isNone(),
           "invalid squares cannot alias a legal Move");
}

void testAttacks() {
    expect(Attacks::knight(Square::A1) == 0x0000000000020400ULL,
           "knight attacks from A1");
    expect(Attacks::knight(Square::D4) == 0x0000142200221400ULL,
           "knight attacks from D4");
    expect(Attacks::king(Square::A1) == 0x0000000000000302ULL,
           "king attacks from A1");
    expect(Attacks::pawn(Color::White, Square::A2) == 0x0000000000020000ULL,
           "white pawn attacks from A2");
    expect(Attacks::pawn(Color::White, Square::D4) == 0x0000001400000000ULL,
           "white pawn attacks from D4");
    expect(Attacks::pawn(Color::Black, Square::D4) == 0x0000000000140000ULL,
           "black pawn attacks from D4");
    expect(Attacks::queen(Square::None, FULL_BB) == EMPTY_BB,
           "invalid-square attacks are safe");

    const Bitboard rookBlockers = bit(Square::D6) | bit(Square::D2) |
                                  bit(Square::B4) | bit(Square::F4);
    const Bitboard bishopBlockers = bit(Square::B6) | bit(Square::F6) |
                                    bit(Square::B2) | bit(Square::F2);
    expect(Attacks::rook(Square::D4, rookBlockers) == 0x0000080836080800ULL,
           "rook blockers include first blocker only");
    expect(Attacks::bishop(Square::D4, bishopBlockers) == 0x0000221400142200ULL,
           "bishop blockers include first blocker only");

    for (int squareIndex = 0; squareIndex < SQUARE_NB; ++squareIndex) {
        const Square square = static_cast<Square>(squareIndex);
        for (Bitboard occupied : {EMPTY_BB, FULL_BB}) {
            expect(Attacks::rook(square, occupied) == slowRook(square, occupied),
                   "rook empty/full occupancy oracle");
            expect(Attacks::bishop(square, occupied) == slowBishop(square, occupied),
                   "bishop empty/full occupancy oracle");
        }

        const Bitboard rookMask = Attacks::relevantRookMask(square);
        Bitboard subset = EMPTY_BB;
        do {
            expect(Attacks::rook(square, subset) == slowRook(square, subset),
                   "exhaustive rook magic subset");
            subset = (subset - rookMask) & rookMask;
        } while (subset != EMPTY_BB);

        const Bitboard bishopMask = Attacks::relevantBishopMask(square);
        subset = EMPTY_BB;
        do {
            expect(Attacks::bishop(square, subset) == slowBishop(square, subset),
                   "exhaustive bishop magic subset");
            subset = (subset - bishopMask) & bishopMask;
        } while (subset != EMPTY_BB);
    }

    Position position;
    StateInfo root;
    expect(load(position, root,
                "4k3/8/1b1r4/8/3P4/2N5/8/4K3 w - - 0 1"),
           "load attackersTo test FEN");
    expect(position.attackersTo(Square::D4, Color::Black) ==
               (bit(Square::B6) | bit(Square::D6)),
           "attackersTo finds slider attackers");

    expect(load(position, root,
                "4k3/8/8/8/1b6/5n2/8/4K3 w - - 0 1"),
           "load double-check FEN");
    expect(position.state()->checkers ==
               (bit(Square::B4) | bit(Square::F3)),
           "root StateInfo records all checkers");
}

void expectPerft(std::string_view fen,
                 const std::uint64_t* expected,
                 int maximumDepth,
                 std::string_view label) {
    Position position;
    StateInfo root;
    if (!load(position, root, fen)) {
        expect(false, label);
        return;
    }
    seedAccumulator(root, 17);
    const PositionSnapshot before = snapshot(position);
    for (int depth = 0; depth <= maximumDepth; ++depth) {
        const std::uint64_t actual = perft(position, depth);
        if (actual != expected[depth]) {
            std::cerr << "perft mismatch " << label << " depth " << depth
                      << ": expected " << expected[depth]
                      << ", got " << actual << '\n';
        }
        expect(actual == expected[depth], label);
        expect(matches(position, before), "perft restores root exactly");
    }
}

void testPerft() {
    constexpr std::uint64_t startExpected[] = {
        1ULL, 20ULL, 400ULL, 8902ULL, 197281ULL, 4865609ULL
    };
    expectPerft(kStartFen, startExpected, 5, "startpos perft");

    constexpr std::uint64_t kiwipeteExpected[] = {
        1ULL, 48ULL, 2039ULL, 97862ULL, 4085603ULL
    };
    expectPerft(
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        kiwipeteExpected, 4, "Kiwipete perft");

    constexpr std::uint64_t epExpected[] = {
        1ULL, 14ULL, 191ULL, 2812ULL, 43238ULL
    };
    expectPerft("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
                epExpected, 4, "EP/check perft");

    constexpr std::uint64_t promotionExpected[] = {
        1ULL, 6ULL, 264ULL, 9467ULL, 422333ULL
    };
    expectPerft(
        "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
        promotionExpected, 4, "promotion/castling perft");
}

bool exerciseMove(std::string_view fen,
                  std::string_view uci,
                  MoveType type,
                  std::string_view expectedFen) {
    Position position;
    StateInfo root;
    if (!load(position, root, fen)) return false;
    seedAccumulator(root, 0x12345678ULL);
    const PositionSnapshot before = snapshot(position);
    const Move move = moveFromUci(position, uci);
    expect(!move.isNone(), "UCI parser accepts legal move");
    expect(move.type() == type, "UCI parser preserves explicit MoveType");
    expect(moveToUci(move) == uci, "UCI move round-trip");
    if (move.isNone()) return false;

    StateInfo child;
    const bool made = position.doMove(move, child);
    expect(made, "doMove accepts legal move");
    if (!made) return false;
    expect(child.previous == before.stateAddress,
           "StateInfo links to its exact parent");
    expect(position.isConsistent(), "position is consistent after doMove");
    expect(toFen(position) == expectedFen, "post-move FEN");
    expect(child.accumulator.validMask == 0 &&
               child.accumulator.generation ==
                   before.state.accumulator.generation &&
               child.accumulator.values == before.state.accumulator.values,
           "non-null move preserves accumulator storage and invalidates it");

    position.undoMove(move);
    expect(matches(position, before), "undoMove restores every parent field");
    expect(position.isConsistent(), "position is consistent after undoMove");
    return true;
}

void testSpecialMoves() {
    exerciseMove(kStartFen, "e2e4", MoveType::Normal,
                 "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1");
    exerciseMove("7k/8/8/3n4/4P3/8/8/4K3 w - - 17 42",
                 "e4d5", MoveType::Normal,
                 "7k/8/8/3P4/8/8/8/4K3 b - - 0 42");
    exerciseMove("4k3/8/8/3pP3/8/8/8/4K3 w - d6 12 30",
                 "e5d6", MoveType::EnPassant,
                 "4k3/8/3P4/8/8/8/8/4K3 b - - 0 30");
    exerciseMove("4k3/8/8/8/3Pp3/8/8/4K3 b - d3 12 30",
                 "e4d3", MoveType::EnPassant,
                 "4k3/8/8/8/8/3p4/8/4K3 w - - 0 31");

    exerciseMove("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1",
                 "e1g1", MoveType::Castle,
                 "r3k2r/8/8/8/8/8/8/R4RK1 b kq - 1 1");
    exerciseMove("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1",
                 "e1c1", MoveType::Castle,
                 "r3k2r/8/8/8/8/8/8/2KR3R b kq - 1 1");
    exerciseMove("r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1",
                 "e8g8", MoveType::Castle,
                 "r4rk1/8/8/8/8/8/8/R3K2R w KQ - 1 2");
    exerciseMove("r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1",
                 "e8c8", MoveType::Castle,
                 "2kr3r/8/8/8/8/8/8/R3K2R w KQ - 1 2");

    constexpr std::array<char, 4> suffixes{{'n', 'b', 'r', 'q'}};
    constexpr std::array<MoveType, 4> promotionTypes{{
        MoveType::PromotionKnight, MoveType::PromotionBishop,
        MoveType::PromotionRook, MoveType::PromotionQueen
    }};
    for (std::size_t i = 0; i < suffixes.size(); ++i) {
        const std::string whiteQuiet = std::string("a7a8") + suffixes[i];
        const std::string whiteCapture = std::string("b7a8") + suffixes[i];
        const std::string blackQuiet = std::string("a2a1") + suffixes[i];
        const std::string blackCapture = std::string("b2a1") + suffixes[i];
        const char whitePiece = suffixes[i] == 'n' ? 'N' :
                                suffixes[i] == 'b' ? 'B' :
                                suffixes[i] == 'r' ? 'R' : 'Q';
        const char blackPiece = suffixes[i];
        exerciseMove("7k/P7/8/8/8/8/8/4K3 w - - 0 1",
                     whiteQuiet, promotionTypes[i],
                     std::string(1, whitePiece) + "6k/8/8/8/8/8/8/4K3 b - - 0 1");
        exerciseMove("r6k/1P6/8/8/8/8/8/4K3 w - - 0 1",
                     whiteCapture, promotionTypes[i],
                     std::string(1, whitePiece) + "6k/8/8/8/8/8/8/4K3 b - - 0 1");
        exerciseMove("4k3/8/8/8/8/8/p7/7K b - - 0 1",
                     blackQuiet, promotionTypes[i],
                     "4k3/8/8/8/8/8/8/" + std::string(1, blackPiece) + "6K w - - 0 2");
        exerciseMove("4k3/8/8/8/8/8/1p6/R6K b - - 0 1",
                     blackCapture, promotionTypes[i],
                     "4k3/8/8/8/8/8/8/" + std::string(1, blackPiece) + "6K w - - 0 2");
    }
}

void expectIllegal(Position& position, Move move, std::string_view label) {
    const PositionSnapshot before = snapshot(position);
    StateInfo child;
    expect(!position.doMove(move, child), label);
    expect(matches(position, before), "illegal move is mutation-free");
}

void testIllegalMovesAndFen() {
    Position position;
    StateInfo root;
    expect(load(position, root, kStartFen), "load startpos for illegal moves");
    seedAccumulator(root, 91);
    expectIllegal(position, Move(Square::E3, Square::E4), "empty source rejected");
    expectIllegal(position, Move(Square::E7, Square::E5), "opponent move rejected");
    expectIllegal(position, Move(Square::E1, Square::E2), "own target rejected");
    expectIllegal(position, Move(Square::A1, Square::B2), "rook geometry rejected");
    expectIllegal(position, Move(Square::C1, Square::C3), "bishop geometry rejected");
    expectIllegal(position, Move(Square::G1, Square::G3), "knight geometry rejected");
    expectIllegal(position, Move(Square::E2, Square::E5), "long pawn move rejected");
    expectIllegal(position, Move(Square::E1, Square::G1),
                  "castling encoded as Normal is rejected");
    for (std::uint16_t type = 7; type <= 14; ++type) {
        const std::uint16_t raw = static_cast<std::uint16_t>(
            Move(Square::E2, Square::E4).raw() | (type << 12));
        expectIllegal(position, Move::fromRaw(raw), "reserved MoveType rejected");
    }

    expect(load(position, root,
                "4k3/8/8/8/8/8/4Q3/4K3 w - - 0 1"),
           "load king-capture FEN");
    expectIllegal(position, Move(Square::E2, Square::E8),
                  "king capture rejected");

    expect(load(position, root,
                "4r1k1/8/8/8/8/8/4R3/4K3 w - - 0 1"),
           "load pinned-piece FEN");
    expectIllegal(position, Move(Square::E2, Square::F2),
                  "self-check move rejected and rolled back");

    expect(load(position, root,
                "k3r3/8/8/3pP3/8/8/8/4K3 w - d6 0 1"),
           "load pinned-EP FEN");
    expectIllegal(position, Move(Square::E5, Square::D6, MoveType::EnPassant),
                  "pinned en passant rejected");

    expect(load(position, root,
                "r3k2r/5r2/8/8/8/8/8/R3K2R w KQkq - 0 1"),
           "load attacked-transit castling FEN");
    expectIllegal(position, Move(Square::E1, Square::G1, MoveType::Castle),
                  "castling through check rejected");

    expect(load(position, root,
                "r3k2r/8/8/8/8/8/8/RN2K2R w KQkq - 0 1"),
           "load obstructed queenside castling FEN");
    expectIllegal(position, Move(Square::E1, Square::C1, MoveType::Castle),
                  "queenside castling requires B-file vacancy");

    expect(load(position, root, kStartFen), "reload startpos before bad FEN");
    seedAccumulator(root, 55);
    const PositionSnapshot before = snapshot(position);
    std::string error;
    expect(!setFromFen(position,
                       "8/8/8/8/8/8/8/7X w - - 0 1",
                       root, &error),
           "malformed FEN rejected");
    expect(!error.empty(), "malformed FEN reports an error");
    expect(matches(position, before), "failed FEN load is transactional");
}

void play(Position& position,
          std::array<StateInfo, 16>& states,
          int& ply,
          std::string_view uci) {
    const Move move = moveFromUci(position, uci);
    expect(!move.isNone(), "sequence UCI move is legal");
    if (move.isNone()) return;
    expect(position.doMove(move, states[static_cast<std::size_t>(ply)]),
           "sequence doMove succeeds");
    ++ply;
}

void testNullRepetitionAndRule50() {
    Position position;
    StateInfo root;
    expect(load(position, root,
                "4k3/8/8/3pP3/8/8/8/4K3 w - d6 12 30"),
           "load null-move FEN");
    seedAccumulator(root, 77);
    const PositionSnapshot beforeNull = snapshot(position);
    StateInfo nullState;
    expect(position.doNullMove(nullState), "legal null move succeeds");
    expect(position.sideToMove() == Color::Black &&
               position.epSquare() == Square::None &&
               position.rule50() == 12 && position.pliesFromNull() == 0,
           "null move updates only its defined state");
    expect(nullState.accumulator == beforeNull.state.accumulator,
           "null move preserves accumulator exactly");
    expect(position.isConsistent(), "null-move position is consistent");
    position.undoNullMove();
    expect(matches(position, beforeNull), "undoNullMove restores exact parent");

    expect(load(position, root,
                "4r1k1/8/8/8/8/8/8/4K3 w - - 0 1"),
           "load in-check null FEN");
    const PositionSnapshot checked = snapshot(position);
    expect(!position.doNullMove(nullState), "null move while in check rejected");
    expect(matches(position, checked), "rejected null move is mutation-free");

    expect(load(position, root, kStartFen), "load repetition startpos");
    std::array<StateInfo, 16> states{};
    int ply = 0;
    for (int cycle = 0; cycle < 2; ++cycle) {
        play(position, states, ply, "g1f3");
        play(position, states, ply, "g8f6");
        play(position, states, ply, "f3g1");
        play(position, states, ply, "f6g8");
        expect(position.isRepetition(cycle + 1),
               "repetition chain finds required prior occurrence");
        expect(!position.isRepetition(cycle + 2),
               "repetition chain does not overcount");
    }
    while (ply > 0) {
        const Move move = position.state()->move;
        position.undoMove(move);
        --ply;
    }

    expect(load(position, root,
                "8/8/8/8/8/7k/8/R6K w - - 99 75"),
           "load 50-move FEN");
    const Move rookMove = moveFromUci(position, "a1a2");
    StateInfo next;
    expect(position.doMove(rookMove, next), "quiet non-pawn move succeeds");
    expect(position.rule50() == 100 && position.isFiftyMoveDraw(),
           "50-move rule reaches draw at 100 halfmoves");
    position.undoMove(rookMove);
}

std::uint64_t splitMix64(std::uint64_t& state) {
    state += 0x9E3779B97F4A7C15ULL;
    std::uint64_t value = state;
    value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ULL;
    value = (value ^ (value >> 27)) * 0x94D049BB133111EBULL;
    return value ^ (value >> 31);
}

void testRandomMakeUnmake() {
    constexpr int sequenceCount = 2048;
    constexpr int maximumPlies = 50;
    std::uint64_t randomState = 0x534E495045523233ULL;

    for (int sequence = 0; sequence < sequenceCount; ++sequence) {
        Position position;
        StateInfo root;
        if (!load(position, root, kStartFen)) {
            expect(false, "random sequence root FEN");
            return;
        }
        seedAccumulator(root, static_cast<std::uint64_t>(sequence + 1));
        const PositionSnapshot rootSnapshot = snapshot(position);
        std::array<StateInfo, maximumPlies> states{};
        std::array<Move, maximumPlies> moves{};
        std::array<PositionSnapshot, maximumPlies> parents{};
        int made = 0;

        for (; made < maximumPlies; ++made) {
            MoveList legal;
            generateLegalMoves(position, legal);
            if (legal.size == 0) break;
            std::sort(legal.moves.begin(), legal.moves.begin() +
                      static_cast<std::ptrdiff_t>(legal.size),
                      [](Move lhs, Move rhs) { return lhs.raw() < rhs.raw(); });
            const std::size_t choice = static_cast<std::size_t>(
                splitMix64(randomState) % legal.size);
            parents[static_cast<std::size_t>(made)] = snapshot(position);
            moves[static_cast<std::size_t>(made)] = legal[choice];
            const bool ok = position.doMove(
                moves[static_cast<std::size_t>(made)],
                states[static_cast<std::size_t>(made)]);
            expect(ok, "random legal move makes successfully");
            if (!ok) break;
            StateInfo* current = position.state();
            current->accumulator.values[static_cast<std::size_t>(made & 1)]
                                       [static_cast<std::size_t>(made % 256)] =
                sequence * maximumPlies + made;
            current->accumulator.validMask = 3;
            current->accumulator.generation =
                static_cast<std::uint64_t>(sequence * maximumPlies + made + 1);
            expect(position.isConsistent(), "random make consistency");
        }

        while (made > 0) {
            --made;
            position.undoMove(moves[static_cast<std::size_t>(made)]);
            expect(matches(position, parents[static_cast<std::size_t>(made)]),
                   "random undo restores exact parent");
            expect(position.isConsistent(), "random undo consistency");
        }
        expect(matches(position, rootSnapshot),
               "random full-unmake restores exact root");
    }
}

void testIndependentPositions() {
    Position first;
    Position second;
    StateInfo firstRoot;
    StateInfo secondRoot;
    expect(load(first, firstRoot, kStartFen) && load(second, secondRoot, kStartFen),
           "load two independent positions");
    const PositionSnapshot firstBefore = snapshot(first);
    const PositionSnapshot secondBefore = snapshot(second);

    StateInfo firstChild;
    StateInfo secondChild;
    const Move e4 = moveFromUci(first, "e2e4");
    const Move d4 = moveFromUci(second, "d2d4");
    expect(first.doMove(e4, firstChild), "first Position moves independently");
    expect(second.doMove(d4, secondChild), "second Position moves independently");
    second.undoMove(d4);
    expect(matches(second, secondBefore), "second Position undo is isolated");
    first.undoMove(e4);
    expect(matches(first, firstBefore), "first Position undo is isolated");
}

void testOptionsAndSearchState() {
    EngineOptions options;
    expect(options.hashMegabytes == 256 && options.moveOverheadMs == 30,
           "Q resource defaults");
    expect(options.useNNUE &&
               options.evalFile == "firstnet_v5_10b.snnue" &&
               options.nnueOutputMode == NNUEOutputMode::Residual &&
               options.residualScale == 100 && options.residualGuard &&
               options.absoluteBlend == 35,
           "Q NNUE defaults are ON and residual");
    expect(!options.useIIR && !options.useSEEPruning &&
               !options.useAEGIS && !options.useLIMBO,
           "experimental search defaults are OFF");

    const SearchConfig frozen = options.snapshot();
    std::string error;
    expect(options.set("UseNNUE", "false", &error) && !options.useNNUE,
           "boolean option update");
    expect(frozen.useNNUE && frozen.revision == 0,
           "SearchConfig is an immutable value snapshot");
    expect(options.revision == 1, "option revision changes once");
    expect(options.set("usennue", "false", &error) && options.revision == 1,
           "setting the same option is idempotent");
    expect(!options.set("UseNNUE", "maybe", &error) && !error.empty(),
           "invalid boolean rejected");

    const std::uint64_t beforeInvalid = options.revision;
    expect(!options.set("Hash", "0", &error) &&
               options.hashMegabytes == 256 &&
               options.revision == beforeInvalid,
           "out-of-range Hash is mutation-free");
    expect(options.set("Hash", "4096", &error) &&
               options.hashMegabytes == 4096,
           "maximum Hash accepted");
    expect(options.set("MoveOverhead", "0", &error) &&
               options.moveOverheadMs == 0,
           "MoveOverhead lower bound accepted");
    expect(options.set("NNUEOutputMode", "Absolute", &error) &&
               options.nnueOutputMode == NNUEOutputMode::Absolute,
           "absolute NNUE mode parsed");
    expect(options.set("ResidualScale", "200", &error) &&
               options.residualScale == 200,
           "ResidualScale upper bound accepted");
    expect(options.set("NNUEBlend", "41", &error) &&
               options.absoluteBlend == 41 && options.residualScale == 200,
           "legacy NNUEBlend maps only to AbsoluteBlend");
    expect(!options.set("EvalFile", "", &error),
           "empty EvalFile rejected");
    expect(!options.set("UnknownFeature", "1", &error),
           "unknown option rejected");

    HistoryTables history;
    history.clear();
    history.updateMain(Color::White, Square::E2, Square::E4, 4000);
    expect(history.mainScore(Color::White, Square::E2, Square::E4) > 0 &&
               history.mainScore(Color::Black, Square::E2, Square::E4) == 0,
           "main history is color-local");
    const int positive = history.mainScore(Color::White, Square::E2, Square::E4);
    history.decay();
    expect(history.mainScore(Color::White, Square::E2, Square::E4) ==
               positive / 2,
           "history decay is deterministic");
    for (int count = 0; count < 100; ++count)
        history.updateMain(Color::White, Square::E2, Square::E4, 20000);
    expect(history.mainScore(Color::White, Square::E2, Square::E4) <=
               HistoryTables::MAX_HISTORY,
           "history gravity saturates safely");
    history.updateCapture(Color::White, PieceType::Pawn, Square::D5,
                          PieceType::Queen, 5000);
    expect(history.captureScore(Color::White, PieceType::Pawn, Square::D5,
                                PieceType::Queen) > 0,
           "capture history update");
    const Move reply(Square::G8, Square::F6);
    history.setCounterMove(Color::White, PieceType::Pawn, Square::E4, reply);
    expect(history.counterMove(Color::White, PieceType::Pawn, Square::E4) == reply,
           "countermove round-trip");
    history.clear();
    expect(history.mainScore(Color::White, Square::E2, Square::E4) == 0 &&
               history.counterMove(Color::White, PieceType::Pawn,
                                   Square::E4).isNone(),
           "history new-game clear");

    TranspositionTable table(1);
    SearchWorker first(table);
    SearchWorker second(table);
    SearchConfig config;
    config.useNNUE = true;
    config.revision = 7;
    expect(first.beginSearch(config), "Worker starts one search session");
    config.useNNUE = false;
    config.revision = 8;
    expect(first.config().useNNUE && first.config().revision == 7,
           "Worker owns its config snapshot");
    first.histories().updateMain(Color::White, Square::A2, Square::A4, 1000);
    expect(second.histories().mainScore(Color::White, Square::A2,
                                        Square::A4) == 0,
           "Worker histories are independent");
    first.statistics().nodes = 99;
    expect(second.statistics().nodes == 0,
           "Worker statistics are independent");
    first.requestStop();
    expect(first.stopRequested() && !second.stopRequested(),
           "Worker stop flags are independent atomics");
    expect(!first.beginSearch(first.config()),
           "Worker rejects an overlapping search session");
    first.finishSearch();
    expect(first.beginSearch(first.config()),
           "Worker can start after the prior session completes");
    expect(!first.stopRequested() && first.statistics().nodes == 0 &&
               first.histories().mainScore(Color::White, Square::A2,
                                           Square::A4) != 0,
           "beginSearch resets transient state but preserves history");
    expect(first.stackAt(-4).ply == 0 && first.stackAt(12).ply == 12 &&
               first.stackAt(12).staticEval == VALUE_NONE &&
               first.stackAt(12).ttValue == VALUE_NONE &&
               &first.stateAt(0) != &first.stateAt(1),
           "Worker owns padded stacks and independent StateInfo slots");
    first.finishSearch();
    first.resetForNewGame();
    expect(first.histories().mainScore(Color::White, Square::A2,
                                       Square::A4) == 0,
           "new game clears Worker history");
}

Key collisionKey(std::uint16_t tag, std::uint16_t index = 0) {
    return (static_cast<Key>(tag) << 48U) | static_cast<Key>(index);
}

void testTranspositionTable() {
    static_assert(sizeof(TTEntry) == 16, "TT entry layout regression");
    static_assert(sizeof(TTCluster) == 64, "TT cluster layout regression");

    TranspositionTable disabled;
    expect(!disabled.enabled() && disabled.hashfull() == 0,
           "zero-size TT is disabled safely");
    TTData data;
    data.move = Move(Square::E2, Square::E4);
    data.value = 42;
    data.eval = -17;
    data.depth = 12;
    data.bound = TTBound::Exact;
    data.pv = true;
    disabled.store(collisionKey(1), data, 0);
    expect(!disabled.probe(collisionKey(1), 0).hit,
           "disabled TT ignores stores");

    TranspositionTable table(1);
    expect(table.enabled() && table.clusterCount() != 0 &&
               (table.clusterCount() & (table.clusterCount() - 1)) == 0 &&
               table.sizeMegabytes() == 1,
           "TT resize selects a power-of-two cluster count");
    const Key key = collisionKey(0x1234);
    table.store(key, data, 3);
    TTProbe hit = table.probe(key, 3);
    expect(hit.hit && hit.data.move == data.move &&
               hit.data.value == data.value && hit.data.eval == data.eval &&
               hit.data.depth == data.depth &&
               hit.data.bound == TTBound::Exact && hit.data.pv,
           "TT field round-trip");
    expect(table.hashfull() > 0, "TT hashfull counts current generation");

    TTData deeper = data;
    deeper.depth = 20;
    deeper.value = 100;
    deeper.move = Move(Square::D2, Square::D4);
    table.store(key, deeper, 0);
    TTData shallow = data;
    shallow.depth = 4;
    shallow.value = -500;
    shallow.bound = TTBound::Upper;
    shallow.move = Move::none();
    table.store(key, shallow, 0);
    hit = table.probe(key, 0);
    expect(hit.hit && hit.data.depth == 20 && hit.data.value == 100 &&
               hit.data.move == deeper.move,
           "deep exact TT entry resists shallow overwrite");

    TTData equalDowngrade = deeper;
    equalDowngrade.value = -700;
    equalDowngrade.bound = TTBound::Upper;
    equalDowngrade.pv = false;
    equalDowngrade.move = Move(Square::C2, Square::C4);
    table.store(key, equalDowngrade, 0);
    hit = table.probe(key, 0);
    expect(hit.hit && hit.data.value == deeper.value &&
               hit.data.bound == TTBound::Exact && hit.data.pv &&
               hit.data.move == deeper.move,
           "same-depth non-exact/non-PV write cannot downgrade exact PV TT data");

    TTData equalNonPv = deeper;
    equalNonPv.value = -701;
    equalNonPv.pv = false;
    table.store(key, equalNonPv, 0);
    hit = table.probe(key, 0);
    expect(hit.hit && hit.data.value == deeper.value && hit.data.pv,
           "same-depth exact write cannot discard an existing PV marker");

    TTData updated = deeper;
    updated.depth = 21;
    updated.value = 101;
    updated.move = Move::none();
    table.store(key, updated, 0);
    hit = table.probe(key, 0);
    expect(hit.hit && hit.data.value == 101 && hit.data.move == deeper.move,
           "same-key Move::none preserves the existing TT move");

    for (int offset : {0, 1, 5, 17}) {
        const Value positiveMate = MATE - 10;
        const Value negativeMate = -MATE + 10;
        const Value storedPositive =
            TranspositionTable::valueToTT(positiveMate, offset);
        const Value storedNegative =
            TranspositionTable::valueToTT(negativeMate, offset);
        expect(TranspositionTable::valueFromTT(storedPositive, offset) ==
                   positiveMate &&
                   TranspositionTable::valueFromTT(storedNegative, offset) ==
                   negativeMate,
               "mate score same-ply normalization round-trip");
    }
    expect(TranspositionTable::valueFromTT(
               TranspositionTable::valueToTT(MATE - 10, 8), 3) == MATE - 5,
           "positive mate distance adjusts across ply");
    expect(TranspositionTable::valueFromTT(
               TranspositionTable::valueToTT(-MATE + 10, 8), 3) == -MATE + 5,
           "negative mate distance adjusts across ply");

    table.clear();
    std::array<Key, 5> keys{};
    for (std::size_t index = 0; index < keys.size(); ++index)
        keys[index] = collisionKey(static_cast<std::uint16_t>(0x2000U + index));
    for (std::size_t index = 0; index < 4; ++index) {
        TTData entry = data;
        entry.bound = TTBound::Lower;
        entry.pv = false;
        entry.depth = static_cast<Depth>(1 + static_cast<int>(index) * 10);
        entry.value = static_cast<Value>(index);
        table.store(keys[index], entry, 0);
    }
    for (std::size_t index = 0; index < 4; ++index)
        expect(table.probe(keys[index], 0).hit,
               "four colliding TT keys coexist");
    TTData fifth = data;
    fifth.depth = 40;
    table.store(keys[4], fifth, 0);
    expect(!table.probe(keys[0], 0).hit && table.probe(keys[4], 0).hit &&
               table.probe(keys[1], 0).hit && table.probe(keys[2], 0).hit &&
               table.probe(keys[3], 0).hit,
           "fifth collision replaces the deterministic weakest entry");

    table.clear();
    table.store(key, data, 0);
    for (int generation = 0; generation < 32; ++generation)
        table.newSearch();
    expect(table.currentGeneration() == 0 && !table.probe(key, 0).hit,
           "TT generation wrap clears ambiguous old entries");
    table.store(key, data, 0);
    table.clear();
    expect(!table.probe(key, 0).hit && table.hashfull() == 0,
           "TT clear removes entries and hashfull state");
    table.resize(0);
    expect(!table.enabled(), "TT can be disabled after allocation");
}

void testMovePicker() {
    Position position;
    StateInfo root;
    expect(load(position, root, kStartFen), "load MovePicker startpos");
    HistoryTables histories;
    histories.clear();
    const Move tt = moveFromUci(position, "e2e4");
    const Move killer1 = moveFromUci(position, "g1f3");
    const Move killer2 = moveFromUci(position, "b1c3");
    const Move counter = moveFromUci(position, "d2d4");
    const Move bestQuiet = moveFromUci(position, "e2e3");
    histories.updateMain(Color::White, bestQuiet.from(), bestQuiet.to(), 6000);

    MovePicker picker(position, histories, tt, {{killer1, killer2}}, counter);
    expect(picker.size() == 0,
           "MovePicker construction does not generate the complete move list");
    std::array<bool, 65536> seen{};
    std::vector<Move> ordered;
    std::vector<MoveStage> stages;
    for (;;) {
        const Move move = picker.next();
        if (move.isNone()) break;
        expect(!seen[move.raw()], "MovePicker never returns a duplicate");
        seen[move.raw()] = true;
        expect(position.isLegal(move), "MovePicker returns only legal moves");
        ordered.push_back(move);
        stages.push_back(picker.lastStage());
    }
    expect(ordered.size() == 20 && picker.size() == 20 &&
               picker.returned() == 20,
           "MovePicker returns every startpos legal move once");
    expect(ordered.size() >= 5 && ordered[0] == tt &&
               stages[0] == MoveStage::TT && ordered[1] == killer1 &&
               stages[1] == MoveStage::Killer1 && ordered[2] == killer2 &&
               stages[2] == MoveStage::Killer2 && ordered[3] == counter &&
               stages[3] == MoveStage::CounterMove &&
               ordered[4] == bestQuiet && stages[4] == MoveStage::Quiet,
           "MovePicker stage priority and quiet partial selection");

    const Move stale(Square::E2, Square::E5);
    MovePicker stalePicker(position, histories, stale,
                           {{stale, stale}}, stale);
    std::size_t staleCount = 0;
    while (!stalePicker.next().isNone()) ++staleCount;
    expect(staleCount == 20,
           "illegal TT/killer/counter candidates are ignored");

    MovePicker quietQ(position, histories, Move::none(),
                      {{Move::none(), Move::none()}}, Move::none(),
                      PickerMode::Quiescence, 4);
    expect(quietQ.next().isNone(),
           "quiet startpos moves are excluded from normal qsearch");

    expect(load(position, root,
                "r3k3/p7/8/3q4/4P3/8/8/R3K3 w - - 0 1"),
           "load staged capture FEN");
    const Move good = moveFromUci(position, "e4d5");
    const Move bad = moveFromUci(position, "a1a7");
    MovePicker tactical(position, histories);
    MoveList tacticalLegal;
    generateLegalMoves(position, tacticalLegal);
    const Move firstTactical = tactical.next();
    expect(firstTactical == good && tactical.size() < tacticalLegal.size,
           "good-capture cutoff path avoids generating quiet moves");
    int goodOrder = -1;
    int quietOrder = -1;
    int badOrder = -1;
    MoveStage goodStage = MoveStage::Done;
    MoveStage badStage = MoveStage::Done;
    int order = 0;
    if (firstTactical == good) {
        goodOrder = order++;
        goodStage = tactical.lastStage();
    }
    for (;;) {
        const Move move = tactical.next();
        if (move.isNone()) break;
        if (move == good) {
            goodOrder = order;
            goodStage = tactical.lastStage();
        } else if (move == bad) {
            badOrder = order;
            badStage = tactical.lastStage();
        } else if (quietOrder < 0 && tactical.lastStage() == MoveStage::Quiet) {
            quietOrder = order;
        }
        ++order;
    }
    expect(goodOrder >= 0 && quietOrder >= 0 && badOrder >= 0 &&
               goodOrder < quietOrder && quietOrder < badOrder &&
               goodStage == MoveStage::GoodCapture &&
               badStage == MoveStage::BadCapture,
           "good captures precede quiets and losing captures follow them");

    MovePicker ttBad(position, histories, bad);
    expect(ttBad.next() == bad && ttBad.lastStage() == MoveStage::TT,
           "legal TT move overrides its normal capture stage");

    expect(load(position, root,
                "4k3/8/8/8/8/8/4B3/K3R3 w - - 0 1"),
           "load discovered-check qsearch FEN");
    const Move discovered = moveFromUci(position, "e2d3");
    MovePicker checkPicker(position, histories, Move::none(),
                           {{Move::none(), Move::none()}}, Move::none(),
                           PickerMode::Quiescence, 0);
    bool foundDiscovered = false;
    for (Move move = checkPicker.next(); !move.isNone();
         move = checkPicker.next()) {
        if (move == discovered) foundDiscovered = true;
    }
    expect(foundDiscovered,
           "qsearch quiet-check generation includes discovered checks");

    expect(load(position, root,
                "1r5k/P7/8/8/8/8/8/7K w - - 0 1"),
           "load attacked quiet-promotion FEN");
    const Move quietPromotion = moveFromUci(position, "a7a8q");
    MovePicker promotionPicker(position, histories);
    bool foundQuietPromotion = false;
    for (Move move = promotionPicker.next(); !move.isNone();
         move = promotionPicker.next()) {
        if (move == quietPromotion) {
            foundQuietPromotion = true;
            expect(promotionPicker.lastStage() == MoveStage::GoodCapture,
                   "quiet promotion remains in the forcing/good stage");
        }
    }
    expect(foundQuietPromotion,
           "MovePicker returns the legal quiet promotion");

    expect(load(position, root,
                "4r1k1/8/8/8/8/8/8/4K3 w - - 0 1"),
           "load MovePicker evasion FEN");
    MoveList legal;
    generateLegalMoves(position, legal);
    MovePicker evasion(position, histories, Move::none(),
                       {{Move::none(), Move::none()}}, Move::none(),
                       PickerMode::Evasion);
    std::size_t evasionCount = 0;
    while (!evasion.next().isNone()) ++evasionCount;
    expect(evasionCount == legal.size,
           "evasion picker returns every legal check evasion");
}

void testSee() {
    Position position;
    StateInfo root;

    // Undefended pawn: queen simply wins it.
    expect(load(position, root, "4k3/8/8/3p4/8/8/8/3QK3 w - - 0 1"),
           "load SEE undefended-pawn FEN");
    expect(See::see(position, moveFromUci(position, "d1d5")) == 100,
           "SEE of an undefended pawn capture is +100");
    expect(See::seeGe(position, moveFromUci(position, "d1d5"), 100),
           "seeGe accepts the exact threshold");
    expect(!See::seeGe(position, moveFromUci(position, "d1d5"), 101),
           "seeGe rejects a threshold above the true value");

    // Pawn-defended pawn: the queen is recaptured and the trade loses material.
    expect(load(position, root, "4k3/8/2p1p3/3p4/8/8/8/3QK3 w - - 0 1"),
           "load SEE queen-loses FEN");
    expect(See::see(position, moveFromUci(position, "d1d5")) == -800,
           "SEE of a queen capturing a pawn defended by a pawn is -800");

    // Equal pawn-for-pawn trade.
    expect(load(position, root, "4k3/8/2p5/3p4/4P3/8/8/4K3 w - - 0 1"),
           "load SEE equal-trade FEN");
    expect(See::see(position, moveFromUci(position, "e4d5")) == 0,
           "SEE of an equal pawn trade is 0");

    // X-ray recapture sequence: a rear rook only becomes an attacker once the
    // front rook it was blocking has moved onto the exchange square.
    expect(load(position, root, "r3k3/8/r7/8/8/R7/8/R3K3 w - - 0 1"),
           "load SEE x-ray FEN");
    expect(See::see(position, moveFromUci(position, "a3a6")) == 500,
           "SEE resolves an x-ray double-rook exchange to +500");

    // Pinned "defender": the black knight on e5 is pinned to e8 by the rook
    // on e1 and cannot legally recapture.  g6 is a square the knight would
    // otherwise defend but that the black king (far away on e8) cannot reach,
    // so a naive pin-unaware SEE would wrongly price in the knight recapture.
    expect(load(position, root, "4k3/8/6p1/4n3/8/8/8/KQ2R3 w - - 0 1"),
           "load SEE pinned-defender FEN");
    expect(See::see(position, moveFromUci(position, "b1g6")) == 100,
           "a pinned defender cannot recapture, so SEE stays +100");

    // Promotion: capturing into a queen promotion adds the promotion bonus.
    expect(load(position, root, "n6k/1P6/8/8/8/8/8/K7 w - - 0 1"),
           "load SEE promotion FEN");
    expect(See::see(position, moveFromUci(position, "b7a8q")) == 1120,
           "SEE of an undefended promoting capture includes the promotion bonus");
}

void testEngineBoundary() {
    Engine engine;
    expect(engine.options().hashMegabytes == 256 && engine.options().useNNUE &&
               engine.options().nnueOutputMode == NNUEOutputMode::Residual,
           "Engine exposes Q defaults before initialization");
    std::string error;
    const std::vector<std::string_view> noMoves;
    expect(!engine.applyMoves(noMoves, &error) &&
               engine.prepareSearch(&error) == nullptr,
           "uninitialized Engine rejects even empty work and search setup");
    expect(engine.setOption("Hash", "1", &error),
           "Engine option may be configured before initialization");
    expect(engine.initialize(&error) && engine.initialized() &&
               toFen(engine.position()) == kStartFen &&
               engine.table().sizeMegabytes() == 1,
           "Engine initializes owned Position, TT, and Worker");

    const std::vector<std::string_view> opening{
        "e2e4", "e7e5", "g1f3"
    };
    expect(engine.applyMoves(opening, &error) && engine.gamePly() == 3 &&
               toFen(engine.position()) ==
                   "rnbqkbnr/pppp1ppp/8/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 1 2",
           "Engine applies a legal UCI move list");

    const std::string beforeBadSequence = toFen(engine.position());
    const std::size_t beforeBadPly = engine.gamePly();
    const std::vector<std::string_view> badSequence{"b8c6", "e1e8"};
    expect(!engine.applyMoves(badSequence, &error) &&
               toFen(engine.position()) == beforeBadSequence &&
               engine.gamePly() == beforeBadPly,
           "Engine move-list application is transactional");

    const std::string beforeBadFen = toFen(engine.position());
    expect(!engine.setPosition("8/8/8/8/8/8/8/7X w - - 0 1", &error) &&
               toFen(engine.position()) == beforeBadFen,
           "Engine rejects malformed FEN without losing the game");

    expect(engine.setPosition(kStartFen, &error) && engine.gamePly() == 0,
           "Engine position reset rebinds root StateInfo");
    SearchWorker* workerPointer = engine.prepareSearch(&error);
    expect(workerPointer != nullptr, "Engine prepares one search session");
    SearchWorker& worker = *workerPointer;
    expect(worker.config().useNNUE && worker.config().revision ==
               engine.options().revision,
           "Engine freezes options at the search boundary");
    TTData entry;
    entry.bound = TTBound::Exact;
    entry.depth = 4;
    worker.table().store(collisionKey(0x7777), entry, 0);
    expect(worker.table().probe(collisionKey(0x7777), 0).hit,
           "Engine Worker references the owned TT");

    const std::uint64_t frozenRevision = worker.config().revision;
    expect(engine.prepareSearch(&error) == nullptr &&
               !engine.setOption("UseNNUE", "false", &error) &&
               worker.config().useNNUE &&
               worker.config().revision == frozenRevision &&
               worker.table().probe(collisionKey(0x7777), 0).hit,
           "active search rejects overlap and option/TT mutation");
    engine.stop();
    expect(worker.stopRequested(), "Engine stop reaches the Worker atomic flag");
    engine.finishSearch();
    expect(engine.setOption("UseNNUE", "false", &error) &&
               !worker.table().probe(collisionKey(0x7777), 0).hit,
           "completed search permits option commit and TT invalidation");
    SearchWorker* nextPointer = engine.prepareSearch(&error);
    expect(nextPointer != nullptr, "Engine prepares the next search session");
    SearchWorker& nextSearch = *nextPointer;
    expect(!nextSearch.config().useNNUE && !nextSearch.stopRequested(),
           "next search receives the new config and clears stop");
    nextSearch.histories().updateMain(Color::White, Square::E2, Square::E4,
                                      1000);
    expect(!engine.newGame(&error),
           "active search prevents game-state mutation");
    engine.finishSearch();
    expect(engine.newGame(&error) &&
               nextSearch.histories().mainScore(Color::White, Square::E2,
                                                Square::E4) == 0,
           "Engine newGame clears Worker history");
}

} // namespace

int main() {
    Eunshin::Random::reset();
    Eunshin::Attacks::initialize();
    Eunshin::Zobrist::initialize();

    expect(Eunshin::Attacks::initialized(), "attack tables initialized");
    expect(Eunshin::Zobrist::initialized(), "Zobrist tables initialized");

    testTypesAndMoves();
    testAttacks();
    testPerft();
    testSpecialMoves();
    testIllegalMovesAndFen();
    testNullRepetitionAndRule50();
    testRandomMakeUnmake();
    testIndependentPositions();
    testOptionsAndSearchState();
    testTranspositionTable();
    testMovePicker();
    testSee();
    testEngineBoundary();

    if (failures != 0) {
        std::cerr << failures << " of " << checks << " checks failed\n";
        return 1;
    }

    std::cout << "PASS: " << checks
              << " checks; checkpoints 1-2 core state, perft, 102400 "
                 "make/unmake plies, options, Worker, Engine boundary, "
                 "MovePicker, and clustered TT verified\n";
    return 0;
}
