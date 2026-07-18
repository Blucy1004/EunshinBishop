#include "search/search.h"

#include "core/attacks.h"
#include "core/bitboard.h"
#include "eval/evaluator.h"
#include "position/movegen.h"
#include "position/position.h"
#include "search/aegis.h"
#include "search/history.h"
#include "search/move_picker.h"
#include "search/search_config.h"
#include "search/tt.h"
#include "search/worker.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <thread>

namespace Eunshin {
namespace {

constexpr int MAX_ITERATIVE_DEPTH = MAX_PLY - 8;
constexpr std::uint64_t CLOCK_CHECK_INTERVAL = 64;
constexpr Value MATE_SCORE_THRESHOLD = MATE - MAX_PLY;

struct NodeResult final {
    Value value = VALUE_NONE;
    bool aborted = false;

    [[nodiscard]] static NodeResult complete(Value score) noexcept {
        return NodeResult{score, false};
    }

    [[nodiscard]] static NodeResult abort() noexcept {
        return NodeResult{VALUE_NONE, true};
    }
};

[[nodiscard]] constexpr int pieceValue(PieceType type) noexcept {
    switch (type) {
    case PieceType::Pawn:   return 100;
    case PieceType::Knight: return 320;
    case PieceType::Bishop: return 330;
    case PieceType::Rook:   return 500;
    case PieceType::Queen:  return 900;
    case PieceType::King:   return 20000;
    default:                return 0;
    }
}

[[nodiscard]] bool isCapture(const Position& position, Move move) noexcept {
    return move.isEnPassant() || isValid(position.pieceOn(move.to()));
}

[[nodiscard]] PieceType capturedType(const Position& position,
                                     Move move) noexcept {
    return move.isEnPassant() ? PieceType::Pawn
                              : typeOf(position.pieceOn(move.to()));
}

[[nodiscard]] bool hasNonPawnMaterial(const Position& position,
                                      Color color) noexcept {
    return (position.pieces(color, PieceType::Knight) |
            position.pieces(color, PieceType::Bishop) |
            position.pieces(color, PieceType::Rook) |
            position.pieces(color, PieceType::Queen)) != EMPTY_BB;
}

[[nodiscard]] bool isAdvancedPawnMove(const Position& position,
                                      Move move) noexcept {
    if (typeOf(position.pieceOn(move.from())) != PieceType::Pawn)
        return false;
    const int rank = rankOf(move.to());
    return move.isPromotion() ||
        (position.sideToMove() == Color::White ? rank >= 6 : rank <= 1);
}

[[nodiscard]] bool touchesEnemyKingRing(const Position& position,
                                        Move move) noexcept {
    const Square enemyKing = position.kingSquare(
        opposite(position.sideToMove()));
    return isValid(enemyKing) &&
        (Attacks::king(enemyKing) & bit(move.to())) != EMPTY_BB;
}

class SearchContext final {
public:
    SearchContext(Position& position,
                  SearchWorker& worker,
                  const SearchLimits& limits,
                  SearchInfoCallback callback,
                  void* userData) noexcept
        : position_(position),
          worker_(worker),
          limits_(limits),
          callback_(callback),
          userData_(userData) {
        const int gamePly = std::max(0, (position_.fullmoveNumber() - 1) * 2 +
            (position_.sideToMove() == Color::Black ? 1 : 0));
        timeManager_.initialize(limits_, position_.sideToMove(), gamePly,
                                worker_.config().moveOverheadMs);
        for (int ply = 0; ply < MAX_PLY; ++ply) clearPv(ply);
    }

