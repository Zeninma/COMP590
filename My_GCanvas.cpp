#include "GCanvas.h"
#include <math.h>
#include "GPixel.h"
#include "GBitmap.h"
#include "GCanvas.h"
#include "GColor.h"
#include "GRect.h"
#include "GPoint.h"
#include "GMatrix.h"
#include "GShader.h"
#include "GContour.h"
#include "GMath.h"
#include "CompositeShader.cpp"
#include <stdio.h>
#include <stack>
#include <list>
#include <vector>
#include <assert.h>

#define TWO_FIVE_FIVE 255
#define OPT_FACTOR 65793
#define LEFT_SHIFT 23
#define RIGHT_SHIFT 24

struct edge {
	int start_y;
	int end_y;
	float slope;
	float curr_x;
	int winding;

	bool operator< (const edge& a) const{
		if(start_y == a.start_y){
			return curr_x < a.curr_x;
		}
		return start_y < a.start_y;
	}
    
};

struct tri{
	GPoint vertices[3];
};

class My_GCanvas : public GCanvas
{	
	private:
		const GBitmap& bitmap;

	public:
		std::stack<GMatrix> matrix_stack;
		GMatrix my_CTM;
		// new method added for PA 4
		void setCTM(const GMatrix& new_matrix);
		void drawRect(const GRect& new_rec, const GPaint& new_paint);
		void drawConvexPolygon(const GPoint new_points[], int count, const GPaint& new_paint);
		void scan_line_shader_color(float x_left, float x_right,int curr_y, const GColor& src_color);
		// new method added for PA 4
		void clear(const GColor& inputColor);
		// void fillRect(const GRect& rect, const GColor& inputColor);
		void fillBitmapRect(const GBitmap& src, const GRect& dst);
		void clip_rect(const GBitmap& src, GIRect& dst_rect);
		edge make_edge(GPoint a, GPoint b);
		GPixel premulPixel(const GColor& inputColor);
		GPixel blend_src_dst(const GPixel& src, const GPixel& dst);
		void scan_line_shader(float x_start, float x_end, int curr_y, const GPaint& paint);
		bool check_invalid_pts(GPoint points[],int count);
		void drawContours(const GContour ctrs[], int count, const GPaint& paint);
		void connect_contour(std::vector<edge> &total_edge, const GContour &curr_ctr, int count, int &total_edge_num);
		void color_survivor(std::list<edge> &survivor, int curr_y,const GPaint& paint);
		void check_survivor(std::list<edge> &survivor, std::vector<edge> &total_edge, int total_edge_num, int curr_y, int &total_edge_idx);
		void translate(float tx, float ty);
		void scale(float sx, float sy);
		void rotate(float radians);
		void save();
		void restore();
		void concat(const GMatrix& new_matrix);
		/**********************************PA6**************************************************/
		void explode_contour(const GContour ori_ctrs[], GContour* dst_ctr,int count, int &dst_count, float width, float miterLimit);
		void stroke_contour(GContour ori_ctr,GContour* dst_ctr, int &dst_count, float width, float miterLimit);
		void make_contour(GPoint a, GPoint b, GPoint c, GContour* dst_ctr ,float width, int& dst_count ,float miterLimit);
		/**********************************PA6**************************************************/
		/**********************************PA7**************************************************/
		 void drawMesh(int triCount, const GPoint pts[], const int indices[],
		 const GColor colors[], const GPoint tex[], const GPaint& paint);
		/**********************************PA7**************************************************/
		My_GCanvas(const GBitmap& inputBitmap): bitmap(inputBitmap){
		}
};

GCanvas* GCanvas::Create(const GBitmap& bitmap){
    if(bitmap.fWidth<0 || bitmap.fHeight<0|| bitmap.fRowBytes<bitmap.fWidth*4){
        return NULL;
    }
    else{
        return new My_GCanvas(bitmap);
    }
}
// PA4 new function
void My_GCanvas::translate(float tx, float ty){
	my_CTM.preTranslate(tx,ty);
}

void My_GCanvas::scale(float sx, float sy){
	my_CTM.preTranslate(sx,sy);
}

void My_GCanvas::rotate(float radians){
	my_CTM.preRotate(radians);
}

void My_GCanvas::setCTM(const GMatrix& new_matrix){
	my_CTM = new_matrix;
}

//PA5 new GMatrix operation
void My_GCanvas::save(){
	matrix_stack.push(my_CTM);
}

void My_GCanvas::restore(){
	my_CTM = matrix_stack.top();
	matrix_stack.pop();
}

void My_GCanvas::concat(const GMatrix& new_matrix){
	my_CTM.preConcat(new_matrix);
}

