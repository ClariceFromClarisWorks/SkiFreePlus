#ifndef SKI_CAMERA_H
#define SKI_CAMERA_H
/* SE look-ahead compromise. Build with -DSKI_CAMERA_DIVISOR=3 for
   the original anchor in matched comparisons. No per-frame smoothing. */
#ifndef SKI_CAMERA_DIVISOR
#define SKI_CAMERA_DIVISOR 4
#endif
#define SKI_CAMERA_Y(height) ((height) / SKI_CAMERA_DIVISOR)
#endif
