/*
Authors: Nishant Kumar, Deevashwer Rathee
Modified by Wen-jie Lu
Copyright:
Copyright (c) 2021 Microsoft Research
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "library_fixed_uniform.h"

#include "cleartext_library_fixed_uniform.h"
#include "functionalities_uniform.h"
#include "globals.h"
#include "library_fixed_common.h"
#define LOG_LAYERWISE
#define VERIFY_LAYERWISE
#undef VERIFY_LAYERWISE // undefine this to turn OFF the verifcation
// #undef LOG_LAYERWISE // undefine this to turn OFF the log

#ifdef SCI_HE
uint64_t prime_mod = sci::default_prime_mod.at(41);
#elif SCI_OT
uint64_t prime_mod = (bitlength == 64 ? 0ULL : 1ULL << bitlength);
uint64_t moduloMask = prime_mod - 1;
uint64_t moduloMidPt = prime_mod / 2;
#endif

#if !USE_CHEETAH
void MatMul2D(int32_t s1, int32_t s2, int32_t s3, const intType *A,
              const intType *B, intType *C, bool modelIsA) {
#ifdef LOG_LAYERWISE
  INIT_ALL_IO_DATA_SENT;
  INIT_TIMER;
#endif

  std::cout << "Matmul called s1,s2,s3 = " << s1 << " " << s2 << " " << s3
            << std::endl;

  // By default, the model is A and server/Alice has it
  // So, in the AB mult, party with A = server and party with B = client.
  int partyWithAInAB_mul = sci::ALICE;
  int partyWithBInAB_mul = sci::BOB;
  if (!modelIsA) {
    // Model is B
    partyWithAInAB_mul = sci::BOB;
    partyWithBInAB_mul = sci::ALICE;
  }

#if defined(SCI_OT)
#ifndef MULTITHREADED_MATMUL
#ifdef USE_LINEAR_UNIFORM
  if (partyWithAInAB_mul == sci::ALICE) {
    if (party == sci::ALICE) {
      multUniform->funcOTSenderInputA(s1, s2, s3, A, C, iknpOT);
    } else {
      multUniform->funcOTReceiverInputB(s1, s2, s3, B, C, iknpOT);
    }
  } else {
    if (party == sci::BOB) {
      multUniform->funcOTSenderInputA(s1, s2, s3, A, C, iknpOTRoleReversed);
    } else {
      multUniform->funcOTReceiverInputB(s1, s2, s3, B, C, iknpOTRoleReversed);
    }
  }
#else  // USE_LINEAR_UNIFORM
  if (modelIsA) {
    mult->matmul_cross_terms(s1, s2, s3, A, B, C, bitlength, bitlength,
                             bitlength, true, MultMode::Alice_has_A);
  } else {
    mult->matmul_cross_terms(s1, s2, s3, A, B, C, bitlength, bitlength,
                             bitlength, true, MultMode::Alice_has_B);
  }
#endif // USE_LINEAR_UNIFORM

  if (party == sci::ALICE) {
    // Now irrespective of whether A is the model or B is the model and whether
    //	server holds A or B, server should add locally A*B.
    //
    // Add also A*own share of B
    intType *CTemp = new intType[s1 * s3];
#ifdef USE_LINEAR_UNIFORM
    multUniform->ideal_func(s1, s2, s3, A, B, CTemp);
#else  // USE_LINEAR_UNIFORM
    mult->matmul_cleartext(s1, s2, s3, A, B, CTemp, true);
#endif // USE_LINEAR_UNIFORM
    sci::elemWiseAdd<intType>(s1 * s3, C, CTemp, C);
    delete[] CTemp;
  } else {
    // For minionn kind of hacky runs, switch this off
#ifndef HACKY_RUN
    if (modelIsA) {
      for (int i = 0; i < s1 * s2; i++)
        assert(A[i] == 0);
    } else {
      for (int i = 0; i < s1 * s2; i++)
        assert(B[i] == 0);
    }
#endif
  }

#else // MULTITHREADED_MATMUL is ON
  int required_num_threads = num_threads;
  if (s2 < num_threads) {
    required_num_threads = s2;
  }
  intType *C_ans_arr[required_num_threads];
  std::thread matmulThreads[required_num_threads];
  for (int i = 0; i < required_num_threads; i++) {
    C_ans_arr[i] = new intType[s1 * s3];
    matmulThreads[i] = std::thread(funcMatmulThread, i, required_num_threads,
                                   s1, s2, s3, (intType *)A, (intType *)B,
                                   (intType *)C_ans_arr[i], partyWithAInAB_mul);
  }
  for (int i = 0; i < required_num_threads; i++) {
    matmulThreads[i].join();
  }
  for (int i = 0; i < s1 * s3; i++) {
    C[i] = 0;
  }
  for (int i = 0; i < required_num_threads; i++) {
    for (int j = 0; j < s1 * s3; j++) {
      C[j] += C_ans_arr[i][j];
    }
    delete[] C_ans_arr[i];
  }

  if (party == sci::ALICE) {
    intType *CTemp = new intType[s1 * s3];
#ifdef USE_LINEAR_UNIFORM
    multUniform->ideal_func(s1, s2, s3, A, B, CTemp);
#else  // USE_LINEAR_UNIFORM
    mult->matmul_cleartext(s1, s2, s3, A, B, CTemp, true);
#endif // USE_LINEAR_UNIFORM
    sci::elemWiseAdd<intType>(s1 * s3, C, CTemp, C);
    delete[] CTemp;
  } else {
    // For minionn kind of hacky runs, switch this off
#ifndef HACKY_RUN
    if (modelIsA) {
      for (int i = 0; i < s1 * s2; i++)
        assert(A[i] == 0);
    } else {
      for (int i = 0; i < s1 * s2; i++)
        assert(B[i] == 0);
    }
#endif
  }
#endif
  intType moduloMask = (1ULL << bitlength) - 1;
  if (bitlength == 64)
    moduloMask = -1;
  for (int i = 0; i < s1 * s3; i++) {
    C[i] = C[i] & moduloMask;
  }

#elif defined(SCI_HE)
  // We only support matrix vector multiplication.
  assert(modelIsA == false &&
         "Assuming code generated by compiler produces B as the model.");
  std::vector<std::vector<intType>> At(s2);
  std::vector<std::vector<intType>> Bt(s3);
  std::vector<std::vector<intType>> Ct(s3);
  for (int i = 0; i < s2; i++) {
    At[i].resize(s1);
    for (int j = 0; j < s1; j++) {
      At[i][j] = getRingElt(Arr2DIdxRowM(A, s1, s2, j, i));
    }
  }
  for (int i = 0; i < s3; i++) {
    Bt[i].resize(s2);
    Ct[i].resize(s1);
    for (int j = 0; j < s2; j++) {
      Bt[i][j] = getRingElt(Arr2DIdxRowM(B, s2, s3, j, i));
    }
  }
  he_fc->matrix_multiplication(s3, s2, s1, Bt, At, Ct);
  for (int i = 0; i < s1; i++) {
    for (int j = 0; j < s3; j++) {
      Arr2DIdxRowM(C, s1, s3, i, j) = getRingElt(Ct[j][i]);
    }
  }
#endif

#ifdef LOG_LAYERWISE
  auto temp = TIMER_TILL_NOW;
  MatMulTimeInMilliSec += temp;
  std::cout << "Time in sec for current matmul = " << (temp / 1000.0)
            << std::endl;
  uint64_t curComm;
  FIND_ALL_IO_TILL_NOW(curComm);
  MatMulCommSent += curComm;
#endif

#ifdef VERIFY_LAYERWISE
#ifdef SCI_HE
  for (int i = 0; i < s1; i++) {
    for (int j = 0; j < s3; j++) {
      assert(Arr2DIdxRowM(C, s1, s3, i, j) < prime_mod);
    }
  }
#endif
  if (party == SERVER) {
    funcReconstruct2PCCons(nullptr, A, s1 * s2);
    funcReconstruct2PCCons(nullptr, B, s2 * s3);
    funcReconstruct2PCCons(nullptr, C, s1 * s3);
  } else {
    signedIntType *VA = new signedIntType[s1 * s2];
    funcReconstruct2PCCons(VA, A, s1 * s2);
    signedIntType *VB = new signedIntType[s2 * s3];
    funcReconstruct2PCCons(VB, B, s2 * s3);
    signedIntType *VC = new signedIntType[s1 * s3];
    funcReconstruct2PCCons(VC, C, s1 * s3);

    std::vector<std::vector<uint64_t>> VAvec;
    std::vector<std::vector<uint64_t>> VBvec;
    std::vector<std::vector<uint64_t>> VCvec;
    VAvec.resize(s1, std::vector<uint64_t>(s2, 0));
    VBvec.resize(s2, std::vector<uint64_t>(s3, 0));
    VCvec.resize(s1, std::vector<uint64_t>(s3, 0));

    for (int i = 0; i < s1; i++) {
      for (int j = 0; j < s2; j++) {
        VAvec[i][j] = getRingElt(Arr2DIdxRowM(VA, s1, s2, i, j));
      }
    }
    for (int i = 0; i < s2; i++) {
      for (int j = 0; j < s3; j++) {
        VBvec[i][j] = getRingElt(Arr2DIdxRowM(VB, s2, s3, i, j));
      }
    }

    MatMul2D_pt(s1, s2, s3, VAvec, VBvec, VCvec, 0);

    bool pass = true;
    for (int i = 0; i < s1; i++) {
      for (int j = 0; j < s3; j++) {
        if (Arr2DIdxRowM(VC, s1, s3, i, j) != getSignedVal(VCvec[i][j])) {
          pass = false;
        }
      }
    }
    if (pass == true)
      std::cout << GREEN << "MatMul Output Matches" << RESET << std::endl;
    else
      std::cout << RED << "MatMul Output Mismatch" << RESET << std::endl;

    delete[] VA;
    delete[] VB;
    delete[] VC;
  }
#endif
}
#endif

static void Conv2D(int32_t N, int32_t H, int32_t W, int32_t CI, int32_t FH,
                   int32_t FW, int32_t CO, int32_t zPadHLeft,
                   int32_t zPadHRight, int32_t zPadWLeft, int32_t zPadWRight,
                   int32_t strideH, int32_t strideW, uint64_t *inputArr,
                   uint64_t *filterArr, uint64_t *outArr) {
  int32_t reshapedFilterRows = CO;

  int32_t reshapedFilterCols = ((FH * FW) * CI);

  int32_t reshapedIPRows = ((FH * FW) * CI);

  int32_t newH =
      ((((H + (zPadHLeft + zPadHRight)) - FH) / strideH) + (int32_t)1);

  int32_t newW =
      ((((W + (zPadWLeft + zPadWRight)) - FW) / strideW) + (int32_t)1);

  int32_t reshapedIPCols = ((N * newH) * newW);

  uint64_t *filterReshaped =
      make_array<uint64_t>(reshapedFilterRows, reshapedFilterCols);

  uint64_t *inputReshaped =
      make_array<uint64_t>(reshapedIPRows, reshapedIPCols);

  uint64_t *matmulOP = make_array<uint64_t>(reshapedFilterRows, reshapedIPCols);
  Conv2DReshapeFilter(FH, FW, CI, CO, filterArr, filterReshaped);
  Conv2DReshapeInput(N, H, W, CI, FH, FW, zPadHLeft, zPadHRight, zPadWLeft,
                     zPadWRight, strideH, strideW, reshapedIPRows,
                     reshapedIPCols, inputArr, inputReshaped);
  MatMul2D(reshapedFilterRows, reshapedFilterCols, reshapedIPCols,
           filterReshaped, inputReshaped, matmulOP, 1);
  Conv2DReshapeMatMulOP(N, newH, newW, CO, matmulOP, outArr);
  ClearMemSecret2(reshapedFilterRows, reshapedFilterCols, filterReshaped);
  ClearMemSecret2(reshapedIPRows, reshapedIPCols, inputReshaped);
  ClearMemSecret2(reshapedFilterRows, reshapedIPCols, matmulOP);
}

#if !USE_CHEETAH
void Conv2DWrapper(signedIntType N, signedIntType H, signedIntType W,
                   signedIntType CI, signedIntType FH, signedIntType FW,
                   signedIntType CO, signedIntType zPadHLeft,
                   signedIntType zPadHRight, signedIntType zPadWLeft,
                   signedIntType zPadWRight, signedIntType strideH,
                   signedIntType strideW, intType *inputArr, intType *filterArr,
                   intType *outArr) {
#ifdef LOG_LAYERWISE
  INIT_ALL_IO_DATA_SENT;
  INIT_TIMER;
#endif

  static int ctr = 1;
  std::cout << "Conv2DCSF " << ctr << " called N=" << N << ", H=" << H
            << ", W=" << W << ", CI=" << CI << ", FH=" << FH << ", FW=" << FW
            << ", CO=" << CO << ", S=" << strideH << std::endl;
  ctr++;

  signedIntType newH = (((H + (zPadHLeft + zPadHRight) - FH) / strideH) + 1);
  signedIntType newW = (((W + (zPadWLeft + zPadWRight) - FW) / strideW) + 1);

#ifdef SCI_OT
  // If its a ring, then its a OT based -- use the default Conv2DCSF
  // implementation that comes from the EzPC library
  Conv2D(N, H, W, CI, FH, FW, CO, zPadHLeft, zPadHRight, zPadWLeft, zPadWRight,
         strideH, strideW, inputArr, filterArr, outArr);
#endif

#ifdef SCI_HE
  // If its a field, then its a HE based -- use the HE based conv implementation
  std::vector<std::vector<std::vector<std::vector<intType>>>> inputVec;
  inputVec.resize(N, std::vector<std::vector<std::vector<intType>>>(
                         H, std::vector<std::vector<intType>>(
                                W, std::vector<intType>(CI, 0))));

  std::vector<std::vector<std::vector<std::vector<intType>>>> filterVec;
  filterVec.resize(FH, std::vector<std::vector<std::vector<intType>>>(
                           FW, std::vector<std::vector<intType>>(
                                   CI, std::vector<intType>(CO, 0))));

  std::vector<std::vector<std::vector<std::vector<intType>>>> outputVec;
  outputVec.resize(N, std::vector<std::vector<std::vector<intType>>>(
                          newH, std::vector<std::vector<intType>>(
                                    newW, std::vector<intType>(CO, 0))));

  for (int i = 0; i < N; i++) {
    for (int j = 0; j < H; j++) {
      for (int k = 0; k < W; k++) {
        for (int p = 0; p < CI; p++) {
          inputVec[i][j][k][p] =
              getRingElt(Arr4DIdxRowM(inputArr, N, H, W, CI, i, j, k, p));
        }
      }
    }
  }
  for (int i = 0; i < FH; i++) {
    for (int j = 0; j < FW; j++) {
      for (int k = 0; k < CI; k++) {
        for (int p = 0; p < CO; p++) {
          filterVec[i][j][k][p] =
              getRingElt(Arr4DIdxRowM(filterArr, FH, FW, CI, CO, i, j, k, p));
        }
      }
    }
  }

  he_conv->convolution(N, H, W, CI, FH, FW, CO, zPadHLeft, zPadHRight,
                       zPadWLeft, zPadWRight, strideH, strideW, inputVec,
                       filterVec, outputVec);

  for (int i = 0; i < N; i++) {
    for (int j = 0; j < newH; j++) {
      for (int k = 0; k < newW; k++) {
        for (int p = 0; p < CO; p++) {
          Arr4DIdxRowM(outArr, N, newH, newW, CO, i, j, k, p) =
              getRingElt(outputVec[i][j][k][p]);
        }
      }
    }
  }

#endif

#ifdef LOG_LAYERWISE
  auto temp = TIMER_TILL_NOW;
  ConvTimeInMilliSec += temp;
  std::cout << "Time in sec for current conv = " << (temp / 1000.0)
            << std::endl;
  uint64_t curComm;
  FIND_ALL_IO_TILL_NOW(curComm);
  ConvCommSent += curComm;
#endif

#ifdef VERIFY_LAYERWISE
#ifdef SCI_HE
  for (int i = 0; i < N; i++) {
    for (int j = 0; j < newH; j++) {
      for (int k = 0; k < newW; k++) {
        for (int p = 0; p < CO; p++) {
          assert(Arr4DIdxRowM(outArr, N, newH, newW, CO, i, j, k, p) <
                 prime_mod);
        }
      }
    }
  }
#endif
  if (party == SERVER) {
    funcReconstruct2PCCons(nullptr, inputArr, N * H * W * CI);
    funcReconstruct2PCCons(nullptr, filterArr, FH * FW * CI * CO);
    funcReconstruct2PCCons(nullptr, outArr, N * newH * newW * CO);
  } else {
    signedIntType *VinputArr = new signedIntType[N * H * W * CI];
    funcReconstruct2PCCons(VinputArr, inputArr, N * H * W * CI);
    signedIntType *VfilterArr = new signedIntType[FH * FW * CI * CO];
    funcReconstruct2PCCons(VfilterArr, filterArr, FH * FW * CI * CO);
    signedIntType *VoutputArr = new signedIntType[N * newH * newW * CO];
    funcReconstruct2PCCons(VoutputArr, outArr, N * newH * newW * CO);

    std::vector<std::vector<std::vector<std::vector<uint64_t>>>> VinputVec;
    VinputVec.resize(N, std::vector<std::vector<std::vector<uint64_t>>>(
                            H, std::vector<std::vector<uint64_t>>(
                                   W, std::vector<uint64_t>(CI, 0))));

    std::vector<std::vector<std::vector<std::vector<uint64_t>>>> VfilterVec;
    VfilterVec.resize(FH, std::vector<std::vector<std::vector<uint64_t>>>(
                              FW, std::vector<std::vector<uint64_t>>(
                                      CI, std::vector<uint64_t>(CO, 0))));

    std::vector<std::vector<std::vector<std::vector<uint64_t>>>> VoutputVec;
    VoutputVec.resize(N, std::vector<std::vector<std::vector<uint64_t>>>(
                             newH, std::vector<std::vector<uint64_t>>(
                                       newW, std::vector<uint64_t>(CO, 0))));

    for (int i = 0; i < N; i++) {
      for (int j = 0; j < H; j++) {
        for (int k = 0; k < W; k++) {
          for (int p = 0; p < CI; p++) {
            VinputVec[i][j][k][p] =
                getRingElt(Arr4DIdxRowM(VinputArr, N, H, W, CI, i, j, k, p));
          }
        }
      }
    }
    for (int i = 0; i < FH; i++) {
      for (int j = 0; j < FW; j++) {
        for (int k = 0; k < CI; k++) {
          for (int p = 0; p < CO; p++) {
            VfilterVec[i][j][k][p] = getRingElt(
                Arr4DIdxRowM(VfilterArr, FH, FW, CI, CO, i, j, k, p));
          }
        }
      }
    }

    Conv2DWrapper_pt(N, H, W, CI, FH, FW, CO, zPadHLeft, zPadHRight, zPadWLeft,
                     zPadWRight, strideH, strideW, VinputVec, VfilterVec,
                     VoutputVec); // consSF = 0

    bool pass = true;
    for (int i = 0; i < N; i++) {
      for (int j = 0; j < newH; j++) {
        for (int k = 0; k < newW; k++) {
          for (int p = 0; p < CO; p++) {
            if (Arr4DIdxRowM(VoutputArr, N, newH, newW, CO, i, j, k, p) !=
                getSignedVal(VoutputVec[i][j][k][p])) {
              pass = false;
            }
          }
        }
      }
    }
    if (pass == true)
      std::cout << GREEN << "Convolution Output Matches" << RESET << std::endl;
    else
      std::cout << RED << "Convolution Output Mismatch" << RESET << std::endl;

    delete[] VinputArr;
    delete[] VfilterArr;
    delete[] VoutputArr;
  }
#endif
}
#endif

#ifdef SCI_OT
void Conv2DGroup(int32_t N, int32_t H, int32_t W, int32_t CI, int32_t FH,
                 int32_t FW, int32_t CO, int32_t zPadHLeft, int32_t zPadHRight,
                 int32_t zPadWLeft, int32_t zPadWRight, int32_t strideH,
                 int32_t strideW, int32_t G, intType *inputArr,
                 intType *filterArr, intType *outArr);
#endif

void Conv2DGroupWrapper(signedIntType N, signedIntType H, signedIntType W,
                        signedIntType CI, signedIntType FH, signedIntType FW,
                        signedIntType CO, signedIntType zPadHLeft,
                        signedIntType zPadHRight, signedIntType zPadWLeft,
                        signedIntType zPadWRight, signedIntType strideH,
                        signedIntType strideW, signedIntType G,
                        intType *inputArr, intType *filterArr,
                        intType *outArr) {
#ifdef LOG_LAYERWISE
  INIT_ALL_IO_DATA_SENT;
  INIT_TIMER;
#endif

  static int ctr = 1;
  std::cout << "Conv2DGroupCSF " << ctr << " called N=" << N << ", H=" << H
            << ", W=" << W << ", CI=" << CI << ", FH=" << FH << ", FW=" << FW
            << ", CO=" << CO << ", S=" << strideH << ",G=" << G << std::endl;
  ctr++;

#ifdef SCI_OT
  // If its a ring, then its a OT based -- use the default Conv2DGroupCSF
  // implementation that comes from the EzPC library
  Conv2DGroup(N, H, W, CI, FH, FW, CO, zPadHLeft, zPadHRight, zPadWLeft,
              zPadWRight, strideH, strideW, G, inputArr, filterArr, outArr);
#endif

#ifdef SCI_HE
  if (G == 1)
    Conv2DWrapper(N, H, W, CI, FH, FW, CO, zPadHLeft, zPadHRight, zPadWLeft,
                  zPadWRight, strideH, strideW, inputArr, filterArr, outArr);
  else
    assert(false && "Grouped conv not implemented in HE");
#endif

#ifdef LOG_LAYERWISE
  auto temp = TIMER_TILL_NOW;
  ConvTimeInMilliSec += temp;
  std::cout << "Time in sec for current conv = " << (temp / 1000.0)
            << std::endl;
  uint64_t curComm;
  FIND_ALL_IO_TILL_NOW(curComm);
  ConvCommSent += curComm;
#endif
}

#if !USE_CHEETAH
void ElemWiseActModelVectorMult(int32_t size, intType *inArr,
                                intType *multArrVec, intType *outputArr) {
  CountElementMul += size;
#ifdef LOG_LAYERWISE
  INIT_ALL_IO_DATA_SENT;
  INIT_TIMER;
#endif

  if (party == CLIENT) {
    for (int i = 0; i < size; i++) {
      assert((multArrVec[i] == 0) &&
             "The semantics of ElemWiseActModelVectorMult dictate multArrVec "
             "should be the model and client share should be 0 for it.");
    }
  }

  static int batchNormCtr = 1;
  std::cout << "Starting fused batchNorm #" << batchNormCtr << std::endl;
  batchNormCtr++;

#ifdef SCI_OT
#ifdef MULTITHREADED_DOTPROD
  std::thread dotProdThreads[num_threads];
  int chunk_size = ceil(size / double(num_threads));
  intType *inputArrPtr;
  if (party == SERVER) {
    inputArrPtr = multArrVec;
  } else {
    inputArrPtr = inArr;
  }
  for (int i = 0; i < num_threads; i++) {
    int offset = i * chunk_size;
    int curSize;
    curSize =
        ((i + 1) * chunk_size > size ? std::max(0, size - offset) : chunk_size);
    /*
    if (i == (num_threads - 1)) {
        curSize = size - offset;
    }
    else{
        curSize = chunk_size;
    }
    */
    dotProdThreads[i] = std::thread(funcDotProdThread, i, num_threads, curSize,
                                    multArrVec + offset, inArr + offset,
                                    outputArr + offset, false);
  }
  for (int i = 0; i < num_threads; ++i) {
    dotProdThreads[i].join();
  }