    [[nodiscard]] SearchResult run() noexcept {
        SearchResult result;
        worker_.statistics().clear();

        if (!worker_.hasEvaluator()) {
            result.bestMove = legalFallback();
            result.stopReason = StopReason::NoEvaluator;
            return result;
        }

        if (worker_.stopRequested()) stopReason_ = StopReason::ExternalStop;
        else if (timeManager_.hardTimeReached()) stopReason_ = StopReason::TimeLimit;

        const int maximumDepth = limits_.depth > 0
            ? std::min(limits_.depth, MAX_ITERATIVE_DEPTH)
            : MAX_ITERATIVE_DEPTH;

        Value previousScore = 0;
        Value scoreBeforePrevious = 0;
        Move previousBest = Move::none();
        bool havePrevious = false;
        bool haveTwoScores = false;
        bool guardNextIteration = false;

        for (int depth = 1; depth <= maximumDepth && stopReason_ == StopReason::None;
             ++depth) {
            if (worker_.stopRequested()) {
                stopReason_ = StopReason::ExternalStop;
                break;
            }
            if (timeManager_.hardTimeReached()) {
                stopReason_ = StopReason::TimeLimit;
                break;
            }

            rootIterationUnstable_ = worker_.config().useAEGIS &&
                                     guardNextIteration;
            aspirationFailures_ = 0;
            rootMoveChanges_ = 0;

            Value alpha = -INF;
            Value beta = INF;
            if (depth >= 5 && havePrevious) {
                alpha = std::max(-INF, previousScore - 30);
                beta = std::min(INF, previousScore + 30);
            }

            NodeResult iteration;
            Move iterationBest = Move::none();
            PVLine iterationPv;
            for (;;) {
                rootBestMove_ = Move::none();
                rootBestScore_ = -INF;
                rootSecondScore_ = -INF;
                clearPv(0);

                iteration = alphaBeta(alpha, beta, depth, 0,
                                      NodeType::Root, true);
                if (iteration.aborted) break;

                iterationBest = rootBestMove_;
                iterationPv = copyPv(0);
                if (iteration.value <= alpha && alpha > -INF) {
                    alpha = std::max(-INF, alpha - 120);
                    ++aspirationFailures_;
                    continue;
                }
                if (iteration.value >= beta && beta < INF) {
                    beta = std::min(INF, beta + 120);
                    ++aspirationFailures_;
                    continue;
                }
                break;
            }

            if (iteration.aborted) break;

            // Root terminal nodes have no move.  Otherwise the direct PV must
            // begin with the legal root move selected by this completed pass.
            if (!iterationBest.isNone() &&
                (iterationPv.length == 0 || iterationPv.first() != iterationBest)) {
                iterationPv.clear();
                iterationPv.moves[0] = iterationBest;
                iterationPv.length = 1;
            }

            result.bestMove = iterationBest;
            result.ponder = iterationPv.ponder();
            result.score = iteration.value;
            result.completedDepth = depth;
            result.selDepth = selectiveDepth_;
            result.pv = iterationPv;
            result.completedAnyIteration = true;

            const bool rootGap = rootBestScore_ > -MATE_SCORE_THRESHOLD &&
                                 rootSecondScore_ > -MATE_SCORE_THRESHOLD &&
                                 rootBestScore_ - rootSecondScore_ >= 140;
            IterationInfo completed;
            completed.depth = depth;
            completed.bestMove = iterationBest;
            completed.score = iteration.value;
            completed.rootMoveChanges = rootMoveChanges_;
            completed.aspirationFailures = aspirationFailures_;
            completed.aegisRootUnstable = rootIterationUnstable_ ||
                                          (worker_.config().useAEGIS && rootGap);
            completed.limboRootRisk = false;
            timeManager_.recordCompletedIteration(completed);

            result.nodes = worker_.statistics().nodes;
            result.qnodes = worker_.statistics().qnodes;
            result.timeMs = timeManager_.elapsedTimeMs();
            emitInfo(result);

            const bool scoreSwing = havePrevious &&
                std::abs(iteration.value - previousScore) >= 70;
            const bool bestChanged = havePrevious && depth >= 5 &&
                !previousBest.isNone() && !iterationBest.isNone() &&
                previousBest != iterationBest;
            guardNextIteration = scoreSwing || bestChanged || rootGap;

            if (havePrevious) {
                scoreBeforePrevious = previousScore;
                haveTwoScores = true;
            }
            previousScore = iteration.value;
            previousBest = iterationBest;
            havePrevious = true;

            if (std::abs(iteration.value) >= MATE_SCORE_THRESHOLD) {
                stopReason_ = StopReason::MateFound;
                break;
            }
            if (depth == maximumDepth) {
                if (limits_.depth > 0)
                    stopReason_ = StopReason::DepthLimit;
                break;
            }
            if (!timeManager_.shouldStartNextIteration()) {
                stopReason_ = StopReason::TimeLimit;
                break;
            }

            // Keep the variable live in optimized and diagnostic builds: the
            // two-score history is intentionally retained for future bounded
            // root-stability diagnostics without changing search behavior.
            (void)scoreBeforePrevious;
            (void)haveTwoScores;
        }

        // A protocol-level unbounded search owns the worker until an explicit
        // stop. Reaching the engine's safe representable depth is not itself a
        // UCI stop condition; retain the last completed result and wait rather
        // than wrapping depth or recursing beyond MAX_PLY.
        const bool hasClockLimit = limits_.moveTimeMs >= 0 ||
                                   limits_.whiteTimeMs >= 0 ||
                                   limits_.blackTimeMs >= 0;
        const bool unbounded = limits_.depth == 0 && limits_.nodes == 0 &&
                               (limits_.infinite || !hasClockLimit);
        if (stopReason_ == StopReason::None && unbounded) {
            while (!worker_.stopRequested())
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            stopReason_ = StopReason::ExternalStop;
        }

        if (result.bestMove.isNone()) {
            result.bestMove = legalFallback();
            result.ponder = Move::none();
            if (!result.completedAnyIteration && !result.bestMove.isNone()) {
                result.pv.clear();
                result.pv.moves[0] = result.bestMove;
                result.pv.length = 1;
            }
        }

        if (result.bestMove.isNone()) stopReason_ = StopReason::NoLegalMoves;

        result.nodes = worker_.statistics().nodes;
        result.qnodes = worker_.statistics().qnodes;
        result.selDepth = selectiveDepth_;
        result.timeMs = timeManager_.elapsedTimeMs();
        result.stopReason = stopReason_;
        if (result.stopReason == StopReason::None)
            result.stopReason = StopReason::DepthLimit;
        return result;
    }

private:
    [[nodiscard]] bool pollStop(bool quiescence, int ply) noexcept {
        SearchStatistics& statistics = worker_.statistics();
        selectiveDepth_ = std::max(selectiveDepth_, ply);

        if (worker_.stopRequested()) {
            stopReason_ = StopReason::ExternalStop;
            return true;
        }
        if (limits_.nodes > 0 && statistics.nodes >= limits_.nodes) {
            stopReason_ = StopReason::NodeLimit;
            return true;
        }

        ++statistics.nodes;
        if (quiescence) ++statistics.qnodes;

        if ((statistics.nodes % CLOCK_CHECK_INTERVAL) == 0 &&
            timeManager_.hardTimeReached()) {
            stopReason_ = StopReason::TimeLimit;
            return true;
        }
        return false;
    }

