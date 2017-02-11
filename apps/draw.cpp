/**
 *  Copyright 2015 Mike Reed
 *
 *  COMP 575 -- Fall 2015
 */

#include "GWindow.h"
#include "GBitmap.h"
#include "GCanvas.h"
#include "GColor.h"
#include "GMatrix.h"
#include "GRandom.h"
#include "GRect.h"
#include "GShader.h"

#include "../mike_mesh.h"
#include "../mike_utils.h"

#include <cstdlib>
#include <vector>

extern int mike_mesh_compute_level(const GPoint[4], float tol = 1);

static const float CORNER_SIZE = 9;

template <typename T> int find_index(const std::vector<T*>& list, T* target) {
    for (int i = 0; i < list.size(); ++i) {
        if (list[i] == target) {
            return i;
        }
    }
    return -1;
}

static GRandom gRand;

static GColor rand_color() {
    return GColor::MakeARGB(0.5f, gRand.nextF(), gRand.nextF(), gRand.nextF());
}

static GRect make_from_pts(const GPoint& p0, const GPoint& p1) {
    return GRect::MakeLTRB(std::min(p0.fX, p1.fX), std::min(p0.fY, p1.fY),
                           std::max(p0.fX, p1.fX), std::max(p0.fY, p1.fY));
}

static bool contains(const GRect& rect, float x, float y) {
    return rect.left() < x && x < rect.right() && rect.top() < y && y < rect.bottom();
}

static GRect offset(const GRect& rect, float dx, float dy) {
    return GRect::MakeLTRB(rect.left() + dx, rect.top() + dy,
                           rect.right() + dx, rect.bottom() + dy);
}

static bool hit_test(float x0, float y0, float x1, float y1) {
    const float dx = fabs(x1 - x0);
    const float dy = fabs(y1 - y0);
    return std::max(dx, dy) <= CORNER_SIZE;
}

static bool in_resize_corner(const GRect& r, GPoint p, GPoint* anchor) {
    if (hit_test(r.left(), r.top(), p.x(), p.y())) {
        anchor->set(r.right(), r.bottom());
        return true;
    } else if (hit_test(r.right(), r.top(), p.x(), p.y())) {
        anchor->set(r.left(), r.bottom());
        return true;
    } else if (hit_test(r.right(), r.bottom(), p.x(), p.y())) {
        anchor->set(r.left(), r.top());
        return true;
    } else if (hit_test(r.left(), r.bottom(), p.x(), p.y())) {
        anchor->set(r.right(), r.top());
        return true;
    }
    return false;
}

static void draw_corner(GCanvas* canvas, const GColor& c, float x, float y, float dx, float dy) {
    canvas->fillRect(make_from_pts(GPoint::Make(x, y - 1), GPoint::Make(x + dx, y + 1)), c);
    canvas->fillRect(make_from_pts(GPoint::Make(x - 1, y), GPoint::Make(x + 1, y + dy)), c);
}

static void draw_hilite(GCanvas* canvas, const GRect& r) {
    const float size = CORNER_SIZE;
    GColor c = GColor::MakeARGB(1, 0, 0, 0);
    draw_corner(canvas, c, r.fLeft, r.fTop, size, size);
    draw_corner(canvas, c, r.fLeft, r.fBottom, size, -size);
    draw_corner(canvas, c, r.fRight, r.fTop, -size, size);
    draw_corner(canvas, c, r.fRight, r.fBottom, -size, -size);
}

static void constrain_color(GColor* c) {
    c->fA = std::max(std::min(c->fA, 1.f), 0.1f);
    c->fR = std::max(std::min(c->fR, 1.f), 0.f);
    c->fG = std::max(std::min(c->fG, 1.f), 0.f);
    c->fB = std::max(std::min(c->fB, 1.f), 0.f);
}

class Shape {
    float fRotation = 0;

public:
    virtual ~Shape() {}

    void preRotate(float rad) {
        fRotation += rad;
    }

    GMatrix computeMatrix() {
        const GRect r = this->getRect();
        GMatrix m;
        m.preTranslate(r.centerX(), r.centerY());
        m.preRotate(fRotation);
        m.preTranslate(-r.centerX(), -r.centerY());
        return m;
    }
    