#else
  matmul->hadamard_cross_terms(size, multArrVec, inArr, outputArr, bitlength,
                               bitlength, bitlength, MultMode::Alice_has_A);
#endif

  if (party == SERVER) {
    for (int i = 0; i < size; i++) {
      outputArr[i] += (inArr[i] * multArrVec[i]);
    }
  } else {
    for (int i = 0; i < size; i++) {
      assert(multArrVec[i] == 0 && "Client's share of model is non-zero.");
    }
  }
#endif // SCI_OT

#ifdef SCI_HE
  std::vector<uint64_t> tempInArr(size);
  std::vector<uint64_t> tempOutArr(size);
  std::vector<uint64_t> tempMultArr(size);

  for (int i = 0; i < size; i++) {
    tempInArr[i] = getRingElt(inArr[i]);
    tempMultArr[i] = getRingElt(multArrVec[i]);
  }

  he_prod->elemwise_product(size, tempInArr, tempMultArr, tempOutArr);

  for (int i = 0; i < size; i++) {
    outputArr[i] = getRingElt(tempOutArr[i]);
  }
#endif

#ifdef LOG_LAYERWISE
  auto temp = TIMER_TILL_NOW;
  BatchNormInMilliSec += temp;
  uint64_t curComm;
  FIND_ALL_IO_TILL_NOW(curComm);
  BatchNormCommSent += curComm;
  std::cout << "Time in sec for current BN = [" << (temp / 1000.0) << "] sent ["
            << (curComm / 1024. / 1024.) << "] MB" << std::endl;
