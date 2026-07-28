#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#  *  http://www.apache.org/licenses/LICENSE-2.0
#  * Unless required by applicable law or agreed to in writing,
#  software distributed under the License is distributed on an
#  "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
#  KIND, either express or implied.  See the License for the
#  specific language governing permissions and limitations
#  under the License.
#

# ------------------------------------------------------------------
# 1. 工具链配置
# ------------------------------------------------------------------
CROSS_COMPILE ?= /home/xie/workspace/codes/Smart_light/toolchain/rk3588/aarch64-linux/bin/aarch64-cortexa53-linux-gnu-
CC      := $(CROSS_COMPILE)gcc
CXX     := $(CROSS_COMPILE)g++
LD      := $(CROSS_COMPILE)gcc
AR      := $(CROSS_COMPILE)ar
AS      := $(CROSS_COMPILE)as
NM      := $(CROSS_COMPILE)nm
OBJDUMP := $(CROSS_COMPILE)objdump
OBJCOPY := $(CROSS_COMPILE)objcopy
SIZE    := $(CROSS_COMPILE)size

# ------------------------------------------------------------------
# 2. 核心路径配置（统一输出目录）
# ------------------------------------------------------------------
NIMBLE_ROOT := ../../..
OUTPUT_ROOT := $(NIMBLE_ROOT)/output
LIB_DIR     := $(OUTPUT_ROOT)/lib
INCLUDE_DIR := $(OUTPUT_ROOT)/include
OBJ_DIR     := $(OUTPUT_ROOT)/obj

# Mbed TLS 源码及构建路径（使用绝对路径）
MBEDTLS_SRC_DIR  := $(abspath $(NIMBLE_ROOT)/ext/mbedtls)
MBEDTLS_BUILD_DIR := $(OUTPUT_ROOT)/mbedtls_build
MBEDTLS_LIB_DIR   := $(abspath $(OUTPUT_ROOT)/mbedtls_lib)

# Mbed TLS 静态库目标 + stamp 防止 -j 竞态
#   (多目标规则在 make -j 下会为每个 .a 独立触发 recipe，并行踩踏)
MBEDTLS_LIBS := \
    $(MBEDTLS_LIB_DIR)/lib/libmbedcrypto.a \
    $(MBEDTLS_LIB_DIR)/lib/libmbedx509.a   \
    $(MBEDTLS_LIB_DIR)/lib/libmbedtls.a
MBEDTLS_DONE_STAMP := $(MBEDTLS_LIB_DIR)/.mbedtls_built

NULL :=

# ------------------------------------------------------------------
# 3. NimBLE 核心配置（启用 Mbed TLS，禁用 TinyCrypt）
# ------------------------------------------------------------------
NIMBLE_CFG_MBEDTLS := 1

# ------------------------------------------------------------------
# 4. 跳过不兼容文件
# ------------------------------------------------------------------
NIMBLE_IGNORE := $(NIMBLE_ROOT)/porting/nimble/src/hal_timer.c \
                 $(NIMBLE_ROOT)/porting/nimble/src/os_cputime.c \
                 $(NIMBLE_ROOT)/porting/nimble/src/os_cputime_pwr2.c \
                 $(NULL)

# ------------------------------------------------------------------
# 5. 引入 NimBLE 基础编译规则
# ------------------------------------------------------------------
include $(NIMBLE_ROOT)/porting/nimble/Makefile.defs

