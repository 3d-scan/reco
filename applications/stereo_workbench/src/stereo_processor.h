/*
 * stereo_processor.h
 *
 *  Created on: Sep 28, 2015
 *      Author: Gregory Kramida
 *   Copyright: 2015 Gregory Kramida
 */
//TODO: 750 remove header guards globally
#pragma once

//qt
#include <QObject>
//datapipe
#include <reco/datapipe/typedefs.h>
//utils
#include <reco/utils/worker.h>
//opencv
#include <opencv2/core/core_c.h>
#include <opencv2/calib3d/calib3d.hpp>
//#include <opencv2/xfeatures2d/xfeatures2d.hpp>

//misc
#include <reco/misc/calibration_parameters.h>
//calibu
#include <calibu/Calibu.h>
//std
#include <mutex>

#define INHOUSE_RECTIFICATION

namespace reco {
namespace stereo_workbench {

class stereo_processor: public QObject, public utils::worker {

Q_OBJECT
public:
	stereo_processor(datapipe::frame_buffer_type input_frame_buffer,
			datapipe::frame_buffer_type output_frame_buffer,
			std::shared_ptr<calibu::Rigd>  calibration);
	virtual ~stereo_processor();

#if CV_VERSION_EPOCH == 2 || (!defined CV_VERSION_EPOCH && CV_VERSION_MAJOR == 2)
	cv::StereoSGBM stereo_matcher;
#elif CV_VERSION_MAJOR == 3
	cv::Ptr<cv::StereoSGBM> stereo_matcher;
#endif

	bool rectification_enabled;
	int get_v_offset();
	void set_calibration(std::shared_ptr<calibu::Rigd> calibration);

protected:
	virtual bool do_unit_of_work();
	virtual void pre_thread_join();
private:
	datapipe::frame_buffer_type input_frame_buffer;
	datapipe::frame_buffer_type output_frame_buffer;
	bool worker_shutting_down;
	int right_v_offset = 0;

	cv::Mat last_left;
	cv::Mat last_right;

	void compute_disparity(cv::Mat left, cv::Mat right);


#if CV_VERSION_MAJOR == 3
	void compute_disparity_daisy(cv::Mat left, cv::Mat right);
#endif

	std::shared_ptr<calibu::Rigd> calibration;

	calibu::LookupTable left_lut;
	calibu::LookupTable right_lut;
public slots:
	void set_minimum_disparity(int value);
	void set_num_disparities(int value);
	void set_window_size(int value);
	void set_p1(int value);
	void set_p2(int value);
	void set_pre_filter_cap(int value);
	void set_uniqueness_ratio(int value);
	void set_speckle_window_size(int value);
	void set_speckle_range(int value);
	void set_v_offset(int value);
	void save_current();


signals:
	void frame(std::shared_ptr<std::vector<cv::Mat>> images);

};

} /* namespace stereo_workbench */
} /* namespace reco */
