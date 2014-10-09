#include "pattern.h"

#include <cctype>
#include <deque>
#include <istream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace peria {

namespace {

std::vector<Pattern> g_pattern;

std::string Trim(std::string line) {
  size_t pos = line.find('#');
  // Remove comments starting with '#'.
  if (pos != std::string::npos)
    line = line.substr(0, pos);
  size_t start = line.find_first_not_of(' ');
  size_t last = line.find_last_not_of(' ');
  if (start == std::string::npos)
    start = 0;
  if (last == std::string::npos)
    last = line.size();
  return line.substr(start, last - start + 1);
}

}  // namespace

void Pattern::ReadBook(std::istream& is) {
  g_pattern.clear();

  std::unique_ptr<Pattern> pattern(new Pattern);
  for (std::string line; std::getline(is, line);) {
    line = Trim(line);
    if (line.empty())
      continue;
    std::istringstream iss(line);
    std::string first_segment;
    iss >> first_segment;
    if (first_segment.find(':') == std::string::npos) {
      pattern->AppendField(first_segment);
      continue;
    }

    if (first_segment == "NAME:") {
      if (pattern && !pattern->name_.empty()) {
        pattern->Optimize();
        g_pattern.push_back(*pattern);
      }
      pattern.reset(new Pattern);
      iss >> pattern->name_;
    } else if (first_segment == "SCORE:") {
      int score;
      iss >> score;
      pattern->score_ = score;
    } else if (first_segment == "MAX:") {
      int max_puyos;
      iss >> max_puyos;
      pattern->max_puyos_ = max_puyos;
    }
  }

  if (pattern && !pattern->name_.empty()) {
    pattern->Optimize();
    g_pattern.push_back(*pattern);
  }
}

const std::vector<Pattern>& Pattern::GetAllPattern() {
  return g_pattern;
}

int Pattern::Match(const CoreField& field) const {
  if (max_puyos_ > 0 && field.countPuyos() > max_puyos_)
    return 0;

  std::map<char, std::map<PuyoColor, int> > matching0;
  std::map<char, std::map<PuyoColor, int> > matching1;  // Mirroring
  const int height = pattern_.size();
  for (int y = 1; y <= height; ++y) {
    for (int x = 1; x <= PlainField::WIDTH; ++x) {
      const char c0 = pattern_[y - 1][x - 1];
      const char c1 = pattern_[y - 1][PlainField::WIDTH - x];
      PuyoColor color = field.get(x, y);
      if (c0 != '.' && color != OJAMA)
        ++matching0[c0][color];
      if (c1 != '.' && color != OJAMA)
      ++matching1[c1][color];
    }
  }
  
  return std::max(GetScore(matching0), GetScore(matching1));
}

void Pattern::Optimize() {
  static const int dx[] = {0, 1};
  static const int dy[] = {1, 0};

  const int height = pattern_.size();
  for (int y0 = 0; y0 < height; ++y0) {
    for (int x0 = 0; x0 < PlainField::WIDTH; ++x0) {
      for (int i = 0; i < 2; ++i) {
        int x1 = x0 + dx[i], y1 = y0 + dy[i];
        if (x1 >= PlainField::WIDTH || y1 >= height)
          continue;
        char c0 = pattern_[y0][x0];
        char c1 = pattern_[y1][x1];
        neighbors_.insert(Neighbor(c0, c1));
        neighbors_.insert(Neighbor(c1, c0));
      }
    }
  } 

  // Debug output
  LOG(INFO) << "name: " << name_;
  LOG(INFO) << "score: " << score_;
  LOG(INFO) << "max: " << max_puyos_;
  LOG(INFO) << "num: " << num_puyos_;
  for (auto& line : pattern_)
    LOG(INFO) << line;
}

void Pattern::AppendField(std::string line) {
  DCHECK(line.size() == PlainField::WIDTH);
  
  for (auto& c : line) {
    if (std::islower(c))
      c = '.';
    else  // std::isupper(c) || c == '_'
      ++num_puyos_;
  }
  pattern_.push_front(line);
}

int Pattern::GetScore(
    std::map<char, std::map<PuyoColor, int> >& matching) const {
  int sum = 0;
  matching['_'][EMPTY] += 0;
  if (matching['_'].size() == 1)
    sum = matching['_'][EMPTY];
  matching.erase('_');

  for (auto& matching_itr : matching) {
    DCHECK(std::isupper(matching_itr.first));
    auto& char_base = matching_itr.second;
    char_base.erase(EMPTY);
    if (char_base.size() > 1)
      return 0;
    sum += char_base.begin()->second;
  }

  // Neighborhoods cannot be same color.
  for (const auto& neighbor : neighbors_) {
    char c0 = neighbor.first;
    char c1 = neighbor.second;
    if (matching[c0].empty() || matching[c1].empty())
      continue;
    if (matching[c0].begin()->first == matching[c1].begin()->first)
      return 0;
  }

  return score_ * sum / num_puyos_;
}

}  // namespace peria
