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

class BitmapShader : public GShader{
public:
    const GMatrix internal_matrix;
    GMatrix local_matrix;
    GMatrix shader_m;
    GMatrix final_matrix;
    GMatrix invert_shader_m;
    GMatrix tmp_matrix;
    float shader_alpha;
    const GBitmap shader_bitmap;
    const GShader::TileMode tile_mode;
    const int src_width;
    const int src_height;

    BitmapShader(const GBitmap& new_bitmap, const GMatrix& internalMatrix, GShader::TileMode new_tilemode)
    :internal_matrix(internalMatrix),shader_bitmap(new_bitmap),tile_mode(new_tilemode)
    ,src_width(new_bitmap.width()),src_height(new_bitmap.height()){
        local_matrix = GMatrix(src_width,0,0,0,src_height,0);
    }

    bool setContext(const GMatrix& new_ctm_matrix, float alpha){
        shader_alpha = alpha;
        shader_m.setConcat(new_ctm_matrix,internal_matrix);
        tmp_matrix.setConcat(shader_m,local_matrix);
        shader_m.invert(&invert_shader_m);
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
        //did not transform into the 0,1 interval
        GPoint loc = invert_shader_m.mapXY(x+0.5,y+0.5);
        float dx = invert_shader_m[GMatrix::SX];
        float dy = invert_shader_m[GMatrix::KY];
        for(int i = 0; i< count; ++i){
            int int_curr_x = (int)(loc.fX);
            int int_curr_y = (int)(loc.fY);
            int_curr_x =  fmin(shader_bitmap.width()-1,fmax(0,int_curr_x));
            int_curr_y =  fmin(shader_bitmap.height()-1,fmax(0,int_curr_y));
            GPixel src_pix =  *shader_bitmap.getAddr(int_curr_x, int_curr_y);
            GPixel blended_src_pix = blend_with_alpha(src_pix);
            row[i] = blended_src_pix;
            loc.fX += dx;
            loc.fY += dy;
        }
    }

    void shadeRow_repeat(int x, int y, int count, GPixel row[]){
        GPoint loc = final_matrix.mapXY(x+0.5,y+0.5);
        float w_x = loc.x();
        float w_y = loc.y();
        float dx = final_matrix[GMatrix::SX];
        float dy = final_matrix[GMatrix::KY];
        for(int i = 0; i<count ; i++){
            w_x = w_x - floor(w_x);
            w_y = w_y - floor(w_y);
            int int_curr_x = (int) (w_x * src_width);
            int int_curr_y = (int) (w_y * src_height);
            int_curr_x =  fmin(shader_bitmap.width()-1, fmax(0,int_curr_x));
            int_curr_y =  fmin(shader_bitmap.height()-1, fmax(0,int_curr_y));
            GPixel src_pix = *shader_bitmap.getAddr(int_curr_x, int_curr_y);
            GPixel blended_src_pix = blend_with_alpha(src_pix);
            row[i] = blended_src_pix;
            w_x += dx;
            w_y += dy;
        }     
    }

    void shadeRow_mirror(int x, int y, int count, GPixel row[]){
        GPoint loc = final_matrix.mapXY(x+0.5,y+0.5);
        float w_x = loc.x();
        float w_y = loc.y();
        float dx = final_matrix[GMatrix::SX];
        float dy = final_matrix[GMatrix::KY];
        float w_x_curr;
        float w_y_curr;
        for(int i = 0; i<count ; i++){
            w_x_curr = w_x;
            w_x_curr *= 0.5;
            w_x_curr -= floorf(w_x_curr);
            w_x_curr *= 2.0;
            if(w_x_curr>=1){
                w_x_curr = 2.0-w_x_curr;
            }
            w_y_curr = w_y;
            w_y_curr *= 0.5;
            w_y_curr -= floorf(w_y_curr);
            w_y_curr *= 2.0;
            if(w_y_curr>=1){
                w_y_curr = 2.0-w_y_curr;
            }
            int int_curr_x = (int) (w_x_curr * src_width);
            int int_curr_y = (int) (w_y_curr * src_height);
            int_curr_x =  fmin(shader_bitmap.width()-1, fmax(0,int_curr_x));
            int_curr_y =  fmin(shader_bitmap.height()-1, fmax(0,int_curr_y));
            GPixel src_pix = *shader_bitmap.getAddr(int_curr_x, int_curr_y);
            GPixel blended_src_pix = blend_with_alpha(src_pix);
            row[i] = blended_src_pix;
            w_x += dx;
            w_y += dy;
        }     
    }
    
    GPixel blend_with_alpha(GPixel& pix){
        int new_a = (uint8_t) (GPixel_GetA(pix)*shader_alpha);
        int new_r = (uint8_t) (GPixel_GetR(pix)*shader_alpha);
        int new_g = (uint8_t) (GPixel_GetG(pix)*shader_alpha);
        int new_b = (uint8_t) (GPixel_GetB(pix)*shader_alpha);
        GPixel the_pixel = GPixel_PackARGB(new_a, new_r, new_g, new_b);
        return the_pixel;
    }
};
    
GShader* GShader::FromBitmap(const GBitmap& bitmap, const GMatrix& localMatrix, GShader::TileMode new_tileMode){
        return new BitmapShader(bitmap,localMatrix,new_tileMode);
}