    GMatrix computeInverse() {
        GMatrix inverse;
        this->computeMatrix().invert(&inverse);
        return inverse;
    }

    void draw(GCanvas* canvas) {
        canvas->save();
        canvas->concat(this->computeMatrix());
        this->onDraw(canvas);
        canvas->restore();
    }

    virtual GRect getRect() = 0;
    virtual void setRect(const GRect&) {}
    virtual GColor getColor() = 0;
    virtual void setColor(const GColor&) {}

    virtual GClick* findClick(GPoint) { return nullptr; }
    virtual bool drawHilite(GCanvas*) { return false; }
    virtual bool doSym(KeySym) { return false; }

protected:
    virtual void onDraw(GCanvas* canvas) {}
};

class RectShape : public Shape {
public:
    RectShape(GColor c) : fColor(c) {
        fRect = GRect::MakeXYWH(0, 0, 0, 0);
    }

    void onDraw(GCanvas* canvas) override {
        canvas->fillRect(fRect, fColor);
    }

    GRect getRect() override { return fRect; }
    void setRect(const GRect& r) override { fRect = r; }
    GColor getColor() override { return fColor; }
    void setColor(const GColor& c) override { fColor = c; }

private:
    GRect   fRect;
    GColor  fColor;
};

class BitmapShape : public Shape {
public:
    BitmapShape(const GBitmap& bm) : fBM(bm) {
        const int w = std::max(bm.width(), 100);
        const int h = std::max(bm.height(), 100);
        fRect = GRect::MakeXYWH(100, 100, w, h);
    }
    
    void onDraw(GCanvas* canvas) override {
        canvas->fillBitmapRect(fBM, fRect);
    }
    
    GRect getRect() override { return fRect; }
    void setRect(const GRect& r) override { fRect = r; }
    GColor getColor() override { return GColor::MakeARGB(1, 0, 0, 0); }
    void setColor(const GColor&) override {}
    
private:
    GRect   fRect;
    GBitmap fBM;
};

class PtClick : public GClick {
    GPoint* fAddr;
    
public:
    PtClick(GPoint loc, GPoint* addr)
    : GClick(loc, "pt-click")
    , fAddr(addr)
    {}
    
    bool doMove() override {
        *fAddr = this->curr();
        return true;
    }
};

class TransClick : public GClick {
    GPoint* fAddr;
    
public:
    TransClick(GPoint loc, GPoint* addr)
    : GClick(loc, "trans-click")
    , fAddr(addr)
    {}
    
    bool doMove() override {
        *fAddr = *fAddr + this->curr() - this->prev();
//        printf("trans %g %g\n", fAddr->fX, fAddr->fY);
        return true;
    }
};

static float   fExp = 1;

class MeshShape : public Shape {
    GPoint  fPts[4];
    GColor  fColors[4];
    GBitmap fBitmap;
    GPoint  fOffCurve[8];

    int     fLevel = 1;
    float   fTexScale = 1;
    GPoint  fTexTrans = { 0, 0 };
    int     fCornerIndex = 0;
    bool    fShowColors = false;
    bool    fShowDiag = false;
    bool    fOffDiag = false;
    bool    fShowBitmap = false;
    bool    fUseCProc = false;
    bool    fUseEProc = false;

    static GPoint eval_cubic(GPoint p0, GPoint p1, GPoint p2, GPoint p3, float t0) {
        float t1 = 1 - t0;
        return t1*t1*t1*p0 + 3*t1*t1*t0*p1 + 3*t1*t0*t0*p2 + t0*t0*t0*p3;
    }

    GPoint edge_proc(MeshEdge edge, float t) {
        GPoint p0, p3;
        switch (edge) {
            case MeshEdge::kTop:
                p0 = fPts[0];
                p3 = fPts[1];
                break;
            case MeshEdge::kRight:
                p0 = fPts[1];
                p3 = fPts[2];
                break;
            case MeshEdge::kBottom:
                p0 = fPts[3];
                p3 = fPts[2];
                break;
            case MeshEdge::kLeft:
                p0 = fPts[0];
                p3 = fPts[3];
                break;
        }
        return eval_cubic(p0, fOffCurve[edge*2+0], fOffCurve[edge*2+1], p3, t);
        
    }