#endif

#ifdef VERIFY_LAYERWISE
#ifdef SCI_HE
  for (int i = 0; i < size; i++) {
    assert(outputArr[i] < prime_mod);
  }
#endif
  if (party == SERVER) {
    funcReconstruct2PCCons(nullptr, inArr, size);
    funcReconstruct2PCCons(nullptr, multArrVec, size);
    funcReconstruct2PCCons(nullptr, outputArr, size);
  } else {
    signedIntType *VinArr = new signedIntType[size];
    funcReconstruct2PCCons(VinArr, inArr, size);
    signedIntType *VmultArr = new signedIntType[size];
    funcReconstruct2PCCons(VmultArr, multArrVec, size);
    signedIntType *VoutputArr = new signedIntType[size];
    funcReconstruct2PCCons(VoutputArr, outputArr, size);

    std::vector<uint64_t> VinVec(size);
    std::vector<uint64_t> VmultVec(size);
    std::vector<uint64_t> VoutputVec(size);

    for (int i = 0; i < size; i++) {
      VinVec[i] = getRingElt(VinArr[i]);
      VmultVec[i] = getRingElt(VmultArr[i]);
    }

    ElemWiseActModelVectorMult_pt(size, VinVec, VmultVec, VoutputVec);

    bool pass = true;
    for (int i = 0; i < size; i++) {
      if (VoutputArr[i] != getSignedVal(VoutputVec[i])) {
        pass = false;
      }
    }
    if (pass == true)
      std::cout << GREEN << "ElemWiseSecretVectorMult Output Matches" << RESET
                << std::endl;
    else
      std::cout << RED << "ElemWiseSecretVectorMult Output Mismatch" << RESET
                << std::endl;

    delete[] VinArr;
    delete[] VmultArr;
    delete[] VoutputArr;
  }
#endif
}
#endif

void ArgMax(int32_t s1, int32_t s2, intType *inArr, intType *outArr) {
#ifdef LOG_LAYERWISE
  INIT_ALL_IO_DATA_SENT;
  INIT_TIMER;
#endif

  static int ctr = 1;
  std::cout << "ArgMax " << ctr << " called, s1=" << s1 << ", s2=" << s2
            << std::endl;
  ctr++;

  assert(s1 == 1 && "ArgMax impl right now assumes s1==1");
  argmax->ArgMaxMPC(s2, inArr, outArr);

#ifdef LOG_LAYERWISE
  auto temp = TIMER_TILL_NOW;
  ArgMaxTimeInMilliSec += temp;
  uint64_t curComm;
  FIND_ALL_IO_TILL_NOW(curComm);
  ArgMaxCommSent += curComm;
#endif

#ifdef VERIFY_LAYERWISE
  if (party == SERVER) {
    funcReconstruct2PCCons(nullptr, inArr, s1 * s2);
    funcReconstruct2PCCons(nullptr, outArr, s1);
  } else {
    signedIntType *VinArr = new signedIntType[s1 * s2];
    funcReconstruct2PCCons(VinArr, inArr, s1 * s2);
    signedIntType *VoutArr = new signedIntType[s1];
    funcReconstruct2PCCons(VoutArr, outArr, s1);

    std::vector<std::vector<uint64_t>> VinVec;
    VinVec.resize(s1, std::vector<uint64_t>(s2, 0));
    std::vector<uint64_t> VoutVec(s1);

    for (int i = 0; i < s1; i++) {
      for (int j = 0; j < s2; j++) {
        VinVec[i][j] = getRingElt(Arr2DIdxRowM(VinArr, s1, s2, i, j));
      }
    }

    ArgMax_pt(s1, s2, VinVec, VoutVec);

    bool pass = true;
    for (int i = 0; i < s1; i++) {
      std::cout << VoutArr[i] << " =? " << getSignedVal(VoutVec[i])
                << std::endl;
      if (VoutArr[i] != getSignedVal(VoutVec[i])) {
        pass = false;
      }
    }

    if (pass == true) {
      std::cout << GREEN << "ArgMax1 Output Matches" << RESET << std::endl;
    } else {
      std::cout << RED << "ArgMax1 Output Mismatch" << RESET << std::endl;
    }

    delete[] VinArr;
    delete[] VoutArr;
  }
#endif
}

