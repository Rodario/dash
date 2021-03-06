#ifndef DASH__UTIL__PATTERN_METRICS_H__
#define DASH__UTIL__PATTERN_METRICS_H__

#include <dash/Types.h>

#include <algorithm>
#include <array>
#include <vector>


namespace dash {
namespace util {

template<typename PatternT>
class PatternMetrics
{
private:
  typedef typename PatternT::index_type index_t;
  typedef typename PatternT::size_type  extent_t;

public:

  PatternMetrics(const PatternT & pattern)
  {
    init_metrics(pattern);
  }

  constexpr int num_blocks() const noexcept {
    return _num_blocks;
  }

  /**
   * Relation of (max. elements per unit) / (min. elements per unit).
   * Imbalance factor of 1.0 indicates perfect balance such that every unit
   * is mapped to the same number of elements in the pattern.
   */
  constexpr double imbalance_factor() const noexcept {
    return _imb_factor;
  }

  /**
   * Minimum number of blocks mapped to any unit.
   */
  constexpr int min_blocks_per_unit() const noexcept {
    return _min_blocks;
  }

  /**
   * Minimum number of elements mapped to any unit.
   */
  constexpr int min_elements_per_unit() const noexcept {
    return _min_blocks * _block_size;
  }

  /**
   * Maximum number of blocks mapped to any unit.
   */
  constexpr int max_blocks_per_unit() const noexcept {
    return _max_blocks;
  }

  /**
   * Maximum number of elements mapped to any unit.
   */
  constexpr int max_elements_per_unit() const noexcept {
    return _max_blocks * _block_size;
  }

  /**
   * Number of units mapped to minimum number of blocks per unit.
   */
  constexpr int num_balanced_units() const noexcept {
    return _num_bal_units;
  }

  /**
   * Number of units mapped to maximum number of blocks per unit.
   */
  constexpr int num_imbalanced_units() const noexcept {
    return _num_imb_units;
  }

  /**
   * Number of blocks mapped to given unit.
   */
  constexpr int unit_local_blocks(dash::team_unit_t unit) const noexcept {
    return _unit_blocks[unit];
  }

private:
  /**
   * Calculate mapping balancing metrics of given pattern instance.
   */
  void init_metrics(const PatternT & pattern)
  {
    _num_blocks   = pattern.blockspec().size();

    size_t nunits = pattern.teamspec().size();
    _unit_blocks.resize(nunits);

    for (size_t u = 0; u < nunits; ++u) {
      _unit_blocks[u] = 0;
    }
    for (int bi = 0; bi < _num_blocks; ++bi) {
      auto block      = pattern.block(bi);
      auto block_unit = pattern.unit_at(std::array<index_t, 2> {{
                          block.offset(0),
                          block.offset(1)
			    }});
      _unit_blocks[block_unit]++;
    }

    _block_size      = pattern.blocksize(0) * pattern.blocksize(1);
    _min_blocks      = *std::min_element(_unit_blocks.begin(),
                                         _unit_blocks.begin() + nunits);
    _max_blocks      = *std::max_element(_unit_blocks.begin(),
                                         _unit_blocks.begin() + nunits);
    _num_bal_units   = std::count(_unit_blocks.begin(),
                                   _unit_blocks.begin() + nunits,
                                  _min_blocks);
    _num_imb_units   = _min_blocks == _max_blocks
                       ? 0
                       : std::count(_unit_blocks.begin(),
                                    _unit_blocks.begin() + nunits,
                                    _max_blocks);

    int min_elements = _min_blocks * _block_size;
    int max_elements = _max_blocks * _block_size;
    _imb_factor = static_cast<float>(max_elements) /
                  static_cast<float>(min_elements);
  }

private:
  std::vector<int> _unit_blocks;
  int              _num_blocks    = 0;
  int              _block_size    = 0;
  int              _min_blocks    = 0;
  int              _max_blocks    = 0;
  int              _num_imb_units = 0;
  int              _num_bal_units = 0;
  double           _imb_factor    = 0.0;
};

} // namespace util
} // namespace dash

#endif // DASH__UTIL__PATTERN_METRICS_H__
