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


class TriColorShader: public GShader{
    public:
    const GColor c0, c1, c2;
    const GPoint p0, p1, p2;
    float del_ua, del_ur, del_ug, del_ub,
    del_va, del_vr, del_vg, del_vb;
    GMatrix local_matrix;
    GMatrix final_matrix;
    float shader_alpha;

    TriColorShader(const GPoint& new_p0, const GPoint& new_p1, const GPoint& new_p2,
    const GColor& new_c0, const GColor& new_c1, const GColor& new_c2)
    :c0(new_c0),c1(new_c1),c2(new_c2),p0(new_p0),p1(new_p1),p2(new_p2){
        float del_p1x = p1.x()-p0.x();
        float del_p1y = p1.y()-p0.y();
        float del_p2x = p2.x()-p0.x();
        float del_p2y = p2.y()-p0.y();
        del_ua = c1.fA - c0.fA;
        del_ur = c1.fR - c0.fR;
        del_ug = c1.fG - c0.fG;
        del_ub = c1.fB - c0.fB;
        del_va = c2.fA - c0.fA;
        del_vr = c2.fR - c0.fR;
        del_vg = c2.fG - c0.fG;
        del_vb = c2.fB - c0.fB;
        local_matrix =  GMatrix(del_p1x, del_p2x, p0.x(), del_p1y, del_p2y, p0.y());
    }

    bool setContext(const GMatrix& new_ctm, float new_alpha){
        shader_alpha = new_alpha;
        GMatrix tmp_matrix;
        tmp_matrix.setConcat(new_ctm, local_matrix);
        return tmp_matrix.invert(&final_matrix);
    }

    void shadeRow(int x, int y, int count, GPixel row[]){
        GPoint start_loc = final_matrix.mapXY(x+0.5, y+0.5);
        float u = start_loc.x();
        float v = start_loc.y();
        float a = u*del_ua + v*del_va + c0.fA;
        float r = u*del_ur + v*del_vr + c0.fR;
        float g = u*del_ug + v*del_vg + c0.fG;
        float b = u*del_ub + v*del_vb + c0.fB;
        row[0] = blend_with_alpha(a,r,g,b);
        for (int i = 1; i< count; ++i){
            u+= final_matrix[GMatrix::SX];
            v+= final_matrix[GMatrix::KY];
            a = u*del_ua + v*del_va + c0.fA;
            r = u*del_ur + v*del_vr + c0.fR;
            g = u*del_ug + v*del_vg + c0.fG;
            b = u*del_ub + v*del_vb + c0.fB;
            row[i] = blend_with_alpha(a,r,g,b);
        }
    }

    GPixel blend_with_alpha(float a0,float r0,float g0,float b0){
        //need to make sure the color has been pin to unit
        int new_a = (uint8_t) (GPinToUnit(a0 * shader_alpha)*255);
        int new_r = (uint8_t) (GPinToUnit(r0*a0 * shader_alpha)*255);
        int new_g = (uint8_t) (GPinToUnit(g0 *a0 *shader_alpha)*255);
        int new_b = (uint8_t) (GPinToUnit(b0 *a0* shader_alpha)*255);
        GPixel the_pixel = GPixel_PackARGB(new_a, new_r, new_g, new_b);
        return the_pixel;
    }
};