void My_GCanvas::clip_rect(const GBitmap& canvas, GIRect& dst_rect){
	if (dst_rect.fTop < 0 ){
		dst_rect.fTop = 0;
	}
	if (dst_rect.fLeft < 0){
		dst_rect.fLeft = 0;
	}
	if (dst_rect.fBottom > canvas.fHeight){
		dst_rect.fBottom = canvas.fHeight;
	}
	if (dst_rect.fRight > canvas.fWidth){
		dst_rect.fRight = canvas.fWidth;
	}
}

// pre determine whether the dst Rect is legal
// compute the inverse Matrix's component
void My_GCanvas::fillBitmapRect(const GBitmap& src, const GRect& dst){
	 bool row_byte_error = src.rowBytes() < (src.width() * 4);
	 GMatrix trans_m;
	 float top = dst.top();
	 float left = dst.left();
	 float x_scale = dst.width()/src.width();
	 float y_scale = dst.height()/src.height();
	 trans_m.setTranslate(left, top);
	 trans_m.preScale(x_scale,y_scale);
	 GPoint vertex[4];
	
	 vertex[0] = GPoint::Make(dst.left(),dst.top());
	 vertex[1] = GPoint::Make(dst.right(),dst.top());
	 vertex[2] = GPoint::Make(dst.right(),dst.bottom());
	 vertex[3] = GPoint::Make(dst.left(),dst.bottom());
	 GShader* new_shader = GShader::FromBitmap(src,trans_m);
	 GPaint new_paint = GPaint(new_shader);
	 drawConvexPolygon(vertex, 4, new_paint);
	 
}

GPixel My_GCanvas::premulPixel(const GColor& inputColor){
	GColor correctedColor = inputColor.pinToUnit();
	int a = GRoundToInt(TWO_FIVE_FIVE * correctedColor.fA);
	int r =  GRoundToInt(TWO_FIVE_FIVE * correctedColor.fA * correctedColor.fR); 
	int b =  GRoundToInt(TWO_FIVE_FIVE * correctedColor.fA * correctedColor.fB);
	int g =  GRoundToInt(TWO_FIVE_FIVE * correctedColor.fA * correctedColor.fG);
	return GPixel_PackARGB(a,r,g,b);
}

GPixel My_GCanvas::blend_src_dst(const GPixel& src, const GPixel& dst){
		int src_a = GPixel_GetA(src);
		int blendA = (uint8_t) (src_a + (((TWO_FIVE_FIVE-src_a) * GPixel_GetA(dst) *  OPT_FACTOR + (1<<LEFT_SHIFT)) >> RIGHT_SHIFT));
		int blendR = (uint8_t) (GPixel_GetR(src) + (((TWO_FIVE_FIVE-src_a) * GPixel_GetR(dst) *  OPT_FACTOR + (1<<LEFT_SHIFT)) >> RIGHT_SHIFT));
		int blendG = (uint8_t) (GPixel_GetG(src) + (((TWO_FIVE_FIVE-src_a) * GPixel_GetG(dst) *  OPT_FACTOR + (1<<LEFT_SHIFT)) >> RIGHT_SHIFT));
		int blendB = (uint8_t) (GPixel_GetB(src) + (((TWO_FIVE_FIVE-src_a) * GPixel_GetB(dst) *  OPT_FACTOR + (1<<LEFT_SHIFT)) >> RIGHT_SHIFT));
		GPixel thePixel = GPixel_PackARGB(blendA, blendR, blendG, blendB);
		return thePixel;
}

/* r,g,b values in GPixel need to be premultiplied*/
void My_GCanvas::clear(const GColor& inputColor){
	GPixel thePixel = premulPixel(inputColor);

    for (int y = 0; y < bitmap.height(); ++y) {
		GPixel* row = (GPixel*) (bitmap.fPixels + bitmap.fRowBytes *y/4);
		for (int x = 0; x < bitmap.width(); ++x) {
			*(row + x) = thePixel;
        }
    }
	return;
}

void My_GCanvas::drawRect(const GRect& new_rec, const GPaint& new_paint){
	GPoint vertex[4];

	vertex[0] = GPoint::Make(new_rec.left(),new_rec.top());
	vertex[1] = GPoint::Make(new_rec.right(),new_rec.top());
	vertex[2] = GPoint::Make(new_rec.right(),new_rec.bottom());
	vertex[3] = GPoint::Make(new_rec.left(),new_rec.bottom());
	//Vertex will be transform in the draw convex polygon
	drawConvexPolygon(vertex,4,new_paint);
	return;
}

