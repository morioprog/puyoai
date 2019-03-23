#include "viddev_source.h"

#include <fcntl.h>
#include <libv4l2.h>
#include <linux/videodev2.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <algorithm>

#include "capture/monitor.h"

using namespace std;

namespace {

void showPixFormat(const struct v4l2_pix_format& pix_fmt)
{
    char fmt_buf[5];
    fmt_buf[0] = (pix_fmt.pixelformat & 0xFF);
    fmt_buf[1] = (pix_fmt.pixelformat >> 8) & 0xFF;
    fmt_buf[2] = (pix_fmt.pixelformat >> 16) & 0xFF;
    fmt_buf[3] = (pix_fmt.pixelformat >> 24) & 0xFF;
    fmt_buf[4] = 0;
    fprintf(stderr, "w=%u h=%u fmt=%s field=%u bypl=%u sz=%u cs=%u priv=%u\n",
            pix_fmt.width, pix_fmt.height, fmt_buf,
            pix_fmt.field, pix_fmt.bytesperline, pix_fmt.sizeimage,
            pix_fmt.colorspace, pix_fmt.priv);
}

void showCurrentInput(int fd)
{
    struct v4l2_input input;
    int input_index;
    if (v4l2_ioctl(fd, VIDIOC_G_INPUT, &input_index) < 0) {
        perror("v4l2_ioctl VIDIOC_G_INPUT");
        exit(EXIT_FAILURE);
    }

    memset(&input, 0, sizeof(input));
    input.index = input_index;

    if (v4l2_ioctl(fd, VIDIOC_ENUMINPUT, &input) < 0) {
        perror ("v4l2_ioctl VIDIOC_ENUMINPUT");
        exit(EXIT_FAILURE);
    }

    fprintf(stderr, "Current input: %s @%d type=%u audioset=%u\n",
            input.name, input_index, input.type, input.audioset);
}

void adjustStandard(int fd)
{
    v4l2_std_id std_id = 0;
    struct v4l2_standard standard;
    memset (&standard, 0, sizeof(standard));
    standard.index = 0;

    while (v4l2_ioctl(fd, VIDIOC_ENUMSTD, &standard) == 0) {
        if (standard.id & std_id) {
            fprintf(stderr, "Current video standard: %s\n", standard.name);
            errno = 0;
            break;
        }

        standard.index++;
    }

    /* EINVAL indicates the end of the enumeration, which cannot be
       empty unless this device falls under the USB exception. */
    if (errno == EINVAL /* || standard.index == 0 */) {
        perror("v4l2_ioctl VIDIOC_ENUMSTD");
        exit(EXIT_FAILURE);
    }
}

void showCurrentAudio(int fd)
{
    struct v4l2_audio audio;
    if (v4l2_ioctl(fd, VIDIOC_G_AUDIO, &audio) < 0) {
        fprintf(stderr, "Failed to get audio info.\n");
        return;

    }

    fprintf(stderr, "Current audio: %s @%u capability=%u mode=%u\n",
            audio.name, audio.index, audio.capability, audio.mode);
}

template<typename T>
T clamp(T left, T x, T right) { return std::min(std::max(left, x), right); }

}  // anonymous namespace

VidDevSource::VidDevSource(const string& dev) :
    dev_(dev)
{
    init();
}

void VidDevSource::init()
{
    fd_ = v4l2_open(dev_.c_str(), O_RDWR);
    if (fd_ < 0) {
        perror("v4l2_open");
        exit(EXIT_FAILURE);
    }

    struct v4l2_capability cap;
    if (v4l2_ioctl(fd_, VIDIOC_QUERYCAP, &cap) < 0) {
        perror("v4l2_ioctl VIDIOC_QUERYCAP");
        exit(EXIT_FAILURE);
    }

    struct v4l2_format format;
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    const struct v4l2_pix_format& pix_fmt = format.fmt.pix;
    if (v4l2_ioctl(fd_, VIDIOC_G_FMT, &format) < 0) {
        perror("v4l2_ioctl VIDIOC_G_FMT");
        exit(EXIT_FAILURE);
    }
    showPixFormat(pix_fmt);

    format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    if (v4l2_ioctl(fd_, VIDIOC_S_FMT, &format) < 0) {
        perror("v4l2_ioctl VIDIOC_S_FMT");
        exit(EXIT_FAILURE);
    }
    showPixFormat(pix_fmt);

    width_ = pix_fmt.width;
    height_ = pix_fmt.height;

    showCurrentInput(fd_);

    adjustStandard(fd_);

    showCurrentAudio(fd_);

    initBuffers();

    ok_ = true;
}

VidDevSource::~VidDevSource()
{
    quit();
}

void VidDevSource::quit()
{
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (v4l2_ioctl(fd_, VIDIOC_STREAMOFF, &type) < 0) {
        perror("v4l2_ioctl VIDIOC_STREAMOFF");
        exit(EXIT_FAILURE);
    }
    v4l2_close(fd_);
    for (size_t i = 0; i < buf_cnt_; i++) {
        munmap(buffers_[i].start, buffers_[i].length);
    }
    free(buffers_);
}

