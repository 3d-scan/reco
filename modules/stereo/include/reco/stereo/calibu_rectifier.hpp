/*
 * calibu_rectifier.h
 *
 *  Created on: Oct 12, 2015
 *      Author: Gregory Kramida
 *   Copyright: 2015 Gregory Kramida
 */

#pragma once

#include <reco/stereo/rectifier.hpp>

namespace reco {
namespace stereo {

class calibu_rectifier:
		public rectifier {
public:
	calibu_rectifier();
	calibu_rectifier(std::shared_ptr<calibu::Rigd> calibration);
	virtual ~calibu_rectifier();
	virtual void set_calibration(std::shared_ptr<calibu::Rigd> calibration);
	virtual void rectify(const cv::Mat& left,const cv::Mat& right,
				cv::Mat& rect_left, cv::Mat& rect_right);
private:
	calibu::LookupTable left_lut;
	calibu::LookupTable right_lut;

};

} /* namespace stereo */
} /* namespace reco */
