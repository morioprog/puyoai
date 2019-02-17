#ifndef GUI_COMMENTATOR_H_
#define GUI_COMMENTATOR_H_

#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest_prod.h>

#include "core/column_puyo_list.h"
#include "core/core_field.h"
#include "core/kumipuyo_seq.h"
#include "core/rensa_result.h"
#include "core/rensa_tracker/rensa_chain_tracker.h"
#include "core/server/game_state_observer.h"

struct TrackedPossibleRensaInfo {
    TrackedPossibleRensaInfo() {}
    TrackedPossibleRensaInfo(const RensaResult& rensaResult,
                             const ColumnPuyoList& puyosToComplement,
                             const RensaChainTrackResult& trackResult) :
        rensaResult(rensaResult), puyosToComplement(puyosToComplement), trackResult(trackResult) {}

    int chains() const { return rensaResult.chains; }
    int score() const { return rensaResult.score; }
    int frames() const { return rensaResult.frames; }

    RensaResult rensaResult;
    ColumnPuyoList puyosToComplement;
    RensaChainTrackResult trackResult;
};

struct CommentatorResult {
    int frameId[2];
    TrackedPossibleRensaInfo fireableMainChain[2];
    IgnitionRensaResult fireableTsubushiChain[2];
    TrackedPossibleRensaInfo firingChain[2];
    std::string message[2];

    std::deque<std::string> events[2];
};

class CommentatorObserver {
public:
    virtual ~CommentatorObserver() {}
    virtual void onCommentatorResultUpdate(const CommentatorResult& result) = 0;
};

class Commentator : public GameStateObserver {
public:
    Commentator();
    virtual ~Commentator();

    // For GameStateObserver
    virtual void newGameWillStart() override;
    virtual void onUpdate(const GameState&) override;

    void addCommentatorObserver(CommentatorObserver*);

    bool start();
    void stop();
    void runLoop();

private:
    // reset() should be called when a new game has started.
    void reset();

    void update(int pi, const CoreField&, const KumipuyoSeq&);

    void addEventMessage(int pi, const std::string&);
    CommentatorResult result() const;

    std::thread th_;
    volatile bool shouldStop_;
    volatile bool hasStarted_ = false;
    volatile bool needsUpdate_[2];

    std::vector<CommentatorObserver*> observers_;

    mutable std::mutex mu_;
    CoreField field_[2];
    KumipuyoSeq kumipuyoSeq_[2];
    std::string message_[2];

    int frameId_[2];
    std::unique_ptr<TrackedPossibleRensaInfo> fireableMainChain_[2];
    std::unique_ptr<IgnitionRensaResult> fireableTsubushiChain_[2];
    std::unique_ptr<TrackedPossibleRensaInfo> firingChain_[2];

    std::deque<std::string> events_[2];

    FRIEND_TEST(CommentatorTest, getPotentialMaxChain);
};

#endif
