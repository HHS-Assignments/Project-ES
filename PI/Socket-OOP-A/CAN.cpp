#include "CAN.h"

#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <cerrno>

CAN::CAN(const char *iface) : _iface(iface) {}

bool CAN::begin() {
    _fd = ::socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (_fd < 0) { perror("socket(CAN)"); return false; }

    ifreq ifr{};
    std::strncpy(ifr.ifr_name, _iface.c_str(), IFNAMSIZ - 1);
    if (ioctl(_fd, SIOCGIFINDEX, &ifr) < 0) {
        perror("ioctl SIOCGIFINDEX");
        stop();
        return false;
    }

    sockaddr_can addr{};
    addr.can_family  = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (::bind(_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind(CAN)");
        stop();
        return false;
    }
    return true;
}

bool CAN::isConnected() {
    return _fd >= 0;
}

void CAN::stop() {
    if (_fd >= 0) { ::close(_fd); _fd = -1; }
}

bool CAN::readMessage(CanMessage &msg) {
    if (_fd < 0) return false;
    can_frame f{};
    while (true) {
        ssize_t n = ::read(_fd, &f, sizeof(f));
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("read(CAN)");
            return false;
        }
        if (n < (ssize_t)sizeof(can_frame)) continue;
        break;
    }

    msg.id  = f.can_id & (f.can_id & CAN_EFF_FLAG ? CAN_EFF_MASK : CAN_SFF_MASK);
    msg.len = (f.can_dlc > 8) ? 8 : f.can_dlc;
    std::memcpy(msg.data, f.data, msg.len);
    return true;
}

bool CAN::writeMessage(const CanMessage &msg) {
    if (_fd < 0) return false;
    can_frame f{};
    f.can_id = msg.id;
    if (msg.id > 0x7FF) f.can_id |= CAN_EFF_FLAG;
    f.can_dlc = (msg.len > 8) ? 8 : msg.len;
    std::memcpy(f.data, msg.data, f.can_dlc);

    if (::write(_fd, &f, sizeof(f)) != (ssize_t)sizeof(f)) {
        perror("write(CAN)");
        return false;
    }
    return true;
}
