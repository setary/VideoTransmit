#include <vector>

#include "subscriber.h"


VideoSubscriber::VideoSubscriber() {

}

VideoSubscriber::~VideoSubscriber() {
  disable();
}

bool VideoSubscriber::enable() {
  participant_ = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  if (participant_ < 0) {
    printf("dds_create_participant: %s\n", dds_strretcode(-participant_));
    return false;
  }

  topic_ = dds_create_topic(participant_, &video_Frame_desc, "video_Frame", NULL, NULL);
  if (topic_ < 0) {
    printf("dds_create_topic: %s\n", dds_strretcode(-topic_));
    return false;
  }

  dds_qos_t *qos = dds_create_qos();
  dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, DDS_SECS(10));
  reader_ = dds_create_reader (participant_, topic_, qos, NULL);
  if (reader_ < 0) {
    dds_delete_qos(qos);
    printf("dds_create_reader: %s\n", dds_strretcode(-reader_));
    return false;
  }
  dds_delete_qos(qos);

  return true;
}

bool VideoSubscriber::disable() {
  dds_return_t rc = dds_delete (participant_);
  if (rc != DDS_RETCODE_OK) {
    printf("dds_delete: %s\n", dds_strretcode(-rc));
    return false;
  }
  return true;
}

void VideoSubscriber::decode() {
  std::vector<char> frame(frame_->frame_bytes._buffer, frame_->frame_bytes._buffer + frame_->frame_bytes._length);
  cv::Mat img = cv::imdecode(cv::Mat(frame), CV_LOAD_IMAGE_COLOR); // decode
  cv::imshow("image", img);
  cv::waitKey(30);

  // image write
  char name[64];
  sprintf(name, "decode_image.ipg");
  std::vector<int> quality;
  quality[0] = cv::IMWRITE_JPEG_QUALITY;
  quality[1] = 50;
  imwrite(name, img, quality);
}

void VideoSubscriber::run() {
  void *samples[1];
  samples[0] = video_Frame__alloc();
  dds_sample_info_t infos[1];
  while (true) {
    dds_return_t rc = dds_read(reader_, samples, infos, 1, 1);
    if (rc < 0) {
      printf("dds_read: %s\n", dds_strretcode(-rc));
    }

    if ((rc > 0) && (infos[0].valid_data)) {
      frame_ = (video_Frame*) samples[0];
      decode();
    } else {
      dds_sleepfor (DDS_MSECS(100));
    }
  }
  video_Frame_free(samples[0], DDS_FREE_ALL);
}
