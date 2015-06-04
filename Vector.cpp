//
// Created by rain on 27.5.15.
//

#include <cmath>
#include "Vector.h"

Vector::Vector(){
    this->a=Point(0,0);
    this->b=Point(0,0);
}

Vector::Vector(Point a, Point b){
    this->a=a;
    this->b=b;
}

Vector::Vector(Point a, int deg){
    //TODO recount from degrees
}

Vector::Vector(Point a, float rad){
    //TODO recount from radians
}

Vector::Vector(int ax, int ay, int bx, int by){
    this->a = Point(ax,ay);
    this->b = Point(bx,by);
}

const float Vector::diffVector(Vector &v){
    int dx, dy;
    dx=a.getX()-v.a.getX();
    dy=b.getY()-v.b.getY();
    return sqrt(pow(this->b.getX()-v.b.getX()+dx,2)+pow(this->b.getY()-v.b.getY()+dy,2));
}

void Vector::setPoints(Point a, Point b){
    this->a=a;
    this->b=b;
}