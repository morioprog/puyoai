#ifndef CORE_CLIENT_AI_H_
#define CORE_CLIENT_AI_H_

#include <string>

#include "core/client/ai/drop_decision.h"
#include "core/client/connector/client_connector.h"
#include "core/field/core_field.h"
#include "core/field/rensa_result.h"
#include "core/kumipuyo_seq.h"

struct FrameRequest;
class PlainField;

// TODO(mayah): This struct will contain zenkeshi info etc.
class AdditionalThoughtInfo {
public:
    bool isRensaOngoing() const { return isRensaOngoing_; }
    const RensaResult& ongoingRensaResult() const { return ongoingRensaResult_; }
    int ongoingRensaFinishingFrameId() const { return finishingRensaFrameId_; }

    void setOngoingRensa(const RensaResult&, int finishingFrameId);
    void unsetOngoingRensa();

    void setHasZenkeshi(bool flag) { hasZenkeshi_ = flag; }
    bool hasZenkeshi() const { return hasZenkeshi_; }

    void setEnemyHasZenkeshi(bool flag) { enemyHasZenkeshi_ = flag; }
    bool enemyHasZenkeshi() const { return enemyHasZenkeshi_; }

private:
    bool hasZenkeshi_ = false;
    bool enemyHasZenkeshi_ = false;

    bool isRensaOngoing_ = false;
    int finishingRensaFrameId_ = 0;
    RensaResult ongoingRensaResult_;
};

// AI is a utility class of AI.
// You need to implement think() at least.
class AI {
public:
    virtual ~AI();
    const std::string& name() const { return name_; }

    void runLoop();

protected:
    AI(int argc, char* argv[], const std::string& name);
    explicit AI(const std::string& name);

    // |think| will be called when AI should decide the next decision.
    // Basically, this will be called when NEXT2 has appeared.
    // You will have around 300 ms to decide your hand.
    virtual DropDecision think(int frameId, const PlainField&, const KumipuyoSeq&,
                               const AdditionalThoughtInfo&) = 0;

    // |thinkFast| will be called when AI should decide the next decision immediately.
    // Basically this will be called when ojamas are dropped or the enemy has fired some rensa.
    // You will have around 30 ms to decide your hand.
    virtual DropDecision thinkFast(int frameId, const PlainField& field, const KumipuyoSeq& next,
                                   const AdditionalThoughtInfo& info)
    {
        return think(frameId, field, next, info);
    }

    // |gaze| will be called when AI should gaze the enemy's field.
    virtual void gaze(int frameId, const CoreField& enemyField, const KumipuyoSeq&);

    void setBehaviorDefensive(bool flag) { behaviorDefensive_ = flag; }
    void setBehaviorRethinkAfterOpponentRensa(bool flag) { behaviorRethinkAfterOpponentRensa_ = flag; }

    const AdditionalThoughtInfo& additionalThoughtInfo() const { return additionalThoughtInfo_; }

    // These callbacks will be called from the corresponding method.
    // i.e. onXXXYYY() will be called from XXXYYY().
    virtual void onGameWillBegin(const FrameRequest&) {}
    virtual void onGameHasEnded(const FrameRequest&) {}
    virtual void onDecisionRequested(const FrameRequest&) {}
    virtual void onGrounded(const FrameRequest&) {}
    virtual void onNext2Appeared(const FrameRequest&) {}
    virtual void onEnemyDecisionRequested(const FrameRequest&) {}
    virtual void onEnemyGrounded(const FrameRequest&) {}
    virtual void onEnemyNext2Appeared(const FrameRequest&) {}

    // |gameWillBegin| will be called just before a new game will begin.
    // FrameRequest might contain NEXT and NEXT2 puyos, but it's not guaranteed.
    // Please initialize your AI in this function.
    void gameWillBegin(const FrameRequest&);

    // |gameHasEnded| will be called just after a game has ended.
    void gameHasEnded(const FrameRequest&);

    void decisionRequested(const FrameRequest&);
    void grounded(const FrameRequest&);
    void next2Appeared(const FrameRequest&);

    // When enemy will start to move puyo, this callback will be called.
    void enemyDecisionRequested(const FrameRequest&);

    // When enemy's puyo is grounded, this callback will be called.
    // Enemy's rensa is automatically checked, so you don't need to do that. (Use AdditionalThoughtInfo)
    void enemyGrounded(const FrameRequest&);

    // When enemy's NEXT2 has appeared, this callback will be called.
    // You can update the enemy information here.
    void enemyNext2Appeared(const FrameRequest&);

    // Should rethink just before sending next decision.
    void requestRethink() { rethinkRequested_ = true; }

private:
    friend class AITest;
    friend class Endless;
    friend class Solver;

    static bool isFieldInconsistent(const PlainField& ours, const PlainField& provided);
    static void mergeField(CoreField* ours, const PlainField& provided);

    KumipuyoSeq rememberedSequence(int indexFrom) const;

    void resetCurrentField(const CoreField&);

    std::string name_;
    ClientConnector connector_;
    CoreField field_;  // estimated my field.
    KumipuyoSeq seq_;  // remember kumipuyo sequence.
    AdditionalThoughtInfo additionalThoughtInfo_;
    int hand_;
    int enemyHand_;
    bool rethinkRequested_;

    int enemyDecisionRequestFrameId_;
    CoreField enemyField_; // remember enemy's field
    KumipuyoSeq enemySeq_; // remember enemy's kumipuyo sequence.

    bool behaviorDefensive_;
    bool behaviorRethinkAfterOpponentRensa_;
};

#endif