    static void draw_line(GCanvas* canvas, GPoint a, GPoint b, const GPaint& paint) {
        GPoint pts[] = { a, b };
        GContour ctr = { 2, pts, false };
        canvas->drawContours(&ctr, 1, paint);
    }
    static float lerp(float a, float b, float t) {
        return (1 - t)*a + t*b;
    }
    static GPoint lerp(GPoint a, GPoint b, float t) {
        return { lerp(a.fX, b.fX, t), lerp(a.fY, b.fY, t) };
    }
    void show_diagonal(GCanvas* canvas, int level) {
        std::vector<GPoint> diag0, diag1;

        const float dt = 1.0 / level;
        GPaint paint;
        paint.setColor({ 1, .75, .75, .75 });
        paint.setStrokeWidth(2);
        float t = 0;
        for (int i = 0; i <= level; ++i) {
            GPoint u0 = lerp(fPts[0], fPts[1], t);
            GPoint u1 = lerp(fPts[3], fPts[2], t);
            diag0.push_back(lerp(u0, u1, t));
            diag1.push_back(lerp(u0, u1, 1 - t));
            t += dt;
            t = std::min(t, 1.0f);
        }
        
        if (fShowDiag) {
            paint.setColor({1, 1, 0, 0 });
            GContour ctr = { (int)diag0.size(), diag0.data(), false };
            canvas->drawContours(&ctr, 1, paint);
            paint.setColor({1, 0, 0, 1 });
            ctr = { (int)diag1.size(), diag1.data(), false };
            canvas->drawContours(&ctr, 1, paint);
        }
    }

    static void init_off(GPoint* pts, GPoint a, GPoint b) {
        pts[0] = lerp(a, b, 1.0f / 3);
        pts[1] = lerp(a, b, 2.0f / 3);
    }

public:
    MeshShape() {
        fPts[0].set(20,   20);
        fPts[1].set(400,  20);
        fPts[2].set(400, 400);
        fPts[3].set(20,  400);

        fColors[0] = { 1, 1, 0, 0 };
        fColors[1] = { 1, 0, 1, 0 };
        fColors[2] = { 1, 0, 0, 1 };
        fColors[3] = { 1, 0.5,0.5,0.5 };

        fBitmap.readFromFile("apps/spock.png");
    }

    static GColor color_proc(const GColor[], float u, float v) {
        float uu = sin(u * M_PI);
        float vv = sin(v * M_PI);
        float scale = powf(uu * vv, fExp);
        return { 1, scale, scale, scale };
    }

    void onDraw(GCanvas* canvas) override {
        std::unique_ptr<GShader> shader;
        GPaint paint;
        if (fShowBitmap) {
            GMatrix mx;
            mx.setTranslate(fTexTrans.x(), fTexTrans.y());
            mx.postScale(fTexScale / fBitmap.width(), fTexScale / fBitmap.height());
            shader = std::unique_ptr<GShader>(GShader::FromBitmap(fBitmap, mx, GShader::kRepeat));
            paint.setShader(shader.get());
        }
        
        const GColor* colors = fShowColors ? fColors : nullptr;
        std::function<GPoint(MeshEdge, float)> eproc = [this](MeshEdge e, float t) {
            return edge_proc(e, t);
        };
        mike_mesh(canvas, fPts, colors, paint, fLevel, fOffDiag,
                  fUseCProc ? color_proc : nullptr,
                  fUseEProc ? eproc : nullptr);

        if (fShowDiag) {
            this->show_diagonal(canvas, fLevel);
        }

        if (fUseEProc) {
            GPaint pn0, pn1({1,1,1,1});
            for (GPoint p : fOffCurve) {
                canvas->drawRect(GRect::MakeXYWH(p.fX - 4, p.fY - 4, 8, 8), pn0);
                canvas->drawRect(GRect::MakeXYWH(p.fX - 2, p.fY - 2, 4, 4), pn1);
            }
        }
    }

    GRect getRect() override {
        return GRect::MakeWH(200, 200);
    }

    GColor getColor() override { return fColors[fCornerIndex]; }
    void setColor(const GColor& c) override { fColors[fCornerIndex] = c; }

