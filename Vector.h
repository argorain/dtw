//
// Created by rain on 27.5.15.
//

#ifndef __DTW_VECTOR_H__
#define __DTW_VECTOR_H__

#include "Point.h"

class Vector {
private:
    Point a, b;

public:
    Vector();
    Vector(Point a, Point b);
    Vector(Point a, int deg);
    Vector(Point a, float rad);
    Vector(int ax, int ay, int bx, int by);

    const float diffVector(Vector &v);
    void setPoints(Point a, Point b);

};


#endif //DTW_VECTOR_H
