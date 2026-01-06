#include <pthread.h>
#include <vector>
#include <arpa/inet.h>
#include <algorithm>

#include "client.h"
#include "publisher.h"


static const int MTU = 1400;
static const uint32_t TS_INC = 90000 / 30; // 3000 (90kHz时钟)

struct RTPHeader {
    uint8_t version:2;        // 协议版本（2）
    uint8_t padding:1;        // 塌余填充
    uint8_t extension:1;      // 扩展标识
    uint8_t csrc_count:4;     // 贡献源数量
    uint8_t marker:1;         // 完整帧标记
    uint8_t payload_type:7;   // 负载类型（JPEG=26）
    uint16_t seq_no;          // 网络字节序
    uint32_t timestamp;       // 时间戳（90kHz）
    uint32_t ssrc;            // 同步源标识
} __attribute__((packed));


struct JPEGHeader {
    uint8_t type_specific;    // 固定为0
    uint8_t jpeg_type;        // Baseline=0
    uint8_t q;                // 量化因子（0=默认）
    uint8_t width;            // 原始宽/8（640=80）
    uint8_t height;           // 原始高/8（480=60）
    uint8_t offset[3];        // 分片偏移量（24位）
} __attribute__((packed));

VideoPublisher::VideoPublisher() {
  memset(&frame_, 0, sizeof(video_Frame));
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

  dig_hole("pub_data");
  dig_hole("pub_meta");

  std::string sub_meta_ip;
  int sub_meta_port = 0;
  while (!get_address_by_name("sub_meta", sub_meta_ip, sub_meta_port)) {
    printf("get peer addr failed, try again.\n");
    dds_sleepfor(DDS_SECS (1));
    dig_hole("pub_data");
    dig_hole("pub_meta");
  }
  printf("get sub_meta ip: %s, port: %d\n", sub_meta_ip.c_str(), sub_meta_port);

  std::string pub_data_ip;
  int pub_data_port = 0;
  if (!get_address_by_name("pub_data", pub_data_ip, pub_data_port)) {
    printf("get peer addr failed, try again.\n");
    return false;
  }
  printf("get pub_data ip: %s, port: %d\n", pub_data_ip.c_str(), pub_data_port);

  std::string pub_meta_ip;
  int pub_meta_port = 0;
  if (!get_address_by_name("pub_meta", pub_meta_ip, pub_meta_port)) {
    printf("get peer addr failed, try again.\n");
    return false;
  }
  printf("get pub_meta ip: %s, port: %d\n", pub_meta_ip.c_str(), pub_meta_port);

  int base = 5000;
  int UnicastDataOffset = pub_data_port - base;
  int UnicastMetaOffset = pub_meta_port - base;
  printf("base: %d, UnicastDataOffset: %d, UnicastMetaOffset: %d\n", base, UnicastDataOffset, UnicastMetaOffset);
  char config[1024];
  memset(config, 0, sizeof(config));
  sprintf(config, "<CycloneDDS><Domain Id=\"any\">"
      "<General>"
      "<AllowMulticast>false</AllowMulticast>"
      "<EnableMulticastLoopback>false</EnableMulticastLoopback>"
      //"<ExternalNetworkAddress>%s</ExternalNetworkAddress>"
      //"<ExternalNetworkMask>255.255.255.0</ExternalNetworkMask>"
      "</General>"
      "<Discovery>"
      "<DefaultMulticastAddress>none</DefaultMulticastAddress>"
      "<SPDPMulticastAddress>none</SPDPMulticastAddress>"
      "<ParticipantIndex>0</ParticipantIndex>"
      "<Peers> < Peer Address = \"%s:%d\" / > < / Peers>"
      "<Ports>"
        "<Base>%d</Base>"
        "<UnicastDataOffset>%d</UnicastDataOffset>"
        "<UnicastMetaOffset>%d</UnicastMetaOffset>"
      "</Ports>"
      "</Discovery>"
      "</Domain></CycloneDDS>", /*pub_meta_ip.c_str(),*/ sub_meta_ip.c_str(), sub_meta_port,
      base, UnicastDataOffset, UnicastMetaOffset);
  domain_ = dds_create_domain(DDS_DOMAIN_DEFAULT, config);

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

  dds_qos_t* qos = dds_create_qos();
  dds_qset_reliability(qos, DDS_RELIABILITY_RELIABLE, DDS_SECS(10));
  dds_qset_history(qos, DDS_HISTORY_KEEP_ALL, 0);
  dds_qset_resource_limits(qos, 300, DDS_LENGTH_UNLIMITED, DDS_LENGTH_UNLIMITED);

  writer_ = dds_create_writer(participant_, topic_, qos, NULL);
  if (writer_ < 0) {
    printf("dds_create_writer: %s\n", dds_strretcode(-writer_));
    return false;
  }
  printf("enable success.\n");

  return true;
}

