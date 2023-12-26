add_library(stb-image STATIC "src/stb/image.cc" "src/stb/image_write.cc")
target_include_directories(stb-image PUBLIC "dep/stb")
target_include_directories(stb-image PUBLIC "inc")
