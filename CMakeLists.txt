# Copyright (C) 2007-2009 LuaDist.
# Created by Peter Kapec
# Redistribution and use of this file is allowed according to the terms of the MIT license.
# For details see the COPYRIGHT file distributed with LuaDist.
# Please note that the package source code is licensed under its own license.

PROJECT(luaffi C)
CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
INCLUDE(dist.cmake)

# Find LibFFI
FIND_PATH(FFI_INCLUDE_DIR NAMES ffi.h 
	PATHS /usr/include/x86_64-linux-gnu/		# Ubuntu 9.10 - libffi-dev 3.0.1
)
INCLUDE_DIRECTORIES (${FFI_INCLUDE_DIR})
FIND_LIBRARY(FFI_LIBRARY NAMES ffi)

# Build modules
ADD_LUA_MODULE(luaffi luaffi.c)
TARGET_LINK_LIBRARIES(luaffi ${FFI_LIBRARY})

# Build test lib
ADD_LIBRARY(test SHARED test/test.c )
TARGET_LINK_LIBRARIES(test ${FFI_LIBRARY})
SET_TARGET_PROPERTIES(test PROPERTIES PREFIX "")

# Install all files and documentation
INSTALL(TARGETS luaffi DESTINATION ${INSTALL_CMOD})
INSTALL(FILES LICENSE DESTINATION ${INSTALL_DATA})

INSTALL(FILES test/test.lua DESTINATION ${INSTALL_TEST})
INSTALL(TARGETS test DESTINATION ${INSTALL_TEST})
