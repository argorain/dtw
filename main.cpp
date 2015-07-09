#include <iostream>
#include "dtw.h"
#include "record.h"

using namespace std;

int main() {
    cout << "Dynamic Time Warping" << endl << "--------------------" << endl << endl;

    dtw w = dtw(20, 5);

    //w.testMatrix();
    //cout << w << endl;

    int sa[] = {0, 1, 2, 3, 1, 2, 1, 2, 3, 1, 0, 1, 7, 8, 1, 0, 1, 2, 3, 1};
    int sb[] = {0, 0, 0, 2, 1, 3, 1, 4, 1, 2, 0, 0, 0, 2, 1, 3, 1, 0, 1, 4};

    w.diffs(sa,sb);
    cout << "Calculated distances" << endl << w << endl;

    w.path();

    cout << "Found path" << endl << w << endl;

    Record rec = Record();
    cerr << "Return code initRecord: " << rec.initRecord() << endl;
    cerr << "Return code record: " << rec.recordSound() << endl;

    return 0;
}
