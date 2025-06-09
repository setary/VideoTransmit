#include <pthread.h>
#include <vector>
#include <arpa/inet.h>
#include <algorithm>

#include "publisher.h"


static const int MTU = 1400;
static const uint32_t TS_INC = 90000 / 30; // 3000 (90kHz时钟)


VideoPublisher::VideoPublisher() {
  seq_no_ = 0;
  timestamp_ = 0;
}

VideoPublisher::~VideoPublisher() {
  disable();
}

bool VideoPublisher::enable(int camera_id) {
  printf("camera id: %d\n", camera_id);
  cap_.open(camera_id);
  if (!cap_.isOpened()) {
    printf("unable to open the camera.\n");
    return false;
  }
  printf("open camera success.\n");

  cap_.set(cv::CAP_PROP_FRAME_WIDTH, 640);
  cap_.set(cv::CAP_PROP_FRAME_HEIGHT, 480);

  participant_ = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
  if (participant_ < 0) {
    printf("dds_create_participant: %s\n", dds_strretcode(-participant_));
    return false;
  }

  topic_ = dds_create_topic(participant_, &video_Frame_desc, "video_Frame", NULL, NULL);
  if (topic_ < 0) {
    printf("dds_create_topic: %s\n", dds_strretcode(-topic_));
    return false;
  }

  writer_ = dds_create_writer(participant_, topic_, NULL, NULL);
  if (writer_ < 0) {
    printf("dds_create_writer: %s\n", dds_strretcode(-writer_));
    return false;
  }
  printf("enable success.\n");

  return true;
}

bool VideoPublisher::disable() {
  dds_return_t status = dds_delete (participant_);
  if (status < 0)
    printf("dds_delete: %s\n", dds_strretcode(-status));

  printf("release the camera capture.\n");
  cap_.release();
}

bool VideoPublisher::capture() {
  cv::Mat frame;
  bool ret = cap_.read(frame);
  if (!ret || frame.empty()) {
    printf("no frame has been grabbed, camera is disconnected or there is no frame.\n");
    return false;
  }
  cv::cvtColor(frame, image_, cv::COLOR_BGR2YUV_IYUV); // 图像预处理，保持BGR格式
  return true;
}

bool VideoPublisher::encode() {
  std::vector<int> quality;
  quality.push_back(cv::IMWRITE_JPEG_QUALITY);
  quality.push_back(50);  // compression ratio is 50%
  if (!cv::imencode(".jpg", image_, jpeg_data_, quality)) {
    printf("encode jpeg failed.\n");
    return false;
  }
  return true;
}

int VideoPublisher::rtpPack(int offset) {
  const int JPEG_HDR_SIZE = sizeof(JPEGHeader);
  const int RTP_HDR_SIZE = sizeof(RTPHeader);
  uint32_t total_size = jpeg_data_.size();

  int payload_size = std::min(MTU, (int)total_size - offset);
  rtp_pkt_.resize(RTP_HDR_SIZE + JPEG_HDR_SIZE + payload_size);

  // 填充RTP头部
  RTPHeader* rtp_hdr = (RTPHeader*)rtp_pkt_.data();
  memset(rtp_hdr, 0, RTP_HDR_SIZE);
  rtp_hdr->version = 2;
  rtp_hdr->payload_type = 26; // 静态JPEG类型
  rtp_hdr->marker = (offset + payload_size == total_size) ? 1 : 0;
  rtp_hdr->seq_no = htons(seq_no_++);
  rtp_hdr->timestamp = htonl(timestamp_);
  rtp_hdr->ssrc = htonl(0x12345678); // 随机SSRC[citation:12]

  // 填充JPEG头部
  JPEGHeader* jpeg_hdr = (JPEGHeader*)(rtp_pkt_.data() + RTP_HDR_SIZE);
  jpeg_hdr->type_specific = 0;
  jpeg_hdr->jpeg_type = 0;    // Baseline JPEG
  jpeg_hdr->q = 0;            // 默认量化表
  jpeg_hdr->width = 640 / 8;  // 80
  jpeg_hdr->height = 480 / 8; // 60
  uint32_t offset_be = htonl(offset << 8); // 转换为24位
  memcpy(jpeg_hdr->offset, ((uint8_t*)&offset_be) + 1, 3); // 取后3字节

  // 拷贝JPEG数据
  memcpy(rtp_pkt_.data() + RTP_HDR_SIZE + JPEG_HDR_SIZE,
        jpeg_data_.data() + offset, payload_size);

  return offset + payload_size;
}

bool VideoPublisher::publish(uint8_t* data, int dataLen) {
  frame_.frame_bytes._buffer = (char*)dds_alloc (dataLen);
  frame_.frame_bytes._length = dataLen;
  frame_.frame_bytes._release = true;
  for (int i = 0; i < dataLen; i++) {
    frame_.frame_bytes._buffer[i] = data[i];
  }

  dds_return_t status = dds_write(writer_, &frame_);
  if (status != DDS_RETCODE_OK) {
    printf("dds_write: %s\n", dds_strretcode(-status));
  } else {
    frame_.frame_id++;
  }
  dds_free (frame_.frame_bytes._buffer);
  return true;
}

void* VideoPublisher::runThread(void* arg) {
  VideoPublisher* self = static_cast<VideoPublisher*>(arg);
  uint32_t status;
  dds_return_t rc = dds_set_status_mask(self->writer_, DDS_PUBLICATION_MATCHED_STATUS);
  if (rc != DDS_RETCODE_OK) {
    printf("dds_set_status_mask: %s\n", dds_strretcode(-rc));
    return NULL;
  }

  while(!(status & DDS_PUBLICATION_MATCHED_STATUS))
  {
    rc = dds_get_status_changes (self->writer_, &status);
    if (rc != DDS_RETCODE_OK) {
      printf("dds_get_status_changes: %s\n", dds_strretcode(-rc));
      return NULL;
    }
    dds_sleepfor(DDS_MSECS (100));
  }

  while (true) {
    if (!self->capture()) {
      printf("grab image failed.\n");
      dds_sleepfor(DDS_SECS(1));
      continue;
    }

    if (!self->encode()) {
      printf("encode frame failed, drop this frame.\n");
      continue;
    }

    int offset = 0;
    while (offset < self->jpeg_data_.size()) {
      offset = self->rtpPack(offset);
      self->publish(self->rtp_pkt_.data(), self->rtp_pkt_.size());
      self->timestamp_ += TS_INC;
    }

    dds_sleepfor(DDS_MSECS(1));
  }
  return NULL;
}

void VideoPublisher::run() {
  pthread_t t;
  int ret = pthread_create(&t, NULL, VideoPublisher::runThread, this);
  if (ret) {
    printf("pthread_create failed, ret: %d\n", ret);
    return;
  }
  pthread_join(t, NULL);
}