void Relu(int32_t size, intType *inArr, intType *outArr, int sf,
          bool doTruncation) {
#ifdef LOG_LAYERWISE
  INIT_ALL_IO_DATA_SENT;
  INIT_TIMER;
#endif

  static int ctr = 1;
  printf("Relu #%d on %d points, truncate=%d by %d bits\n", ctr++, size,
         doTruncation, sf);
  ctr++;

  intType moduloMask = sci::all1Mask(bitlength);
  int eightDivElemts = ((size + 8 - 1) / 8) * 8; //(ceil of s1*s2/8.0)*8
  uint8_t *msbShare = new uint8_t[eightDivElemts];
  intType *tempInp = new intType[eightDivElemts];
  intType *tempOutp = new intType[eightDivElemts];
  sci::copyElemWisePadded(size, inArr, eightDivElemts, tempInp, 0);

// #ifndef MULTITHREADED_NONLIN
#if 0
  relu->relu(tempOutp, tempInp, eightDivElemts, nullptr, doTruncation, true);
#else
  std::thread relu_threads[num_threads];
  int chunk_size = (eightDivElemts / (8 * num_threads)) * 8;
  for (int i = 0; i < num_threads; ++i) {
    int offset = i * chunk_size;
    int lnum_relu;
    if (i == (num_threads - 1)) {
      lnum_relu = eightDivElemts - offset;
    } else {
      lnum_relu = chunk_size;
    }
    relu_threads[i] =
        std::thread(funcReLUThread, i, tempOutp + offset, tempInp + offset,
                    lnum_relu, nullptr, false, doTruncation, /*approx*/ true);
  }
  for (int i = 0; i < num_threads; ++i) {
    relu_threads[i].join();
  }
#endif

#ifdef LOG_LAYERWISE
  auto temp = TIMER_TILL_NOW;
  ReluTimeInMilliSec += temp;
  std::cout << "Time in sec for current relu = " << (temp / 1000.0)
            << std::endl;
  uint64_t curComm;
  FIND_ALL_IO_TILL_NOW(curComm);
  ReluCommSent += curComm;
#endif

  if (doTruncation) {
#ifdef LOG_LAYERWISE
    INIT_ALL_IO_DATA_SENT;
    INIT_TIMER;
#endif
    for (int i = 0; i < eightDivElemts; i++) {
      msbShare[i] = 0; // After relu, all numbers are +ve
    }

    intType *tempTruncOutp = new intType[eightDivElemts];
#ifdef SCI_OT
    for (int i = 0; i < eightDivElemts; i++) {
      tempOutp[i] = tempOutp[i] & moduloMask;
    }

#if USE_CHEETAH == 0
    funcTruncateTwoPowerRingWrapper(eightDivElemts, tempOutp, tempTruncOutp, sf,
                                    bitlength, true, msbShare);
#else
    funcReLUTruncateTwoPowerRingWrapper(eightDivElemts, tempOutp, tempTruncOutp,
                                        sf, bitlength, true);
#endif

#else
    funcFieldDivWrapper<intType>(eightDivElemts, tempOutp, tempTruncOutp,
                                 1ULL << sf, msbShare);
#endif
    memcpy(outArr, tempTruncOutp, size * sizeof(intType));
    delete[] tempTruncOutp;

#ifdef LOG_LAYERWISE
    auto temp = TIMER_TILL_NOW;
    TruncationTimeInMilliSec += temp;
    uint64_t curComm;
    FIND_ALL_IO_TILL_NOW(curComm);
    TruncationCommSent += curComm;
#endif
  } else {
    for (int i = 0; i < size; i++) {
      outArr[i] = tempOutp[i];
    }
  }

#ifdef SCI_OT
  for (int i = 0; i < size; i++) {
    outArr[i] = outArr[i] & moduloMask;
  }
#endif

#ifdef VERIFY_LAYERWISE
#ifdef SCI_HE
  for (int i = 0; i < size; i++) {
    assert(tempOutp[i] < prime_mod);
    assert(outArr[i] < prime_mod);
  }
#endif

  if (party == SERVER) {
    funcReconstruct2PCCons(nullptr, inArr, size);
    funcReconstruct2PCCons(nullptr, tempOutp, size);
    funcReconstruct2PCCons(nullptr, outArr, size);
  } else {
    signedIntType *VinArr = new signedIntType[size];
    funcReconstruct2PCCons(VinArr, inArr, size);
    signedIntType *VtempOutpArr = new signedIntType[size];
    funcReconstruct2PCCons(VtempOutpArr, tempOutp, size);
    signedIntType *VoutArr = new signedIntType[size];
    funcReconstruct2PCCons(VoutArr, outArr, size);

    std::vector<uint64_t> VinVec;
    VinVec.resize(size, 0);

    std::vector<uint64_t> VoutVec;
    VoutVec.resize(size, 0);

    for (int i = 0; i < size; i++) {
      VinVec[i] = getRingElt(VinArr[i]);
    }

    Relu_pt(size, VinVec, VoutVec, 0, false); // sf = 0

    bool pass = true;
    for (int i = 0; i < size; i++) {
      if (VtempOutpArr[i] != getSignedVal(VoutVec[i])) {
        pass = false;
      }
    }
    if (pass == true)
      std::cout << GREEN << "ReLU Output Matches" << RESET << std::endl;
    else
      std::cout << RED << "ReLU Output Mismatch" << RESET << std::endl;

    ScaleDown_pt(size, VoutVec, sf);

    pass = true;
#if USE_CHEETAH
    constexpr signedIntType error_upper = 1;
#else
    constexpr signedIntType error_upper = 0;
#endif
    for (int i = 0; i < size; i++) {
      if (std::abs(VoutArr[i] - getSignedVal(VoutVec[i])) > error_upper) {
        pass = false;
      }
    }
    if (pass == true)
      std::cout << GREEN << "Truncation (after ReLU) Output Matches" << RESET
                << std::endl;
    else
      std::cout << RED << "Truncation (after ReLU) Output Mismatch" << RESET
                << std::endl;

    delete[] VinArr;
    delete[] VtempOutpArr;
    delete[] VoutArr;
  }
#endif

  delete[] tempInp;
  delete[] tempOutp;
  delete[] msbShare;
}

void MaxPool(int32_t N, int32_t H, int32_t W, int32_t C, int32_t ksizeH,
             int32_t ksizeW, int32_t zPadHLeft, int32_t zPadHRight,
             int32_t zPadWLeft, int32_t zPadWRight, int32_t strideH,
             int32_t strideW, int32_t N1, int32_t imgH, int32_t imgW,
             int32_t C1, intType *inArr, intType *outArr) {
#ifdef LOG_LAYERWISE
  INIT_ALL_IO_DATA_SENT;
  INIT_TIMER;
#endif

  static int ctr = 1;
  std::cout << "Maxpool " << ctr << " called N=" << N << ", H=" << H
            << ", W=" << W << ", C=" << C << ", ksizeH=" << ksizeH
            << ", ksizeW=" << ksizeW << std::endl;
  ctr++;

  uint64_t moduloMask = sci::all1Mask(bitlength);
  int rowsOrig = N * H * W * C;
  int rows = ((rowsOrig + 8 - 1) / 8) * 8; //(ceil of rows/8.0)*8
  int cols = ksizeH * ksizeW;

  intType *reInpArr = new intType[rows * cols];
  intType *maxi = new intType[rows];
  intType *maxiIdx = new intType[rows];

  int rowIdx = 0;
  for (int n = 0; n < N; n++) {
    for (int c = 0; c < C; c++) {
      int32_t leftTopCornerH = -zPadHLeft;
      int32_t extremeRightBottomCornerH = imgH - 1 + zPadHRight;
      while ((leftTopCornerH + ksizeH - 1) <= extremeRightBottomCornerH) {
        int32_t leftTopCornerW = -zPadWLeft;
        int32_t extremeRightBottomCornerW = imgW - 1 + zPadWRight;
        while ((leftTopCornerW + ksizeW - 1) <= extremeRightBottomCornerW) {
          for (int fh = 0; fh < ksizeH; fh++) {
            for (int fw = 0; fw < ksizeW; fw++) {
              int32_t colIdx = fh * ksizeW + fw;
              int32_t finalIdx = rowIdx * (ksizeH * ksizeW) + colIdx;

              int32_t curPosH = leftTopCornerH + fh;
              int32_t curPosW = leftTopCornerW + fw;

              intType temp = 0;
              if ((((curPosH < 0) || (curPosH >= imgH)) ||
                   ((curPosW < 0) || (curPosW >= imgW)))) {
                temp = 0;
              } else {
                temp = Arr4DIdxRowM(inArr, N, imgH, imgW, C, n, curPosH,
                                    curPosW, c);
              }
              reInpArr[finalIdx] = temp;
            }
          }

          rowIdx += 1;
          leftTopCornerW = leftTopCornerW + strideW;
        }

        leftTopCornerH = leftTopCornerH + strideH;
      }
    }
  }

  for (int i = rowsOrig; i < rows; i++) {
    for (int j = 0; j < cols; j++) {
      reInpArr[i * cols + j] = 0; // The extra padded values
    }
  }

#ifndef MULTITHREADED_NONLIN
  maxpool->funcMaxMPC(rows, cols, reInpArr, maxi, maxiIdx);
#else
  std::thread maxpool_threads[num_threads];
  int chunk_size = (rows / (8 * num_threads)) * 8;
  for (int i = 0; i < num_threads; ++i) {
    int offset = i * chunk_size;
    int lnum_rows;
    if (i == (num_threads - 1)) {
      lnum_rows = rows - offset;
    } else {
      lnum_rows = chunk_size;
    }
    maxpool_threads[i] =
        std::thread(funcMaxpoolThread, i, lnum_rows, cols,
                    reInpArr + offset * cols, maxi + offset, maxiIdx + offset);
  }
  for (int i = 0; i < num_threads; ++i) {
    maxpool_threads[i].join();
  }
#endif

  for (int n = 0; n < N; n++) {
    for (int c = 0; c < C; c++) {
      for (int h = 0; h < H; h++) {
        for (int w = 0; w < W; w++) {
          int iidx = n * C * H * W + c * H * W + h * W + w;
          Arr4DIdxRowM(outArr, N, H, W, C, n, h, w, c) = getRingElt(maxi[iidx]);
        }
      }
    }
  }

  delete[] reInpArr;
  delete[] maxi;
  delete[] maxiIdx;

#ifdef LOG_LAYERWISE
  auto temp = TIMER_TILL_NOW;
  MaxpoolTimeInMilliSec += temp;
  std::cout << "Time in sec for current maxpool = " << (temp / 1000.0)
            << std::endl;
  uint64_t curComm;
  FIND_ALL_IO_TILL_NOW(curComm);
  MaxpoolCommSent += curComm;
#endif

#ifdef VERIFY_LAYERWISE
#ifdef SCI_HE
  for (int i = 0; i < N; i++) {
    for (int j = 0; j < H; j++) {
      for (int k = 0; k < W; k++) {
        for (int p = 0; p < C; p++) {
          assert(Arr4DIdxRowM(outArr, N, H, W, C, i, j, k, p) < prime_mod);
        }
      }
    }
  }
#endif
  if (party == SERVER) {
    funcReconstruct2PCCons(nullptr, inArr, N * imgH * imgW * C);
    funcReconstruct2PCCons(nullptr, outArr, N * H * W * C);
  } else {
    signedIntType *VinArr = new signedIntType[N * imgH * imgW * C];
    funcReconstruct2PCCons(VinArr, inArr, N * imgH * imgW * C);
    signedIntType *VoutArr = new signedIntType[N * H * W * C];
    funcReconstruct2PCCons(VoutArr, outArr, N * H * W * C);

    std::vector<std::vector<std::vector<std::vector<uint64_t>>>> VinVec;
    VinVec.resize(N, std::vector<std::vector<std::vector<uint64_t>>>(
                         imgH, std::vector<std::vector<uint64_t>>(
                                   imgW, std::vector<uint64_t>(C, 0))));

    std::vector<std::vector<std::vector<std::vector<uint64_t>>>> VoutVec;
    VoutVec.resize(N, std::vector<std::vector<std::vector<uint64_t>>>(
                          H, std::vector<std::vector<uint64_t>>(
                                 W, std::vector<uint64_t>(C, 0))));

    for (int i = 0; i < N; i++) {
      for (int j = 0; j < imgH; j++) {
        for (int k = 0; k < imgW; k++) {
          for (int p = 0; p < C; p++) {
            VinVec[i][j][k][p] =
                getRingElt(Arr4DIdxRowM(VinArr, N, imgH, imgW, C, i, j, k, p));
          }
        }
      }
    }

    MaxPool_pt(N, H, W, C, ksizeH, ksizeW, zPadHLeft, zPadHRight, zPadWLeft,
               zPadWRight, strideH, strideW, N1, imgH, imgW, C1, VinVec,
               VoutVec);

    bool pass = true;
    for (int i = 0; i < N; i++) {
      for (int j = 0; j < H; j++) {
        for (int k = 0; k < W; k++) {
          for (int p = 0; p < C; p++) {
            if (Arr4DIdxRowM(VoutArr, N, H, W, C, i, j, k, p) !=
                getSignedVal(VoutVec[i][j][k][p])) {
              pass = false;
              // std::cout << i << "\t" << j << "\t" << k << "\t" << p << "\t"
              // << Arr4DIdxRowM(VoutArr,N,H,W,C,i,j,k,p) << "\t" <<
              // getSignedVal(VoutVec[i][j][k][p]) << std::endl;
            }
          }
        }
      }
    }
    if (pass == true)
      std::cout << GREEN << "Maxpool Output Matches" << RESET << std::endl;
    else
      std::cout << RED << "Maxpool Output Mismatch" << RESET << std::endl;

    delete[] VinArr;
    delete[] VoutArr;
  }
#endif
}

