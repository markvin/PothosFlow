########################################################################
# Smoke test executable (not installed): the flow sources and the main
# window without a display
########################################################################
add_executable(PothosFlowSmokeTest FlowSmokeTest.cpp ${FLOW_RESOURCES_QRC})
set_target_properties(PothosFlowSmokeTest PROPERTIES AUTORCC ON)
target_link_libraries(PothosFlowSmokeTest PRIVATE PothosFlowApp)
flow_target(PothosFlowSmokeTest)