bool My_GCanvas::check_invalid_pts (GPoint points[], int count){
	//all on left?	
	int invalid_num = 0;
	for(int i = 0; i < count; ++i){
		if(points[i].x()<0){
			invalid_num++;
		}
	}
	if(invalid_num == count){
		return true;
	}
	//all on right?
	invalid_num = 0;
	for(int i = 0; i < count; ++i){
		if(points[i].x()>bitmap.width()-1){
			invalid_num++;
		}
	}
	if(invalid_num == count){
		return true;
	}
	//below bound?
	invalid_num = 0;
	for(int i = 0; i < count; ++i){
		if(points[i].y()<0){
			invalid_num++;
		}
	}
	if(invalid_num == count){
		return true;
	}
	//over bound?
	invalid_num = 0;
	for(int i = 0; i < count; ++i){
		if(points[i].y()>bitmap.width()-1){
			invalid_num++;
		}
	}
	if(invalid_num == count){
		return true;
	}
	return false;
}

edge My_GCanvas::make_edge(GPoint a, GPoint b){
	edge e;
	if (b.y()>a.y()){
		 e.winding = 1;
	}
	else if (b.y()<a.y()){
		e.winding = -1;
	}
	float exMin = std::min(a.y(),b.y());
	float exMax = std::max(a.y(),b.y());
	e.start_y = std::min(std::max((int)floor(exMin+0.5),0), bitmap.height());
	e.end_y = std::max(0,(std::min((int)(floor(exMax+0.5)), bitmap.height())));
	if(b.y() == a.y()){
		e.slope = 0;
	}
	else{
		e.slope = (b.x()-a.x())/(b.y()-a.y());
	}

	float intercept = a.x() - e.slope*a.y();
	e.curr_x = intercept + e.slope * (e.start_y+0.5);

	return e;
}


void My_GCanvas::drawConvexPolygon(const GPoint new_points[], int count, const GPaint& paint){
	GPoint points[count];

	if (count<2){
		return;
	}

	my_CTM.mapPoints(points,new_points,count);


	if(check_invalid_pts(points, count)){
		return;
	}
	
	edge edges[count];
	//connect all the edges according to given order
	for(int i = 0; i < count-1; i++){
		edges[i] = make_edge(points[i],points[i+1]);
	}
	edges[count-1] = make_edge(points[count-1],points[0]);
	std::sort(&edges[0],&edges[count]);

	edge left = edges[0];
	edge right = edges[1];
	int new_edge = 2;
	while(new_edge<= count){
		int start = std::max(left.start_y,right.start_y);
		int end = std::min(left.end_y,right.end_y);
		for(int y = start; y< end; ++y){
			if (paint.getShader() == nullptr){
				scan_line_shader_color(left.curr_x,right.curr_x,y,paint.getColor());
			}
			else{
				scan_line_shader(left.curr_x,right.curr_x,y,paint);
			}
			left.curr_x = left.curr_x + left.slope;
			right.curr_x = right.curr_x + right.slope;
		}
		if(left.end_y == end){
			left = edges[new_edge];
			new_edge++;
		}
		else{
			right = edges[new_edge];
			new_edge++;
		}
	}
}
/************************************************PA5!!!!!!*************************************************************************/
/************************************************PA5!!!!!!*************************************************************************/
/************************************************PA5!!!!!!*************************************************************************/
/************************************************PA5!!!!!!*************************************************************************/
/************************************************PA5!!!!!!*************************************************************************/
/************************************************PA5!!!!!!*************************************************************************/
/************************************************PA5!!!!!!*************************************************************************/

bool compare_y_end(const edge& a, const edge& b){
	return a.end_y < b.end_y;
}

void My_GCanvas::drawContours(const GContour ctrs[], int count, const GPaint& paint){
	if(paint.getStrokeWidth()>0){
		int ctr_num = 0;
		for(int i = 0; i<count; ++i){
			ctr_num += ctrs[i].fCount;
		}
		ctr_num = ctr_num*4+8;
		GContour dst_ctrs[ctr_num];
		int dst_count = 0;
		explode_contour(ctrs, dst_ctrs, count, dst_count, paint.getStrokeWidth(), paint.getMiterLimit());
		GPaint new_paint = paint;
		new_paint.setStrokeWidth(-1);
		drawContours(dst_ctrs, dst_count, new_paint);
	}
	else{
		int total_edge_num = 0;
		std::vector<edge> total_edge;
		for (int i = 0; i < count;i++){
			connect_contour(total_edge, ctrs[i], ctrs[i].fCount, total_edge_num);
		}
		if (total_edge_num <1){
			return;
		}
		//sort all the edges in the total_edge
		std::sort(total_edge.begin(), total_edge.end(),compare_y_end);
		int y_end = GRoundToInt(total_edge[total_edge_num-1].end_y);
		std::sort(total_edge.begin(), total_edge.end());
		int y_start = GRoundToInt(total_edge[0].start_y);
		std::list<edge> survivor;
		int total_edge_idx = 0;
		for(int curr_y = y_start; curr_y < y_end; ++curr_y){
			check_survivor(survivor, total_edge,total_edge_num, curr_y, total_edge_idx);
			color_survivor(survivor,curr_y,paint);
		}
	}
}


