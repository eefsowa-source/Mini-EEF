if (NOT DEFINED PROCESSOR_SOURCE)
    message (FATAL_ERROR "PROCESSOR_SOURCE is required")
endif()

file (READ "${PROCESSOR_SOURCE}" processor_source)

foreach (forbidden_call
         "synth->renderNextBlock ("
         "synth->renderNextBlock("
         "synth->allNotesOff ("
         "synth->allNotesOff(")
    string (FIND "${processor_source}" "${forbidden_call}" call_position)
    if (NOT call_position EQUAL -1)
        message (FATAL_ERROR
            "Realtime contract violation: ${forbidden_call} uses JUCE Synthesiser's internal CriticalSection")
    endif()
endforeach()

foreach (required_call
         "renderNotesOnlyNoLock"
         "resetVoicesNoLock")
    string (FIND "${processor_source}" "${required_call}" call_position)
    if (call_position EQUAL -1)
        message (FATAL_ERROR
            "Realtime contract violation: missing ${required_call}")
    endif()
endforeach()

message (STATUS "Realtime audio contract: no JUCE Synthesiser lock entry points in process path")