bool VideoPublisher::disable() {
  fill_all_holes();
  dds_return_t status = dds_delete (participant_);
  if (status < 0)
    printf("dds_delete: %s\n", dds_strretcode(-status));

  printf("release the camera capture.\n");
  cap_.release();
  return true;
}

bool VideoPublisher::capture() {
  bool ret = cap_.read(image_);
  if (!ret || image_.empty()) {
    printf("no frame has been grabbed, camera is disconnected or there is no frame.\n");
    return false;
  }
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

void VideoPublisher::rtpPack() {
  const int JPEG_HDR_SIZE = sizeof(JPEGHeader);
  const int RTP_HDR_SIZE = sizeof(RTPHeader);
  uint32_t total_size = jpeg_data_.size();

  rtp_pkt_.resize(RTP_HDR_SIZE + JPEG_HDR_SIZE + total_size);

  // 填充RTP头部
  RTPHeader* rtp_hdr = (RTPHeader*)rtp_pkt_.data();
  memset(rtp_hdr, 0, RTP_HDR_SIZE);
  rtp_hdr->version = 2;
  rtp_hdr->payload_type = 26; // 静态JPEG类型
  rtp_hdr->marker = 1;
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
  uint32_t offset_be = htonl(total_size << 8); // 转换为24位
  memcpy(jpeg_hdr->offset, ((uint8_t*)&offset_be) + 1, 3); // 取后3字节

  // 拷贝JPEG数据
  memcpy(rtp_pkt_.data() + RTP_HDR_SIZE + JPEG_HDR_SIZE,
        jpeg_data_.data(), total_size);
}

bool VideoPublisher::publish(uint8_t* data, int dataLen) {
  if (frame_.frame_bytes._buffer != NULL && !frame_.frame_bytes._release) {
    dds_free(frame_.frame_bytes._buffer);
  }

  // 分配新的缓冲区并复制数据
  frame_.frame_bytes._buffer = (char*)dds_alloc(dataLen);
  frame_.frame_bytes._length = dataLen;
  frame_.frame_bytes._release = true; // 让 DDS 负责释放内存
  
  memcpy(frame_.frame_bytes._buffer, data, dataLen);

  // 执行写入操作
  dds_return_t status = dds_write(writer_, &frame_);
  if (status != DDS_RETCODE_OK) {
    printf("dds_write failed: %s\n", dds_strretcode(-status));
    
    // 写入失败时释放内存
    if (frame_.frame_bytes._release) {
      dds_free(frame_.frame_bytes._buffer);
      frame_.frame_bytes._buffer = NULL;
    }
    return false;
  } else {
    frame_.frame_id++;
    return true;
  }

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

  printf("subscriber is online\n");
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

    self->rtpPack();
    self->timestamp_ += TS_INC;

    self->publish(self->rtp_pkt_.data(), self->rtp_pkt_.size());
    dds_sleepfor(DDS_MSECS (1));
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
