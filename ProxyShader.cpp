#include <math.h>
#include "GPixel.h"
#include "GBitmap.h"
#include "GCanvas.h"
#include "GColor.h"
#include "GRect.h"
#include "GPoint.h"
#include "GMatrix.h"
#include "GShader.h"
#include <stdio.h>
#include <cstdint>


class ProxyShader: public GShader{
    public:
    GShader* fshader;
    const GMatrix fmatrix;
    ProxyShader(GShader* new_fshader, GMatrix& new_fmatrix)
    : fmatrix(new_fmatrix){
        fshader = new_fshader;
    }

    bool setContext(const GMatrix& new_ctm,float new_alpha){
        GMatrix tmp_matrix;
        tmp_matrix.setConcat(new_ctm,fmatrix);
        return fshader->setContext(tmp_matrix, new_alpha);
    }

    void shadeRow(int x, int y, int count, GPixel row[]){
        fshader->shadeRow(x,y,count,row);
    }
};