//
// Created by rain on 27.5.15.
//

#ifndef __DTW_POINT_H__
#define __DTW_POINT_H__


class Point {
private:
    int x, y;

public:
    Point(int x, int y);
    Point();

    void setX(const int x);
    void setY(const int y);
    int getX();
    int getY();
};


#endif //DTW_POINT_H
