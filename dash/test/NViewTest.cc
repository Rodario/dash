
#include "NViewTest.h"

#include <gtest/gtest.h>

#include <dash/View.h>
#include <dash/Array.h>
#include <dash/Matrix.h>

#include <array>


namespace dash {
namespace test {

  template <class MatrixT>
  void initialize_matrix(MatrixT & matrix) {
    if (dash::myid() == 0) {
      for(size_t i = 0; i < matrix.extent(0); ++i) {
        for(size_t k = 0; k < matrix.extent(1); ++k) {
          matrix[i][k] = (i + 1) * 0.100 + (k + 1) * 0.001;
        }
      }
    }
    matrix.barrier();

    for(size_t i = 0; i < matrix.local_size(); ++i) {
      matrix.lbegin()[i] += dash::myid();
    }
    matrix.barrier();
  }

  template <class ValueRange>
  std::string range_str(
    const ValueRange & vrange) {
    typedef typename ValueRange::value_type value_t;
    std::ostringstream ss;
    auto idx = dash::index(vrange);
    int  i   = 0;
    for (const auto & v : vrange) {
      ss << "[" << *(dash::begin(idx) + i) << "] "
         << static_cast<value_t>(v) << " ";
      ++i;
    }
    return ss.str();
  }

}
}

using dash::test::range_str;


TEST_F(NViewTest, MatrixBlocked1DimLocalView)
{
  auto nunits = dash::size();

  int block_rows = 5;
  int block_cols = 3;

  int nrows = nunits * block_rows;
  int ncols = nunits * block_cols;

  // columns distributed in blocks of same size:
  //
  //  0 0 0 | 1 1 1 | 2 2 2 | ...
  //  0 0 0 | 1 1 1 | 2 2 2 | ...
  //  0 0 0 | 1 1 1 | 2 2 2 | ...
  //
  dash::Matrix<int, 2> mat(
      nrows,      ncols,
      dash::NONE, dash::BLOCKED);

  mat.barrier();

  DASH_LOG_DEBUG("NViewTest.MatrixBlocked1DimLocalView",
                 "Matrix initialized");

  // select first 2 matrix rows:
  auto nview_rows_g = dash::sub<0>(1, 3, mat);
  auto nview_cols_g = dash::sub<1>(2, 7, mat);
  auto nview_cr_s_g = dash::sub<1>(2, 7, nview_rows_g);
  auto nview_rc_s_g = dash::sub<0>(1, 3, nview_cols_g);

  DASH_LOG_DEBUG("NViewTest.MatrixBlocked1DimLocalView",
                 "mat ->",
                 "offsets:", mat.offsets(),
                 "extents:", mat.extents(),
                 "size:",    mat.size());

  DASH_LOG_DEBUG("NViewTest.MatrixBlocked1DimLocalView",
                 "sub<0>(1,3, mat) ->",
                 "offsets:", nview_rows_g.offsets(),
                 "extents:", nview_rows_g.extents(),
                 "size:",    nview_rows_g.size());

  DASH_LOG_DEBUG("NViewTest.MatrixBlocked1DimLocalView",
                 "sub<1>(2,7, mat) ->",
                 "offsets:", nview_cols_g.offsets(),
                 "extents:", nview_cols_g.extents(),
                 "size:",    nview_cols_g.size());

  DASH_LOG_DEBUG("NViewTest.MatrixBlocked1DimLocalView",
                 "sub<1>(2,7, sub<0>(1,3, mat) ->",
                 "offsets:", nview_cr_s_g.offsets(),
                 "extents:", nview_cr_s_g.extents(),
                 "size:",    nview_cr_s_g.size());

  DASH_LOG_DEBUG("NViewTest.MatrixBlocked1DimLocalView",
                 "sub<0>(1,3, sub<0>(2,7, mat) ->",
                 "offsets:", nview_rc_s_g.offsets(),
                 "extents:", nview_rc_s_g.extents(),
                 "size:",    nview_rc_s_g.size());

  EXPECT_EQ_U(2,             nview_rows_g.extent<0>());
  EXPECT_EQ_U(mat.extent(1), nview_rows_g.extent<1>());

  EXPECT_EQ_U(nview_rc_s_g.extents(), nview_cr_s_g.extents());
  EXPECT_EQ_U(nview_rc_s_g.offsets(), nview_cr_s_g.offsets());

#if __TODO__
  auto nview_rows_l = dash::local(nview_rows_g);

  DASH_LOG_DEBUG_VAR("NViewTest.MatrixBlocked1DimLocalView",
                     nview_rows_l.extents());

  EXPECT_EQ_U(2,             nview_rows_l.extent<0>());
  EXPECT_EQ_U(block_cols,    nview_rows_l.extent<1>());
#endif
}

