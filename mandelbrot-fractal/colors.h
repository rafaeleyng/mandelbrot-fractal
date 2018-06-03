//
//  colors.h
//  mandelbrot-fractal
//
//  Created by Rafael Eyng on 6/2/18.
//  Copyright © 2018 Rafael Eyng. All rights reserved.
//

#ifndef colors_h
#define colors_h

static void colors_init(int *colors, int length) {
  for (int i= 0; i < length - 1; i++) {
    int r = rand();
    int g = rand();
    int b = rand();
    colors[i] = (r << 16) + (g << 8) + b;
  }

  // last color is black
  colors[length] = 0;
}

#endif /* colors_h */
