if (NOT DEFINED INPUT_ARCHIVE OR NOT EXISTS "${INPUT_ARCHIVE}")
  message(FATAL_ERROR "INPUT_ARCHIVE is missing or does not exist: ${INPUT_ARCHIVE}")
endif()
get_filename_component(INPUT_ARCHIVE "${INPUT_ARCHIVE}" ABSOLUTE)

if (NOT DEFINED OUTPUT_ARCHIVE)
  message(FATAL_ERROR "OUTPUT_ARCHIVE is required")
endif()
get_filename_component(OUTPUT_ARCHIVE "${OUTPUT_ARCHIVE}" ABSOLUTE)

if (NOT DEFINED CMAKE_AR OR CMAKE_AR STREQUAL "")
  message(FATAL_ERROR "CMAKE_AR is required")
endif()

get_filename_component(_output_dir "${OUTPUT_ARCHIVE}" DIRECTORY)
file(MAKE_DIRECTORY "${_output_dir}")

set(_work_dir "${_output_dir}/fmodstudio_stub_work")
file(REMOVE_RECURSE "${_work_dir}")
file(MAKE_DIRECTORY "${_work_dir}")

execute_process(
  COMMAND "${CMAKE_AR}" t "${INPUT_ARCHIVE}"
  OUTPUT_VARIABLE _members_raw
  RESULT_VARIABLE _members_result
)
if (NOT _members_result EQUAL 0)
  message(FATAL_ERROR "Failed to list members in ${INPUT_ARCHIVE}")
endif()

string(REPLACE "\r" "" _members_raw "${_members_raw}")
string(REGEX REPLACE "\n$" "" _members_raw "${_members_raw}")
string(REPLACE "\n" ";" _members "${_members_raw}")

set(_filtered_members "")
foreach(_member IN LISTS _members)
  if (NOT _member MATCHES "_head\\.o$")
    list(APPEND _filtered_members "${_member}")
  endif()
endforeach()

if (NOT _filtered_members)
  message(FATAL_ERROR "No members left after filtering ${INPUT_ARCHIVE}")
endif()

execute_process(
  COMMAND "${CMAKE_AR}" x "${INPUT_ARCHIVE}"
  WORKING_DIRECTORY "${_work_dir}"
  RESULT_VARIABLE _extract_result
)
if (NOT _extract_result EQUAL 0)
  message(FATAL_ERROR "Failed extracting members from ${INPUT_ARCHIVE}")
endif()

file(REMOVE "${OUTPUT_ARCHIVE}")
execute_process(
  COMMAND "${CMAKE_AR}" qc "${OUTPUT_ARCHIVE}" ${_filtered_members}
  WORKING_DIRECTORY "${_work_dir}"
  RESULT_VARIABLE _create_result
)
if (NOT _create_result EQUAL 0)
  message(FATAL_ERROR "Failed creating sanitized archive ${OUTPUT_ARCHIVE}")
endif()

if (DEFINED CMAKE_RANLIB AND NOT CMAKE_RANLIB STREQUAL "")
  execute_process(
    COMMAND "${CMAKE_RANLIB}" "${OUTPUT_ARCHIVE}"
    RESULT_VARIABLE _ranlib_result
  )
  if (NOT _ranlib_result EQUAL 0)
    message(FATAL_ERROR "ranlib failed for ${OUTPUT_ARCHIVE}")
  endif()
endif()

message(STATUS "Sanitized FMOD Studio stub: ${OUTPUT_ARCHIVE}")