    GClick* findClick(GPoint pt) override {
        for (int i = 0; i < 4; ++i) {
            if (hit_test(fPts[i].fX, fPts[i].fY, pt.fX, pt.fY)) {
                fCornerIndex = i;
                return new PtClick(pt, &fPts[i]);
            }
        }
        if (fUseEProc) {
            for (auto& oc : fOffCurve) {
                if (hit_test(oc.fX, oc.fY, pt.fX, pt.fY)) {
                    return new PtClick(pt, &oc);
                }
            }
        }
        return new TransClick(pt, &fTexTrans);
        return nullptr;
    }

    bool drawHilite(GCanvas*) override { return true; }

    bool doSym(KeySym sym) override {
        switch (sym) {
            case '-': fLevel = std::max(fLevel - 1, 1); return true;
            case '=': fLevel = std::min(fLevel + 1, 64); return true;
            case 's': fShowColors = !fShowColors; return true;
            case 'd': fShowDiag = !fShowDiag; return true;
            case 'o': fOffDiag = !fOffDiag; return true;
            case 't': fShowBitmap = !fShowBitmap; return true;
            case XK_Up: fTexScale *= 1.25f; return true;
            case XK_Down: fTexScale *= 0.8f; return true;
            case '[': fExp *= .9f; return true;
            case ']': fExp *= 1.1f; return true;
            case 'p': fUseCProc = !fUseCProc; return true;
            case 'e':
                fUseEProc = !fUseEProc;
                if (fUseEProc) {
                    init_off(&fOffCurve[0], fPts[0], fPts[1]);
                    init_off(&fOffCurve[2], fPts[1], fPts[2]);
                    init_off(&fOffCurve[4], fPts[3], fPts[2]);
                    init_off(&fOffCurve[6], fPts[0], fPts[3]);
                }
                return true;
            default:
                break;
        }
        return false;
    }
};

static void alloc_bitmap(GBitmap* bm, int w, int h) {
    bm->fWidth = w;
    bm->fHeight = h;
    bm->fRowBytes = bm->fWidth * sizeof(GPixel);
    bm->fPixels = (GPixel*)calloc(bm->height(), bm->rowBytes());
}

static Shape* cons_up_shape(int index) {
    return new MeshShape;
    const char* names[] = {
        "apps/spock.png",
        "expected/blend_black.png",
        "expected/circles_blend.png",
        "expected/solid_ramp.png",
        "expected/spocks_zoom.png",
        "expected/blend_white.png",
        "expected/circles_fat.png",
        "expected/spocks_quad.png",
    };
    GBitmap bm;
    if (index < GARRAY_COUNT(names) && bm.readFromFile(names[index])) {
        return new BitmapShape(bm);
    }
    return NULL;
}

class ResizeClick : public GClick {
    GPoint  fAnchor;
public:
    ResizeClick(GPoint loc, GPoint anchor) : GClick(loc, "resize"), fAnchor(anchor) {}

    GRect makeRect(const GMatrix& inverse) const {
        return make_from_pts(inverse.mapPt(this->curr()), fAnchor);
    }
};

class TestWindow : public GWindow {
    std::vector<Shape*> fList;
    Shape* fShape;
    GColor fBGColor;

public:
    TestWindow(int w, int h) : GWindow(w, h) {
        fBGColor = GColor::MakeARGB(1, 1, 1, 1);
        fShape = NULL;
    }

    ~TestWindow() override {}
    
protected:
    void onDraw(GCanvas* canvas) override {
        canvas->clear(fBGColor);

        for (int i = 0; i < fList.size(); ++i) {
            fList[i]->draw(canvas);
        }
        if (fShape && !fShape->drawHilite(canvas)) {
            canvas->save();
            canvas->concat(fShape->computeMatrix());
            draw_hilite(canvas, fShape->getRect());
            canvas->restore();
        }
    }

