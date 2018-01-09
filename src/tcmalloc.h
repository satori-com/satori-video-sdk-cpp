#pragma once

#ifdef HAS_GPERFTOOLS
#include <gperftools/heap-profiler.h>
#endif

namespace satori {
namespace video {

#ifdef HAS_GPERFTOOLS
inline void init_tcmalloc() {
  // Touch tcmalloc code to initialize when it is statically linked on Mac.
  // http://www.awenius.de/blog/2014/10/02/the-tcmalloc-heap-profiler-does-not-work-when-tcmalloc-is-statically-linked/
  IsHeapProfilerRunning();
}
#else
inline void init_tcmalloc() { }
#endif

}
}
