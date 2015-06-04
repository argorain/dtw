#include <iostream>
#include "dtw.h"

using namespace std;

int main() {
    cout << "Dynamic Time Warping" << endl;

    dtw w = dtw(20);

    //w.testMatrix();
    //cout << w << endl;

    int sa[] = {0, 1, 2, 3, 1, 2, 1, 2, 3, 1, 0, 1, 7, 8, 1, 0, 1, 2, 3, 1};
    int sb[] = {0, 0, 0, 2, 1, 3, 1, 4, 1, 2, 0, 0, 0, 2, 1, 3, 1, 0, 1, 2};

    w.diffs(sa,sb);
    cout << w << endl;

    w.path();

    w.printResult();

    return 0;
}