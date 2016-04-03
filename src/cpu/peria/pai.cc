#include "cpu/peria/pai.h"

#include <gflags/gflags.h>
#include <glog/logging.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <map>
#include <numeric>
#include <sstream>
#include <vector>

#include "base/time.h"
#include "core/core_field.h"
#include "core/kumipuyo_seq_generator.h"
#include "core/plan/plan.h"
#include "core/rensa/rensa_detector.h"
#include "core/score.h"

#include "pattern.h"

DEFINE_int32(beam_length, 30, "The number of Kumipuyos to append in simulations.");
DEFINE_int32(beam_width_1, 400, "Bandwidth in the beamseach");
DEFINE_int32(beam_width_2, 40, "Bandwidth in the beamseach");
DEFINE_int32(known_length, 5, "Assume at most this number of Tsumos are known.");

namespace peria {

namespace {

struct State;

using int64 = std::int64_t;
using EvaluateFunc = std::function<void(State*)>;

// TODO: Introduce part of enemy's state to handle their attacks.
struct State {
  Decision firstDecision;
  CoreField field;
  bool isZenkeshi;
  int score;
  int expectChain;
  int expectScore;
  int frameId;

  double key;    // to be used in beam-search sorting
  double value;  // to be used in picking up
};

void GenerateNext(State state, const PlayerState& enemy, const EvaluateFunc& evalFunc,
                  const RefPlan& plan, std::vector<State>* nextStates) {
  if (!state.firstDecision.isValid()) {
    state.firstDecision = plan.firstDecision();
  }
  state.field = plan.field();
  state.score += plan.score();
  if (plan.isRensaPlan()) {
    if (state.isZenkeshi) {
      state.score += ZENKESHI_BONUS;
    }
    state.isZenkeshi = plan.hasZenkeshi();
  }

  // Simulate enemy's attack
  // TODO: Work for TAIOU
  if (enemy.isRensaOngoing() && state.frameId < enemy.rensaFinishingFrameId()
      && enemy.rensaFinishingFrameId() < state.frameId + plan.totalFrames()) {
    int numOjama = enemy.currentRensaResult.score / SCORE_FOR_OJAMA;
    numOjama = std::min(numOjama, 5 * 6);
    state.field.fallOjama((numOjama + 3) / 6);
  }
  state.frameId += plan.totalFrames();

  int expectScore = 0;
  int expectChain = 0;
  bool prohibits[FieldConstant::MAP_WIDTH] {};
  auto complementCallback = [&expectScore, &expectChain](CoreField&& field, const ColumnPuyoList&) {
      RensaResult result = field.simulate();
      expectScore = std::max(expectScore, result.score);
      expectChain = std::max(expectChain, result.chains);
  };
  RensaDetector::detectByDropStrategy(state.field, prohibits, PurposeForFindingRensa::FOR_FIRE, 2, 13, complementCallback);
  state.expectScore = expectScore;
  state.expectChain = expectChain;

  evalFunc(&state);

  nextStates->push_back(state);
}

void evalScore(State* s) {
  s->key = s->score + s->expectScore;
  s->value = s->score;
}

void evalTemplate(State* s) {
  const CoreField& field = s->field;

  int best = 0;
  for (const Pattern& pattern : Pattern::GetAllPattern()) {
    int score = pattern.Match(field);
    if (score > best) {
      best = score;
    }
  }

  s->key = best;
  s->value = best;
}

bool CompKey(const State& a, const State& b) {
  return a.key > b.key;
};

}  // namespace

Pai::Pai(int argc, char* argv[]): ::AI(argc, argv, "peria") {}

Pai::~Pai() {}

DropDecision Pai::think(int frameId,
                        const CoreField& field,
                        const KumipuyoSeq& seq,
                        const PlayerState& myState,
                        const PlayerState& enemyState,
                        bool fast) const {
  std::ostringstream oss;

  int64 startTime = currentTimeInMillis();
  int64 dueTime = startTime + (fast ? 25 : 250);

  int fullIterationDepth = FLAGS_beam_length;
  auto evaluator = evalScore;

  if (!myState.hasZenkeshi && field.countPuyos() < 10) {
    fullIterationDepth = FLAGS_beam_length / 2;
    evaluator = evalTemplate;
  }

  const int detectIterationDepth = std::min(seq.size(), FLAGS_known_length);
  const int unknownIterationDepth = fullIterationDepth - detectIterationDepth;
  std::vector<std::vector<State>> states(fullIterationDepth + 1);
  CHECK_GE(unknownIterationDepth, 0);
  LOG(INFO) << detectIterationDepth << " / " << fullIterationDepth << " search";

  Decision finalDecision;
  double bestValue = 0;
  
  State firstState;
  // intentionally leave firstState.firstDecision unset
  firstState.field = field;
  firstState.isZenkeshi = myState.hasZenkeshi;
  firstState.score = myState.unusedScore;
  firstState.expectChain = 0;
  firstState.expectScore = 0;
  firstState.frameId = frameId;

  states[0].push_back(firstState);
  for (int i = 0; i < detectIterationDepth; ++i) {
    auto& nextStates = states[i + 1];
    for (const State& s : states[i]) {
      auto generateNext = std::bind(GenerateNext, s, enemyState, evaluator, std::placeholders::_1, &nextStates);
      Plan::iterateAvailablePlans(field, {seq.get(i)}, 1, generateNext);
    }
    std::sort(nextStates.begin(), nextStates.end(), CompKey);
    if (static_cast<int>(nextStates.size()) > FLAGS_beam_width_1)
      nextStates.resize(FLAGS_beam_width_1);

    for (const State& s : nextStates) {
      if (s.value > bestValue) {
        finalDecision = s.firstDecision;
        bestValue = s.value;
      }
    }
  }

  if (states[detectIterationDepth].size() == 0) {
    oss << "Die";
    return DropDecision(Decision(3, 0), oss.str());
  }

  int64 detectTime = currentTimeInMillis();
  oss << "Known: "
      << (detectTime - startTime) << " ms / "
      << states[detectIterationDepth].size() << " states / "
      << detectIterationDepth << " hands / "
      << "(" << finalDecision.axisX() << "-" << finalDecision.rot() << ")"
      << " " << bestValue << " pts\n";

  int nTest = 0;
  std::map<Decision, std::vector<double>> vote;
  for (nTest = 0; currentTimeInMillis() < dueTime; ++nTest) {
    Decision decision;
    double value = 0;

    KumipuyoSeq pseudoSeq = KumipuyoSeqGenerator::generateRandomSequence(unknownIterationDepth);
    for (int i = detectIterationDepth, j = 0; i < fullIterationDepth; ++i, ++j) {
      auto& nextStates = states[i + 1];
      nextStates.clear();
      for (const State& s : states[i]) {
        auto generateNext = std::bind(GenerateNext, s, enemyState, evaluator, std::placeholders::_1, &nextStates);
        Plan::iterateAvailablePlans(field, {seq.get(j)}, 1, generateNext);
      }
      std::sort(nextStates.begin(), nextStates.end(), CompKey);
      if (static_cast<int>(nextStates.size()) > FLAGS_beam_width_2) {
        nextStates.resize(FLAGS_beam_width_2);
      }

      for (const State& s : nextStates) {
        if (s.value > value) {
          decision = s.firstDecision;
          value = s.value;
        }
      }
    }
    if (decision.isValid()) {
      LOG(INFO) << "vote: " << decision << " " << value;
      vote[decision].push_back(value);
    }
  }

  int64 endTime = currentTimeInMillis();
  oss << "Unknown: " << (endTime - detectTime) << " ms / " << nTest << " tests\n";

  if (vote.size()) {
    double bestAvgValue = bestValue;
    for (auto& v : vote) {
      double avg = std::accumulate(v.second.begin(), v.second.end(), 0.0);
      avg /= v.second.size();
      LOG(INFO) << v.first << " " << v.second.size() << " " << avg;
      if (avg > bestAvgValue) {
        bestAvgValue = avg;
        finalDecision = v.first;
      }
    }
    oss << "Final: (" << finalDecision.axisX() << "-" << finalDecision.rot() << ")"
        << " " << bestAvgValue << " pts\n";
  } else {
    oss << "Final: no updates\n";
  }

  return DropDecision(finalDecision, oss.str());
}

}  // namespace peria