    [[nodiscard]] Value evaluate() noexcept {
        return worker_.evaluator()->evaluate(position_, worker_.config()).finalScore;
    }

    void updateAccumulator() noexcept {
        // Disabled NNUE has no accumulator work in the hot path.
        if (worker_.config().useNNUE)
            (void)worker_.evaluator()->updateAccumulatorAfterMove(
                position_, worker_.config());
    }

    [[nodiscard]] NodeResult quiescence(Value alpha,
                                        Value beta,
                                        int ply,
                                        int qply) noexcept {
        clearPv(ply);
        if (pollStop(true, ply)) return NodeResult::abort();
        if (position_.isFiftyMoveDraw() || position_.isRepetition())
            return NodeResult::complete(0);
        if (ply >= MAX_PLY - 2) return NodeResult::complete(evaluate());

        const bool inCheck = position_.inCheck();
        if (inCheck) ++worker_.statistics().qCheckNodes;

        Value standPat = -INF;
        Value best = -INF;
        if (!inCheck) {
            standPat = evaluate();
            best = standPat;
            if (standPat >= beta) return NodeResult::complete(standPat);
            if (standPat > alpha) alpha = standPat;
        }

        MovePicker picker(position_, worker_.histories(), Move::none(),
                          {{Move::none(), Move::none()}}, Move::none(),
                          PickerMode::Quiescence, qply);
        int legal = 0;
        for (Move move = picker.next(); !move.isNone(); move = picker.next()) {
            const bool capture = isCapture(position_, move);
            const PieceType captured = capturedType(position_, move);
            const int promotionGain = move.isPromotion()
                ? pieceValue(move.promotionType()) - pieceValue(PieceType::Pawn)
                : 0;

            StateInfo& childState = worker_.stateAt(ply + 1);
            if (!position_.doMove(move, childState)) continue;
            updateAccumulator();
            const bool givesCheck = position_.inCheck();

            if (!inCheck && capture && !move.isPromotion() && !givesCheck &&
                standPat + pieceValue(captured) + promotionGain + 200 <= alpha) {
                position_.undoMove(move);
                continue;
            }

            ++legal;
            NodeResult child = quiescence(-beta, -alpha, ply + 1, qply + 1);
            position_.undoMove(move);
            if (child.aborted) return child;
            if (worker_.stopRequested()) {
                stopReason_ = StopReason::ExternalStop;
                return NodeResult::abort();
            }

            const Value score = -child.value;
            if (score > best) best = score;
            if (score > alpha) {
                alpha = score;
                setPv(ply, move);
                if (alpha >= beta) break;
            }
        }

        if (inCheck && legal == 0) {
            ++worker_.statistics().qCheckmates;
            return NodeResult::complete(-MATE + ply);
        }
        if (!inCheck && legal == 0 && !hasAnyLegalMove())
            return NodeResult::complete(0);
        return NodeResult::complete(best == -INF ? alpha : std::max(best, alpha));
    }