// For the given contour, first map all the pts by my_CTM, then connect all the points into edges,
// At last, append these edges into the total_edge list
void My_GCanvas::connect_contour(std::vector<edge> &total_edge, const GContour &curr_ctr,const int count, int &total_edge_num){
	GPoint mapped_pts[count];
	my_CTM.mapPoints(mapped_pts,curr_ctr.fPts, count);
	for(int i = 0; i<count-1; i++){
		if(GRoundToInt(mapped_pts[i].y())!=GRoundToInt(mapped_pts[i+1].y())){
			edge new_edge = make_edge(mapped_pts[i],mapped_pts[i+1]);
			total_edge.push_back(new_edge);
			total_edge_num++;
		}
	}
	if(GRoundToInt(mapped_pts[count-1].y())!=GRoundToInt(mapped_pts[0].y())){
		edge new_edge = make_edge(mapped_pts[count-1],mapped_pts[0]);
		total_edge.push_back(new_edge);
		total_edge_num++;
	}
}

bool compare_x(const edge& a,const edge& b){
		return a.curr_x < b.curr_x;
	}



void My_GCanvas::check_survivor(std::list<edge> &survivor, std::vector<edge> &total_edge
, int total_edge_num, int curr_y, int &total_edge_idx){
	//empty list appear!!!!!!!!!
	std::list<edge>::iterator it = survivor.begin();
	while(it != survivor.end()){
		if ((*it).end_y<=curr_y){
			it = survivor.erase(it);
		}
		else{
			++it;
		}
	}
	//total edge is not a list but a vector!
	for(int i = total_edge_idx; i < total_edge_num; ++i){
		assert(abs(total_edge[i].winding) <=1);
		if (total_edge[i].start_y==curr_y){
			survivor.push_back(total_edge[i]);
			total_edge_idx++;
		}
	}
}


void My_GCanvas::color_survivor(std::list<edge> &survivor, int curr_y,const GPaint& paint){
	int curr_winding = 0;
	survivor.sort(compare_x);

	for(std::list<edge>::iterator it = survivor.begin();it!= survivor.end();++it){
		curr_winding += (*it).winding;
		if (curr_winding != 0){
			if(paint.getShader() == nullptr){
				scan_line_shader_color((*it).curr_x, (*(std::next(it,1))).curr_x,curr_y,paint.getColor());
			}
			else{
				scan_line_shader((*it).curr_x, (*(std::next(it,1))).curr_x,curr_y,paint);
			}
		}
	}
	for(std::list<edge>::iterator it=survivor.begin(); it != survivor.end(); ++it){
		(*it).curr_x += (*it).slope;
	}
}

/**********************************PA6**************************************************/
/**********************************PA6**************************************************/
/**********************************PA6**************************************************/


void My_GCanvas::explode_contour(const GContour ori_ctrs[], GContour* dst_ctr, int count, int& dst_count ,float width, float limit){
	for(int i = 0; i < count; ++i){
		stroke_contour(ori_ctrs[i], dst_ctr, dst_count, width, limit);
	}
}

