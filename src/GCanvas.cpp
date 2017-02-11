/**
 *  Copyright 2016 Mike Reed
 */

#include "GCanvas.h"
#include "GMatrix.h"

void GCanvas::translate(float tx, float ty) {
    GMatrix m;
    m.setTranslate(tx, ty);
    this->concat(m);
}

void GCanvas::scale(float sx, float sy) {
    GMatrix m;
    m.setScale(sx, sy);
    this->concat(m);
}

void GCanvas::rotate(float radians) {
    GMatrix m;
    m.setRotate(radians);
    this->concat(m);
}
