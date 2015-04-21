#ifndef CORE_ALGORITHM_RENSA_DETECTOR_H_
#define CORE_ALGORITHM_RENSA_DETECTOR_H_

#include <functional>

#include "core/field_constant.h"

class ColumnPuyoList;
class CoreField;
class RensaChainTrackResult;
class RensaCoefResult;
class RensaVanishingPositionResult;
struct RensaResult;

class RensaDetectorStrategy {
public:
    enum class Mode {
        DROP, FLOAT, EXTEND,
    };

    RensaDetectorStrategy(Mode mode,
                          int maxNumOfComplementPuyosForKey,
                          int maxNumOfComplementPuyosForFire,
                          bool allowsPuttingKeyPuyoOn13thRow) :
        mode_(mode),
        maxNumOfComplementPuyosForKey_(maxNumOfComplementPuyosForKey),
        maxNumOfComplementPuyosForFire_(maxNumOfComplementPuyosForFire),
        allowsPuttingKeyPuyoOn13thRow_(allowsPuttingKeyPuyoOn13thRow)
    {
    }

    static RensaDetectorStrategy defaultFloatStrategy() { return RensaDetectorStrategy(Mode::FLOAT, 3, 3, true); }
    static RensaDetectorStrategy defaultDropStrategy() { return RensaDetectorStrategy(Mode::DROP, 3, 3, true); }
    static RensaDetectorStrategy defaultExtendStrategy() { return RensaDetectorStrategy(Mode::EXTEND, 3, 3, false); }

    Mode mode() const { return mode_; }
    int maxNumOfComplementPuyosForKey() const { return maxNumOfComplementPuyosForKey_; }
    int maxNumOfComplementPuyosForFire() const { return maxNumOfComplementPuyosForFire_; }
    bool allowsPuttingKeyPuyoOn13thRow() const { return allowsPuttingKeyPuyoOn13thRow_; }

private:
    Mode mode_;
    int maxNumOfComplementPuyosForKey_;
    int maxNumOfComplementPuyosForFire_;
    bool allowsPuttingKeyPuyoOn13thRow_;
};

enum class PurposeForFindingRensa {
    FOR_FIRE,
    FOR_KEY,
};

// RensaDetector is a set of functions to find a rensa from the specified field.
// Using iteratePossibleRensasIteratively() is recommended for most cases.
class RensaDetector {
public:
    typedef std::function<void (CoreField*, const ColumnPuyoList&)> SimulationCallback;
    // Detects a rensa from the field. The ColumnPuyoList to fire a rensa will be passed to
    // |callback|. Note that invalid column puyo list might be passed to |callback|.
    // The field that puyo list is added is also passed to DetectionCallback.
    static void detect(const CoreField&,
                       const RensaDetectorStrategy&,
                       PurposeForFindingRensa,
                       const bool prohibits[FieldConstant::MAP_WIDTH],
                       const SimulationCallback& callback);


    typedef std::function<void (const CoreField&,
                                const RensaResult&,
                                const ColumnPuyoList&)> RensaCallback;
    template<typename TrackResult>
    using TrackedRensaCallback = std::function<void (const CoreField&,
                                                     const RensaResult&,
                                                     const ColumnPuyoList&,
                                                     const TrackResult&)>;

    // TODO(mayah): Consider simplify these methods.

    // Finds rensa from the specified field. We put |maxKeyPuyo| puyos as key puyo.
    static void iteratePossibleRensas(const CoreField&,
                                      int maxKeyPuyo,
                                      const RensaDetectorStrategy&,
                                      const RensaCallback&);

    typedef TrackedRensaCallback<RensaChainTrackResult> TrackedPossibleRensaCallback;
    static void iteratePossibleRensasWithTracking(const CoreField&,
                                                  int maxKeyPuyos,
                                                  const RensaDetectorStrategy&,
                                                  const TrackedPossibleRensaCallback&);

    typedef TrackedRensaCallback<RensaCoefResult> CoefPossibleRensaCallback;
    static void iteratePossibleRensasWithCoefTracking(const CoreField&,
                                                      int maxKeyPuyos,
                                                      const RensaDetectorStrategy&,
                                                      const CoefPossibleRensaCallback&);

    typedef TrackedRensaCallback<RensaVanishingPositionResult> VanishingPositionPossibleRensaCallback;
    static void iteratePossibleRensasWithVanishingPositionTracking(const CoreField&,
                                                                   int maxKeyPuyos,
                                                                   const RensaDetectorStrategy&,
                                                                   const VanishingPositionPossibleRensaCallback&);

    // Finds a rensa from CoreField. TrackedPossibleRensaCallback is called for all detected rensas.
    // Algorithm is like the following (not accurate):
    // 1. Vanish puyos from CoreField.
    // 2. After (1), Also vanish puyos from CoreField. The complemneted puyos to fire this rensa are considered
    //    as key puyos. Iterate this (maxIteration - 1) times.
    // 3. Complement all puyos for (1) and (2), and check the rensa is not corrupted. If not corrupted,
    //    call the callback.
    static void iteratePossibleRensasIteratively(const CoreField&,
                                                 int maxIteration,
                                                 const RensaDetectorStrategy&,
                                                 const TrackedPossibleRensaCallback&);

    static void makeProhibitArray(const RensaResult&,
                                  const RensaChainTrackResult&,
                                  const CoreField& originalField,
                                  const ColumnPuyoList& firePuyos,
                                  bool prohibits[FieldConstant::MAP_WIDTH]);
};

#endif