    [[nodiscard]] NodeResult alphaBeta(Value alpha,
                                       Value beta,
                                       Depth depth,
                                       int ply,
                                       NodeType nodeType,
                                       bool allowNull) noexcept {
        if (depth <= 0) return quiescence(alpha, beta, ply, 0);
        clearPv(ply);
        if (pollStop(false, ply)) return NodeResult::abort();
        if (ply >= MAX_PLY - 4) return quiescence(alpha, beta, ply, 0);

        const bool root = nodeType == NodeType::Root;
        const bool pvNode = nodeType != NodeType::NonPV;
        SearchStack& stack = worker_.stackAt(ply);
        stack = SearchStack{};
        stack.ply = ply;

        if (!root) {
            if (position_.isFiftyMoveDraw() || position_.isRepetition())
                return NodeResult::complete(0);

            alpha = std::max(alpha, -MATE + ply);
            beta = std::min(beta, MATE - ply - 1);
            if (alpha >= beta) return NodeResult::complete(alpha);
        }

        const bool inCheck = position_.inCheck();
        stack.inCheck = inCheck;
        if (inCheck) ++depth;

        const Value originalAlpha = alpha;
        TTProbe tt = worker_.table().probe(position_.key(), ply);
        Move ttMove = Move::none();
        if (tt.hit) {
            ++worker_.statistics().ttHits;
            ttMove = tt.data.move;
            stack.ttValue = tt.data.value;
            stack.ttPv = tt.data.pv;
            if (!root && !pvNode && tt.data.depth >= depth) {
                if (tt.data.bound == TTBound::Exact ||
                    (tt.data.bound == TTBound::Lower && tt.data.value >= beta) ||
                    (tt.data.bound == TTBound::Upper && tt.data.value <= alpha))
                    return NodeResult::complete(tt.data.value);
            }
        }

        const Value staticEval = inCheck ? VALUE_NONE : evaluate();
        stack.staticEval = staticEval;

        AegisAssessment aegis;
        if (worker_.config().useAEGIS && !inCheck) {
            const Value parentEval = ply > 0
                ? worker_.stackAt(ply - 1).staticEval : VALUE_NONE;
            aegis = assessAegis(position_, staticEval, tt, parentEval,
                                root && rootIterationUnstable_);
            recordAegis(aegis);
            stack.instability = aegis.signals;
        }

        if (worker_.config().useIIR && !root && !inCheck && ttMove.isNone() &&
            depth >= 6) {
            if (worker_.config().useAEGIS && aegisBlocksIIR(aegis))
                ++worker_.statistics().aegisIIRBlocks;
            else
                --depth;
        }

        const int aegisMargin = worker_.config().useAEGIS
            ? aegisMarginAllowance(aegis) : 0;

        if (!pvNode && !inCheck && depth <= 6 &&
            std::abs(beta) < MATE - 256 &&
            staticEval - (85 * depth + aegisMargin) >= beta)
            return NodeResult::complete(staticEval);

        if (!pvNode && allowNull && !inCheck && depth >= 3 &&
            hasNonPawnMaterial(position_, position_.sideToMove()) &&
            staticEval >= beta) {
            StateInfo& nullState = worker_.stateAt(ply + 1);
            if (position_.doNullMove(nullState)) {
                const int reduction = 2 + depth / 6;
                NodeResult child = alphaBeta(-beta, -beta + 1,
                                             depth - 1 - reduction, ply + 1,
                                             NodeType::NonPV, false);
                position_.undoNullMove();
                if (child.aborted) return child;
                const Value score = -child.value;
                if (score >= beta && score < MATE - 256)
                    return NodeResult::complete(beta);
            }
        }

        Move counter = Move::none();
        const StateInfo* currentState = position_.state();
        if (currentState && currentState->previous && !currentState->nullMove &&
            isValid(currentState->movedPiece) && !currentState->move.isNone()) {
            counter = worker_.histories().counterMove(
                position_.sideToMove(), typeOf(currentState->movedPiece),
                currentState->move.to());
        }

        MovePicker picker(position_, worker_.histories(), ttMove, stack.killers,
                          counter, inCheck ? PickerMode::Evasion
                                           : PickerMode::MainSearch);
        const bool futile = !pvNode && !inCheck && depth <= 4 &&
            std::abs(alpha) < MATE - 256 &&
            staticEval + 90 * depth + 80 + aegisMargin <= alpha;

        int legal = 0;
        Value bestScore = -INF;
        Value secondBest = -INF;
        Move bestMove = Move::none();
        bool aegisReliefUsed = false;
        bool scoreGapRecorded = false;

        for (Move move = picker.next(); !move.isNone(); move = picker.next()) {
            const Color us = position_.sideToMove();
            const Piece movedPiece = position_.pieceOn(move.from());
            const PieceType movedType = typeOf(movedPiece);
            const PieceType captureType = capturedType(position_, move);
            const bool capture = isCapture(position_, move);
            const bool quiet = !capture && !move.isPromotion();
            const bool advancedPawn = isAdvancedPawnMove(position_, move);
            const bool kingRingMove = touchesEnemyKingRing(position_, move);
            const bool recapture = capture && ply > 0 &&
                !worker_.stackAt(ply - 1).currentMove.isNone() &&
                worker_.stackAt(ply - 1).currentMove.to() == move.to();

            StateInfo& childState = worker_.stateAt(ply + 1);
            if (!position_.doMove(move, childState)) continue;
            updateAccumulator();
            const bool givesCheck = position_.inCheck();

            if (futile && quiet && legal > 0 && !givesCheck &&
                !kingRingMove && !advancedPawn) {
                position_.undoMove(move);
                continue;
            }

            ++legal;
            stack.currentMove = move;
            stack.moveCount = legal;

            int extension = 0;
            if (advancedPawn && depth >= 3 && ply < 80) extension = 1;
            if (recapture && depth >= 4 && ply < 80) extension = 1;
            if (quiet && kingRingMove && depth >= 5 && legal <= 10 && ply < 80)
                extension = 1;

            NodeResult child;
            if (legal == 1) {
                const NodeType childType = pvNode ? NodeType::PV
                                                  : NodeType::NonPV;
                child = alphaBeta(-beta, -alpha, depth - 1 + extension,
                                  ply + 1, childType, true);
            } else {
                int reduction = 0;
                if (depth >= 3 && legal > 3 && quiet && !inCheck &&
                    !givesCheck && !kingRingMove && !advancedPawn && !recapture) {
                    reduction = static_cast<int>(0.5 +
                        std::log(static_cast<double>(depth)) *
                        std::log(static_cast<double>(legal)) / 2.4);
                    if (pvNode && reduction > 0) --reduction;
                    reduction = std::max(0, std::min(reduction, depth - 1));
                }

                if (worker_.config().useAEGIS && legal <= 12 &&
                    bestScore > -MATE + 256 && secondBest > -MATE + 256 &&
                    bestScore - secondBest >= 180) {
                    if (!scoreGapRecorded) {
                        addAegisScoreGap(aegis);
                        stack.instability = aegis.signals;
                        ++worker_.statistics().aegisTriggers;
                        ++worker_.statistics().aegisSignals[5];
                        scoreGapRecorded = true;
                    }
                    const int before = reduction;
                    reduction = aegisRelieveReduction(
                        reduction, aegis, aegisReliefUsed);
                    if (reduction < before)
                        ++worker_.statistics().aegisLMRReliefs;
                } else if (worker_.config().useAEGIS) {
                    const int before = reduction;
                    reduction = aegisRelieveReduction(
                        reduction, aegis, aegisReliefUsed);
                    if (reduction < before)
                        ++worker_.statistics().aegisLMRReliefs;
                }

                child = alphaBeta(-alpha - 1, -alpha,
                                  depth - 1 - reduction + extension,
                                  ply + 1, NodeType::NonPV, true);
                if (!child.aborted && -child.value > alpha && reduction > 0) {
                    child = alphaBeta(-alpha - 1, -alpha,
                                      depth - 1 + extension, ply + 1,
                                      NodeType::NonPV, true);
                }
                if (!child.aborted && -child.value > alpha &&
                    -child.value < beta) {
                    child = alphaBeta(-beta, -alpha,
                                      depth - 1 + extension, ply + 1,
                                      NodeType::PV, true);
                }
            }

            position_.undoMove(move);
            if (child.aborted) return child;
            if (worker_.stopRequested()) {
                stopReason_ = StopReason::ExternalStop;
                return NodeResult::abort();
            }

            const Value score = -child.value;
            if (score > bestScore) {
                secondBest = bestScore;
                bestScore = score;
                bestMove = move;
                setPv(ply, move);
                if (root) {
                    if (!rootBestMove_.isNone() && rootBestMove_ != move)
                        ++rootMoveChanges_;
                    rootBestMove_ = move;
                    rootBestScore_ = bestScore;
                    rootSecondScore_ = secondBest;
                }
            } else if (score > secondBest) {
                secondBest = score;
                if (root) rootSecondScore_ = secondBest;
            }

            if (score > alpha) {
                alpha = score;
                if (alpha >= beta) {
                    ++worker_.statistics().betaCutoffs;
                    if (quiet) {
                        if (stack.killers[0] != move) {
                            stack.killers[1] = stack.killers[0];
                            stack.killers[0] = move;
                        }
                        worker_.histories().updateMain(
                            us, move.from(), move.to(), depth * depth);
                        if (currentState && currentState->previous &&
                            !currentState->nullMove &&
                            isValid(currentState->movedPiece) &&
                            !currentState->move.isNone()) {
                            worker_.histories().setCounterMove(
                                us, typeOf(currentState->movedPiece),
                                currentState->move.to(), move);
                        }
                    } else if (capture) {
                        worker_.histories().updateCapture(
                            us, movedType, move.to(), captureType,
                            depth * depth);
                    }
                    break;
                }
            }
        }

        stack.currentMove = Move::none();
        if (legal == 0)
            return NodeResult::complete(inCheck ? -MATE + ply : 0);

        const TTBound bound = bestScore >= beta ? TTBound::Lower
            : bestScore > originalAlpha ? TTBound::Exact : TTBound::Upper;
        TTData data;
        data.move = bestMove;
        data.value = bestScore;
        data.eval = staticEval;
        data.depth = depth;
        data.bound = bound;
        data.pv = pvNode;
        worker_.table().store(position_.key(), data, ply);
        return NodeResult::complete(bestScore);
    }

