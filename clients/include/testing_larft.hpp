/* ************************************************************************
 * Copyright 2018 Advanced Micro Devices, Inc.
 * ************************************************************************ */

#include <cmath> // std::abs
#include <fstream>
#include <iostream>
#include <limits> // std::numeric_limits<T>::epsilon();
#include <stdlib.h>
#include <string>
#include <vector>

#include "arg_check.h"
#include "cblas_interface.h"
#include "norm.h"
#include "rocblas_test_unique_ptr.hpp"
#include "rocsolver.hpp"
#include "unit.h"
#include "utility.h"
#ifdef GOOGLE_TEST
#include <gtest/gtest.h>
#endif

#define ERROR_EPS_MULTIPLIER 5000

using namespace std;

template <typename T> 
rocblas_status testing_larft(Arguments argus) 
{
    rocblas_int K = argus.K;
    rocblas_int N = argus.N;
    rocblas_int ldv = argus.ldv;
    rocblas_int ldt = argus.ldt;
    char directchar = argus.direct_option;    
    int hot_calls = argus.iters;
    rocsolver_direct direct;

    std::unique_ptr<rocblas_test::handle_struct> unique_ptr_handle(new rocblas_test::handle_struct);
    rocblas_handle handle = unique_ptr_handle->handle;

    // check invalid size and quick return
    if (N < 1 || K < 1 || ldv < N || ldt < K) {
        auto dV_managed = rocblas_unique_ptr{rocblas_test::device_malloc(sizeof(T)), rocblas_test::device_free};
        T *dV = (T *)dV_managed.get();

        auto dtau_managed = rocblas_unique_ptr{rocblas_test::device_malloc(sizeof(T)), rocblas_test::device_free};
        T *dtau = (T *)dtau_managed.get();

        auto dF_managed = rocblas_unique_ptr{rocblas_test::device_malloc(sizeof(T)), rocblas_test::device_free};
        T *dF = (T *)dF_managed.get();
        
        if (!dV || !dtau || !dF) {
            PRINT_IF_HIP_ERROR(hipErrorOutOfMemory);
            return rocblas_status_memory_error;
        }
        
        return rocsolver_larft<T>(handle, direct, N, K, dV, ldv, dtau, dF, ldt);
    }

    rocblas_int sizeF = ldt * K;
    rocblas_int sizeV = ldv * K;    
    if (directchar == 'F') {
        direct = rocsolver_forward_direction;
    } else if (directchar == 'B') {
        direct = rocsolver_backward_direction;
    } else {
        throw runtime_error("Unsupported side option.");
    }

    // Naming: dK is in GPU (device) memory. hK is in CPU (host) memory
    vector<T> hV(sizeV);
    vector<T> hF(sizeF);
    vector<T> hF_r(sizeF);
    vector<T> htau(K);

    auto dV_managed = rocblas_unique_ptr{rocblas_test::device_malloc(sizeof(T)*sizeV), rocblas_test::device_free};
    T *dV = (T *)dV_managed.get();
    auto dtau_managed = rocblas_unique_ptr{rocblas_test::device_malloc(sizeof(T)*K), rocblas_test::device_free};
    T *dtau = (T *)dtau_managed.get();
    auto dF_managed = rocblas_unique_ptr{rocblas_test::device_malloc(sizeof(T)*sizeF), rocblas_test::device_free};
    T *dF = (T *)dF_managed.get();
    if (!dV || !dtau || !dF) {
        PRINT_IF_HIP_ERROR(hipErrorOutOfMemory);
        return rocblas_status_memory_error;
    }

    //initialize full random inputs with reals from -1 to 1
    rocblas_init<T>(hV.data(), N, K, ldv); 
    rocblas_init<T>(htau.data(), 1, K, 1); 
    for (int j=0;j<K;++j) {
        htau[j] = (htau[j]-5)/5.0; 
        for (int i=0;i<N;++i) {
            hV[i+j*ldv] = (hV[i+j*ldv]-5)/5.0; 
        }
    }

//        printf("\n");
  //      for (int i = 0; i < N; i++) {
    //        for (int j = 0; j < K; j++) {
      //          printf("%2.15f ",hV[i+j*ldt]);
        //    }
          //  printf("\n");
//        }
  //      printf("\n");
    //    for (int i = 0; i < K; i++) {
      //      printf("%2.15f ",htau[i]);
        //}
        //printf("\n\n");




    // copy data from CPU to device
    CHECK_HIP_ERROR(hipMemcpy(dV, hV.data(), sizeof(T) * sizeV, hipMemcpyHostToDevice));
    CHECK_HIP_ERROR(hipMemcpy(dtau, htau.data(), sizeof(T) * K, hipMemcpyHostToDevice));

    double gpu_time_used, cpu_time_used;
    double error_eps_multiplier = ERROR_EPS_MULTIPLIER;
    double eps = std::numeric_limits<T>::epsilon();
    double max_err_1 = 0.0, max_val = 0.0;
    double diff;

/* =====================================================================
           ROCSOLVER
    =================================================================== */  

    if (argus.unit_check || argus.norm_check) {
        //GPU lapack
        CHECK_ROCBLAS_ERROR(rocsolver_larft<T>(handle, direct, N, K, dV, ldv, dtau, dF, ldt));
        
        //copy output from device to cpu
        CHECK_HIP_ERROR(hipMemcpy(hF_r.data(), dF, sizeof(T) * sizeF, hipMemcpyDeviceToHost));

        //CPU lapack
        cpu_time_used = get_time_us();
        cblas_larft<T>(directchar, 'C', N, K, hV.data(), ldv, htau.data(), hF.data(), ldt);
        cpu_time_used = get_time_us() - cpu_time_used;

        // +++++++++ Error Check +++++++++++++
//        printf("\n");
  //      for (int i = 0; i < K; i++) {
    //        for (int j = 0; j < K; j++) {
      //          printf("%2.15f ",hF[i+j*ldt]);
        //    }
          //  printf("\n");
//        }
  //      printf("\n");
    //    for (int i = 0; i < K; i++) {
      //      for (int j = 0; j < K; j++) {
        //        printf("%2.15f ",hF_r[i+j*ldt]);
          //  }
//            printf("\n");
  //      }


        for (int i = 0; i < K; i++) {
            for (int j = 0; j < K; j++) {
                if ((j >= i && directchar == 'F') || (j <= i && directchar == 'B')) {
                    diff = fabs(hF[i + j * ldt]);
                    max_val = max_val > diff ? max_val : diff;
                    diff = hF[i + j * ldt];
                    diff = fabs(hF_r[i + j * ldt] - diff);
                    max_err_1 = max_err_1 > diff ? max_err_1 : diff;
                }
            }
        }

        max_err_1 = max_err_1 / max_val;
        
        if(argus.unit_check)
            getf2_err_res_check<T>(max_err_1, K, K, error_eps_multiplier, eps);
    }

    if (argus.timing) {
        // GPU rocBLAS
        int cold_calls = 2;

        for(int iter = 0; iter < cold_calls; iter++)
            rocsolver_larft<T>(handle, direct, N, K, dV, ldv, dtau, dF, ldt);
        gpu_time_used = get_time_us();
        for(int iter = 0; iter < hot_calls; iter++)
            rocsolver_larft<T>(handle, direct, N, K, dV, ldv, dtau, dF, ldt);
        gpu_time_used = (get_time_us() - gpu_time_used) / hot_calls;       

        // only norm_check return an norm error, unit check won't return anything
        cout << "direct,N,K,ldv,ldt,gpu_time(us),cpu_time(us)";

        if (argus.norm_check)
            cout << ",norm_error_host_ptr";

        cout << endl;
        cout << directchar << "," << N << "," << K << "," << ldv << "," << ldt << "," << gpu_time_used << "," << cpu_time_used;

        if (argus.norm_check)
            cout << "," << max_err_1;

        cout << endl;
    }
    
    return rocblas_status_success;
}

#undef ERROR_EPS_MULTIPLIER
