/* ************************************************************************
 * Copyright 2018 Advanced Micro Devices, Inc.
 *
 * ************************************************************************ */

#include "testing_larft.hpp"
#include "utility.h"
#include <gtest/gtest.h>
#include <math.h>
#include <stdexcept>
#include <vector>

using ::testing::Combine;
using ::testing::TestWithParam;
using ::testing::Values;
using ::testing::ValuesIn;
using namespace std;


typedef std::tuple<vector<int>, vector<int>> mTuple;

//{N,ldv}
const vector<vector<int>> order_size_range = {
    {-1,1}, {0,1}, {10,5}, {15,15}, {20,20}, {35,50}  
};

//{K,ldt,d}
//if d = 0, then direct = 'F'
//if d = 1, then direct = 'B'
//FOR NOW ONLY FORWARD DIRECTION HAS BEEN IMPLEMENTED
const vector<vector<int>> reflector_size_range = {
    {0,1,0}, {5,1,0}, {5,5,0}, {10,20,0}, {15,15,0}
};

const vector<vector<int>> large_order_size_range = {
    {192,192}, {640,700}, {1024,1024}, {2547,2550}
};

const vector<vector<int>> large_reflector_size_range = {
    {15,15,0}, {25,40,0}, {45,45,0}, {60,70,0}, {75,75,0}
};

Arguments larft_setup_arguments(mTuple tup) {

  vector<int> order_size = std::get<0>(tup);
  vector<int> reflector_size = std::get<1>(tup);

  Arguments arg;

  arg.N = order_size[0];
  arg.ldv = order_size[1];
  arg.K = reflector_size[0];
  arg.ldt = reflector_size[1];

  arg.direct_option = reflector_size[2] == 1 ? 'B' : 'F';

  arg.timing = 0;

  return arg;
}

class HHreflec_blk : public ::TestWithParam<mTuple> {
protected:
  HHreflec_blk() {}
  virtual ~HHreflec_blk() {}
  virtual void SetUp() {}
  virtual void TearDown() {}
};

TEST_P(HHreflec_blk, larft_float) {
  Arguments arg = larft_setup_arguments(GetParam());

  rocblas_status status = testing_larft<float>(arg);

  // if not success, then the input argument is problematic, so detect the error
  // message
  if (status != rocblas_status_success) {

    if (arg.N < 0 || arg.K < 1 || arg.ldv < arg.N || arg.ldt < arg.K) {
      EXPECT_EQ(rocblas_status_invalid_size, status);
    } 
  }
}

TEST_P(HHreflec_blk, larft_double) {
  Arguments arg = larft_setup_arguments(GetParam());

  rocblas_status status = testing_larft<double>(arg);

  // if not success, then the input argument is problematic, so detect the error
  // message
  if (status != rocblas_status_success) {

    if (arg.N < 0 || arg.K < 1 || arg.ldv < arg.N || arg.ldt < arg.K) {
      EXPECT_EQ(rocblas_status_invalid_size, status);
    } 
  }
}

INSTANTIATE_TEST_CASE_P(daily_lapack, HHreflec_blk,
                        Combine(ValuesIn(large_order_size_range),
                                ValuesIn(large_reflector_size_range)));

INSTANTIATE_TEST_CASE_P(checkin_lapack, HHreflec_blk,
                        Combine(ValuesIn(order_size_range),
                                ValuesIn(reflector_size_range)));
