target_sources(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/PluginProcessor.cpp
    ${CMAKE_CURRENT_LIST_DIR}/PluginEditor.cpp
    ${CMAKE_CURRENT_LIST_DIR}/DSP/BitCrusher.h
    ${CMAKE_CURRENT_LIST_DIR}/DSP/Decimator.h
    ${CMAKE_CURRENT_LIST_DIR}/DSP/Distortion.h
    ${CMAKE_CURRENT_LIST_DIR}/DSP/NoiseGenerator.h
    ${CMAKE_CURRENT_LIST_DIR}/DSP/OutputStage.h)
