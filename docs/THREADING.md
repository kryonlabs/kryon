# Native raster workers

Native C and C++ source builds use Ziran's `POSIX_THREADS` capability to
parallelize large pixel clears, rounded fills and decoded images by default.
The renderer reuses `std/parallel_linux` workers; `ZIRAN_PAR_THREADS` selects
1–64 lanes, with four by default. Small damaged regions stay on their owner to
avoid worker wake overhead. Portable bundles and other platforms keep the
same pixel algorithms without requiring pthreads.

The frame owner submits paint in order, prepares glyph caches and keeps the
surface, clipping and image assets immutable during each raster operation.
Workers write disjoint rows and complete before the next paint starts.
Decoded images that overlap the output surface retain serial ordering.
This does not make UI trees or application state safe for concurrent mutation.

`PixmapEndInto` releases the borrowed surface after all work has completed.
Call `PixmapCloseWorkers` before unloading the host or its library; workers
otherwise remain idle between frames. `PixmapWorkerCount` reports the actual
number of lanes, including the frame owner. Thread creation failures reduce
the pool to the lanes available and preserve complete rendering.

Run `sh tests/ziran_pixmap_parallel_test.sh` to compare large native rasters at 1,
4 and 8 lanes, including clipping, transparency, rotation, retained damage,
aliased image storage, guard pixels, pool shutdown and restart.