void AvgPool(int32_t N, int32_t H, int32_t W, int32_t C, int32_t ksizeH,
             int32_t ksizeW, int32_t zPadHLeft, int32_t zPadHRight,
             int32_t zPadWLeft, int32_t zPadWRight, int32_t strideH,
             int32_t strideW, int32_t N1, int32_t imgH, int32_t imgW,
             int32_t C1, intType *inArr, intType *outArr) {
#ifdef LOG_LAYERWISE
  INIT_ALL_IO_DATA_SENT;
  INIT_TIMER;
#endif

  static int ctr = 1;
  std::cout << "AvgPool " << ctr << " called N=" << N << ", H=" << H
            << ", W=" << W << ", C=" << C << ", ksizeH=" << ksizeH
            << ", ksizeW=" << ksizeW << std::endl;
  ctr++;

  uint64_t moduloMask = sci::all1Mask(bitlength);
  int rows = N * H * W * C;
  int rowsPadded = ((rows + 8 - 1) / 8) * 8;
  intType *filterSum = new intType[rowsPadded];
  intType *filterAvg = new intType[rowsPadded];

  int rowIdx = 0;
  for (int n = 0; n < N; n++) {
    for (int c = 0; c < C; c++) {
      int32_t leftTopCornerH = -zPadHLeft;
      int32_t extremeRightBottomCornerH = imgH - 1 + zPadHRight;
      while ((leftTopCornerH + ksizeH - 1) <= extremeRightBottomCornerH) {
        int32_t leftTopCornerW = -zPadWLeft;
        int32_t extremeRightBottomCornerW = imgW - 1 + zPadWRight;
        while ((leftTopCornerW + ksizeW - 1) <= extremeRightBottomCornerW) {
          intType curFilterSum = 0;
          for (int fh = 0; fh < ksizeH; fh++) {
            for (int fw = 0; fw < ksizeW; fw++) {
              int32_t curPosH = leftTopCornerH + fh;
              int32_t curPosW = leftTopCornerW + fw;

              intType temp = 0;
              if ((((curPosH < 0) || (curPosH >= imgH)) ||
                   ((curPosW < 0) || (curPosW >= imgW)))) {
                temp = 0;
              } else {
                temp = Arr4DIdxRowM(inArr, N, imgH, imgW, C, n, curPosH,
                                    curPosW, c);
              }
#ifdef SCI_OT
              curFilterSum += temp;
#else
              curFilterSum =
                  sci::neg_mod(curFilterSum + temp, (int64_t)prime_mod);
#endif
            }
          }

          filterSum[rowIdx] = curFilterSum;
          rowIdx += 1;
          leftTopCornerW = leftTopCornerW + strideW;
        }

        leftTopCornerH = leftTopCornerH + strideH;
      }
    }
  }

  for (int i = rows; i < rowsPadded; i++) {
    filterSum[i] = 0;
  }

#ifdef SCI_OT
  for (int i = 0; i < rowsPadded; i++) {
    filterSum[i] = filterSum[i] & moduloMask;
  }
  funcAvgPoolTwoPowerRingWrapper(rowsPadded, filterSum, filterAvg,
                                 ksizeH * ksizeW);
#else
  for (int i = 0; i < rowsPadded; i++) {
    filterSum[i] = sci::neg_mod(filterSum[i], (int64_t)prime_mod);
  }
  funcFieldDivWrapper<intType>(rowsPadded, filterSum, filterAvg,
                               ksizeH * ksizeW, nullptr);
#endif

  for (int n = 0; n < N; n++) {
    for (int c = 0; c < C; c++) {
      for (int h = 0; h < H; h++) {
        for (int w = 0; w < W; w++) {
          int iidx = n * C * H * W + c * H * W + h * W + w;
          Arr4DIdxRowM(outArr, N, H, W, C, n, h, w, c) = filterAvg[iidx];
#ifdef SCI_OT
          Arr4DIdxRowM(outArr, N, H, W, C, n, h, w, c) =
              Arr4DIdxRowM(outArr, N, H, W, C, n, h, w, c) & moduloMask;
#endif
        }
      }
    }
  }

  delete[] filterSum;
  delete[] filterAvg;

#ifdef LOG_LAYERWISE
  auto temp = TIMER_TILL_NOW;
  AvgpoolTimeInMilliSec += temp;
  std::cout << "Time in sec for current avgpool = " << (temp / 1000.0)
            << std::endl;
  uint64_t curComm;
  FIND_ALL_IO_TILL_NOW(curComm);
  AvgpoolCommSent += curComm;
#endif

#ifdef VERIFY_LAYERWISE
#ifdef SCI_HE
  for (int i = 0; i < N; i++) {
    for (int j = 0; j < H; j++) {
      for (int k = 0; k < W; k++) {
        for (int p = 0; p < C; p++) {
          assert(Arr4DIdxRowM(outArr, N, H, W, C, i, j, k, p) < prime_mod);
        }
      }
    }
  }
#endif
  if (party == SERVER) {
    funcReconstruct2PCCons(nullptr, inArr, N * imgH * imgW * C);
    funcReconstruct2PCCons(nullptr, outArr, N * H * W * C);
  } else {
    signedIntType *VinArr = new signedIntType[N * imgH * imgW * C];
    funcReconstruct2PCCons(VinArr, inArr, N * imgH * imgW * C);
    signedIntType *VoutArr = new signedIntType[N * H * W * C];
    funcReconstruct2PCCons(VoutArr, outArr, N * H * W * C);

    std::vector<std::vector<std::vector<std::vector<uint64_t>>>> VinVec;
    VinVec.resize(N, std::vector<std::vector<std::vector<uint64_t>>>(
                         imgH, std::vector<std::vector<uint64_t>>(
                                   imgW, std::vector<uint64_t>(C, 0))));

    std::vector<std::vector<std::vector<std::vector<uint64_t>>>> VoutVec;
    VoutVec.resize(N, std::vector<std::vector<std::vector<uint64_t>>>(
                          H, std::vector<std::vector<uint64_t>>(
                                 W, std::vector<uint64_t>(C, 0))));

    for (int i = 0; i < N; i++) {
      for (int j = 0; j < imgH; j++) {
        for (int k = 0; k < imgW; k++) {
          for (int p = 0; p < C; p++) {
            VinVec[i][j][k][p] =
                getRingElt(Arr4DIdxRowM(VinArr, N, imgH, imgW, C, i, j, k, p));
          }
        }
      }
    }

    AvgPool_pt(N, H, W, C, ksizeH, ksizeW, zPadHLeft, zPadHRight, zPadWLeft,
               zPadWRight, strideH, strideW, N1, imgH, imgW, C1, VinVec,
               VoutVec);

    bool pass = true;
    for (int i = 0; i < N; i++) {
      for (int j = 0; j < H; j++) {
        for (int k = 0; k < W; k++) {
          for (int p = 0; p < C; p++) {
            if (Arr4DIdxRowM(VoutArr, N, H, W, C, i, j, k, p) !=
                getSignedVal(VoutVec[i][j][k][p])) {
              pass = false;
            }
          }
        }
      }
    }

    if (pass == true)
      std::cout << GREEN << "AvgPool Output Matches" << RESET << std::endl;
    else
      std::cout << RED << "AvgPool Output Mismatch" << RESET << std::endl;

    delete[] VinArr;
    delete[] VoutArr;
  }
#endif
}

