/*
 * feed_viewer.h
 *
 *     Authors: Gregory Kramida
 *     License: Apache v. 2
 *   Copyright: (c) Gregory Kramida 2015 
 */

#ifndef RECO_DATAPIPE_SRC_FEED_VIEWER_H_
#define RECO_DATAPIPE_SRC_FEED_VIEWER_H_
#pragma once

//Qt
#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>

//datapipe
#include <reco/datapipe/video_widget.h>
#include <reco/datapipe/freenect2_pipe.h>

namespace reco {
namespace datapipe {

class multichannel_viewer: public QWidget {
	Q_OBJECT
private:
	QLayout* layout = new QVBoxLayout();
	QLabel* no_source_connected_label = new QLabel();
	std::shared_ptr<freenect2_pipe> pipe;
	freenect2_pipe::buffer_type buffer;
	std::vector<std::tuple<int,datapipe::video_widget*>> video_widgets;
	void add_video_widget(int ix_feed);
public:
	static const float depth_inv_factor;
	enum feed_type{
		RGB = 0,
		DEPTH = 1,
		ALL = 2,
	};
	/**
	 * @brief primary constructor
	 * @param window_title what to set the title of the window is this widget is shown in a separate window
	 * @param parent the widget's parent widget
	 */
	multichannel_viewer(QString window_title = "Feed Viewer", QWidget* parent = NULL);

	virtual ~multichannel_viewer();
	/**
	 * @brief Hooks current object to show feed from the desired pipe
	 * @param pipe the kinect v2 rgb/depth pipe to hook to
	 */
	void hook_to_pipe(std::shared_ptr<freenect2_pipe> pipe, feed_type type);
	/**
	 * @brief Unhooks current object from the previously-connected pipe
	 */
	void unhook_from_pipe();
public slots:
	/**
	 * @brief triggered when a new frame becomes available.
	 * @param images
	 */
	void on_frame(std::shared_ptr<hal::ImageArray> images);

};

} /* namespace datapipe */
} /* namespace reco */

#endif /* DATAPIPE_SRC_FEED_VIEWER_H_ */