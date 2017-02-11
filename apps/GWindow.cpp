/**
 *  Copyright 2015 Mike Reed
 *
 *  COMP 575 -- Fall 2015
 */

#include "GWindow.h"
#include "GBitmap.h"
#include "GCanvas.h"
#include "GTime.h"
#include <stdio.h>

GClick::GClick(GPoint loc, const char* name) {
    fCurr = fPrev = fOrig = loc;
    fState = kDown_State;
    fName = name;
}

#define G_SelectInputMask (StructureNotifyMask | ExposureMask | KeyPressMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask)

GWindow::GWindow(int width, int height) {
    fDisplay = XOpenDisplay(NULL);
    if (!fDisplay) {
        fprintf(stderr, "can't open xdisplay\n");
        return;
    }

    fClick = NULL;
    fWidth = width;
    fHeight = height;
    fReadyToQuit = false;
    
    int screenNo = DefaultScreen(fDisplay);
    Window root = RootWindow(fDisplay, screenNo);
    fWindow = XCreateSimpleWindow(fDisplay, root, 0, 0, width, height, 1,
                                  BlackPixel(fDisplay, screenNo),
                                  WhitePixel(fDisplay, screenNo));

    XSelectInput(fDisplay, fWindow, G_SelectInputMask);
    XMapWindow(fDisplay, fWindow);

    fGC = XCreateGC(fDisplay, fWindow, 0, NULL);

    this->setupBitmap(width, height);
    fCanvas = GCanvas::Create(fBitmap);
}

GWindow::~GWindow() {
    delete fCanvas;
    free(fBitmap.fPixels);

    if (fDisplay) {
        XFreeGC(fDisplay, fGC);
        XDestroyWindow(fDisplay, fWindow);
        XCloseDisplay(fDisplay);
    }
}

void GWindow::setTitle(const char title[]) {
    XStoreName(fDisplay, fWindow, title);
}

void GWindow::requestDraw() {
    if (!fNeedDraw) {
        fNeedDraw = true;
        
        XEvent evt;
        memset(&evt, 0, sizeof(evt));
        evt.type = Expose;
        evt.xexpose.display = fDisplay;
        XSendEvent(fDisplay, fWindow, false, ExposureMask, &evt);
    }
}

static bool gDoTime;

bool GWindow::handleEvent(XEvent* evt) {
//     printf("event %d\n", evt->type);
    switch (evt->type) {
        case ConfigureNotify: {
            const int w = evt->xconfigure.width;
            const int h = evt->xconfigure.height;
            if (w != fWidth || h != fHeight) {
                fWidth = w;
                fHeight = h;
                this->onResize(w, h);
                
                delete fCanvas;
                this->setupBitmap(w, h);
                fCanvas = GCanvas::Create(fBitmap);
                // assume we will get called to redraw
            }
            return true;
        }
        case Expose:
            if (0 == evt->xexpose.count) {
                if (gDoTime) {
                    unsigned now = GTime::GetMSec();
                    int N = 200;
                    for (int i = 0; i < N; ++i) {
                        this->onDraw(fCanvas);
                    }
                    char buffer[100];
                    unsigned dur = GTime::GetMSec() - now;
                    buffer[sprintf(buffer, "MS = %5.2f", dur * 1.0 / N)] = 0;
                    this->setTitle(buffer);
                }

                fNeedDraw = false;
                this->onDraw(fCanvas);
                this->drawCanvasToWindow();

                if (gDoTime) {
                    this->requestDraw();
                }
            }
            return true;
        case KeyPress: {
            char buffer[128];
            KeySym sym;
            memset(buffer, 0, sizeof(buffer));
            (void)XLookupString(&evt->xkey, buffer, sizeof(buffer), &sym, NULL);

            if (this->onKeyPress(*evt, sym)) {
                return true;
            }
            if ('M' == sym) {
                gDoTime = !gDoTime;
                this->requestDraw();
                return true;
            }
            if (XK_Escape == sym) {
                this->setReadyToQuit();
                return true;
            }
            break;
        }
        case ButtonPress: {
            if (fClick) {
                delete fClick;
            }
            fClick = this->onFindClickHandler(GPoint::Make(evt->xbutton.x, evt->xbutton.y));
//            printf("ButtonPress [%d %d] %x %d\n", evt->xbutton.x, evt->xbutton.y, evt->xbutton.state, evt->xbutton.button);
            break;
        }
        case ButtonRelease: {
            if (fClick) {
                fClick->fState = GClick::kUp_State;
                this->onHandleClick(fClick);
                delete fClick;
                fClick = NULL;
            }
//            printf("ButtonRelease [%d %d] %x %d\n", evt->xbutton.x, evt->xbutton.y, evt->xbutton.state, evt->xbutton.button);
            break;
        }
        case MotionNotify: {
            if (fClick) {
                fClick->fState = GClick::kMove_State;
                fClick->fPrev = fClick->fCurr;
                fClick->fCurr.set(evt->xmotion.x, evt->xmotion.y);
                this->onHandleClick(fClick);
//            if (evt->xmotion.state) {
//                printf("MotionNotify [%d %d] %x\n", evt->xmotion.x, evt->xmotion.y, evt->xmotion.state);
            }
            break;
        }
        default:
            break;
    }
    return false;
}

static void set_image_from_bitmap(XImage* image, const GBitmap& bitmap) {
    const int w = bitmap.width();
    const int h = bitmap.height();

    memset(image, 0, sizeof(XImage));
    
    image->width = w;
    image->height = h;
    image->format = ZPixmap;
    image->data = (char*)bitmap.pixels();
    image->byte_order = LSBFirst;
    image->bitmap_unit = 32;
    image->bitmap_bit_order = LSBFirst;
    image->bitmap_pad = 32;
    image->depth = 24;
    image->bytes_per_line = bitmap.rowBytes() - w * 4;
    image->bits_per_pixel = 32;

    XInitImage(image);
}

void GWindow::drawCanvasToWindow() {
    XImage image;
    set_image_from_bitmap(&image, fBitmap);

    int x = 0;
    int y = 0;
    int w = fBitmap.width();
    int h = fBitmap.height();
    XPutImage(fDisplay, fWindow, fGC, &image, 0, 0, x, y, w, h);
}

void GWindow::setupBitmap(int w, int h) {
    if (fBitmap.fPixels) {
        free(fBitmap.fPixels);
    }

    fBitmap.fWidth = w;
    fBitmap.fHeight = h;
    fBitmap.fRowBytes = w * sizeof(GPixel);
    const size_t size = fBitmap.fRowBytes * fBitmap.fHeight;
    fBitmap.fPixels = (GPixel*)malloc(size);
    memset(fBitmap.fPixels, 0, size);
}

int GWindow::run() {
    if (!fDisplay) {
        return -1;
    }

    for (;;) {
        XEvent evt;
        XNextEvent(fDisplay, &evt);
        this->handleEvent(&evt);
        if (fReadyToQuit) {
            break;
        }
    }
    return 0;
}
