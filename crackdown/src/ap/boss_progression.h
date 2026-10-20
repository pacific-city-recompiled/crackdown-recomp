#pragma once
#include <cstdint>
#include <vector>

namespace crackdown::ap {
// One gang's Progressive item. n receipts open the first n bosses of order;
// the kingpin is last. Opening only allows the native encounter to activate.
struct GangProgression {
  int64_t item = 0;
  std::vector<int> order;  // native boss IDs
  bool operator==(const GangProgression&) const = default;
};
}  // namespace crackdown::ap
