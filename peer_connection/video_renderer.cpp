#include "video_renderer.h"
#include "video_chat.h"

VideoRenderer::VideoRenderer(QObject *vc, webrtc::VideoTrackInterface* track_to_render):
    width_(0), height_(0), vc(vc), rendered_track_(track_to_render)
{
    rendered_track_->AddOrUpdateSink(this, rtc::VideoSinkWants());
}

VideoRenderer::~VideoRenderer()
{
//    rendered_track_->RemoveSink(this);
}

void VideoRenderer::setSize(int width, int height)
{
    mutex_setSize.lock();
    if (width_ == width && height_ == height) {
      return;
    }

    width_ = width;
    height_ = height;
    image_.reset(new uint8_t[width * height * 4]);
    mutex_setSize.unlock();
}

void VideoRenderer::OnFrame(const webrtc::VideoFrame &video_frame)
{
    mutex_onFrame.lock();
    rtc::scoped_refptr<webrtc::I420BufferInterface> buffer(
                video_frame.video_frame_buffer()->ToI420());
    if (video_frame.rotation() != webrtc::kVideoRotation_0) {
        buffer = webrtc::I420Buffer::Rotate(*buffer, video_frame.rotation());
    }
    setSize(buffer->width(), buffer->height());

    libyuv::I420ToABGR(buffer->DataY(), buffer->StrideY(), buffer->DataU(),
                       buffer->StrideU(), buffer->DataV(), buffer->StrideV(),
                       image_.get(), width_ * 4, buffer->width(),
                       buffer->height());
    VideoChat *tmp = static_cast<VideoChat *>(vc);
    mutex_onFrame.unlock();
    tmp->StreamVideo();
}