    void recordAegis(const AegisAssessment& assessment) noexcept {
        if (!assessment.unstable()) return;
        SearchStatistics& statistics = worker_.statistics();
        ++statistics.aegisTriggers;
        constexpr std::array<std::uint16_t, 6> SIGNALS{{
            TTEvalDisagreement, EvaluationSwing, KingPressure,
            PasserRace, RootIterationSwing, ScoreGap
        }};
        for (std::size_t index = 0; index < SIGNALS.size(); ++index) {
            if ((assessment.signals & SIGNALS[index]) != 0)
                ++statistics.aegisSignals[index];
        }
    }

    void clearPv(int ply) noexcept {
        if (ply < 0 || ply >= MAX_PLY) return;
        pvLength_[static_cast<std::size_t>(ply)] = 0;
        pvTable_[static_cast<std::size_t>(ply)].fill(Move::none());
    }

    void setPv(int ply, Move move) noexcept {
        if (ply < 0 || ply >= MAX_PLY || move.isNone()) return;
        auto& line = pvTable_[static_cast<std::size_t>(ply)];
        line[0] = move;
        const int childLength = ply + 1 < MAX_PLY
            ? pvLength_[static_cast<std::size_t>(ply + 1)] : 0;
        const int copied = std::min(childLength, MAX_PLY - 1);
        for (int index = 0; index < copied; ++index) {
            line[static_cast<std::size_t>(index + 1)] =
                pvTable_[static_cast<std::size_t>(ply + 1)]
                        [static_cast<std::size_t>(index)];
        }
        pvLength_[static_cast<std::size_t>(ply)] = copied + 1;
    }

