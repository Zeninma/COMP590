/*
 *  Copyright 2016 Mike Reed
 */

#include "GContour.h"
#include "GPath.h"

GPath& GPath::moveTo(const GPoint& pt) {
    if (fVerbs.size() > 0 && fVerbs.back() == Verb::kMove) {
        fPts.back() = pt;
    } else {
        fPts.push_back(pt);
        fVerbs.push_back(Verb::kMove);
    }
    return *this;
}

GPath& GPath::lineTo(const GPoint& pt) {
    GASSERT(fVerbs.size() > 0);
    fPts.push_back(pt);
    fVerbs.push_back(Verb::kLine);
    return *this;
}

GPath::Iter::Iter(const GPath& path) {
    if (path.fPts.size() > 0) {
        fPts = &path.fPts.front();
        fVerbs = &path.fVerbs.front();
        fStopVerbs = fVerbs + path.fVerbs.size();
    } else {
        fPts = nullptr;
        fVerbs = fStopVerbs = nullptr;
    }
}

bool GPath::Iter::next(GContour* ctr) {
    const GPoint* outPts;

    do {
        outPts = fPts;
        if (fVerbs == fStopVerbs) {
            ctr->fCount = 0;
            return false;
        }

        const GPath::Verb* verbs = fVerbs;
        GASSERT(*verbs == GPath::Verb::kMove);
        while (++verbs < fStopVerbs && *verbs != GPath::Verb::kMove)
            ;
        ctr->fCount = (int)(verbs - fVerbs);
        // now update the iterator
        fVerbs = verbs;
        fPts += ctr->fCount;
    } while (ctr->fCount == 1);
    ctr->fPts = outPts;
    return true;
}

void GPath::dump() const {
    Iter iter(*this);
    GContour ctr;

    int contour = 0;
    while (iter.next(&ctr)) {
        GASSERT(ctr.fCount > 1);
        printf("[%d]", contour++);
        for (int i = 0; i < ctr.fCount; ++i) {
            printf(" (%g %g)", ctr.fPts[i].x(), ctr.fPts[i].y());
        }
        printf("\n");
    }
}