TEST_F(NViewTest, MatrixBlocked1DimSub)
{
  auto nunits = dash::size();

  int block_rows = 4;
  int block_cols = 3;

  int nrows = nunits * block_rows;
  int ncols = nunits * block_cols;

  // columns distributed in blocks of same size:
  //
  //  0 0 0 | 1 1 1 | 2 2 2 | ...
  //  0 0 0 | 1 1 1 | 2 2 2 | ...
  //  0 0 0 | 1 1 1 | 2 2 2 | ...
  //
  dash::Matrix<double, 2> mat(
      dash::SizeSpec<2>(
        nrows,
        ncols),
      dash::DistributionSpec<2>(
        dash::NONE,
        dash::TILE(block_cols)),
      dash::Team::All(),
      dash::TeamSpec<2>(
        1,
        nunits));

  dash::test::initialize_matrix(mat);

  if (dash::myid() == 0) {
    for (int r = 0; r < nrows; ++r) {
      std::vector<double> row_values;
      for (int c = 0; c < ncols; ++c) {
        row_values.push_back(
          static_cast<double>(mat[r][c]));
      }
      DASH_LOG_DEBUG("NViewTest.MatrixBlocked1DimSub",
                     "row[", r, "]", row_values);
    }
  }
  mat.barrier();

  DASH_LOG_DEBUG_VAR("NViewTest.MatrixBlocked1DimSub", mat.extents());
  DASH_LOG_DEBUG_VAR("NViewTest.MatrixBlocked1DimSub",
                     mat.pattern().local_extents());
  DASH_LOG_DEBUG_VAR("NViewTest.MatrixBlocked1DimSub",
                     mat.pattern().local_size());

  if (dash::myid() == 0) {
    auto allsub_view = dash::sub<0>(
                         0, mat.extents()[0],
                         mat);

    DASH_LOG_DEBUG("NViewTest.MatrixBlocked1DimSub",
                   dash::internal::typestr(allsub_view));
    DASH_LOG_DEBUG_VAR("NViewTest.MatrixBlocked1DimSub",
                       allsub_view.extents());
    DASH_LOG_DEBUG_VAR("NViewTest.MatrixBlocked1DimSub",
                        allsub_view.extent(0));
    DASH_LOG_DEBUG_VAR("NViewTest.MatrixBlocked1DimSub",
                        allsub_view.extent(1));
    DASH_LOG_DEBUG_VAR("NViewTest.MatrixBlocked1DimSub",
                        allsub_view.size(0));
    DASH_LOG_DEBUG_VAR("NViewTest.MatrixBlocked1DimSub",
                        allsub_view.size(1));
    DASH_LOG_DEBUG_VAR("NViewTest.MatrixBlocked1DimSub",
                       index(allsub_view).size());
    DASH_LOG_DEBUG("NViewTest.MatrixBlocked1DimSub",
                   "allsub_view:", range_str(allsub_view));
  }

  // -- Local View -----------------------------------
  //
  
  auto loc_view = dash::local(
                    dash::sub<0>(
                      0, mat.extents()[0],
                      mat));

  EXPECT_EQ_U(2, decltype(loc_view)::rank::value);
  EXPECT_EQ_U(2, loc_view.ndim());
  
  int  lrows    = loc_view.extent<0>();
  int  lcols    = loc_view.extent<1>();

  DASH_LOG_DEBUG("NViewTest.MatrixBlocked1DimSub",
                 dash::internal::typestr(loc_view));
  DASH_LOG_DEBUG_VAR("NViewTest.MatrixBlocked1DimSub", loc_view.extents());
  DASH_LOG_DEBUG_VAR("NViewTest.MatrixBlocked1DimSub", lrows);
  DASH_LOG_DEBUG_VAR("NViewTest.MatrixBlocked1DimSub", lcols);
  DASH_LOG_DEBUG_VAR("NViewTest.MatrixBlocked1DimSub", loc_view.size());
  DASH_LOG_DEBUG_VAR("NViewTest.MatrixBlocked1DimSub",
                     loc_view.begin().pos());
  DASH_LOG_DEBUG_VAR("NViewTest.MatrixBlocked1DimSub",
                     loc_view.end().pos());
  DASH_LOG_DEBUG_VAR("NViewTest.MatrixBlocked1DimSub",
                     (loc_view.end() - loc_view.begin()));
  DASH_LOG_DEBUG("NViewTest.MatrixBlocked1DimSub",
                 "loc_view:", range_str(loc_view));

  EXPECT_EQ_U(mat.local_size(), lrows * lcols);

  return;

  for (int r = 0; r < lrows; ++r) {
    std::vector<double> row_values;
    for (int c = 0; c < lcols; ++c) {
      row_values.push_back(
        static_cast<double>(*(loc_view.begin() + (r * lrows + c))));
    }
    DASH_LOG_DEBUG("NViewTest.MatrixBlocked1DimSub",
                   "lrow[", r, "]", row_values);
  }

  return;

  mat.barrier();

  // -- Sub-Section ----------------------------------
  //
  
  if (dash::myid() == 0) {
    auto nview_sub  = dash::sub<0>(1, nrows - 1,
                        dash::sub<1>(1, ncols - 1,
                          mat) );
    auto nview_rows = nview_sub.extent<0>();
    auto nview_cols = nview_sub.extent<1>();

    DASH_LOG_DEBUG_VAR("NViewTest.MatrixBlocked1DimSub", nview_rows);
    DASH_LOG_DEBUG_VAR("NViewTest.MatrixBlocked1DimSub", nview_cols);

    for (int r = 0; r < nview_rows; ++r) {
      std::vector<double> row_values;
      for (int c = 0; c < nview_cols; ++c) {
        row_values.push_back(nview_sub[r * nview_cols + c]);
      }
      DASH_LOG_DEBUG("NViewTest.MatrixBlocked1DimSub",
                     "row[", r, "]", row_values);
    }
    for (int r = 0; r < nview_rows; ++r) {
      auto row_view = dash::sub<0>(r, r+1, nview_sub);
      DASH_LOG_DEBUG("NViewTest.MatrixBlocked1DimSub",
                     "row[", r, "]",
                     range_str(row_view));
    }
  }

}

