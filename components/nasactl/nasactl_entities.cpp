// Bridge file: includes subdirectory .cpp files so they get compiled.
// ESPHome only copies top-level component files to the build tree;
// the -I build flag added in __init__.py lets the compiler find these
// in the original source directory.

#include "climate/nasactl_climate.cpp"
