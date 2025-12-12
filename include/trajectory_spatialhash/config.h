#ifndef TRAJECTORY_SPATIALHASH_CONFIG_H
#define TRAJECTORY_SPATIALHASH_CONFIG_H

// Version information
#define TSH_VERSION_MAJOR 0
#define TSH_VERSION_MINOR 1
#define TSH_VERSION_PATCH 0

// Export macros for shared library builds
#ifdef _WIN32
  #ifdef TSH_BUILDING_LIBRARY
    #define TSH_API __declspec(dllexport)
  #elif defined(TSH_USING_SHARED)
    #define TSH_API __declspec(dllimport)
  #else
    #define TSH_API
  #endif
#else
  #ifdef TSH_BUILDING_LIBRARY
    #define TSH_API __attribute__((visibility("default")))
  #else
    #define TSH_API
  #endif
#endif

#endif // TRAJECTORY_SPATIALHASH_CONFIG_H
