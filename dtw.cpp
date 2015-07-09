//
// Created by rain on 27.5.15.
//

#include <iomanip>
#include "dtw.h"
#include "check.h"

dtw::dtw(unsigned int size, unsigned int corridor){
    this->corrSize = corridor;
    this->corridor = true;

    matrix = new float *[size];
    result = new char *[size];
    for(unsigned int i=0; i<size; i++){
        matrix[i] = new float[size];
        result[i] = new char[size];
    }
    this->size = size;

    for(unsigned int y=0; y<size; y++){
        for(unsigned int x=0; x<size; x++) {
            result[x][y] = ' ';
        }
    }
}

dtw::~dtw(){
    for(unsigned int i=0; i<this->getSize(); i++){
        delete[](matrix[i]);
        delete[](result[i]);
    }
    delete[](matrix);
    delete[](result);
}

Vector * dtw::vectorize(const int * series){
    Vector * vectS = new Vector[size];
    for(unsigned int i=0; i<size-1; i++){
        vectS[i].setPoints(Point(i, series[i]),Point(i+1, series[i+1]));
    }
    return vectS;
}

unsigned int dtw::getSize(){
    return size;
}

void dtw::diffs(const int * sA, const int * sB){
    Vector * vectSA = vectorize(sA);
    Vector * vectSB = vectorize(sB);

    for(unsigned int y=0; y<size; y++){
        for(unsigned int x=0; x<size; x++){
            matrix[x][y] = vectSA[x].diffVector(vectSB[y]);
        }
    }

    delete[](vectSA);
    delete[](vectSB);
}

void dtw::path(){
    unsigned int x=0;
    unsigned int y=0;
    float num=0;

    while(x<size-1 && y<size-1){
        num=matrix[x][y];
        if(matrix[x+1][y+1] < matrix[x+1][y] && matrix[x+1][y+1] < matrix[x][y+1]){
            //diag
            result[x][y]='o';
            x++;
            y++;
        }else if(matrix[x+1][y+1] > matrix[x+1][y] && matrix[x+1][y] < matrix[x][y+1]){
            //po ose x
            result[x][y]='o';
            x++;
        }else{
            //po ose y
            result[x][y]='o';
            y++;
        }
    }
    result[x][y]='o';
}

void dtw::testMatrix(void){
    int p=0;
    for(unsigned int y=0; y<size; y++){
        for(unsigned int x=0; x<size; x++){
            matrix[x][y] = p++;
        }
    }
}

void dtw::printResult(){
    std::cout << std::endl << std::setw(0);
    for(unsigned int x=0; x<size; x++) {
        for (unsigned int y = 0; y < size; y++) {
            std::cout << result[x][y];
        }
        std::cout << std::endl;
    }
}

void dtw::enableCorridor(void){
    corridor = true;
}

void dtw::disableCorridor(void){
    corridor = false;
}

std::ostream& operator<<(std::ostream& aOStream, const dtw &p){
    aOStream << std::setprecision(1) << std::fixed;
    for(unsigned int x=0; x<p.size; x++){
        for(unsigned int y=0; y<p.size; y++){
            aOStream << p.matrix[x][y] << std::setw(5);
            if(p.corridor){
                if(x-y<p.corrSize || y-x<p.corrSize){
                    if(p.result[x][y]=='o'){
                        aOStream << '(';
                        aOStream << std::right << std::setw(5) << p.matrix[x][y] << ')';
                    }else{
                        aOStream << std::right << std::setw(6) << p.matrix[x][y] << ' ';
                    }
                }else{
                    aOStream << std::setw(7) << ' ';
                }
            }else{
                if(p.result[x][y]=='o'){
                    aOStream << '(';
                    aOStream << std::right << std::setw(5) << p.matrix[x][y] << ')';
                }else{
                    aOStream << std::right << std::setw(6) << p.matrix[x][y] << ' ';
                }
            }
        }
        aOStream << std::endl;
        
    }
    return aOStream;
}
