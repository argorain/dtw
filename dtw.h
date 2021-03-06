//
// Created by rain on 27.5.15.
//

#ifndef __DTW_H__
#define __DTW_H__

#include <iostream>
#include "Vector.h"

class dtw {
private:
    unsigned int size;
    unsigned int corrSize;
    bool corridor;
    float ** matrix;
    char ** result;

    Vector * vectorize(const int * series);
public:
    dtw(unsigned int size, unsigned int corridor);
    ~dtw();

    unsigned int getSize();

    void diffs(const int * sA, const int * sB);
    void path();

    void printResult();
    void testMatrix(void);
    void enableCorridor(void);
    void disableCorridor(void);

    friend std::ostream& operator<<(std::ostream& aOStream, const dtw &p);

};


#endif //__DTW_H__
