# Copyright (C) 2020-2021 Intel Corporation
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.5.1)

project(cpu-features-download NONE)

include(ExternalProject)
ExternalProject_Add(cpu_features
  GIT_REPOSITORY    https://github.com/google/cpu_features.git
  GIT_TAG           32b49eb5e7809052a28422cfde2f2745fbb0eb76 # master branch on Oct 20, 2021
  SOURCE_DIR        "/home/zr/OpenCheetah/deps/hexl/cmake/third-party/cpu-features/cpu-features-src"
  BINARY_DIR        "/home/zr/OpenCheetah/deps/hexl/cmake/third-party/cpu-features/cpu-features-build"
  CONFIGURE_COMMAND ""
  BUILD_COMMAND     ""
  INSTALL_COMMAND   ""
  TEST_COMMAND      ""
  UPDATE_COMMAND    ""
)