# ------------------------------------------------------------------
# 6. 库源码配置（移除 TinyCrypt）
# ------------------------------------------------------------------
SRC := $(NIMBLE_SRC)
SRC += \
    $(wildcard $(NIMBLE_ROOT)/porting/npl/linux/src/*.c) \
    $(wildcard $(NIMBLE_ROOT)/porting/npl/linux/src/*.cc) \
    $(wildcard $(NIMBLE_ROOT)/nimble/transport/socket/src/*.c) \
    $(NULL)

# ------------------------------------------------------------------
# 7. 头文件路径（统一使用输出目录）
# ------------------------------------------------------------------
INC = \
    ./include \
    $(NIMBLE_ROOT)/porting/npl/linux/include \
    $(NIMBLE_ROOT)/nimble/transport/socket/include \
    $(NIMBLE_INCLUDE) \
    $(INCLUDE_DIR)
INCLUDES := $(addprefix -I, $(INC))

# ------------------------------------------------------------------
# 8. 源码分类与目标文件（全部 .o 输出到 output/obj/ 内）
# ------------------------------------------------------------------
SRC_C  = $(filter %.c,  $(SRC))
SRC_CC = $(filter %.cc, $(SRC))
# 去除 NIMBLE_ROOT 前缀，映射到 OBJ_DIR： ../../../nimble/.../x.c → output/obj/nimble/.../x.o
OBJ_C  := $(patsubst $(NIMBLE_ROOT)/%.c,  $(OBJ_DIR)/%.o, $(SRC_C))
OBJ_CC := $(patsubst $(NIMBLE_ROOT)/%.cc, $(OBJ_DIR)/%.o, $(SRC_CC))
OBJ    := $(OBJ_C) $(OBJ_CC)

# ------------------------------------------------------------------
# 9. 编译选项
# ------------------------------------------------------------------
CFLAGS =                    \
    $(NIMBLE_CFLAGS)        \
    $(INCLUDES)             \
    -g                      \
    -D_GNU_SOURCE           \
    $(NULL)

# ------------------------------------------------------------------
# 10. 静态库目标
# ------------------------------------------------------------------
LIB_TARGET := $(LIB_DIR)/libnimble.a

# ------------------------------------------------------------------
# 11. 伪目标
# ------------------------------------------------------------------
.PHONY: all clean prepare install_headers mbedtls
.DEFAULT: all

# ------------------------------------------------------------------
# 12. 编译入口（先拷贝头文件，再编译 NimBLE）
# ------------------------------------------------------------------
all: prepare $(MBEDTLS_DONE_STAMP) install_headers $(LIB_TARGET)
	@echo "=== 所有构建完成 ==="
	@echo "静态库路径: $(LIB_TARGET)"
	@echo "头文件路径: $(INCLUDE_DIR)"
	@echo "Mbed TLS 库路径: $(MBEDTLS_LIB_DIR)/lib"

# ------------------------------------------------------------------
# 13. 准备目标（创建所有输出目录）
# ------------------------------------------------------------------
prepare:
	@mkdir -p $(LIB_DIR)
	@mkdir -p $(INCLUDE_DIR)
	@mkdir -p $(OBJ_DIR)
	@mkdir -p $(MBEDTLS_LIB_DIR)
	@echo "已创建输出目录: $(OUTPUT_ROOT)"

# ------------------------------------------------------------------
# 14. Mbed TLS 构建规则（stamp 文件防止 make -j 竞态）
# ------------------------------------------------------------------
MBEDTLS_CONFIGURE_MARKER := $(MBEDTLS_BUILD_DIR)/.cmake_done

$(MBEDTLS_CONFIGURE_MARKER): | prepare
	@echo "=== 配置 Mbed TLS (RK3588) ==="
	cd $(MBEDTLS_SRC_DIR) && git submodule update --init
	mkdir -p $(MBEDTLS_BUILD_DIR)
	cd $(MBEDTLS_BUILD_DIR) && \
	cmake $(MBEDTLS_SRC_DIR) \
	    -DCMAKE_C_COMPILER=$(CC) \
	    -DCMAKE_AR=$(AR) \
	    -DCMAKE_C_FLAGS="-march=armv8-a -fPIC" \
	    -DUSE_SHARED_MBEDTLS_LIBRARY=OFF \
	    -DENABLE_TESTING=OFF \
	    -DENABLE_PROGRAMS=OFF \
	    -DCMAKE_INSTALL_PREFIX=$(MBEDTLS_LIB_DIR)
	@touch $@

$(MBEDTLS_DONE_STAMP): $(MBEDTLS_CONFIGURE_MARKER)
	@echo "=== 构建 Mbed TLS (RK3588) ==="
	cd $(MBEDTLS_BUILD_DIR) && $(MAKE)
	cd $(MBEDTLS_BUILD_DIR) && $(MAKE) install
	@touch $@
	@echo "=== Mbed TLS 构建完成 ==="

$(MBEDTLS_LIBS): $(MBEDTLS_DONE_STAMP)
	@true

mbedtls: $(MBEDTLS_DONE_STAMP)

# ------------------------------------------------------------------
# 15. 清理规则（删除整个输出目录）
# ------------------------------------------------------------------
clean:
	rm -rf $(OUTPUT_ROOT)
	rm -rf $(MBEDTLS_BUILD_DIR)
	rm -f $(MBEDTLS_DONE_STAMP)
	@echo "=== 清理完成 ==="

# ------------------------------------------------------------------
# 16. C 文件编译规则（全部 .o 输出到 output/obj/ 内）
# ------------------------------------------------------------------
$(OBJ_DIR)/%.o: $(NIMBLE_ROOT)/%.c
	@mkdir -p $(dir $@)
	@echo "CC      $<"
	@$(CC) -c $(CFLAGS) -o $@ $<

# ------------------------------------------------------------------
# 17. C++ 文件编译规则
# ------------------------------------------------------------------
$(OBJ_DIR)/%.o: $(NIMBLE_ROOT)/%.cc
	@mkdir -p $(dir $@)
	@echo "CXX     $<"
	@$(CXX) -c $(CFLAGS) -o $@ $<

# ------------------------------------------------------------------
# 18. 静态库打包规则
# ------------------------------------------------------------------
$(LIB_TARGET): $(OBJ)
	@echo "AR      $@"
	@$(AR) rcs $@ $^
	@$(SIZE) $@
	@echo "静态库生成完成: $@"

# ------------------------------------------------------------------
# 19. 头文件拷贝规则（统一拷贝所有依赖头文件到 INCLUDE_DIR）
# ------------------------------------------------------------------
install_headers:
	@cp -r $(NIMBLE_ROOT)/nimble/include/* $(INCLUDE_DIR)/
	@cp -r $(NIMBLE_ROOT)/nimble/host/include/* $(INCLUDE_DIR)/
	@cp -r $(NIMBLE_ROOT)/nimble/host/services/*/include/* $(INCLUDE_DIR)/
	@cp -r $(NIMBLE_ROOT)/nimble/host/store/config/include/* $(INCLUDE_DIR)/
	@cp -r $(NIMBLE_ROOT)/nimble/host/util/include/* $(INCLUDE_DIR)/
	@cp -r $(NIMBLE_ROOT)/nimble/transport/include/* $(INCLUDE_DIR)/
	@cp -r $(NIMBLE_ROOT)/porting/npl/linux/include/* $(INCLUDE_DIR)/
	@cp -r $(NIMBLE_ROOT)/nimble/transport/socket/include/* $(INCLUDE_DIR)/
	@cp -r $(NIMBLE_ROOT)/porting/nimble/include/* $(INCLUDE_DIR)/
	@cp -r $(MBEDTLS_SRC_DIR)/include/* $(INCLUDE_DIR)/
	@cp -r ./include/* $(INCLUDE_DIR)/ 2>/dev/null || :
	@echo "头文件拷贝完成: $(INCLUDE_DIR)"