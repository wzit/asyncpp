AUX_SOURCE_DIRECTORY(. SOURCE_FILES)

SET(DEPEND_LIBS pthread)
ADD_LIBRARY(asyncpp STATIC ${SOURCE_FILES})
