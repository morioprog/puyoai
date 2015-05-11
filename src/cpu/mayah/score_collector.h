#ifndef MAYAH_AI_SCORE_COLLECTOR_H_
#define MAYAH_AI_SCORE_COLLECTOR_H_

#include <array>
#include <map>
#include <string>
#include <vector>

#include "core/column_puyo_list.h"

#include "collected_score.h"
#include "evaluation_feature.h"
#include "evaluation_parameter.h"

// This collector collects only score.
class SimpleScoreCollector {
public:
    typedef CollectedSimpleScore CollectedScore;

    explicit SimpleScoreCollector(const EvaluationParameterMap& paramMap) :
        paramMap_(paramMap)
    {
    }

    void addScore(EvaluationFeatureKey key, double v)
    {
        for (const auto& mode : ALL_EVALUATION_MODES) {
            collectedSimpleScore_.moveScore.scoreMap[ordinal(mode)] += paramMap_.parameter(mode).score(key, v);
        }
    }

    void addScore(EvaluationSparseFeatureKey key, int idx, int n)
    {
        for (const auto& mode : ALL_EVALUATION_MODES) {
            collectedSimpleScore_.moveScore.scoreMap[ordinal(mode)] += paramMap_.parameter(mode).score(key, idx, n);
        }
    }
    void merge(const CollectedSimpleScore& collectedSimpleScore)
    {
        for (const auto& mode : ALL_EVALUATION_MODES) {
            collectedSimpleScore_.rensaScore.scoreMap[ordinal(mode)] += collectedSimpleScore.score(mode);
        }
    }

    void setCoef(const CollectedCoef& coef) { collectedCoef_ = coef; }
    const CollectedCoef& collectedCoef() const { return collectedCoef_; }

    const CollectedSimpleScore& collectedScore() const { return collectedSimpleScore_; }
    const EvaluationParameterMap& evaluationParameterMap() const { return paramMap_; }

    void setEstimatedRensaScore(int s) { estimatedRensaScore_ = s; }
    int estimatedRensaScore() const { return estimatedRensaScore_; }

    void setBookname(const std::string&) {}
    void setPuyosToComplement(const ColumnPuyoList&) {}

private:
    const EvaluationParameterMap& paramMap_;
    CollectedCoef collectedCoef_;
    CollectedSimpleScore collectedSimpleScore_;
    int estimatedRensaScore_ = 0;
};

// This collector collects all features.
class FeatureScoreCollector {
public:
    typedef CollectedFeatureScore CollectedScore;

    FeatureScoreCollector(const EvaluationParameterMap& paramMap) : paramMap_(paramMap) {}

    void addScore(EvaluationFeatureKey key, double v)
    {
        for (const auto& mode : ALL_EVALUATION_MODES) {
            collectedFeatureScore_.moveScore.simpleScore.scoreMap[ordinal(mode)] += paramMap_.parameter(mode).score(key, v);
        }
        collectedFeatureScore_.moveScore.collectedFeatures[key] += v;
    }

    void addScore(EvaluationSparseFeatureKey key, int idx, int n)
    {
        for (const auto& mode : ALL_EVALUATION_MODES) {
            collectedFeatureScore_.moveScore.simpleScore.scoreMap[ordinal(mode)] += paramMap_.parameter(mode).score(key, idx, n);
        }
        for (int i = 0; i < n; ++i)
            collectedFeatureScore_.moveScore.collectedSparseFeatures[key].push_back(idx);
    }

    void merge(const CollectedFeatureScore& cfs)
    {
        // TODO(mayah): Currently, we don't distinguish moveScore and rensaScore.
        // merged score is merged into rensaScore, now.

        for (const auto& mode : ALL_EVALUATION_MODES) {
            collectedFeatureScore_.rensaScore.simpleScore.scoreMap[ordinal(mode)] += cfs.moveScore.score(mode);
        }

        for (const auto& entry : cfs.moveScore.collectedFeatures) {
            collectedFeatureScore_.rensaScore.collectedFeatures[entry.first] = entry.second;
        }
        for (const auto& entry : cfs.moveScore.collectedSparseFeatures) {
            collectedFeatureScore_.rensaScore.collectedSparseFeatures[entry.first].insert(
                collectedFeatureScore_.rensaScore.collectedSparseFeatures[entry.first].end(),
                entry.second.begin(),
                entry.second.end());
        }
    }

    void setBookname(const std::string& bookname) { collectedFeatureScore_.rensaScore.bookname = bookname; }
    const std::string& bookname() const { return collectedFeatureScore_.rensaScore.bookname; }

    void setCoef(const CollectedCoef& coef) { collectedCoef_ = coef; }
    const CollectedCoef& collectedCoef() const { return collectedCoef_; }
    const CollectedFeatureScore& collectedScore() const { return collectedFeatureScore_; }
    const EvaluationParameterMap& evaluationParameterMap() const { return paramMap_; }

    void setEstimatedRensaScore(int s) { estimatedRensaScore_ = s; }
    int estimatedRensaScore() const { return estimatedRensaScore_; }

    void setPuyosToComplement(const ColumnPuyoList& cpl) { collectedFeatureScore_.rensaScore.puyosToComplement = cpl; }
private:
    const EvaluationParameterMap& paramMap_;
    CollectedCoef collectedCoef_;
    CollectedFeatureScore collectedFeatureScore_;
    int estimatedRensaScore_ = 0;
};

#endif