    [[nodiscard]] PVLine copyPv(int ply) const noexcept {
        PVLine result;
        if (ply < 0 || ply >= MAX_PLY) return result;
        result.length = std::max(0, std::min(
            pvLength_[static_cast<std::size_t>(ply)], MAX_PLY));
        for (int index = 0; index < result.length; ++index) {
            result.moves[static_cast<std::size_t>(index)] =
                pvTable_[static_cast<std::size_t>(ply)]
                        [static_cast<std::size_t>(index)];
        }
        return result;
    }

    [[nodiscard]] Move legalFallback() noexcept {
        MoveList pseudo;
        generateMoves(position_, pseudo, GenerationType::All);
        for (std::size_t index = 0; index < pseudo.size; ++index) {
            StateInfo probe;
            if (!position_.doMove(pseudo[index], probe)) continue;
            position_.undoMove(pseudo[index]);
            return pseudo[index];
        }
        return Move::none();
    }

    [[nodiscard]] bool hasAnyLegalMove() noexcept {
        MoveList pseudo;
        generateMoves(position_, pseudo, GenerationType::All);
        for (std::size_t index = 0; index < pseudo.size; ++index) {
            StateInfo probe;
            if (!position_.doMove(pseudo[index], probe)) continue;
            position_.undoMove(pseudo[index]);
            return true;
        }
        return false;
    }

