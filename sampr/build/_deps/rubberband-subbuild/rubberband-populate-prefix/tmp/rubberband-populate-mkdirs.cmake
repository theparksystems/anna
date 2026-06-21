# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "C:/anna/sampr/build/_deps/rubberband-src")
  file(MAKE_DIRECTORY "C:/anna/sampr/build/_deps/rubberband-src")
endif()
file(MAKE_DIRECTORY
  "C:/anna/sampr/build/_deps/rubberband-build"
  "C:/anna/sampr/build/_deps/rubberband-subbuild/rubberband-populate-prefix"
  "C:/anna/sampr/build/_deps/rubberband-subbuild/rubberband-populate-prefix/tmp"
  "C:/anna/sampr/build/_deps/rubberband-subbuild/rubberband-populate-prefix/src/rubberband-populate-stamp"
  "C:/anna/sampr/build/_deps/rubberband-subbuild/rubberband-populate-prefix/src"
  "C:/anna/sampr/build/_deps/rubberband-subbuild/rubberband-populate-prefix/src/rubberband-populate-stamp"
)

set(configSubDirs Debug)
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/anna/sampr/build/_deps/rubberband-subbuild/rubberband-populate-prefix/src/rubberband-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "C:/anna/sampr/build/_deps/rubberband-subbuild/rubberband-populate-prefix/src/rubberband-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