void My_GCanvas::stroke_contour(GContour ori_ctr,GContour *dst_ctr, int &dst_count, float width, float limit){
	int count = ori_ctr.fCount;
	if(ori_ctr.fClosed){
		for(int i = 0; i < count-2; ++i){
			make_contour(ori_ctr.fPts[i],ori_ctr.fPts[i+1],ori_ctr.fPts[i+2],dst_ctr, width, dst_count, limit);
		}
		make_contour(ori_ctr.fPts[count-2], ori_ctr.fPts[count-1], ori_ctr.fPts[0], dst_ctr, width, dst_count, limit);
		make_contour(ori_ctr.fPts[count-1], ori_ctr.fPts[0], ori_ctr.fPts[1], dst_ctr, width, dst_count, limit);
	}
	else{
		for(int i = 0; i < count-2; ++i){
			make_contour(ori_ctr.fPts[i],ori_ctr.fPts[i+1],ori_ctr.fPts[i+2],dst_ctr, width, dst_count, limit);
		}
		//make contour for the last stroke 
		float rad = width/2;
		GPoint a = ori_ctr.fPts[count-2];
		GPoint b = ori_ctr.fPts[count-1];
		GPoint ab = GPoint::Make(b.fX - a.fX, b.fY - a.fY);
		float len_ab = sqrtf(pow(ab.fX,2)+pow(ab.fY,2));
		GPoint ab_prime = GPoint::Make(rad*ab.fX/len_ab,rad*ab.fY/len_ab);
		GPoint cw_ab_prime = GPoint::Make(-ab_prime.y(),ab_prime.x());
		GPoint ccw_ab_prime = GPoint::Make(ab_prime.y(),-ab_prime.x());

		GPoint a_first = GPoint::Make(a.fX + cw_ab_prime.fX, a.fY + cw_ab_prime.fY );
		GPoint a_second = GPoint::Make(a.fX + ccw_ab_prime.fX, a.fY + ccw_ab_prime.fY );
		GPoint a_third = GPoint::Make(b.fX + ccw_ab_prime.fX, b.fY + ccw_ab_prime.fY );
		GPoint a_fourth = GPoint::Make(b.fX + cw_ab_prime.fX, b.fY + cw_ab_prime.fY );
		GContour last_ctr;
		last_ctr.fCount = 4;
		GPoint * last_ctr_tmp_pts = new GPoint[4];
		last_ctr_tmp_pts[0] = a_first; last_ctr_tmp_pts[1] = a_second;
		last_ctr_tmp_pts[2] = a_third; last_ctr_tmp_pts[3] = a_fourth;
		last_ctr.fPts = last_ctr_tmp_pts;

		last_ctr.fClosed = false;
		dst_ctr[dst_count] = last_ctr;
		dst_count++;
		
		//make cap for the last stroke

		GPoint extended = GPoint::Make(b.fX + ab_prime.fX, b.fY + ab_prime.fY);
		GPoint last_cap_first = a_fourth;
		GPoint last_cap_second = a_third;
		GPoint last_cap_third = GPoint::Make(extended.fX + ccw_ab_prime.fX, extended.fY + ccw_ab_prime.fY);
		GPoint last_cap_fourth = GPoint::Make(extended.fX + cw_ab_prime.fX, extended.fY + cw_ab_prime.fY );
		GContour last_cap;
		last_cap.fCount = 4;

		GPoint * last_cap_tmp_pts = new GPoint[4];
		last_cap_tmp_pts[0] = last_cap_first; last_cap_tmp_pts[1] = last_cap_second;
		last_cap_tmp_pts[2] = last_cap_third; last_cap_tmp_pts[3] = last_cap_fourth;
		last_cap.fPts = last_cap_tmp_pts;

		last_cap.fClosed = false;
		dst_ctr[dst_count] = last_cap;
		dst_count ++;

		//make cap for the first stroke

		GPoint first_a  = ori_ctr.fPts[0];
		GPoint first_b  = ori_ctr.fPts[1];
		GPoint first_ab = GPoint::Make(first_b.fX - first_a.fX, first_b.fY - first_a.fY);
		float len_first_ab = sqrtf(pow(first_ab.fX,2)+pow(first_ab.fY,2));
		GPoint first_ab_prime = GPoint::Make(rad*first_ab.fX/len_first_ab,rad*first_ab.fY/len_first_ab);
		GPoint cw_first_ab_prime = GPoint::Make(-first_ab_prime.y(),first_ab_prime.x());
		GPoint ccw_first_ab_prime = GPoint::Make(first_ab_prime.y(),-first_ab_prime.x());
		GPoint first_extended = GPoint::Make(first_a.fX - first_ab_prime.fX, first_a.fY - first_ab_prime.fY);
		GPoint first_cap_first = GPoint::Make(first_extended.fX + cw_first_ab_prime.fX, first_extended.fY + cw_first_ab_prime.fY);
		GPoint first_cap_second = GPoint::Make(first_extended.fX + ccw_first_ab_prime.fX, first_extended.fY + ccw_first_ab_prime.fY);
		GPoint first_cap_third = GPoint::Make(first_a.fX + ccw_first_ab_prime.fX, first_a.fY + ccw_first_ab_prime.fY);
		GPoint first_cap_fourth = GPoint::Make(first_a.fX + cw_first_ab_prime.fX, first_a.fY + cw_first_ab_prime.fY);
		GContour first_cap;
		first_cap.fCount = 4;

		GPoint * first_cap_tmp_pts = new GPoint[4];
		first_cap_tmp_pts[0] = first_cap_first; first_cap_tmp_pts[1] = first_cap_second;
		first_cap_tmp_pts[2] = first_cap_third; first_cap_tmp_pts[3] = first_cap_fourth;
		first_cap.fPts = first_cap_tmp_pts;

		first_cap.fClosed = false;
		dst_ctr[dst_count] = first_cap;
		dst_count ++;
	}
}

