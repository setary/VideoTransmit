#include <windows.h>
#include <stdint.h>
#include <thread>
#include <vector>

#include "opencv2/highgui.hpp"

#include "client.h"
#include "subscriber.h"

#pragma pack(push, 1)
typedef struct RTPHeader1 {
    uint8_t version : 2;        // 协议版本（2）
    uint8_t padding : 1;        // 塌余填充
    uint8_t extension : 1;      // 扩展标识
    uint8_t csrc_count : 4;     // 贡献源数量
    uint8_t marker : 1;         // 完整帧标记
    uint8_t payload_type : 7;   // 负载类型（JPEG=26）
    uint16_t seq_no;          // 网络字节序
    uint32_t timestamp;       // 时间戳（90kHz）
    uint32_t ssrc;            // 同步源标识
} RTPHeader1;


struct JPEGHeader {
    uint8_t type_specific;    // 固定为0
    uint8_t jpeg_type;        // Baseline=0
    uint8_t q;                // 量化因子（0=默认）
    uint8_t width;            // 原始宽/8（640=80）
    uint8_t height;           // 原始高/8（480=60）
    uint8_t offset[3];        // 分片偏移量（24位）
};
#pragma pack(pop)

const int JPEG_HDR_SIZE = sizeof(JPEGHeader);
const int RTP_HDR_SIZE = sizeof(RTPHeader1);
uint64_t count = 0;
void on_data_available(dds_entity_t reader, void* arg)
{
    VideoSubscriber* self = (VideoSubscriber*)arg;
    dds_sample_info_t infos[1];
    dds_return_t rc;


    rc = dds_take(reader, self->samples, infos, 1, 1);
    if (rc < 0) {
        printf("Take failed: %s\n", dds_strretcode(-rc));
    }
    else if ((rc > 0) && (infos[0].valid_data)) {
        self->frame_ = (video_Frame*)self->samples[0];
        self->decode();
    }
}

VideoSubscriber::VideoSubscriber() {
    samples[0] = video_Frame__alloc();
}

VideoSubscriber::~VideoSubscriber() {
    disable();
    video_Frame_free(samples[0], DDS_FREE_ALL);
}

bool VideoSubscriber::enable() {
    dig_hole("sub_data");
    dig_hole("sub_meta");

    std::string pub_meta_ip;
    int pub_meta_port = 0;
    while (!get_address_by_name("pub_meta", pub_meta_ip, pub_meta_port)) {
        printf("get peer addr failed, try again.\n");
        dds_sleepfor(DDS_SECS(1));
        dig_hole("sub_data");
        dig_hole("sub_meta");
    }
    printf("get pub_meta ip: %s, port: %d\n", pub_meta_ip.c_str(), pub_meta_port);

    std::string sub_data_ip;
    int sub_data_port = 0;
    if (!get_address_by_name("sub_data", sub_data_ip, sub_data_port)) {
        printf("get peer addr failed, try again.\n");
        return false;
    }
    printf("get sub_data ip: %s, port: %d\n", sub_data_ip.c_str(), sub_data_port);

    std::string sub_meta_ip;
    int sub_meta_port = 0;
    if (!get_address_by_name("sub_meta", sub_meta_ip, sub_meta_port)) {
        printf("get peer addr failed, try again.\n");
        return false;
    }
    printf("get sub_meta ip: %s, port: %d\n", sub_meta_ip.c_str(), sub_meta_port);

    int base = 5000;
    int UnicastDataOffset = sub_data_port - base;
    int UnicastMetaOffset = sub_meta_port - base;
    printf("base: %d, UnicastDataOffset: %d, UnicastMetaOffset: %d\n", base, UnicastDataOffset, UnicastMetaOffset);
    char config[1024];
    memset(config, 0, sizeof(config));
    sprintf(config, "<CycloneDDS><Domain Id=\"any\">"
        "<General>"
        "<AllowMulticast>false</AllowMulticast>"
        "<EnableMulticastLoopback>false</EnableMulticastLoopback>"
        "</General>"
        "<Discovery>"
        "<DefaultMulticastAddress>none</DefaultMulticastAddress>"
        "<SPDPMulticastAddress>none</SPDPMulticastAddress>"
        "<ParticipantIndex>0</ParticipantIndex>"
        "<Peers><Peer Address=\"%s:%d\"/></Peers>"
        "<Ports>"
        "<Base>%d</Base>"
        "<UnicastDataOffset>%d</UnicastDataOffset>"
        "<UnicastMetaOffset>%d</UnicastMetaOffset>"
        "</Ports>"
        "</Discovery>"
        "</Domain></CycloneDDS>", /*sub_meta_ip.c_str(),*/ pub_meta_ip.c_str(), pub_meta_port,
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

    /* 创建监听器并设置回调 */
    dds_listener_t* listener = dds_create_listener(NULL);
    dds_lset_data_available_arg(listener, on_data_available, (void*)this, true);
    //dds_set_listener(reader_, listener);
    printf("in this: %p\n", this);

    reader_ = dds_create_reader(participant_, topic_, qos, listener);
    if (reader_ < 0) {
        dds_delete_qos(qos);
        printf("dds_create_reader: %s\n", dds_strretcode(-reader_));
        return false;
    }
    dds_delete_qos(qos);

    return true;
}

bool VideoSubscriber::disable() {
    fill_all_holes();
    dds_return_t rc = dds_delete(participant_);
    if (rc != DDS_RETCODE_OK) {
        printf("dds_delete: %s\n", dds_strretcode(-rc));
        return false;
    }
    return true;
}

void VideoSubscriber::decode() {
    //std::vector<char> frame(frame_->frame_bytes._buffer, frame_->frame_bytes._buffer + frame_->frame_bytes._length);

    // 解析rtp头
    RTPHeader1* rtp_hdr;
    rtp_hdr  = (RTPHeader1*)(frame_->frame_bytes._buffer);
    if (rtp_hdr->payload_type != 26) {
        printf("encode type is not jpeg, drop it!\n");
        return;
    }

    int totalLen = frame_->frame_bytes._length;
    int dataLen = totalLen - RTP_HDR_SIZE - JPEG_HDR_SIZE;
    jpeg_data_.resize(dataLen);
    memcpy(jpeg_data_.data(), frame_->frame_bytes._buffer + RTP_HDR_SIZE + JPEG_HDR_SIZE, dataLen);

    // 解析显示图片
    cv::Mat img = cv::imdecode(cv::Mat(jpeg_data_), CV_LOAD_IMAGE_COLOR); // decode
    cv::namedWindow("image", cv::WindowFlags::WINDOW_NORMAL);
    cv::resizeWindow("image", 1280, 720);
    cv::imshow("image", img);
    cv::waitKey(30);
}

void VideoSubscriber::run() {
    /*void *samples[1];
    samples[0] = video_Frame__alloc();
    dds_sample_info_t infos[1];
    while (true) {
      dds_return_t rc = dds_read(reader_, samples, infos, 1, 1);
      if (rc < 0) {
        printf("dds_read: %s\n", dds_strretcode(-rc));
      }

      if ((rc > 0) && (infos[0].valid_data)) {
          printf("recv a frame\n");
        frame_ = (video_Frame*) samples[0];
        decode();
      } else {
        dds_sleepfor (DDS_MSECS(100));
      }
      video_Frame_free(samples[0], DDS_FREE_ALL);
    }*/

    while (true) {
        dds_sleepfor(DDS_SECS(1));
    };
}