void ScaleDown(int32_t size, intType *inArr, int32_t sf) {
#ifdef LOG_LAYERWISE
  INIT_ALL_IO_DATA_SENT;
  INIT_TIMER;
#endif
  static int ctr = 1;
  printf("Truncate #%d on %d points by %d bits\n", ctr++, size, sf);

  int eightDivElemts = ((size + 8 - 1) / 8) * 8; //(ceil of s1*s2/8.0)*8
  intType *tempInp;
  if (size != eightDivElemts) {
    tempInp = new intType[eightDivElemts];
    memcpy(tempInp, inArr, sizeof(intType) * size);
  } else {
    tempInp = inArr;
  }
  intType *outp = new intType[eightDivElemts];

#ifdef SCI_OT
  uint64_t moduloMask = sci::all1Mask(bitlength);
  for (int i = 0; i < eightDivElemts; i++) {
    tempInp[i] = tempInp[i] & moduloMask;
  }

  funcTruncateTwoPowerRingWrapper(eightDivElemts, tempInp, outp, sf, bitlength,
                                  true, nullptr);
#else
  for (int i = 0; i < eightDivElemts; i++) {
    tempInp[i] = sci::neg_mod(tempInp[i], (int64_t)prime_mod);
  }
  funcFieldDivWrapper<intType>(eightDivElemts, tempInp, outp, 1ULL << sf,
                               nullptr);
#endif

#ifdef LOG_LAYERWISE
  auto temp = TIMER_TILL_NOW;
  TruncationTimeInMilliSec += temp;
  uint64_t curComm;
  FIND_ALL_IO_TILL_NOW(curComm);
  TruncationCommSent += curComm;
#endif

#ifdef VERIFY_LAYERWISE
#ifdef SCI_HE
  for (int i = 0; i < size; i++) {
    assert(outp[i] < prime_mod);
  }
#endif

  if (party == SERVER) {
    funcReconstruct2PCCons(nullptr, inArr, size);
    funcReconstruct2PCCons(nullptr, outp, size);
  } else {
    signedIntType *VinArr = new signedIntType[size];
    funcReconstruct2PCCons(VinArr, inArr, size);
    signedIntType *VoutpArr = new signedIntType[size];
    funcReconstruct2PCCons(VoutpArr, outp, size);

    std::vector<uint64_t> VinVec;
    VinVec.resize(size, 0);

    for (int i = 0; i < size; i++) {
      VinVec[i] = getRingElt(VinArr[i]);
    }

    ScaleDown_pt(size, VinVec, sf);

    bool pass = true;
#if USE_CHEETAH
    constexpr signedIntType error_upper = 1;
#else
    constexpr signedIntType error_upper = 0;
#endif
    for (int i = 0; i < size; i++) {
      if (std::abs(VoutpArr[i] - getSignedVal(VinVec[i])) > error_upper) {
        pass = false;
      }
    }

    if (pass == true)
      std::cout << GREEN << "Truncation4 Output Matches" << RESET << std::endl;
    else
      std::cout << RED << "Truncation4 Output Mismatch" << RESET << std::endl;

    delete[] VinArr;
    delete[] VoutpArr;
  }
#endif

  std::memcpy(inArr, outp, sizeof(intType) * size);
  delete[] outp;
  if (size != eightDivElemts)
    delete[] tempInp;
}

void ScaleUp(int32_t size, intType *arr, int32_t sf) {
  for (int i = 0; i < size; i++) {
#ifdef SCI_OT
    arr[i] = (arr[i] << sf);
#else
    arr[i] = sci::neg_mod(arr[i] << sf, (int64_t)prime_mod);
#endif
  }
}

void StartComputation() {
  assert(bitlength < 64 && bitlength > 0);
  assert(num_threads <= MAX_THREADS);

  std::string backend;

#ifdef SCI_HE
  backend = "PrimeField";
  auto kv = sci::default_prime_mod.find(bitlength);
  if (kv == sci::default_prime_mod.end()) {
    bitlength = 41;
    prime_mod = sci::default_prime_mod.at(bitlength);
  } else {
    prime_mod = kv->second;
  }
#elif SCI_OT
  prime_mod = (bitlength == 64 ? 0ULL : 1ULL << bitlength);
  moduloMask = prime_mod - 1;
  moduloMidPt = prime_mod / 2;
  backend = "Ring";
#endif

#if USE_CHEETAH
  backend += "-SilentOT";
#else
  backend += "-OT";
#endif

  checkIfUsingEigen();
  printf("Doing BaseOT ...\n");
  for (int i = 0; i < num_threads; i++) {
    ioArr[i] = new sci::NetIO(party == sci::ALICE ? nullptr : address.c_str(),
                              port + i, /*quit*/ true);
    otInstanceArr[i] = new sci::IKNP<sci::NetIO>(ioArr[i]);
    prgInstanceArr[i] = new sci::PRG128();
    kkotInstanceArr[i] = new sci::KKOT<sci::NetIO>(ioArr[i]);
#ifdef SCI_OT
    multUniformArr[i] =
        new MatMulUniform<sci::NetIO, intType, sci::IKNP<sci::NetIO>>(
            party, bitlength, ioArr[i], otInstanceArr[i], nullptr);
#endif
    if (i & 1) {
      otpackArr[i] = new sci::OTPack<sci::NetIO>(ioArr[i], 3 - party);
    } else {
      otpackArr[i] = new sci::OTPack<sci::NetIO>(ioArr[i], party);
    }
  }

  io = ioArr[0];
  otpack = otpackArr[0];
  iknpOT = new sci::IKNP<sci::NetIO>(io);
  iknpOTRoleReversed = new sci::IKNP<sci::NetIO>(io);
  kkot = new sci::KKOT<sci::NetIO>(io);
  prg128Instance = new sci::PRG128();

#ifdef SCI_OT
  mult = new LinearOT(party, io, otpack);
  truncation = new Truncation(party, io, otpack);
  multUniform = new MatMulUniform<sci::NetIO, intType, sci::IKNP<sci::NetIO>>(
      party, bitlength, io, iknpOT, iknpOTRoleReversed);
  relu = new ReLURingProtocol<sci::NetIO, intType>(party, RING, io, bitlength,
                                                   MILL_PARAM, otpack);
  maxpool = new MaxPoolProtocol<sci::NetIO, intType>(
      party, RING, io, bitlength, MILL_PARAM, 0, otpack, relu);
  argmax = new ArgMaxProtocol<sci::NetIO, intType>(party, RING, io, bitlength,
                                                   MILL_PARAM, 0, otpack, relu);
  math = new MathFunctions(party, io, otpack);
#endif

#if USE_CHEETAH
  backend += "-Cheetah";
  cheetah_linear = new gemini::CheetahLinear(party, io, prime_mod, num_threads);
#elif defined(SCI_HE)
  backend += "-SCI_HE";
  he_conv = new ConvField(party, io);
#elif defined(SCI_OT)
  backend += "-SCI_OT";
#endif

#ifdef SCI_HE
  relu = new ReLUFieldProtocol<sci::NetIO, intType>(
      party, FIELD, io, bitlength, MILL_PARAM, prime_mod, otpack);
  maxpool = new MaxPoolProtocol<sci::NetIO, intType>(
      party, FIELD, io, bitlength, MILL_PARAM, prime_mod, otpack, relu);
  argmax = new ArgMaxProtocol<sci::NetIO, intType>(
      party, FIELD, io, bitlength, MILL_PARAM, prime_mod, otpack, relu);
  he_fc = new FCField(party, io);
  he_prod = new ElemWiseProdField(party, io);
  assertFieldRun();
#endif

#if defined MULTITHREADED_NONLIN && defined SCI_OT
  for (int i = 0; i < num_threads; i++) {
    if (i & 1) {
      reluArr[i] = new ReLURingProtocol<sci::NetIO, intType>(
          3 - party, RING, ioArr[i], bitlength, MILL_PARAM, otpackArr[i]);
      maxpoolArr[i] = new MaxPoolProtocol<sci::NetIO, intType>(
          3 - party, RING, ioArr[i], bitlength, MILL_PARAM, 0, otpackArr[i],
          reluArr[i]);
      multArr[i] = new LinearOT(3 - party, ioArr[i], otpackArr[i]);
      truncationArr[i] = new Truncation(3 - party, ioArr[i], otpackArr[i]);
    } else {
      reluArr[i] = new ReLURingProtocol<sci::NetIO, intType>(
          party, RING, ioArr[i], bitlength, MILL_PARAM, otpackArr[i]);
      maxpoolArr[i] = new MaxPoolProtocol<sci::NetIO, intType>(
          party, RING, ioArr[i], bitlength, MILL_PARAM, 0, otpackArr[i],
          reluArr[i]);
      multArr[i] = new LinearOT(party, ioArr[i], otpackArr[i]);
      truncationArr[i] = new Truncation(party, ioArr[i], otpackArr[i]);
    }
  }
#endif

#ifdef SCI_HE
  for (int i = 0; i < num_threads; i++) {
    if (i & 1) {
      reluArr[i] = new ReLUFieldProtocol<sci::NetIO, intType>(
          3 - party, FIELD, ioArr[i], bitlength, MILL_PARAM, prime_mod,
          otpackArr[i]);
      maxpoolArr[i] = new MaxPoolProtocol<sci::NetIO, intType>(
          3 - party, FIELD, ioArr[i], bitlength, MILL_PARAM, prime_mod,
          otpackArr[i], reluArr[i]);
    } else {
      reluArr[i] = new ReLUFieldProtocol<sci::NetIO, intType>(
          party, FIELD, ioArr[i], bitlength, MILL_PARAM, prime_mod,
          otpackArr[i]);
      maxpoolArr[i] = new MaxPoolProtocol<sci::NetIO, intType>(
          party, FIELD, ioArr[i], bitlength, MILL_PARAM, prime_mod,
          otpackArr[i], reluArr[i]);
    }
  }
#endif

// Math Protocols
#ifdef SCI_OT
  for (int i = 0; i < num_threads; i++) {
    if (i & 1) {
      auxArr[i] = new AuxProtocols(3 - party, ioArr[i], otpackArr[i]);
      truncationArr[i] =
          new Truncation(3 - party, ioArr[i], otpackArr[i], auxArr[i]);
      xtArr[i] = new XTProtocol(3 - party, ioArr[i], otpackArr[i], auxArr[i]);
      mathArr[i] = new MathFunctions(3 - party, ioArr[i], otpackArr[i]);
    } else {
      auxArr[i] = new AuxProtocols(party, ioArr[i], otpackArr[i]);
      truncationArr[i] =
          new Truncation(party, ioArr[i], otpackArr[i], auxArr[i]);
      xtArr[i] = new XTProtocol(party, ioArr[i], otpackArr[i], auxArr[i]);
      mathArr[i] = new MathFunctions(party, ioArr[i], otpackArr[i]);
    }
  }
  aux = auxArr[0];
  truncation = truncationArr[0];
  xt = xtArr[0];
  mult = multArr[0];
  math = mathArr[0];
#endif

  if (party == sci::ALICE) {
    iknpOT->setup_send();
    iknpOTRoleReversed->setup_recv();
  } else if (party == sci::BOB) {
    iknpOT->setup_recv();
    iknpOTRoleReversed->setup_send();
  }

  std::cout << "After one-time setup, communication" << std::endl;
  start_time = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < num_threads; i++) {
    auto temp = ioArr[i]->counter;
    comm_threads[i] = temp;
    std::cout << "Thread i = " << i << ", total data sent till now = " << temp
              << std::endl;
  }
  std::cout << "-----------Syncronizing-----------" << std::endl;
  io->sync();
  num_rounds = io->num_rounds;
  std::cout << "secret_share_mod: " << prime_mod << " bitlength: " << bitlength
            << std::endl;
  std::cout << "backend: " << backend << std::endl;
  std::cout << "-----------Syncronized - now starting execution-----------"
            << std::endl;
}