void VidDevSource::initBuffers()
{
    struct v4l2_requestbuffers reqbuf;
    memset(&reqbuf, 0, sizeof (reqbuf));
    reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbuf.memory = V4L2_MEMORY_MMAP;
    reqbuf.count = 20;

    if (v4l2_ioctl(fd_, VIDIOC_REQBUFS, &reqbuf) < 0) {
        if (errno == EINVAL)
            fprintf(stderr,
                    "Video capturing or mmap streaming is not supported\n");
        else
            perror("v4l2_ioctl VIDIOC_REQBUFS");
        exit(EXIT_FAILURE);
    }
    fprintf(stderr, "Buffer count=%u type=%u memory=%u\n",
            reqbuf.count, reqbuf.type, reqbuf.memory);

    if (reqbuf.count < 5) {
        fprintf(stderr, "Not enough buffer memory: %u\n", reqbuf.count);
        exit(EXIT_FAILURE);
    }

    buf_cnt_ = reqbuf.count;
    buffers_ = (Buffer*)calloc(reqbuf.count, sizeof(*buffers_));
    assert(buffers_);

    fprintf(stderr, "mmap:");
    for (size_t i = 0; i < reqbuf.count; i++) {
        struct v4l2_buffer buffer;
        memset(&buffer, 0, sizeof (buffer));
        buffer.type = reqbuf.type;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = i;
        if (v4l2_ioctl(fd_, VIDIOC_QUERYBUF, &buffer) < 0) {
            perror("VIDIOC_QUERYBUF");
            exit(EXIT_FAILURE);
        }

        buffers_[i].length = buffer.length; /* remember for munmap() */
        buffers_[i].start = (char*)mmap(nullptr, buffer.length,
                                        PROT_READ | PROT_WRITE, /* recommended */
                                        MAP_SHARED,             /* recommended */
                                        fd_, buffer.m.offset);
        buffers_[i].surface = SDL_CreateRGBSurfaceFrom(
            buffers_[i].start, width_, height_,
            16, width_ * 2, 31 << 11, 63 << 5, 31, 0);
        assert(buffers_[i].surface);

        fprintf(stderr, " %lu:%p+%lu", i, buffers_[i].start, buffers_[i].length);
        if (MAP_FAILED == buffers_[i].start) {
            /* If you do not exit here you should unmap() and free()
               the buffers mapped so far. */
            perror("mmap");
            exit(EXIT_FAILURE);
        }
    }
    fprintf(stderr, "\n");

    for (size_t i = 0; i < reqbuf.count; i++) {
        struct v4l2_buffer buffer;
        memset(&buffer, 0, sizeof (buffer));
        buffer.type = reqbuf.type;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = i;

        if (v4l2_ioctl(fd_, VIDIOC_QBUF, &buffer) < 0) {
            perror("VIDIOC_QBUF");
            exit(EXIT_FAILURE);
        }
    }

    if (v4l2_ioctl(fd_, VIDIOC_STREAMON, &reqbuf.type) < 0) {
        perror("v4l2_ioctl VIDIOC_STREAMON");
        exit(EXIT_FAILURE);
    }
}

UniqueSDLSurface VidDevSource::getNextFrame()
{
    int r;
    do {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd_, &fds);
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000;
        r = select(fd_ + 1, &fds, nullptr, nullptr, &tv);

        if (r == 0) {
            perror("select timeout");
            errno = 0;
            quit();
            init();
            continue;
        }
    } while (r < 0 && errno == EINTR);
    if (r < 0) {
        perror("select");
        exit(EXIT_FAILURE);
    }

    struct v4l2_buffer buffer;
    memset(&buffer, 0, sizeof(buffer));
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer.memory = V4L2_MEMORY_MMAP;

    if (v4l2_ioctl(fd_, VIDIOC_DQBUF, &buffer) < 0) {
        perror("VIDIOC_DQBUF");
        exit(EXIT_FAILURE);
    }
    const Buffer& buf = buffers_[buffer.index];

    if (v4l2_ioctl(fd_, VIDIOC_QBUF, &buffer) < 0) {
        perror("VIDIOC_QBUF");
        exit(EXIT_FAILURE);
    }

    UniqueSDLSurface surf(makeUniqueSDLSurface(
        SDL_CreateRGBSurface(0, kMonitorWidth, kMonitorHeight, 32, 0, 0, 0, 0)));
    // Convert actual captured size (e.g. 720x240) to normalized size (e.g. 640x448).
    const SDL_Rect srcRect {0, 0, width_, height_};
    SDL_BlitScaled(buf.surface, &srcRect, surf.get(), nullptr);

    // Convert YUYV to RGB
    UniqueSDLSurface rgbSurf(makeUniqueSDLSurface(SDL_CreateRGBSurface(0, 640, 480, 32,
                                                                       255 << 24,
                                                                       255 << 16,
                                                                       255 << 8, 255)));
    // http://klabgames.tech.blog.jp.klab.com/archives/1054828175.html
    Uint8* src = reinterpret_cast<Uint8*>(surf->pixels);
    Uint8* dst = reinterpret_cast<Uint8*>(rgbSurf->pixels);
    for (int Y = 0; Y < 480; ++Y) {
        for (int X = 0; X < 640; X += 2) {
            int y0 = static_cast<int>(src[0]) - 16;
            int y1 = static_cast<int>(src[2]) - 16;
            int u = static_cast<int>(src[1]) - 128;
            int v = static_cast<int>(src[3]) - 128;

            dst[0] = 255;
            dst[1] = clamp<int>(0, 1.164383 * y0 + 2.017232 * u, 255);
            dst[2] = clamp<int>(0, 1.164383 * y0 - 0.391762 * u - 0.812968 * v, 255);
            dst[3] = clamp<int>(0, 1.164383 * y0 + 1.596027 * v, 255);
            dst[4] = 255;
            dst[5] = clamp<int>(0, 1.164383 * y1 + 2.017232 * u, 255);
            dst[6] = clamp<int>(0, 1.164383 * y1 - 0.391762 * u - 0.812968 * v, 255);
            dst[7] = clamp<int>(0, 1.164383 * y1 + 1.596027 * v, 255);
            src += 4;
            dst += 8;
        }
    }
    return rgbSurf;
}
