//
// Created by rain on 27.5.15.
//

#include "Point.h"

Point::Point() {
    x=y=0;
}

Point::Point(int x, int y) {
    this->x=x;
    this->y=y;
}

int Point::getX() { return x; }
int Point::getY() { return y; }
void Point::setX(const int x) { this->x=x; }
void Point::setY(const int y) { this->y=y; }