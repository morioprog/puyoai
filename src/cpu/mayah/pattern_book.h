#ifndef CPU_MAYAH_PATTERN_BOOK_H_
#define CPU_MAYAH_PATTERN_BOOK_H_

#include <map>
#include <string>
#include <vector>

#include <toml/toml.h>

#include "base/noncopyable.h"
#include "core/algorithm/column_puyo_list.h"
#include "core/algorithm/field_pattern.h"
#include "core/algorithm/rensa_detector.h"
#include "core/core_field.h"
#include "core/position.h"

class PatternBookField {
public:
    // If ignitionColumn is 0, we ignore ignitionColumn when detecting rensa.
    PatternBookField(const std::string& field, int ignitionColumn, int score);

    int score() const { return score_; }
    int ignitionColumn() const { return ignitionColumn_;}
    const std::vector<Position>& ignitionPositions() const { return ignitionPositions_; }

    bool complement(const CoreField& cf, ColumnPuyoList* cpl) const { return pattern_.complement(cf, cpl); }

    PatternBookField mirror() const
    {
        int mirroredIgnitionColumn = ignitionColumn() == 0 ? 0 : 7 - ignitionColumn();
        return PatternBookField(pattern_.mirror(), mirroredIgnitionColumn, score_);
    }

private:
    PatternBookField(const FieldPattern&, int ignitionColumn, int score);

    FieldPattern pattern_;
    int ignitionColumn_;
    int score_;
    std::vector<Position> ignitionPositions_;
};

class PatternBook : noncopyable {
public:
    typedef std::multimap<std::vector<Position>, PatternBookField> Map;
    typedef Map::const_iterator Iterator;

    bool load(const std::string& filename);
    bool loadFromString(const std::string&);
    bool loadFromValue(const toml::Value&);

    // Finds the PatternBookField from the positions where puyos are erased at the first chain.
    // Multiple PatternBookField might be found, so begin-iterator and end-iterator will be
    // returned. If no such PatternBookField is found, begin-iterator and end-iterator are the same.
    // Note that ignitionPositions must be sorted.
    std::pair<Iterator, Iterator> find(const std::vector<Position>& ignitionPositions) const;

    typedef std::function<void (const CoreField&,
                                const RensaResult&,
                                const ColumnPuyoList& keyPuyos,
                                const ColumnPuyoList& firePuyos,
                                const RensaTrackResult&,
                                int patternScore)> Callback;

    void iteratePossibleRensas(const CoreField&, int maxIteration,
                               const Callback& callback) const;

    size_t size() const { return fields_.size(); }
    Iterator begin() const { return fields_.begin(); }
    Iterator end() const { return fields_.end(); }

private:
    void iteratePossibleRensasInternal(const CoreField& originalField,
                                       int currentChains,
                                       const CoreField& currentField,
                                       const ColumnPuyo& firePuyo,
                                       const ColumnPuyoList& keyPuyos,
                                       const CoreField::SimulationContext&,
                                       int restIteration,
                                       int scoreSum,
                                       const Callback& callback) const;

    void checkRensa(const CoreField& originalField,
                    int currentChains,
                    const ColumnPuyo& firePuyo,
                    const ColumnPuyoList& keyPuyos,
                    int sumScore,
                    const Callback& callback) const;

    Map fields_;
};

#endif