void EndComputation() {
  auto endTimer = std::chrono::high_resolution_clock::now();
  auto execTimeInMilliSec =
      std::chrono::duration_cast<std::chrono::milliseconds>(endTimer -
                                                            start_time)
          .count();
  uint64_t totalComm = 0;
  for (int i = 0; i < num_threads; i++) {
    auto temp = ioArr[i]->counter;
    std::cout << "Thread i = " << i << ", total data sent till now = " << temp
              << std::endl;
    totalComm += (temp - comm_threads[i]);
  }
  uint64_t totalCommClient;
  std::cout << "------------------------------------------------------\n";
  std::cout << "------------------------------------------------------\n";
  std::cout << "------------------------------------------------------\n";
  std::cout << "Total time taken = " << execTimeInMilliSec
            << " milliseconds.\n";
  std::cout << "Total data sent = " << (totalComm / (1.0 * (1ULL << 20)))
            << " MiB." << std::endl;
  std::cout << "Number of rounds = " << ioArr[0]->num_rounds - num_rounds
            << std::endl;
  std::cout << "Total truncation = " << count_trunc << std::endl;
  std::cout << "Total truncation after relu = " << count_relu_trunc << std::endl;
  if (party == SERVER) {
    io->recv_data(&totalCommClient, sizeof(uint64_t));
    std::cout << "Total comm (sent+received) = "
              << ((totalComm + totalCommClient) / (1.0 * (1ULL << 20)))
              << " MiB." << std::endl;
  } else if (party == CLIENT) {
    io->send_data(&totalComm, sizeof(uint64_t));
    std::cout << "Total comm (sent+received) = (see SERVER OUTPUT)"
              << std::endl;
  }
  std::cout << "------------------------------------------------------\n";

#ifdef LOG_LAYERWISE
  std::cout << "Total time in Conv = " << (ConvTimeInMilliSec / 1000.0)
            << " seconds." << std::endl;
  std::cout << "Total time in MatMul = " << (MatMulTimeInMilliSec / 1000.0)
            << " seconds." << std::endl;
  std::cout << "Total time in BatchNorm = " << (BatchNormInMilliSec / 1000.0)
            << " seconds." << std::endl;
  std::cout << "Total time in Truncation = "
            << (TruncationTimeInMilliSec / 1000.0) << " seconds." << std::endl;
  std::cout << "Total time in Relu = " << (ReluTimeInMilliSec / 1000.0)
            << " seconds." << std::endl;
  std::cout << "Total time in MaxPool = " << (MaxpoolTimeInMilliSec / 1000.0)
            << " seconds." << std::endl;
  std::cout << "Total time in AvgPool = " << (AvgpoolTimeInMilliSec / 1000.0)
            << " seconds." << std::endl;
  std::cout << "Total time in ArgMax = " << (ArgMaxTimeInMilliSec / 1000.0)
            << " seconds." << std::endl;
  std::cout << "Total time in MatAdd = " << (MatAddTimeInMilliSec / 1000.0)
            << " seconds." << std::endl;
  std::cout << "Total time in MatAddBroadCast = "
            << (MatAddBroadCastTimeInMilliSec / 1000.0) << " seconds."
            << std::endl;
  std::cout << "Total time in MulCir = " << (MulCirTimeInMilliSec / 1000.0)
            << " seconds." << std::endl;
  std::cout << "Total time in ScalarMul = "
            << (ScalarMulTimeInMilliSec / 1000.0) << " seconds." << std::endl;
  std::cout << "Total time in Sigmoid = " << (SigmoidTimeInMilliSec / 1000.0)
            << " seconds." << std::endl;
  std::cout << "Total time in Tanh = " << (TanhTimeInMilliSec / 1000.0)
            << " seconds." << std::endl;
  std::cout << "Total time in Sqrt = " << (SqrtTimeInMilliSec / 1000.0)
            << " seconds." << std::endl;
  std::cout << "Total time in NormaliseL2 = "
            << (NormaliseL2TimeInMilliSec / 1000.0) << " seconds." << std::endl;
  std::cout << "------------------------------------------------------\n";
  std::cout << "Conv data sent = " << ((ConvCommSent) / (1.0 * (1ULL << 20)))
            << " MiB." << std::endl;
  std::cout << "MatMul data sent = "
            << ((MatMulCommSent) / (1.0 * (1ULL << 20))) << " MiB."
            << std::endl;
  std::cout << "BatchNorm data sent = "
            << ((BatchNormCommSent) / (1.0 * (1ULL << 20))) << " MiB."
            << std::endl;
  std::cout << "Truncation data sent = "
            << ((TruncationCommSent) / (1.0 * (1ULL << 20))) << " MiB."
            << std::endl;
  std::cout << "Relu data sent = " << ((ReluCommSent) / (1.0 * (1ULL << 20)))
            << " MiB." << std::endl;
  std::cout << "Maxpool data sent = "
            << ((MaxpoolCommSent) / (1.0 * (1ULL << 20))) << " MiB."
            << std::endl;
  std::cout << "Avgpool data sent = "
            << ((AvgpoolCommSent) / (1.0 * (1ULL << 20))) << " MiB."
            << std::endl;
  std::cout << "ArgMax data sent = "
            << ((ArgMaxCommSent) / (1.0 * (1ULL << 20))) << " MiB."
            << std::endl;
  std::cout << "MatAdd data sent = "
            << ((MatAddCommSent) / (1.0 * (1ULL << 20))) << " MiB."
            << std::endl;
  std::cout << "MatAddBroadCast data sent = "
            << ((MatAddBroadCastCommSent) / (1.0 * (1ULL << 20))) << " MiB."
            << std::endl;
  std::cout << "MulCir data sent = "
            << ((MulCirCommSent) / (1.0 * (1ULL << 20))) << " MiB."
            << std::endl;
  std::cout << "Sigmoid data sent = "
            << ((SigmoidCommSent) / (1.0 * (1ULL << 20))) << " MiB."
            << std::endl;
  std::cout << "Tanh data sent = " << ((TanhCommSent) / (1.0 * (1ULL << 20)))
            << " MiB." << std::endl;
  std::cout << "Sqrt data sent = " << ((SqrtCommSent) / (1.0 * (1ULL << 20)))
            << " MiB." << std::endl;
  std::cout << "NormaliseL2 data sent = "
            << ((NormaliseL2CommSent) / (1.0 * (1ULL << 20))) << " MiB."
            << std::endl;
  std::cout << "------------------------------------------------------\n";
#if USE_CHEETAH
    std::cout << "<><><><> test 1geometric perspective faithful truncation <><><><>" << endl;

  int64_t rcot = 0;
  for (size_t i = 0; i < 1; ++i) {
    rcot += otpackArr[i]->silent_ot->get_rcot_count();
  }
  std::cout << "Total #Ferret's RCOT " << rcot << std::endl;
  std::cout << "Total #Elementwise Mul " << CountElementMul << std::endl;
  std::cout << "------------------------------------------------------\n";
#endif
  if (party == SERVER) {
    uint64_t ConvCommSentClient = 0;
    uint64_t MatMulCommSentClient = 0;
    uint64_t BatchNormCommSentClient = 0;
    uint64_t TruncationCommSentClient = 0;
    uint64_t ReluCommSentClient = 0;
    uint64_t MaxpoolCommSentClient = 0;
    uint64_t AvgpoolCommSentClient = 0;
    uint64_t ArgMaxCommSentClient = 0;
    uint64_t MatAddCommSentClient = 0;
    uint64_t MatAddBroadCastCommSentClient = 0;
    uint64_t MulCirCommSentClient = 0;
    uint64_t ScalarMulCommSentClient = 0;
    uint64_t SigmoidCommSentClient = 0;
    uint64_t TanhCommSentClient = 0;
    uint64_t SqrtCommSentClient = 0;
    uint64_t NormaliseL2CommSentClient = 0;

    io->recv_data(&ConvCommSentClient, sizeof(uint64_t));
    io->recv_data(&MatMulCommSentClient, sizeof(uint64_t));
    io->recv_data(&BatchNormCommSentClient, sizeof(uint64_t));
    io->recv_data(&TruncationCommSentClient, sizeof(uint64_t));
    io->recv_data(&ReluCommSentClient, sizeof(uint64_t));
    io->recv_data(&MaxpoolCommSentClient, sizeof(uint64_t));
    io->recv_data(&AvgpoolCommSentClient, sizeof(uint64_t));
    io->recv_data(&ArgMaxCommSentClient, sizeof(uint64_t));
    io->recv_data(&MatAddCommSentClient, sizeof(uint64_t));
    io->recv_data(&MatAddBroadCastCommSentClient, sizeof(uint64_t));
    io->recv_data(&MulCirCommSentClient, sizeof(uint64_t));
    io->recv_data(&ScalarMulCommSentClient, sizeof(uint64_t));
    io->recv_data(&SigmoidCommSentClient, sizeof(uint64_t));
    io->recv_data(&TanhCommSentClient, sizeof(uint64_t));
    io->recv_data(&SqrtCommSentClient, sizeof(uint64_t));
    io->recv_data(&NormaliseL2CommSentClient, sizeof(uint64_t));

    std::cout << "Conv data (sent+received) = "
              << ((ConvCommSent + ConvCommSentClient) / (1.0 * (1ULL << 20)))
              << " MiB." << std::endl;
    std::cout << "MatMul data (sent+received) = "
              << ((MatMulCommSent + MatMulCommSentClient) /
                  (1.0 * (1ULL << 20)))
              << " MiB." << std::endl;
    std::cout << "BatchNorm data (sent+received) = "
              << ((BatchNormCommSent + BatchNormCommSentClient) /
                  (1.0 * (1ULL << 20)))
              << " MiB." << std::endl;
    std::cout << "Truncation data (sent+received) = "
              << ((TruncationCommSent + TruncationCommSentClient) /
                  (1.0 * (1ULL << 20)))
              << " MiB." << std::endl;
    std::cout << "Relu data (sent+received) = "
              << ((ReluCommSent + ReluCommSentClient) / (1.0 * (1ULL << 20)))
              << " MiB." << std::endl;
    std::cout << "Maxpool data (sent+received) = "
              << ((MaxpoolCommSent + MaxpoolCommSentClient) /
                  (1.0 * (1ULL << 20)))
              << " MiB." << std::endl;
    std::cout << "Avgpool data (sent+received) = "
              << ((AvgpoolCommSent + AvgpoolCommSentClient) /
                  (1.0 * (1ULL << 20)))
              << " MiB." << std::endl;
    std::cout << "ArgMax data (sent+received) = "
              << ((ArgMaxCommSent + ArgMaxCommSentClient) /
                  (1.0 * (1ULL << 20)))
              << " MiB." << std::endl;
    std::cout << "MatAdd data (sent+received) = "
              << ((MatAddCommSent + MatAddCommSentClient) /
                  (1.0 * (1ULL << 20)))
              << " MiB." << std::endl;
    std::cout << "MatAddBroadCast data (sent+received) = "
              << ((MatAddBroadCastCommSent + MatAddBroadCastCommSentClient) /
                  (1.0 * (1ULL << 20)))
              << " MiB." << std::endl;
    std::cout << "MulCir data (sent+received) = "
              << ((MulCirCommSent + MulCirCommSentClient) /
                  (1.0 * (1ULL << 20)))
              << " MiB." << std::endl;
    std::cout << "ScalarMul data (sent+received) = "
              << ((ScalarMulCommSent + ScalarMulCommSentClient) /
                  (1.0 * (1ULL << 20)))
              << " MiB." << std::endl;
    std::cout << "Sigmoid data (sent+received) = "
              << ((SigmoidCommSent + SigmoidCommSentClient) /
                  (1.0 * (1ULL << 20)))
              << " MiB." << std::endl;
    std::cout << "Tanh data (sent+received) = "
              << ((TanhCommSent + TanhCommSentClient) / (1.0 * (1ULL << 20)))
              << " MiB." << std::endl;
    std::cout << "Sqrt data (sent+received) = "
              << ((SqrtCommSent + SqrtCommSentClient) / (1.0 * (1ULL << 20)))
              << " MiB." << std::endl;
    std::cout << "NormaliseL2 data (sent+received) = "
              << ((NormaliseL2CommSent + NormaliseL2CommSentClient) /
                  (1.0 * (1ULL << 20)))
              << " MiB." << std::endl;

#ifdef WRITE_LOG
    std::string file_addr = "results-Porthos2PC-server.csv";
    bool write_title = true;
    {
      std::fstream result(file_addr.c_str(), std::fstream::in);
      if (result.is_open())
        write_title = false;
      result.close();
    }
    std::fstream result(file_addr.c_str(),
                        std::fstream::out | std::fstream::app);
    if (write_title) {
      result << "Algebra,Bitlen,Base,#Threads,Total Time,Total Comm,Conv "
                "Time,Conv Comm,MatMul Time,MatMul Comm,BatchNorm "
                "Time,BatchNorm Comm,Truncation Time,Truncation Comm,ReLU "
                "Time,ReLU Comm,MaxPool Time,MaxPool Comm,AvgPool Time,AvgPool "
                "Comm,ArgMax Time,ArgMax Comm"
             << std::endl;
    }
    result << (isNativeRing ? "Ring" : "Field") << "," << bitlength << ","
           << MILL_PARAM << "," << num_threads << ","
           << execTimeInMilliSec / 1000.0 << ","
           << (totalComm + totalCommClient) / (1.0 * (1ULL << 20)) << ","
           << ConvTimeInMilliSec / 1000.0 << ","
           << (ConvCommSent + ConvCommSentClient) / (1.0 * (1ULL << 20)) << ","
           << MatMulTimeInMilliSec / 1000.0 << ","
           << (MatMulCommSent + MatMulCommSentClient) / (1.0 * (1ULL << 20))
           << "," << BatchNormInMilliSec / 1000.0 << ","
           << (BatchNormCommSent + BatchNormCommSentClient) /
                  (1.0 * (1ULL << 20))
           << "," << TruncationTimeInMilliSec / 1000.0 << ","
           << (TruncationCommSent + TruncationCommSentClient) /
                  (1.0 * (1ULL << 20))
           << "," << ReluTimeInMilliSec / 1000.0 << ","
           << (ReluCommSent + ReluCommSentClient) / (1.0 * (1ULL << 20)) << ","
           << MaxpoolTimeInMilliSec / 1000.0 << ","
           << (MaxpoolCommSent + MaxpoolCommSentClient) / (1.0 * (1ULL << 20))
           << "," << AvgpoolTimeInMilliSec / 1000.0 << ","
           << (AvgpoolCommSent + AvgpoolCommSentClient) / (1.0 * (1ULL << 20))
           << "," << ArgMaxTimeInMilliSec / 1000.0 << ","
           << (ArgMaxCommSent + ArgMaxCommSentClient) / (1.0 * (1ULL << 20))
           << std::endl;
    result.close();
#endif
  } else if (party == CLIENT) {
    io->send_data(&ConvCommSent, sizeof(uint64_t));
    io->send_data(&MatMulCommSent, sizeof(uint64_t));
    io->send_data(&BatchNormCommSent, sizeof(uint64_t));
    io->send_data(&TruncationCommSent, sizeof(uint64_t));
    io->send_data(&ReluCommSent, sizeof(uint64_t));
    io->send_data(&MaxpoolCommSent, sizeof(uint64_t));
    io->send_data(&AvgpoolCommSent, sizeof(uint64_t));
    io->send_data(&ArgMaxCommSent, sizeof(uint64_t));
    io->send_data(&MatAddCommSent, sizeof(uint64_t));
    io->send_data(&MatAddBroadCastCommSent, sizeof(uint64_t));
    io->send_data(&MulCirCommSent, sizeof(uint64_t));
    io->send_data(&ScalarMulCommSent, sizeof(uint64_t));
    io->send_data(&SigmoidCommSent, sizeof(uint64_t));
    io->send_data(&TanhCommSent, sizeof(uint64_t));
    io->send_data(&SqrtCommSent, sizeof(uint64_t));
    io->send_data(&NormaliseL2CommSent, sizeof(uint64_t));
  }
#endif
}

