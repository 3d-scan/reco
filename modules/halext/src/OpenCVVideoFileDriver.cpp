/*
 * OpenCVVideoFileDriver.cpp
 *
 *  Created on: Sep 21, 2015
 *      Author: Gregory Kramida
 *   Copyright: 2015 Gregory Kramida
 */

#include <include/reco/halext/OpenCVVideoFileDriver.h>
#include <HAL/Utils/TicToc.h>

namespace reco {
namespace halext {


OpenCVVideoFileDriver::OpenCVVideoFileDriver(unsigned int cam_id, bool force_grey)
    : num_channels_(1),
      force_greyscale_(force_grey),
      cam_(cam_id) {
  init();
}

OpenCVVideoFileDriver::OpenCVVideoFileDriver(const std::string & path, bool force_grey):
		num_channels_(1),
		force_greyscale_(force_grey),
		cam_(path) {
  init();
}

OpenCVVideoFileDriver::~OpenCVVideoFileDriver() {}

void OpenCVVideoFileDriver::init(){
  if (!cam_.isOpened()) abort();

  img_width_ = cam_.get(CV_CAP_PROP_FRAME_WIDTH);
  img_height_ = cam_.get(CV_CAP_PROP_FRAME_HEIGHT);
}


bool OpenCVVideoFileDriver::Capture(hal::CameraMsg& images_msg) {
  if(!cam_.isOpened()) {
    std::cerr << "HAL: Error reading from camera." << std::endl;
    return false;
  }
  images_msg.set_device_time(Tic());

  cv::Mat temp;
  bool success = cam_.read(temp);
  hal::ImageMsg* pbImg = images_msg.add_image();
  pbImg->set_type(hal::PB_UNSIGNED_BYTE);
  pbImg->set_height(img_height_);
  pbImg->set_width(img_width_);
  pbImg->set_format(force_greyscale_ ? hal::PB_LUMINANCE : hal::PB_BGR);

  if (!success) return false;

  // This may not store the image in contiguous memory which PbMsgs
  // requires, so we might need to copy it
  cv::Mat cv_image;
  if(force_greyscale_) {
    cvtColor(temp, cv_image, CV_RGB2GRAY);
  } else if (!cv_image.isContinuous()) {
    temp.copyTo(cv_image);
  } else {
    cv_image = temp;
  }

  pbImg->set_data(static_cast<const unsigned char*>(cv_image.data),
                  cv_image.elemSize() * cv_image.total());
  return true;
}

size_t OpenCVVideoFileDriver::NumChannels() const {
  return num_channels_;
}

size_t OpenCVVideoFileDriver::Width(size_t /*idx*/) const {
  return img_width_;
}

size_t OpenCVVideoFileDriver::Height(size_t /*idx*/) const {
  return img_height_;
}

} /* namespace halext */
} /* namespace reco */