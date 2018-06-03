//
//  colors.h
//  mandelbrot-fractal
//
//  Created by Rafael Eyng on 6/2/18.
//  Copyright © 2018 Rafael Eyng. All rights reserved.
//

#ifndef colors_h
#define colors_h

static int palette[] = {
  4333071,
  1640218,
  590127,
  263241,
  1892,
  797834,
  1594033,
  3767761,
  8828389,
  13888760,
  15854015,
  16304479,
  16755200,
  13402112,
  10049280,
  6960131
};

static void colors_init(int *colors, int length) {
  for (int i= 0; i < length - 1; i++) {
    if (i < 16) {
      colors[i] = palette[i];
    } else {
      colors[i] = 0;
    }
  }
}

#endif /* colors_h */
