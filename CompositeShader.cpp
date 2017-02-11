#include <math.h>
#include "GPixel.h"
#include "GBitmap.h"
#include "GCanvas.h"
#include "GColor.h"
#include "GRect.h"
#include "GPoint.h"
#include "GMatrix.h"
#include "GShader.h"
#include "TriColorShader.cpp"
#include "ProxyShader.cpp"
#include <stdio.h>
#include <cstdint>

class CompositeShader: public GShader{
    public:
    TriColorShader* color_shader;
    ProxyShader* tex_shader;

    CompositeShader(TriColorShader* new_color_shader,ProxyShader* new_tex_shader){
        color_shader = new_color_shader;
        tex_shader = new_tex_shader;
    }

    bool setContext(const GMatrix& new_ctm,float new_alpha){
        return (color_shader->setContext(new_ctm, new_alpha)&&tex_shader->setContext(new_ctm,new_alpha));
    }

    void shadeRow(int x, int y, int count, GPixel row[]){
        GPixel row_color[count];
        GPixel row_tex[count];
        color_shader->shadeRow(x,y,count,row_color);
        tex_shader->shadeRow(x,y,count,row_tex);
        for(int i =0; i<count; i++){
            GPixel color_shader_pix = row_color[i];
            GPixel tex_shader_pix = row_tex[i];
            row[i] = blend(color_shader_pix,tex_shader_pix);
        }
    }

    GPixel blend(const GPixel& pix1, const GPixel& pix2){
		float fa = (GPixel_GetA(pix1)/255.0)*(GPixel_GetA(pix2)/255.0);
        float fr = (GPixel_GetR(pix1)/255.0)*(GPixel_GetR(pix2)/255.0);
        float fg = (GPixel_GetG(pix1)/255.0)*(GPixel_GetG(pix2)/255.0);
        float fb = (GPixel_GetB(pix1)/255.0)*(GPixel_GetB(pix2)/255.0);
        GColor fcolor = GColor::MakeARGB(fa,fr,fg,fb);
        GColor correctedColor = fcolor.pinToUnit();
        int a = (uint8_t) (correctedColor.fA*255);
        int r = (uint8_t) (correctedColor.fR*255);
        int g = (uint8_t) (correctedColor.fG*255);
        int b = (uint8_t) (correctedColor.fB*255);
        return GPixel_PackARGB(a,r,g,b);
		
}
};