void My_GCanvas::make_contour(GPoint a, GPoint b, GPoint c,GContour *dst_ctr ,float width, int& dst_count, float limit){
	GPoint ab = GPoint::Make(b.fX - a.fX, b.fY - a.fY);
	GPoint bc = GPoint::Make(c.fX - b.fX, c.fY - b.fY);
	float len_ab = sqrtf(pow(ab.fX,2)+pow(ab.fY,2));
	float len_bc = sqrtf(pow(bc.fX,2)+pow(bc.fY,2));
	float rad = width/2;
	GPoint ab_prime = GPoint::Make(rad*ab.fX/len_ab,rad*ab.fY/len_ab);
	GPoint bc_prime = GPoint::Make(rad*bc.fX/len_bc,rad*bc.fY/len_bc);

	GPoint cw_ab_prime = GPoint::Make(-ab_prime.y(),ab_prime.x());
	GPoint cw_bc_prime = GPoint::Make(-bc_prime.y(),bc_prime.x());
	GPoint ccw_ab_prime = GPoint::Make(ab_prime.y(),-ab_prime.x());
	GPoint ccw_bc_prime = GPoint::Make(bc_prime.y(),-bc_prime.x());

	GPoint a_first = GPoint::Make(a.fX + cw_ab_prime.fX, a.fY + cw_ab_prime.fY );
	GPoint a_second = GPoint::Make(a.fX + ccw_ab_prime.fX, a.fY + ccw_ab_prime.fY );
	GPoint a_third = GPoint::Make(b.fX + ccw_ab_prime.fX, b.fY + ccw_ab_prime.fY );
	GPoint a_fourth = GPoint::Make(b.fX + cw_ab_prime.fX, b.fY + cw_ab_prime.fY );
	GContour first_ctr;
	first_ctr.fCount = 4;

	GPoint* first_ctr_tmp_pts = new GPoint[4];
	first_ctr_tmp_pts[0] = a_first; first_ctr_tmp_pts[1] = a_second;
	first_ctr_tmp_pts[2] = a_third; first_ctr_tmp_pts[3] = a_fourth;
	first_ctr.fPts = first_ctr_tmp_pts;
	first_ctr.fClosed = false;
	dst_ctr[dst_count] = first_ctr;
	dst_count++;

	GPoint unit_ab = GPoint::Make(ab.fX/len_ab, ab.fY/len_ab);
	GPoint unit_bc = GPoint::Make(bc.fX/len_bc , bc.fY/len_bc);
	GPoint unit_ba = GPoint::Make(-ab.fX/len_ab, -ab.fY/len_ab);
	//when sin_theta is negative, both use ccw
	//when sin_theta is positive both use cw
	float sin_theta = unit_ba.fX * unit_bc.fY - unit_ba.fY * unit_bc.fX;
	float d = sqrtf(2/(1 - ab.fX*bc.fX - ab.fY * bc.fY));

	if(sin_theta >0){
		//u and v are norm vectors
		GPoint v = GPoint::Make(-ab.fY/len_ab, ab.fX/len_ab);
		GPoint u = GPoint::Make(-bc.fY/len_bc , bc.fX/len_bc);

		float cos_theta = unit_ba.fX * unit_bc.fX + unit_ba.fY * unit_bc.fY;
		float bp_len = rad*sqrtf(2/(1-cos_theta));
		GPoint poly_fourth = GPoint::Make(b.fX + cw_bc_prime.fX, b.fY + cw_bc_prime.fY);
		if(d>limit){
			GContour tri_ctr;
			tri_ctr.fCount = 3;

			GPoint * tri_ctr_tmp_pts = new GPoint[3];
			tri_ctr_tmp_pts[0] = a_fourth; tri_ctr_tmp_pts[1] = b;
			tri_ctr_tmp_pts[2] = poly_fourth;
			tri_ctr.fPts = tri_ctr_tmp_pts;

			dst_ctr[dst_count] = tri_ctr;
			dst_count++;
		}
		else{
			GPoint bp_direc = GPoint::Make( u.fX + v.fX, u.fY + v.fY );
			float bp_direc_len = sqrtf(pow(bp_direc.fX,2)+pow(bp_direc.fY,2));
			GPoint unit_bp_direc = GPoint::Make(bp_direc.fX/bp_direc_len,bp_direc.fY/bp_direc_len);
			GPoint bp_vec = GPoint::Make(unit_bp_direc.fX * bp_len, unit_bp_direc.fY * bp_len);
			GPoint p = GPoint::Make(b.fX + bp_vec.fX, b.fY + bp_vec.fY);
			GContour fill_poly;
			fill_poly.fCount = 4;

			GPoint * fill_poly_tmp_pts = new GPoint[4];
			fill_poly_tmp_pts[0] = b; fill_poly_tmp_pts[1] = poly_fourth;
			fill_poly_tmp_pts[2] = p; fill_poly_tmp_pts[3] = a_fourth;
			fill_poly.fPts = fill_poly_tmp_pts;

			dst_ctr[dst_count] = fill_poly;
			dst_count++;
		}
	}
	else if(sin_theta<0){
		GPoint u = GPoint::Make(ab.fY/len_ab, -ab.fX/len_ab);
		GPoint v = GPoint::Make(bc.fY/len_bc , -bc.fX/len_bc);
		float cos_theta = unit_ba.fX * unit_bc.fX + unit_ba.fY * unit_bc.fY;
		float bp_len = rad*sqrt(2/(1 - cos_theta));
		GPoint poly_fourth = GPoint::Make(b.fX + ccw_bc_prime.fX, b.fY + ccw_bc_prime.fY);
		if(d>limit){
			GContour tri_ctr;
			tri_ctr.fCount = 3;

			GPoint * tri_ctr_tmp_pts = new GPoint[3];
			tri_ctr_tmp_pts[0] = b; tri_ctr_tmp_pts[1] = a_third;
			tri_ctr_tmp_pts[2] = poly_fourth;
			tri_ctr.fPts = tri_ctr_tmp_pts;

			dst_ctr[dst_count] = tri_ctr;
			dst_count++;
		}
		else{
			GPoint bp_direc = GPoint::Make( u.fX + v.fX, u.fY + v.fY );
			float bp_direc_len = sqrtf(pow(bp_direc.fX,2)+pow(bp_direc.fY,2));
			GPoint unit_bp_direc = GPoint::Make(bp_direc.fX/bp_direc_len,bp_direc.fY/bp_direc_len);
			GPoint bp_vec = GPoint::Make(unit_bp_direc.fX * bp_len, unit_bp_direc.fY * bp_len);
			GPoint p = GPoint::Make(b.fX + bp_vec.fX, b.fY + bp_vec.fY);
			GContour fill_poly;
			fill_poly.fCount = 4;

			GPoint * fill_poly_tmp_pts = new GPoint[4];
			fill_poly_tmp_pts[0] = b; fill_poly_tmp_pts[1] = a_third;
			fill_poly_tmp_pts[2] = p; fill_poly_tmp_pts[3] = poly_fourth;
			fill_poly.fPts = fill_poly_tmp_pts;


			dst_ctr[dst_count] = fill_poly;
			dst_count++;
		}
	}
	else{
		return;
	}



}
/**********************************PA6**************************************************/
/**********************************PA6**************************************************/
/**********************************PA6**************************************************/
/**********************************PA7**************************************************/
/**********************************PA7**************************************************/
/**********************************PA7**************************************************/
void My_GCanvas::drawMesh(int triCount, const GPoint pts[], const int indices[],
 const GColor colors[], const GPoint tex[], const GPaint& paint){
	//construct ctrs
	tri triangles[triCount];
	if(indices){
		for(int i = 0; i<triCount;++i){
			triangles[i].vertices[0] = pts[indices[i*3]];
			triangles[i].vertices[1] = pts[indices[i*3+1]];
			triangles[i].vertices[2] = pts[indices[i*3+2]];
		}
	}
	else{
		for(int i = 0; i<triCount; ++i){
			triangles[i].vertices[0] = pts[i*3];
			triangles[i].vertices[1] = pts[i*3+1];
			triangles[i].vertices[2] = pts[i*3+2];
		}
	}
	if((colors)&&(tex)){
		GColor c0,c1,c2;
		for(int i = 0; i < triCount; ++i){
			tri curr_tri = triangles[i];
			GPoint pt0 = curr_tri.vertices[0];
			GPoint pt1 = curr_tri.vertices[1];
			GPoint pt2 = curr_tri.vertices[2];
			GPoint tex0;
			GPoint tex1;
			GPoint tex2;
			if (indices){
				c0 = colors[indices[i*3]];
				c1 = colors[indices[i*3+1]];
				c2 = colors[indices[i*3+2]];
				tex0 = tex[indices[i*3]];
				tex1 = tex[indices[i*3+1]];
				tex2 = tex[indices[i*3+2]];
			}
			else{
				c0 = colors[i*3];
				c1 = colors[i*3+1];
				c2 = colors[i*3+2];
				tex0 = tex[i*3];
				tex1 = tex[i*3+1];
				tex2 = tex[i*3+2];
			}
			TriColorShader color_shader(pt0,pt1,pt2,c0,c1,c2);
			float del_p1x = pt1.x()-pt0.x();
			float del_p1y = pt1.y()-pt0.y();
			float del_p2x = pt2.x()-pt0.x();
			float del_p2y = pt2.y()-pt0.y();
			GMatrix pts_matrix = GMatrix(del_p1x, del_p2x, pt0.x(), del_p1y, del_p2y, pt0.y());
			float del_tex1x = tex1.x()-tex0.x();
			float del_tex1y = tex1.y()-tex0.y();
			float del_tex2x = tex2.x()-tex0.x();
			float del_tex2y = tex2.y()-tex0.y();
			GMatrix tex_matrix = GMatrix(del_tex1x, del_tex2x, tex0.x(), del_tex1y, del_tex2y, tex0.y());
			GMatrix tmp_matrix;
			GMatrix invert_tex;
			tex_matrix.invert(&invert_tex);
			tmp_matrix.setConcat(pts_matrix,invert_tex);
			ProxyShader tex_shader(paint.getShader(),tmp_matrix);
			CompositeShader composite_shader(&color_shader,&tex_shader);
			GPaint new_paint(&composite_shader);
			drawConvexPolygon(curr_tri.vertices,3,new_paint);
		}
	}
	else if(colors){
		GColor c0,c1,c2;
		for(int i = 0; i < triCount; ++i){
			tri curr_tri = triangles[i];
			GPoint pt0 = curr_tri.vertices[0];
			GPoint pt1 = curr_tri.vertices[1];
			GPoint pt2 = curr_tri.vertices[2];
			if (indices){
				c0 = colors[indices[i*3]];
				c1 = colors[indices[i*3+1]];
				c2 = colors[indices[i*3+2]];
			}
			else{
				c0 = colors[i*3];
				c1 = colors[i*3+1];
				c2 = colors[i*3+2];
			}
			TriColorShader new_shader(pt0,pt1,pt2,c0,c1,c2);
			GPaint new_paint(&new_shader);
			drawConvexPolygon(curr_tri.vertices, 3, new_paint);
		}
	}
	else{
		for(int i = 0; i<triCount;++i){
			tri curr_tri = triangles[i];
			GPoint pt0 = curr_tri.vertices[0];
			GPoint pt1 = curr_tri.vertices[1];
			GPoint pt2 = curr_tri.vertices[2];
			GPoint tex0;
			GPoint tex1;
			GPoint tex2;
			if (indices){
				tex0 = tex[indices[i*3]];
				tex1 = tex[indices[i*3+1]];
				tex2 = tex[indices[i*3+2]];
			}
			else{
				tex0 = tex[i*3];
				tex1 = tex[i*3+1];
				tex2 = tex[i*3+2];
			}
			float del_p1x = pt1.x()-pt0.x();
			float del_p1y = pt1.y()-pt0.y();
			float del_p2x = pt2.x()-pt0.x();
			float del_p2y = pt2.y()-pt0.y();
			GMatrix pts_matrix = GMatrix(del_p1x, del_p2x, pt0.x(), del_p1y, del_p2y, pt0.y());
			float del_tex1x = tex1.x()-tex0.x();
			float del_tex1y = tex1.y()-tex0.y();
			float del_tex2x = tex2.x()-tex0.x();
			float del_tex2y = tex2.y()-tex0.y();
			GMatrix tex_matrix = GMatrix(del_tex1x, del_tex2x, tex0.x(), del_tex1y, del_tex2y, tex0.y());
			GMatrix tmp_matrix;
			GMatrix invert_tex;
			tex_matrix.invert(&invert_tex);
			tmp_matrix.setConcat(pts_matrix,invert_tex);
			ProxyShader proxy_shader(paint.getShader(),tmp_matrix);
			GPaint new_paint(&proxy_shader);
			drawConvexPolygon(curr_tri.vertices,3,new_paint);
		}
	}
}
/**********************************PA7**************************************************/
/**********************************PA7**************************************************/
/**********************************PA7**************************************************/
void My_GCanvas::scan_line_shader(float x_left, float x_right,int curr_y, const GPaint& paint){
	// pay attention to the center error
	int x_int_left = GRoundToInt(x_left);
	int x_int_right = GRoundToInt(x_right);
	int x_start = std::min(x_int_left,x_int_right);
	x_start = std::max(x_start,0);
	int x_end = std::max(x_int_left,x_int_right);
	x_end = std::min(x_end,bitmap.width());

	int pixel_num = x_end - x_start;

	GPixel row[bitmap.width()];
	paint.getShader()->setContext(my_CTM,paint.getAlpha());
	paint.getShader()->shadeRow(x_start,curr_y,pixel_num,row);
	int i = 0;
	for(int x = x_start; x < x_end; ++x){
		GPixel *dst = bitmap.getAddr(x,curr_y);
		*dst = blend_src_dst(row[i],*dst);
		i++;
	}
}

void My_GCanvas::scan_line_shader_color(float x_left, float x_right,int curr_y, const GColor& src_color){
	int x_int_left =GRoundToInt(x_left);
	int x_int_right = GRoundToInt(x_right);
	int x_start = std::min(x_int_left,x_int_right);
	x_start = std::max(x_start,0);
	int x_end = std::max(x_int_left,x_int_right);
	x_end = std::min(x_end,bitmap.width());
	GPixel src_pixel = premulPixel(src_color);
	for(int x = x_start; x < x_end; x++){
		GPixel *dst = bitmap.getAddr(x,curr_y);
		if(src_color.fA==1){
			*dst = src_pixel;
		}
		else{
			*dst = blend_src_dst(src_pixel, * dst);
		}
	}
}

