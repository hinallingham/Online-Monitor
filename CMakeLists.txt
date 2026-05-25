# SPDX-FileCopyrightText: 2017-2024 CERN and the Corryvreckan authors
# SPDX-License-Identifier: MIT

# Define module and return the generated name as MODULE_NAME
CORRYVRECKAN_GLOBAL_MODULE(MODULE_NAME)

# Include the RootMacros.cmake file
INCLUDE(${ROOT_USE_FILE})

# Generate the ROOT dictionary
ROOT_GENERATE_DICTIONARY(GuiDisplayDictionary
    ${CMAKE_CURRENT_SOURCE_DIR}/GuiDisplay.hpp
    LINKDEF
    ${CMAKE_CURRENT_SOURCE_DIR}/Linkdef.h
    OPTIONS
    -inlineInputHeader
    -I${CMAKE_CURRENT_SOURCE_DIR}
    MODULE
    GuiDisplay
)
SET_SOURCE_FILES_PROPERTIES(${CMAKE_CURRENT_BINARY_DIR}/GuiDisplayDictionary.cxx
  PROPERTIES GENERATED TRUE COMPILE_FLAGS "-w"
)

# Add source files to library
CORRYVRECKAN_MODULE_SOURCES(${MODULE_NAME}
    OnlineMonitor.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/GuiDisplayDictionary.cxx
)

TARGET_LINK_LIBRARIES(${MODULE_NAME} ROOT::GuiBld)

# Provide standard install target
CORRYVRECKAN_MODULE_INSTALL(${MODULE_NAME})

# Also install the dictionary objects
INSTALL(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/libGuiDisplay_rdict.pcm
    ${CMAKE_CURRENT_BINARY_DIR}/libGuiDisplay.rootmap
    DESTINATION lib)
