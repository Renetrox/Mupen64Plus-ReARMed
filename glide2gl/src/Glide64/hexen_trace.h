#ifndef GLIDE64_HEXEN_TRACE_H
#define GLIDE64_HEXEN_TRACE_H

#include <stdbool.h>

/* Temporary trace for the Hexen comparison with standalone Glide64mk2. */
struct glide64_hexen_trace
{
   bool enabled;
   bool drawing_triangle;
   unsigned vi_count;
   unsigned f3dex_triangles;
   unsigned received;
   unsigned culled;
   unsigned gl_triangles;
};

extern struct glide64_hexen_trace hexen_trace;

#endif
