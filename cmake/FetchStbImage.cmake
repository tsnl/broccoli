add_library(stb-image STATIC "src/stb/image.cc")
target_include_directories(stb-image PUBLIC "dep/stb")
target_include_directories(stb-image PUBLIC "inc")
