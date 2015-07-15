/*
 * main_window.h
 *
 *  Created on: Dec 17, 2014
 *      Author: Gregory Kramida
 *     License: Apache v2
 *   Copyright: (c) Gregory Kramida 2014
 */

#ifndef RECO_WORKBENCH_MAIN_WINDOW_H_
#define RECO_WORKBENCH_MAIN_WINDOW_H_

#pragma once

//Qt
#include <QMainWindow>

//datapipe
#include <reco/datapipe/video_widget.h>
#include <reco/datapipe/webcam_video_source.h>
#include <reco/datapipe/image_file_video_source.h>
#include <reco/datapipe/freenect2_pipe.h>
#include <reco/datapipe/multi_kinect_rgb_viewer.h>
#include <reco/datapipe/multi_kinect_depth_viewer.h>

//utils
#include <reco/utils/swap_buffer.h>

//OpenCV
#include <opencv2/core/core.hpp>

//pcl
#include <pcl/visualization/pcl_visualizer.h>

//std
#include <memory>
#include <vector>


class Ui_main_window;

namespace reco{
namespace workbench{

class main_window: public QMainWindow {
	Q_OBJECT
public:

	main_window();
	virtual ~main_window();
protected:
	//keep qt naming convention here (override)
	virtual void closeEvent(QCloseEvent* event);
private:
	Ui_main_window* ui;

	datapipe::multi_kinect_rgb_viewer rgb_viewer;
	datapipe::multi_kinect_depth_viewer depth_viewer;
	std::shared_ptr<pcl::visualization::PCLVisualizer> result_viewer;

	datapipe::freenect2_pipe::buffer_type buffer;
	std::shared_ptr<datapipe::freenect2_pipe> pipe;


	void connect_actions();
	void hook_pipe_signals();

private slots:
	void report_error(QString string);
	void open_kinect_devices();
	void open_hal_log();
	void open_image_folder();
	void display_feeds();
	void on_show_rgb_feed_button_clicked();
	void on_show_depth_feed_button_clicked();

};

}//end namespace workbench
} //end namespace reco

#endif /* HMD_MAIN_WINDOW_H_ */