    void emitInfo(const SearchResult& result) noexcept {
        if (!callback_) return;
        SearchInfo info;
        info.depth = result.completedDepth;
        info.selDepth = result.selDepth;
        info.score = result.score;
        info.nodes = result.nodes;
        info.qnodes = result.qnodes;
        info.timeMs = result.timeMs;
        info.hashfull = worker_.table().hashfull();
        info.pv = result.pv;
        callback_(info, userData_);
    }

    Position& position_;
    SearchWorker& worker_;
    SearchLimits limits_{};
    SearchInfoCallback callback_ = nullptr;
    void* userData_ = nullptr;
    TimeManager timeManager_{};

    std::array<std::array<Move, MAX_PLY>, MAX_PLY> pvTable_{};
    std::array<int, MAX_PLY> pvLength_{};

    StopReason stopReason_ = StopReason::None;
    int selectiveDepth_ = 0;
    int aspirationFailures_ = 0;
    int rootMoveChanges_ = 0;
    bool rootIterationUnstable_ = false;
    Move rootBestMove_ = Move::none();
    Value rootBestScore_ = -INF;
    Value rootSecondScore_ = -INF;
};

} // namespace

SearchResult runSearch(Position& position,
                       SearchWorker& worker,
                       const SearchLimits& limits,
                       SearchInfoCallback callback,
                       void* userData) noexcept {
    SearchContext context(position, worker, limits, callback, userData);
    return context.run();
}

} // namespace Eunshin
