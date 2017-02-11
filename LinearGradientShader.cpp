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


class LinearGradientShader: public GShader{
public:
    const GPoint p0;
    const GPoint p1;
    const GColor c0;
    const GColor c1;
    const GShader::TileMode tile_mode;
    float dc_a;
    float dc_r;
    float dc_g;
    float dc_b;
    GMatrix local_matrix;
    GMatrix final_matrix;
    float shader_alpha;

    LinearGradientShader(const GPoint& new_p0, const GPoint& new_p1
    ,const GColor& new_c0, const GColor& new_c1,GShader::TileMode tile_mode)
    :p0(new_p0),p1(new_p1),c0(new_c0),c1(new_c1),tile_mode(tile_mode){
        //calculate the local_matrix
        float dx = p1.x() - p0.x();
        float dy = p1.y() - p0.y();
        local_matrix = GMatrix(dx,-dy,p0.x(),dy,dx,p0.y());
    }

    bool setContext(const GMatrix& new_ctm_matrix, float new_alpha){
        shader_alpha = new_alpha;
        GMatrix tmp_matrix;
        tmp_matrix.setConcat(new_ctm_matrix,local_matrix);
        return tmp_matrix.invert(&final_matrix);
    }

    void shadeRow(int x, int y, int count, GPixel row[]){
        if (tile_mode == GShader::TileMode::kClamp){
            shadeRow_clamp(x,y,count,row);
        }
        else if (tile_mode == GShader::TileMode::kRepeat){
            shadeRow_repeat(x,y,count,row);
        }
        else{
            shadeRow_mirror(x,y,count,row);
        }
    }

    void shadeRow_clamp(int x, int y, int count, GPixel row[]){
        GPoint start_loc = final_matrix.mapXY(x+0.5,y+0.5);
        float w = start_loc.x();
        dc_a = (c1.fA - c0.fA)*final_matrix[GMatrix::SX];
        dc_r = (c1.fR - c0.fR)*final_matrix[GMatrix::SX];
        dc_g = (c1.fG - c0.fG)*final_matrix[GMatrix::SX];
        dc_b = (c1.fB - c0.fB)*final_matrix[GMatrix::SX];
        float a = w * c1.fA + (1-w)*c0.fA;
        float r = w * c1.fR + (1-w)*c0.fR;
        float g = w * c1.fG + (1-w)*c0.fG;
        float b = w * c1.fB + (1-w)*c0.fB;
        if(w<0){
            row[0] = blend_with_alpha(c0.fA,c0.fR,c0.fG,c0.fB);
            w += final_matrix[GMatrix::SX];
            }
        else if(w>1){
            row[0] = blend_with_alpha(c1.fA,c1.fR,c1.fG,c1.fB);
            w += final_matrix[GMatrix::SX];
        }
        else{
            row[0] = blend_with_alpha(a,r,g,b);
        }
        for (int i = 1; i < count; ++i){
            if(w<0){
                row[i] = blend_with_alpha(c0.fA,c0.fR,c0.fG,c0.fB);
                w += final_matrix[GMatrix::SX];
            }
            else if(w>1){
                row[i] = blend_with_alpha(c1.fA,c1.fR,c1.fG,c1.fB);
                w += final_matrix[GMatrix::SX];
            }
            else{
                a += dc_a;
                r += dc_r;
                g += dc_g;
                b += dc_b;
                row[i] = blend_with_alpha(a,r,g,b);
                w += final_matrix[GMatrix::SX];
            }
        }
    }

    void shadeRow_repeat(int x, int y, int count, GPixel row[]){
        GPoint start_loc = final_matrix.mapXY(x+0.5,y+0.5);
        float w = start_loc.x();
        for(int i = 0; i<count;i++){
            w = w - floor(w);
            float a_new = w*c1.fA + (1-w)*c0.fA;
            float r_new = w*c1.fR + (1-w)*c0.fR;
            float g_new = w*c1.fG + (1-w)*c0.fG;
            float b_new = w*c1.fB + (1-w)*c0.fB;
            row[i] = blend_with_alpha(a_new,r_new,g_new,b_new);
            w += final_matrix[GMatrix::SX];
        }
    }

     void shadeRow_mirror(int x, int y, int count, GPixel row[]){
        GPoint start_loc = final_matrix.mapXY(x+0.5,y+0.5);
        float w = start_loc.x();
        for(int i = 0; i<count;i++){
            float curr_w = w*0.5;
            curr_w -= floorf(curr_w);
            curr_w *=2;
            if(curr_w>=1){
                curr_w = 2-curr_w;
            }
            float a_new = curr_w*c1.fA + (1-curr_w)*c0.fA;
            float r_new = curr_w*c1.fR + (1-curr_w)*c0.fR;
            float g_new = curr_w*c1.fG + (1-curr_w)*c0.fG;
            float b_new = curr_w*c1.fB + (1-curr_w)*c0.fB;
            row[i] = blend_with_alpha(a_new,r_new,g_new,b_new);
            w += final_matrix[GMatrix::SX];
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

GShader* GShader::LinearGradient(const GPoint& p0, const GPoint& p1,const GColor& c0, const GColor& c1
, TileMode mode){
    return new LinearGradientShader(p0,p1,c0,c1,mode);
}