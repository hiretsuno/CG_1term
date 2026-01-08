# CMakeList.txt: проект CMake для Laba_3; включите исходный код и определения,
# укажите здесь логику для конкретного проекта.
#

# Добавьте источник в исполняемый файл этого проекта.
add_executable (Laba_3 "Laba_3.cpp" "Laba_3.h")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET Laba_3 PROPERTY CXX_STANDARD 20)
endif()

# TODO: Добавьте тесты и целевые объекты, если это необходимо.
