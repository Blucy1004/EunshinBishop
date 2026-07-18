#include "position/fen.h"

#include "core/attacks.h"
#include "core/bitboard.h"
#include "position/position.h"

#include <array>
#include <charconv>
#include <cctype>
#include <string>

namespace Eunshin {
namespace {

Piece pieceFromFen(char token) noexcept {
    const Color color = token >= 'A' && token <= 'Z' ? Color::White : Color::Black;
    const char lower = static_cast<char>(std::tolower(static_cast<unsigned char>(token)));
    PieceType type = PieceType::None;
    switch (lower) {
    case 'p': type = PieceType::Pawn; break;
    case 'n': type = PieceType::Knight; break;
    case 'b': type = PieceType::Bishop; break;
    case 'r': type = PieceType::Rook; break;
    case 'q': type = PieceType::Queen; break;
    case 'k': type = PieceType::King; break;
    default: return Piece::None;
    }
    return makePiece(color, type);
}

char fenFromPiece(Piece piece) noexcept {
    char token = '?';
    switch (typeOf(piece)) {
    case PieceType::Pawn:   token = 'p'; break;
    case PieceType::Knight: token = 'n'; break;
    case PieceType::Bishop: token = 'b'; break;
    case PieceType::Rook:   token = 'r'; break;
    case PieceType::Queen:  token = 'q'; break;
    case PieceType::King:   token = 'k'; break;
    default: return '?';
    }
    if (colorOf(piece) == Color::White)
        token = static_cast<char>(std::toupper(static_cast<unsigned char>(token)));
    return token;
}

bool parseNonNegative(std::string_view text, int& value) noexcept {
    if (text.empty()) return false;
    int parsed = 0;
    const char* const first = text.data();
    const char* const last = first + text.size();
    const auto result = std::from_chars(first, last, parsed);
    if (result.ec != std::errc{} || result.ptr != last || parsed < 0) return false;
    value = parsed;
    return true;
}

bool splitFen(std::string_view fen,
              std::array<std::string_view, 6>& fields,
              std::size_t& count) noexcept {
    count = 0;
    std::size_t cursor = 0;
    while (cursor < fen.size()) {
        while (cursor < fen.size() && fen[cursor] == ' ') ++cursor;
        if (cursor == fen.size()) break;
        if (count == fields.size()) return false;
        const std::size_t start = cursor;
        while (cursor < fen.size() && fen[cursor] != ' ') ++cursor;
        fields[count++] = fen.substr(start, cursor - start);
    }
    return count >= 4 && count <= 6;
}

bool fail(std::string* error, const char* message) {
    if (error) *error = message;
    return false;
}

} // namespace

bool setFromFen(Position& position,
                std::string_view fen,
                StateInfo& rootState,
                std::string* error) {
    if (error) error->clear();
    std::array<std::string_view, 6> fields{};
    std::size_t fieldCount = 0;
    if (!splitFen(fen, fields, fieldCount))
        return fail(error, "FEN must contain four to six fields");

    // Parse into isolated storage. A malformed FEN must never destroy the
    // engine's current game or partially overwrite its root StateInfo.
    Position candidate;
    StateInfo candidateState;
    candidate.clear(candidateState);

    int rank = 7;
    int file = 0;
    int separators = 0;
    for (char token : fields[0]) {
        if (token == '/') {
            if (file != 8 || rank == 0) return fail(error, "invalid FEN rank width");
            --rank;
            file = 0;
            ++separators;
            continue;
        }
        if (token >= '1' && token <= '8') {
            file += token - '0';
            if (file > 8) return fail(error, "FEN rank exceeds eight files");
            continue;
        }
        const Piece piece = pieceFromFen(token);
        if (!isValid(piece) || file >= 8 || rank < 0)
            return fail(error, "invalid piece placement");
        candidate.putPieceRaw(piece, makeSquare(file, rank));
        ++file;
    }
    if (rank != 0 || file != 8 || separators != 7)
        return fail(error, "FEN does not describe eight complete ranks");

    if (fields[1] == "w") candidate.sideToMove_ = Color::White;
    else if (fields[1] == "b") candidate.sideToMove_ = Color::Black;
    else return fail(error, "invalid side-to-move field");

    candidateState.castlingRights = NoCastling;
    if (fields[2] != "-") {
        for (char token : fields[2]) {
            std::uint8_t right = NoCastling;
            switch (token) {
            case 'K': right = WhiteKingSide; break;
            case 'Q': right = WhiteQueenSide; break;
            case 'k': right = BlackKingSide; break;
            case 'q': right = BlackQueenSide; break;
            default: return fail(error, "invalid castling-rights field");
            }
            if (candidateState.castlingRights & right)
                return fail(error, "duplicate castling right");
            candidateState.castlingRights = static_cast<std::uint8_t>(
                candidateState.castlingRights | right);
        }
    }

    candidateState.epSquare = Square::None;
    if (fields[3] != "-") {
        if (fields[3].size() != 2 || fields[3][0] < 'a' || fields[3][0] > 'h' ||
            fields[3][1] < '1' || fields[3][1] > '8')
            return fail(error, "invalid en-passant field");
        candidateState.epSquare = makeSquare(fields[3][0] - 'a', fields[3][1] - '1');
        const int expectedRank = candidate.sideToMove_ == Color::White ? 5 : 2;
        if (rankOf(candidateState.epSquare) != expectedRank)
            return fail(error, "en-passant rank is inconsistent with side to move");
    }

    candidateState.rule50 = 0;
    if (fieldCount >= 5 && !parseNonNegative(fields[4], candidateState.rule50))
        return fail(error, "invalid halfmove clock");

    candidateState.fullmoveNumber = 1;
    if (fieldCount >= 6 &&
        (!parseNonNegative(fields[5], candidateState.fullmoveNumber) ||
         candidateState.fullmoveNumber < 1))
        return fail(error, "invalid fullmove number");

    candidateState.previous = nullptr;
    candidateState.pliesFromNull = 0;
    candidateState.positionKey = candidate.recomputePositionKey();
    candidateState.pawnKey = candidate.recomputePawnKey();
    candidateState.materialKey = candidate.recomputeMaterialKey();
    candidateState.checkers = candidate.attackersTo(
        candidate.kingSquare(candidate.sideToMove_),
        opposite(candidate.sideToMove_));

    if (!candidate.isConsistent()) {
        const std::string message = candidate.consistencyError();
        return fail(error, message.c_str());
    }

    rootState = candidateState;
    position = candidate;
    position.state_ = &rootState;
    return true;
}

std::string toFen(const Position& position) {
    std::string result;
    result.reserve(96);
    for (int rank = 7; rank >= 0; --rank) {
        int empty = 0;
        for (int file = 0; file < 8; ++file) {
            const Piece piece = position.pieceOn(makeSquare(file, rank));
            if (!isValid(piece)) {
                ++empty;
                continue;
            }
            if (empty) {
                result.push_back(static_cast<char>('0' + empty));
                empty = 0;
            }
            result.push_back(fenFromPiece(piece));
        }
        if (empty) result.push_back(static_cast<char>('0' + empty));
        if (rank != 0) result.push_back('/');
    }

    result += position.sideToMove() == Color::White ? " w " : " b ";
    const std::uint8_t rights = position.castlingRights();
    if (rights == NoCastling) result.push_back('-');
    else {
        if (rights & WhiteKingSide) result.push_back('K');
        if (rights & WhiteQueenSide) result.push_back('Q');
        if (rights & BlackKingSide) result.push_back('k');
        if (rights & BlackQueenSide) result.push_back('q');
    }

    result.push_back(' ');
    if (isValid(position.epSquare())) {
        result.push_back(static_cast<char>('a' + fileOf(position.epSquare())));
        result.push_back(static_cast<char>('1' + rankOf(position.epSquare())));
    } else {
        result.push_back('-');
    }
    result.push_back(' ');
    result += std::to_string(position.rule50());
    result.push_back(' ');
    result += std::to_string(position.fullmoveNumber());
    return result;
}

} // namespace Eunshin
