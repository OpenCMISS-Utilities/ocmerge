cmake_minimum_required (VERSION 2.8.11)
project(ocmerge)

add_executable( ocmerge src/ocmerge.cpp )
install(TARGETS ocmerge DESTINATION bin)
