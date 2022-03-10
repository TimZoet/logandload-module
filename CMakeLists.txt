find_package(common REQUIRED)

include(logandloadVersionString)

set(NAME logandload)
set(TYPE module)
set(INCLUDE_DIR "include/logandload")
set(SRC_DIR "src")

set(HEADERS
    ${INCLUDE_DIR}/format/format_state.h
    ${INCLUDE_DIR}/format/formatter.h
    ${INCLUDE_DIR}/format/message_formatter.h
    ${INCLUDE_DIR}/format/parameter_formatter.h

    ${INCLUDE_DIR}/log/category.h
    ${INCLUDE_DIR}/log/format_type.h
    ${INCLUDE_DIR}/log/log.h
    ${INCLUDE_DIR}/log/ordering.h
    ${INCLUDE_DIR}/log/region.h
    ${INCLUDE_DIR}/log/stream.h

    ${INCLUDE_DIR}/utils/lal_error.h
)

set(SOURCES
	${SRC_DIR}/format/format_state.cpp
	${SRC_DIR}/format/formatter.cpp
	${SRC_DIR}/format/message_formatter.cpp

    ${SRC_DIR}/log/format_type.cpp

    ${SRC_DIR}/utils/lal_error.cpp
)

set(DEPS_PUBLIC
	common::common
)

make_target(
    NAME ${NAME}
    TYPE ${TYPE}
    VERSION ${LOGANDLOAD_VERSION}
    WARNINGS WERROR
    HEADERS "${HEADERS}"
    SOURCES "${SOURCES}"
    DEPS_PUBLIC "${DEPS_PUBLIC}"
)

install_target(
    NAME ${NAME}
    TYPE ${TYPE}
    VERSION ${LOGANDLOAD_VERSION}
    HEADERS "${HEADERS}"
)

target_compile_definitions(
    ${NAME}
    PRIVATE
        LOGANDLOAD_VERSION_MAJOR=${LOGANDLOAD_VERSION_MAJOR}
        LOGANDLOAD_VERSION_MINOR=${LOGANDLOAD_VERSION_MINOR}
        LOGANDLOAD_VERSION_PATCH=${LOGANDLOAD_VERSION_PATCH}
)