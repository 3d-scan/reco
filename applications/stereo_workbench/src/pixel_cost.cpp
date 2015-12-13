/*
 * pixel_cost.cpp
 *
 *  Created on: Dec 10, 2015
 *      Author: Gregory Kramida
 *   Copyright: 2015 Gregory Kramida
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 */
#include "pixel_cost.hpp"
#include <opencv2/xfeatures2d.hpp>
#include <opencv2/hal/intrin.hpp>
#include <reco/utils/debug_util.h>

namespace reco{
namespace stereo_workbench{
using namespace cv;

int max = 0;
void calculate_row_cost_L2( const Mat& img1, const Mat& img2, int y,
        int minD, int maxD, CostType* cost,
        PixType* buffer, const PixType* lookup_table,
        int tabOfs, int){
	int image_width = img1.cols; int channel_number = img1.channels();
	const int minX1 = std::max(maxD, 0), maxX1 = image_width + std::min(minD, 0);
	int D = maxD - minD;
	//under regular circumstances, from maxD up to image_width
	if(channel_number == 1){
		for( int x = minX1; x < maxX1; x++){
			PixType px2 = img2.at<PixType>(y,x);
			for( int d = minD; d < maxD; d++ ){
				PixType px1 = img1.at<PixType>(y,x-d);
				cost[x*D + d] = (CostType)(std::abs(px1 - px2));
			}
		}
	}else{
		for( int x = minX1; x < maxX1; x++){
			cv::Vec3b px1 = img1.at<cv::Vec3b>(y,x);
			for( int d = minD; d < maxD; d++ ){
				cv::Vec3b px2 = img2.at<cv::Vec3b>(y,x-d);
				double norm = cv::norm(px1,px2,cv::NormTypes::NORM_L2);
				cost[x*D + d] = (CostType)(norm*0.579614388);
			}
		}
	}
}
/*
 For each pixel row1[x], max(maxD, 0) <= minX <= x < maxX <= width - max(0, -minD),
 and for each disparity minD<=d<maxD the function
 computes the cost (cost[(x-minX)*(maxD - minD) + (d - minD)]), depending on the difference between
 row1[x] and row2[x-d].

 the temporary buffer should contain width2*2 elements
 */
void calculate_row_cost_DAISY( const Mat& img1, const Mat& img2, int y,
                            int minD, int maxD, CostType* cost,
                            PixType* buffer, const PixType* lookup_table,
                            int tabOfs, int )
{

    int image_width = 921;
    const int minX1 = std::max(maxD, 0), maxX1 = image_width + std::min(minD, 0);
    int D = maxD - minD;
    const int y_offset = y*image_width;
    const double norm_factor = 38.0D;

	//under regular circumstances, from maxD up to image_width

	for( int x = y_offset + minX1, xd = minX1; x < y_offset + maxX1; x++, xd++ ){
		Mat desc1 = img1.row(x);
		for( int d = minD; d < maxD; d++ ){
			Mat desc2 = img2.row(x - d);
			double nrm = cv::norm(desc1,desc2,cv::NormTypes::NORM_L2);
			if(nrm*norm_factor > max){
				max = nrm*norm_factor;
				dpt(max << " : " << nrm);
			}
			cost[xd*D + d] = (CostType)(nrm * norm_factor);
		}
	}
}



/*
 For each pixel row1[x], max(maxD, 0) <= minX <= x < maxX <= width - max(0, -minD),
 and for each disparity minD<=d<maxD the function
 computes the cost (cost[(x-minX)*(maxD - minD) + (d - minD)]), depending on the difference between
 row1[x] and row2[x-d]. The subpixel algorithm from
 "Depth Discontinuities by Pixel-to-Pixel Stereo" by Stan Birchfield and C. Tomasi
 is used, hence the suffix BT.

 the temporary buffer should contain width2*2 elements
 */
void calculate_row_cost_BT( const Mat& img1, const Mat& img2, int y,
                            int minD, int maxD, CostType* cost,
                            PixType* buffer, const PixType* lookup_table,
                            int tabOfs, int ){
	int x, c, image_width = img1.cols, channel_number = img1.channels();
	int minX1 = std::max(maxD, 0), maxX1 = image_width + std::min(minD, 0);
	//under regular circumstances, 0		//under regular circumstances, image_width
	int minX2 = std::max(minX1 - maxD, 0), maxX2 = std::min(maxX1 - minD, image_width);
	int D = maxD - minD;
	int width1 = maxX1 - minX1; //under regular circumstances, image_width - maxD
	int width2 = maxX2 - minX2; //under regular circumstances, image_width

	const PixType* row1 = img1.ptr<PixType>(y);
	const PixType* row2 = img2.ptr<PixType>(y);

	//TODO why is this offset necessary?
	PixType* precomputed_buffer_img1 = buffer + width2*2;
	PixType* precomputed_buffer_img2 = precomputed_buffer_img1 + image_width*channel_number*2;

	//const int TAB_OFS = 256*4, TAB_SIZE = 256 + TAB_OFS*2;
	//TODO Why not precompute this in main loop? tabOfs passed in is always the same.
	lookup_table += tabOfs;

	for( c = 0; c < channel_number*2; c++ ){
		precomputed_buffer_img1[image_width*c] = precomputed_buffer_img1[image_width*c + image_width-1] =
		precomputed_buffer_img2[image_width*c] = precomputed_buffer_img2[image_width*c + image_width-1] = lookup_table[0];
	}

	//TODO: aren't prev1 == prev2 and next1 == next2, since images always have the same dimensions and,
	//therefore, step?
	int prev1 = y > 0 ? -(int)img1.step : 0;
	int prev2 = y > 0 ? -(int)img2.step : 0;
	int next1 = y < img1.rows-1 ? (int)img1.step : 0;
	int next2 = y < img2.rows-1 ? (int)img2.step : 0;



	if( channel_number == 1 ){
		for( x = 1; x < image_width-1; x++ ){
			//image 1
			precomputed_buffer_img1[x] =
					lookup_table[(row1[x+1] - row1[x-1])*2 + //current pixel surround
								  row1[x+prev1+1] - row1[x+prev1-1] + //above pixel surround
								  row1[x+next1+1] - row1[x+next1-1]]; //below pixel surround

			precomputed_buffer_img1[x+image_width] = row1[x]; //current pixel value
			//image 2
			//fill from rightmost postion to left
			precomputed_buffer_img2[image_width-1-x] =
					lookup_table[(row2[x+1] - row2[x-1])*2 +
								  row2[x+prev2+1] - row2[x+prev2-1] +
								  row2[x+next2+1] - row2[x+next2-1]];
			precomputed_buffer_img2[image_width-1-x+image_width] = row2[x]; //current pixel value
		}
	}else{
		//assume 3-channel image
		for( x = 1; x < image_width-1; x++ ){

			//image 1
			precomputed_buffer_img1[x] = //ch1
					lookup_table[(row1[x*3+3] - row1[x*3-3])*2 +
								 row1[x*3+prev1+3] - row1[x*3+prev1-3] +
								 row1[x*3+next1+3] - row1[x*3+next1-3]];
			precomputed_buffer_img1[x+image_width] = //ch2
					lookup_table[(row1[x*3+4] - row1[x*3-2])*2 +
								 row1[x*3+prev1+4] - row1[x*3+prev1-2] +
								 row1[x*3+next1+4] - row1[x*3+next1-2]];
			precomputed_buffer_img1[x+image_width*2] = //ch3
					lookup_table[(row1[x*3+5] - row1[x*3-1])*2 +
								 row1[x*3+prev1+5] - row1[x*3+prev1-1] +
								 row1[x*3+next1+5] - row1[x*3+next1-1]];

			precomputed_buffer_img1[x+image_width*3] = row1[x*3];
			precomputed_buffer_img1[x+image_width*4] = row1[x*3+1];
			precomputed_buffer_img1[x+image_width*5] = row1[x*3+2];

			//image2
			precomputed_buffer_img2[image_width-1-x] =
					lookup_table[(row2[x*3+3] - row2[x*3-3])*2 + row2[x*3+prev2+3] -
								 row2[x*3+prev2-3] + row2[x*3+next2+3] - row2[x*3+next2-3]];
			precomputed_buffer_img2[image_width-1-x+image_width] =
					lookup_table[(row2[x*3+4] - row2[x*3-2])*2 + row2[x*3+prev2+4] -
								 row2[x*3+prev2-2] + row2[x*3+next2+4] - row2[x*3+next2-2]];
			precomputed_buffer_img2[image_width-1-x+image_width*2] =
					lookup_table[(row2[x*3+5] - row2[x*3-1])*2 + row2[x*3+prev2+5] -
								 row2[x*3+prev2-1] + row2[x*3+next2+5] - row2[x*3+next2-1]];
			precomputed_buffer_img2[image_width-1-x+image_width*3] = row2[x*3];
			precomputed_buffer_img2[image_width-1-x+image_width*4] = row2[x*3+1];
			precomputed_buffer_img2[image_width-1-x+image_width*5] = row2[x*3+2];
		}
	}

	memset( cost, 0, width1*D*sizeof(cost[0]) );

	buffer -= minX2;
	cost -= minX1*D + minD; // simplify the cost indices inside the loop

	//for each channel of each image
	//v refers to img2, u refers to img1
	for( c = 0; c < channel_number*2; c++,
		//go to next buffer row
		precomputed_buffer_img1 += image_width, precomputed_buffer_img2 += image_width ){
		int diff_scale = c < channel_number ? 0 : 2;

		// precompute
		//   v0 = min(row2[x-1/2], row2[x], row2[x+1/2]) and
		//   v1 = max(row2[x-1/2], row2[x], row2[x+1/2]) and
		//normally, traverses whole image_width
		for( x = minX2; x < maxX2; x++ ){
			int v = precomputed_buffer_img2[x];//this is in reverse order
			int vl = x > 0 ? (v + precomputed_buffer_img2[x-1])/2 : v;
			int vr = x < image_width-1 ? (v + precomputed_buffer_img2[x+1])/2 : v;
			int v0 = std::min(vl, vr); v0 = std::min(v0, v);
			int v1 = std::max(vl, vr); v1 = std::max(v1, v);
			buffer[x] = (PixType)v0;
			buffer[x + width2] = (PixType)v1;
		}

		//under regular circumstances, from maxD up to image_width
		for( x = minX1; x < maxX1; x++ ){
			int u = precomputed_buffer_img1[x];
			int ul = x > 0 ? (u + precomputed_buffer_img1[x-1])/2 : u;
			int ur = x < image_width-1 ? (u + precomputed_buffer_img1[x+1])/2 : u;
			int u0 = std::min(ul, ur); u0 = std::min(u0, u);
			int u1 = std::max(ul, ur); u1 = std::max(u1, u);
		#if 0
		//#if CV_SIMD128
			v_uint8x16 _u  = v_setall_u8((uchar)u), _u0 = v_setall_u8((uchar)u0);
			v_uint8x16 _u1 = v_setall_u8((uchar)u1);

			for( int d = minD; d < maxD; d += 16 ){
				v_uint8x16 _v  = v_load(precomputed_buffer_img2  + image_width-x-1 + d);
				v_uint8x16 _v0 = v_load(buffer + image_width-x-1 + d);
				v_uint8x16 _v1 = v_load(buffer + image_width-x-1 + d + width2);
				v_uint8x16 c0 = v_max(_u - _v1, _v0 - _u);
				v_uint8x16 c1 = v_max(_v - _u1, _u0 - _v);
				v_uint8x16 diff = v_min(c0, c1);

				v_int16x8 _c0 = v_load_aligned(cost + x*D + d);
				v_int16x8 _c1 = v_load_aligned(cost + x*D + d + 8);

				v_uint16x8 diff1,diff2;
				v_expand(diff,diff1,diff2);
				v_store_aligned(cost + x*D + d,     _c0 + v_reinterpret_as_s16(diff1 >> diff_scale));
				v_store_aligned(cost + x*D + d + 8, _c1 + v_reinterpret_as_s16(diff2 >> diff_scale));
			}
		#else
			for( int d = minD; d < maxD; d++ ){

				int v = precomputed_buffer_img2[image_width-x-1 + d];
				int v0 = buffer[image_width-x-1 + d];
				int v1 = buffer[image_width-x-1 + d + width2];
				int c0 = std::max(0, u - v1); c0 = std::max(c0, v0 - u);
				int c1 = std::max(0, v - u1); c1 = std::max(c1, u0 - v);
				CostType new_cost = (CostType)(cost[x*D+d] + (std::min(c0, c1) >> diff_scale));
				cost[x*D + d] = new_cost;


			}
		#endif
		}
	}
}



void precompute_nothing(const Mat& img1, const Mat& img2, Mat& out_img1, Mat& out_img2){
	out_img1 = img1;
	out_img2 = img2;
//	img1.copyTo(out_img1);
//	img2.copyTo(out_img2);
}
void precompute_DAISY(const Mat& img1, const Mat& img2, Mat& out_img1, Mat& out_img2){
	//descriptor size = (3*8+1)*8 = 200
	Ptr<xfeatures2d::DAISY> daisy = cv::xfeatures2d::DAISY::create(15,3,8,8,cv::xfeatures2d::DAISY::NRM_PARTIAL,cv::noArray(),true,false);
	daisy->compute(img1,out_img1);
	daisy->compute(img2,out_img2);
}
}//stereo_workbench
}//reco