intType SecretAdd(intType x, intType y) {
#ifdef SCI_OT
  return (x + y);
#else
  return sci::neg_mod(x + y, (int64_t)prime_mod);
#endif
}

intType SecretSub(intType x, intType y) {
#ifdef SCI_OT
  return (x - y);
#else
  return sci::neg_mod(x - y, (int64_t)prime_mod);
#endif
}

intType SecretMult(intType x, intType y) {
  // assert(false);
  return x * y;
}

void ElemWiseVectorPublicDiv(int32_t s1, intType *arr1, int32_t divisor,
                             intType *outArr) {
  intType *inp;
  intType *out;
  const int alignment = 8;
  size_t aligned_size =
      (s1 + alignment - 1) & -alignment; // rounding up to multiple of alignment

  if ((size_t)s1 != aligned_size) {
    inp = new intType[aligned_size];
    out = new intType[aligned_size];
    memcpy(inp, arr1, s1 * sizeof(intType));
    memset(inp + s1, 0, (aligned_size - s1) * sizeof(intType));
  } else {
    inp = arr1;
    out = outArr;
  }
  assert(divisor > 0 && "No support for division by a negative divisor.");

#ifdef SCI_OT
  funcAvgPoolTwoPowerRingWrapper(aligned_size, inp, out, (intType)divisor);
#else
  funcFieldDivWrapper(aligned_size, inp, out, (intType)divisor, nullptr);
#endif

  if ((size_t)s1 != aligned_size) {
    memcpy(outArr, out, s1 * sizeof(intType));
    delete[] inp;
    delete[] out;
  }

  return;
}

void ElemWiseSecretSharedVectorMult(int32_t size, intType *inArr,
                                    intType *multArrVec, intType *outputArr) {
#ifdef LOG_LAYERWISE
  INIT_ALL_IO_DATA_SENT;
  INIT_TIMER;
#endif
  static int batchNormCtr = 1;
  std::cout << "Starting fused batchNorm #" << batchNormCtr << std::endl;
  batchNormCtr++;

#ifdef SCI_OT
#ifdef MULTITHREADED_DOTPROD
  std::thread dotProdThreads[num_threads];
  int chunk_size = (size / num_threads);
  for (int i = 0; i < num_threads; i++) {
    int offset = i * chunk_size;
    int curSize;
    if (i == (num_threads - 1)) {
      curSize = size - offset;
    } else {
      curSize = chunk_size;
    }
    dotProdThreads[i] = std::thread(funcDotProdThread, i, num_threads, curSize,
                                    multArrVec + offset, inArr + offset,
                                    outputArr + offset, true);
  }
  for (int i = 0; i < num_threads; ++i) {
    dotProdThreads[i].join();
  }
#else
  matmul->hadamard_cross_terms(size, multArrVec, inArr, outputArr, bitlength,
                               bitlength, bitlength, MultMode::None);
#endif

  for (int i = 0; i < size; i++) {
    outputArr[i] += (inArr[i] * multArrVec[i]);
  }
#endif

#ifdef LOG_LAYERWISE
  auto temp = TIMER_TILL_NOW;
  BatchNormInMilliSec += temp;
  uint64_t curComm;
  FIND_ALL_IO_TILL_NOW(curComm);
  BatchNormCommSent += curComm;
#endif

#ifdef VERIFY_LAYERWISE
  if (party == SERVER) {
    funcReconstruct2PCCons(nullptr, inArr, size);
    funcReconstruct2PCCons(nullptr, multArrVec, size);
    funcReconstruct2PCCons(nullptr, outputArr, size);
  } else {
    signedIntType *VinArr = new signedIntType[size];
    funcReconstruct2PCCons(VinArr, inArr, size);
    signedIntType *VmultArr = new signedIntType[size];
    funcReconstruct2PCCons(VmultArr, multArrVec, size);
    signedIntType *VoutputArr = new signedIntType[size];
    funcReconstruct2PCCons(VoutputArr, outputArr, size);

    std::vector<uint64_t> VinVec(size);
    std::vector<uint64_t> VmultVec(size);
    std::vector<uint64_t> VoutputVec(size);

    for (int i = 0; i < size; i++) {
      VinVec[i] = getRingElt(VinArr[i]);
      VmultVec[i] = getRingElt(VmultArr[i]);
    }

    ElemWiseSecretSharedVectorMult_pt(size, VinVec, VmultVec, VoutputVec);

    bool pass = true;
    for (int i = 0; i < size; i++) {
      if (VoutputArr[i] != getSignedVal(VoutputVec[i])) {
        pass = false;
      }
    }
    if (pass == true)
      std::cout << GREEN << "ElemWiseSecretSharedVectorMult Output Matches"
                << RESET << std::endl;
    else
      std::cout << RED << "ElemWiseSecretSharedVectorMult Output Mismatch"
                << RESET << std::endl;

    delete[] VinArr;
    delete[] VmultArr;
    delete[] VoutputArr;
  }
#endif
}

void Floor(int32_t s1, intType *inArr, intType *outArr, int32_t sf) {
  // Not being used in any of our networks right now
  assert(false);
}
