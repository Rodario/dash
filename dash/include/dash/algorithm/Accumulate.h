#ifndef DASH__ALGORITHM__ACCUMULATE_H__
#define DASH__ALGORITHM__ACCUMULATE_H__

#include <dash/iterator/GlobIter.h>
#include <dash/iterator/IteratorTraits.h>

#include <dash/algorithm/LocalRange.h>
#include <dash/algorithm/Operation.h>


namespace dash {

namespace internal {

  template<typename ValueType>
  struct local_result {
    ValueType value{};
    bool      valid = false;
  };

  template<typename ValueType, typename F>
  void accumulate_custom_fn(
    const void *invec,
          void *inoutvec,
          size_t,
          void *userdata)
  {
    using local_result_t = struct local_result<ValueType>;
    const local_result_t* in    = static_cast<const local_result_t*>(invec);
          local_result_t* inout = static_cast<local_result_t*>(inoutvec);
    F& fn = *static_cast<F*>(userdata);
    if (in->valid) {
      if (inout->valid) {
        inout->value = fn(in->value, inout->value);
      } else {
        inout->value = in->value;
        inout->valid  = true;
      }
    }
  }
} // namespace internal


/**
 * Accumulate values in each process' range [\ref in_first, \ref in_last) using
 * the provided binary reduce function \c binary_op, which must be commutative
 * and linear.
 *
 * The iteration order is defined by the data distribution and the reduction
 * follows a two-step process: each unit first accumulates its local elements
 * in local iteration using \ref binary_op order before combining the results
 * across units.
 *
 * Collective operation.
 *
 * \note: For equivalent of semantics of \c MPI_Accumulate, see
 * \ref dash::transform.
 *
 * \param in_first  Local iterator describing the beginning of the range to
 *                  accumulate.
 * \param in_last   Local iterator describing the end of the range to accumualte
 * \param init      The initial element to use in the accumulation.
 * \param binary_op The binary operation to apply to accumulate two elements
 *                  (default: using \c operator+)
 * \param non_empty Whether all units are guaranteed to provide a non-empty local
 *                  range (default \c false).
 * \param team      The team to use for the collective operation.
 *
 * \ingroup  DashAlgorithms
 */

template <
  class LocalInputIter,
  class ValueType,
  class BinaryOperation,
  typename = typename std::enable_if<
                        !dash::detail::is_global_iterator<LocalInputIter>::value
                      >::type>
ValueType accumulate(
  const LocalInputIter   in_first,
  const LocalInputIter   in_last,
  const ValueType      & init,
  BinaryOperation        binary_op,
  bool                   non_empty = true,
  dash::Team           & team = dash::Team::All())
{
  using local_result_t = struct dash::internal::local_result<ValueType>;
  auto myid        = team.myid();
  auto l_first     = in_first;
  auto l_last      = in_last;

  local_result_t l_result;
  local_result_t g_result;
  if (l_first != l_last) {
    l_result.value = std::accumulate(std::next(l_first),
                                     l_last, *l_first,
                                     binary_op);
    l_result.valid = true;
  }
  dart_operation_t dop =
                  dash::internal::dart_reduce_operation<BinaryOperation>::value;
  dart_datatype_t  dtype = dash::dart_storage<ValueType>::dtype;

  if (!non_empty || dop == DART_OP_UNDEFINED || dtype == DART_TYPE_UNDEFINED)
  {
    dart_type_create_custom(sizeof(local_result_t), &dtype);

    // we need a custom reduction operation because not every unit
    // may have valid values
    dart_op_create(
      &dash::internal::accumulate_custom_fn<ValueType, BinaryOperation>,
      &binary_op, true, dtype, true, &dop);
    dart_allreduce(&l_result, &g_result, 1, dtype, dop, team.dart_id());
    dart_op_destroy(&dop);
    dart_type_destroy(&dtype);
  } else {
    // ideal case: we can use DART predefined reductions
    dart_allreduce(&l_result.value, &g_result.value, 1, dtype, dop, team.dart_id());
    g_result.valid = true;
  }
  if (!g_result.valid) {
    DASH_LOG_ERROR("Found invalid reduction value!");
  }
  auto result = g_result.value;

  result = binary_op(init, result);

  return result;
}

/**
 * Accumulate values across the local ranges \c[in_first,in_last) of each
 * process as the sum of all values in the range.
 *
 * The iteration order is defined by the data distribution and the reduction
 * follows a two-step process: each unit first accumulates its local elements
 * in local iteration using \ref binary_op order before combining the results
 * across units.
 *
 * \note: For equivalent of semantics of \c MPI_Accumulate, see
 * \ref dash::transform.
 *
 * \param in_first  Local pointer describing the beginning of the range to
 *                  accumulate.
 * \param in_last   Local pointer describing the end of the range to accumualte
 * \param init      The initial element to use in the accumulation.
 * \param non_empty Whether all units are guaranteed to provide a non-empty local
 *                  range (default \c false).
 * \param team      The team to use for the collective operation.
 *
 * \ingroup  DashAlgorithms
 */
template <
  class LocalInputIter,
  class ValueType,
  typename = typename std::enable_if<
                        !dash::detail::is_global_iterator<LocalInputIter>::value
                      >::type>
ValueType accumulate(
  const LocalInputIter   in_first,
  const LocalInputIter   in_last,
  const ValueType      & init,
  bool                   non_empty = false,
  dash::Team           & team = dash::Team::All())
{
  return dash::accumulate(
            in_first,
            in_last,
            init,
            dash::plus<ValueType>(),
            non_empty,
            team);
}

/**
 * Accumulate values in the global range [\ref in_first, \ref in_last) using
 * the provided binary reduce function \c binary_op, which must be commutative
 * and linear.
 *
 * The iteration order is defined by the data distribution and the reduction
 * follows a two-step process: each unit first accumulates its local elements
 * in local iteration using \ref binary_op order before combining the results
 * across units.
 *
 * Collective operation.
 *
 * \param in_first  Global iterator describing the beginning of the range to
 *                  accumulate.
 * \param in_last   Global iterator describing the end of the range to accumualte
 * \param init      The initial element to use in the accumulation.
 * \param binary_op The associative, commutative binary operation to to apply.
 *
 * \note: For equivalent of semantics of \c MPI_Accumulate, see
 * \ref dash::transform.
 *
 * \ingroup  DashAlgorithms
 */
template <
  class GlobInputIt,
  class ValueType,
  class BinaryOperation = dash::plus<ValueType>,
  typename = typename std::enable_if<
                        dash::detail::is_global_iterator<GlobInputIt>::value
                      >::type>
ValueType accumulate(
        GlobInputIt   in_first,
        GlobInputIt   in_last,
  const ValueType   & init,
  BinaryOperation     binary_op = dash::plus<ValueType>())
{
  auto & team      = in_first.team();
  auto index_range = dash::local_range(in_first, in_last);
  auto l_first     = index_range.begin;
  auto l_last      = index_range.end;

  // TODO: can we figure out whether or not units are empty?
  static constexpr bool units_non_empty = false;
  return dash::accumulate(l_first,
                          l_last,
                          init,
                          binary_op,
                          units_non_empty,
                          team);
}

} // namespace dash

#endif // DASH__ALGORITHM__ACCUMULATE_H__