    bool onKeyPress(const XEvent&, KeySym sym) override {
        if (sym >= '1' && sym <= '9') {
            fShape = cons_up_shape(sym - '1');
            if (fShape) {
                fList.push_back(fShape);
                this->updateTitle();
                this->requestDraw();
            }
        }

        if (fShape) {
            if (fShape->doSym(sym)) {
                this->requestDraw();
                return true;
            }
            switch (sym) {
                case XK_Up: {
                    int index = find_index(fList, fShape);
                    if (index < fList.size() - 1) {
                        std::swap(fList[index], fList[index + 1]);
                        this->requestDraw();
                        return true;
                    }
                    return false;
                }
                case XK_Down: {
                    int index = find_index(fList, fShape);
                    if (index > 0) {
                        std::swap(fList[index], fList[index - 1]);
                        this->requestDraw();
                        return true;
                    }
                    return false;
                }
                case XK_BackSpace:
                    this->removeShape(fShape);
                    fShape = NULL;
                    this->updateTitle();
                    this->requestDraw();
                    return true;
                case XK_Left:
                case XK_Right: {
                    const float rad = M_PI * 2 / 180;
                    fShape->preRotate(rad * (sym == XK_Left ? 1 : -1));
                    this->requestDraw();
                    return true;
                }
                default:
                    break;
            }
        }

        GColor c = fShape ? fShape->getColor() : fBGColor;
        const float delta = 0.1f;
        switch (sym) {
            case 'a': c.fA -= delta; break;
            case 'A': c.fA += delta; break;
            case 'r': c.fR -= delta; break;
            case 'R': c.fR += delta; break;
            case 'g': c.fG -= delta; break;
            case 'G': c.fG += delta; break;
            case 'b': c.fB -= delta; break;
            case 'B': c.fB += delta; break;
            default:
                return false;
        }
        constrain_color(&c);
        if (fShape) {
            fShape->setColor(c);
        } else {
            c.fA = 1;   // need the bg to stay opaque
            fBGColor = c;
        }
        this->updateTitle();
        this->requestDraw();
        return true;
    }

    GClick* onFindClickHandler(GPoint loc) override {
        if (fShape) {
            GClick* ck = fShape->findClick(loc);
            if (ck) {
                return ck;
            }

            GPoint anchor;
            if (in_resize_corner(fShape->getRect(), fShape->computeInverse().mapPt(loc), &anchor)) {
                return new ResizeClick(loc, anchor);
            }
        }

        for (int i = fList.size() - 1; i >= 0; --i) {
            if (contains(fList[i]->getRect(), loc.x(), loc.y())) {
                fShape = fList[i];
                this->updateTitle();
                return new GClick(loc, "move");
            }
        }
        
        // else create a new shape
        fShape = new RectShape(rand_color());
        fList.push_back(fShape);
        this->updateTitle();
        return new GClick(loc, "create");
    }

    void onHandleClick(GClick* click) override {
        if (!click->doMove()) {
            if (click->isName("move")) {
                const GPoint curr = click->curr();
                const GPoint prev = click->prev();
                fShape->setRect(offset(fShape->getRect(), curr.x() - prev.x(), curr.y() - prev.y()));
            } else if (click->isName("resize")) {
                fShape->setRect(((ResizeClick*)click)->makeRect(fShape->computeInverse()));
            } else {
                fShape->setRect(make_from_pts(click->orig(), click->curr()));
            }
            if (NULL != fShape && GClick::kUp_State == click->state()) {
                if (fShape->getRect().isEmpty()) {
                    this->removeShape(fShape);
                    fShape = NULL;
                }
            }
        }
        this->updateTitle();
        this->requestDraw();
    }

private:
    void removeShape(Shape* target) {
        GASSERT(target);

        std::vector<Shape*>::iterator it = std::find(fList.begin(), fList.end(), target);
        if (it != fList.end()) {
            fList.erase(it);
        } else {
            GASSERT(!"shape not found?");
        }
    }

    void updateTitle() {
        char buffer[100];
        buffer[0] = ' ';
        buffer[1] = 0;

        GColor c = fBGColor;
        if (fShape) {
            c = fShape->getColor();
        }

        sprintf(buffer, "%02X %02X %02X %02X",
                int(c.fA * 255), int(c.fR * 255), int(c.fG * 255), int(c.fB * 255));
        this->setTitle(buffer);
    }

    typedef GWindow INHERITED;
};

int main(int argc, char const* const* argv) {
    GWindow* wind = new TestWindow(640, 480);

    return wind->run();
}

