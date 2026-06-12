SET( SRC_PATH ../../../.. )
SET( SOURCES  ${SRC_PATH}/.github/ci/regression/iccDescribeSinkTest.cpp )
SET( TARGET_NAME iccDescribeSinkTest )

ADD_EXECUTABLE( ${TARGET_NAME} ${SOURCES} )
TARGET_LINK_LIBRARIES( ${TARGET_NAME} ${TARGET_LIB_ICCPROFLIB} )
