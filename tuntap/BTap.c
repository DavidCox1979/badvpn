/**
 * @file BTap.c
 * @author Ambroz Bizjak <ambrop7@gmail.com>
 * 
 * @section LICENSE
 * 
 * This file is part of BadVPN.
 * 
 * BadVPN is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 * 
 * BadVPN is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <string.h>
#include <stdio.h>

#ifdef BADVPN_USE_WINAPI
    #include <windows.h>
    #include <winioctl.h>
    #include <objbase.h>
    #include <wtypes.h>
    #include "wintap-common.h"
    #include <tuntap/tapwin32-funcs.h>
#else
    #include <fcntl.h>
    #include <unistd.h>
    #include <errno.h>
    #include <sys/ioctl.h>
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <sys/socket.h>
    #include <net/if.h>
    #include <net/if_arp.h>
    #ifdef BADVPN_LINUX
        #include <linux/if_tun.h>
    #endif
    #ifdef BADVPN_FREEBSD
        #include <net/if_tun.h>
        #include <net/if_tap.h>
    #endif
#endif

#include <base/BLog.h>

#include <tuntap/BTap.h>

#include <generated/blog_channel_BTap.h>

static void report_error (BTap *o);
static void input_handler_send (BTap *o, uint8_t *data, int data_len);
static void output_handler_recv (BTap *o, uint8_t *data);

#ifdef BADVPN_USE_WINAPI

static void recv_olap_handler (BTap *o, int event, DWORD bytes)
{
    DebugObject_Access(&o->d_obj);
    ASSERT(o->output_packet)
    ASSERT(event == BREACTOR_IOCP_EVENT_SUCCEEDED || event == BREACTOR_IOCP_EVENT_FAILED)
    
    // set no output packet
    o->output_packet = NULL;
    
    if (event == BREACTOR_IOCP_EVENT_FAILED) {
        BLog(BLOG_ERROR, "read operation failed");
        report_error(o);
        return;
    }
    
    ASSERT(bytes >= 0)
    ASSERT(bytes <= o->frame_mtu)
    
    // done
    PacketRecvInterface_Done(&o->output, bytes);
}

#else

static void fd_handler (BTap *o, int events)
{
    DebugError_AssertNoError(&o->d_err);
    DebugObject_Access(&o->d_obj);
    
    if (events&BREACTOR_ERROR) {
        BLog(BLOG_WARNING, "device fd reports error?");
    }
    
    if (events&BREACTOR_WRITE) do {
        ASSERT(o->input_packet_len >= 0)
        
        int bytes = write(o->fd, o->input_packet, o->input_packet_len);
        if (bytes < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // retry later
                break;
            }
            // malformed packets will cause errors, ignore them and act like
            // the packet was accepeted
        } else {
            if (bytes != o->input_packet_len) {
                BLog(BLOG_WARNING, "written %d expected %d", bytes, o->input_packet_len);
            }
        }
        
        // set no input packet
        o->input_packet_len = -1;
        
        // update events
        o->poll_events &= ~BREACTOR_WRITE;
        BReactor_SetFileDescriptorEvents(o->reactor, &o->bfd, o->poll_events);
        
        // inform sender we finished the packet
        PacketPassInterface_Done(&o->input);
    } while (0);
    
    if (events&BREACTOR_READ) do {
        ASSERT(o->output_packet)
        
        // try reading into the buffer
        int bytes = read(o->fd, o->output_packet, o->frame_mtu);
        if (bytes < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // retry later
                break;
            }
            // report fatal error
            report_error(o);
            return;
        }
        
        ASSERT_FORCE(bytes <= o->frame_mtu)
        
        // set no output packet
        o->output_packet = NULL;
        
        // update events
        o->poll_events &= ~BREACTOR_READ;
        BReactor_SetFileDescriptorEvents(o->reactor, &o->bfd, o->poll_events);
        
        // inform receiver we finished the packet
        PacketRecvInterface_Done(&o->output, bytes);
    } while (0);
}

#endif

void report_error (BTap *o)
{
    DEBUGERROR(&o->d_err, o->handler_error(o->handler_error_user));
}

void input_handler_send (BTap *o, uint8_t *data, int data_len)
{
    ASSERT(data_len >= 0)
    ASSERT(data_len <= o->frame_mtu)
    ASSERT(o->input_packet_len == -1)
    DebugError_AssertNoError(&o->d_err);
    DebugObject_Access(&o->d_obj);
    
    #ifdef BADVPN_USE_WINAPI
    
    // ignore frames without an Ethernet header, or we get errors in WriteFile
    if (data_len < 14) {
        goto accept;
    }
    
    memset(&o->send_olap.olap, 0, sizeof(o->send_olap.olap));
    
    // write
    BOOL res = WriteFile(o->device, data, data_len, NULL, &o->send_olap.olap);
    if (res == FALSE && GetLastError() != ERROR_IO_PENDING) {
        BLog(BLOG_ERROR, "WriteFile failed (%u)", GetLastError());
        goto accept;
    }
    
    // wait
    int succeeded;
    DWORD bytes;
    BReactorIOCPOverlapped_Wait(&o->send_olap, &succeeded, &bytes);
    
    if (!succeeded) {
        BLog(BLOG_ERROR, "write operation failed");
    } else {
        ASSERT(bytes >= 0)
        ASSERT(bytes <= data_len)
        
        if (bytes < data_len) {
            BLog(BLOG_ERROR, "write operation didn't write everything");
        }
    }
    
    #else
    
    int bytes = write(o->fd, data, data_len);
    if (bytes < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // retry later in fd_handler
            // remember packet
            o->input_packet = data;
            o->input_packet_len = data_len;
            // update events
            o->poll_events |= BREACTOR_WRITE;
            BReactor_SetFileDescriptorEvents(o->reactor, &o->bfd, o->poll_events);
            return;
        }
        // malformed packets will cause errors, ignore them and act like
        // the packet was accepeted
    } else {
        if (bytes != data_len) {
            BLog(BLOG_WARNING, "written %d expected %d", bytes, data_len);
        }
    }
    
    #endif
    
accept:
    PacketPassInterface_Done(&o->input);
}

void output_handler_recv (BTap *o, uint8_t *data)
{
    ASSERT(data)
    ASSERT(!o->output_packet)
    DebugError_AssertNoError(&o->d_err);
    DebugObject_Access(&o->d_obj);
    
#ifdef BADVPN_USE_WINAPI
    
    memset(&o->recv_olap.olap, 0, sizeof(o->recv_olap.olap));
    
    // read
    BOOL res = ReadFile(o->device, data, o->frame_mtu, NULL, &o->recv_olap.olap);
    if (res == FALSE && GetLastError() != ERROR_IO_PENDING) {
        BLog(BLOG_ERROR, "ReadFile failed (%u)", GetLastError());
        report_error(o);
        return;
    }
    
    o->output_packet = data;
    
#else
    
    // attempt read
    int bytes = read(o->fd, data, o->frame_mtu);
    if (bytes < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // retry later in fd_handler
            // remember packet
            o->output_packet = data;
            // update events
            o->poll_events |= BREACTOR_READ;
            BReactor_SetFileDescriptorEvents(o->reactor, &o->bfd, o->poll_events);
            return;
        }
        // report fatal error
        report_error(o);
        return;
    }
    
    ASSERT_FORCE(bytes <= o->frame_mtu)
    
    PacketRecvInterface_Done(&o->output, bytes);
    
#endif
}

int BTap_Init (BTap *o, BReactor *reactor, char *devname, BTap_handler_error handler_error, void *handler_error_user, int tun)
{
    ASSERT(tun == 0 || tun == 1)
    
    // init arguments
    o->reactor = reactor;
    o->handler_error = handler_error;
    o->handler_error_user = handler_error_user;
    
    #ifdef BADVPN_USE_WINAPI
    
    // parse device specification
    
    if (!devname) {
        BLog(BLOG_ERROR, "no device specification provided");
        return 0;
    }
    
    int devname_len = strlen(devname);
    
    char device_component_id[devname_len + 1];
    char device_name[devname_len + 1];
    uint32_t tun_addrs[3];
    
    if (tun) {
        if (!tapwin32_parse_tun_spec(devname, device_component_id, device_name, tun_addrs)) {
            BLog(BLOG_ERROR, "failed to parse TUN device specification");
            return 0;
        }
    } else {
        if (!tapwin32_parse_tap_spec(devname, device_component_id, device_name)) {
            BLog(BLOG_ERROR, "failed to parse TAP device specification");
            return 0;
        }
    }
    
    // locate device path
    
    char device_path[TAPWIN32_MAX_REG_SIZE];
    
    BLog(BLOG_INFO, "Looking for TAP-Win32 with component ID %s, name %s", device_component_id, device_name);
    
    if (!tapwin32_find_device(device_component_id, device_name, &device_path)) {
        BLog(BLOG_ERROR, "Could not find device");
        goto fail0;
    }
    
    // open device
    
    BLog(BLOG_INFO, "Opening device %s", device_path);
    
    o->device = CreateFile(device_path, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_SYSTEM|FILE_FLAG_OVERLAPPED, 0);
    if (o->device == INVALID_HANDLE_VALUE) {
        BLog(BLOG_ERROR, "CreateFile failed");
        goto fail0;
    }
    
    // set TUN if needed
    
    DWORD len;
    
    if (tun) {
        if (!DeviceIoControl(o->device, TAP_IOCTL_CONFIG_TUN, tun_addrs, sizeof(tun_addrs), tun_addrs, sizeof(tun_addrs), &len, NULL)) {
            BLog(BLOG_ERROR, "DeviceIoControl(TAP_IOCTL_CONFIG_TUN) failed");
            goto fail1;
        }
    }
    
    // get MTU
    
    ULONG umtu;
    
    if (!DeviceIoControl(o->device, TAP_IOCTL_GET_MTU, NULL, 0, &umtu, sizeof(umtu), &len, NULL)) {
        BLog(BLOG_ERROR, "DeviceIoControl(TAP_IOCTL_GET_MTU) failed");
        goto fail1;
    }
    
    if (tun) {
        o->frame_mtu = umtu;
    } else {
        o->frame_mtu = umtu + BTAP_ETHERNET_HEADER_LENGTH;
    }
    
    // set connected
    
    ULONG upstatus = TRUE;
    if (!DeviceIoControl(o->device, TAP_IOCTL_SET_MEDIA_STATUS, &upstatus, sizeof(upstatus), &upstatus, sizeof(upstatus), &len, NULL)) {
        BLog(BLOG_ERROR, "DeviceIoControl(TAP_IOCTL_SET_MEDIA_STATUS) failed");
        goto fail1;
    }
    
    BLog(BLOG_INFO, "Device opened");
    
    // associate device with IOCP
    
    if (!CreateIoCompletionPort(o->device, BReactor_GetIOCPHandle(o->reactor), 0, 0)) {
        BLog(BLOG_ERROR, "CreateIoCompletionPort failed");
        goto fail1;
    }
    
    // init send olap
    BReactorIOCPOverlapped_Init(&o->send_olap, o->reactor, o, NULL);
    
    // init recv olap
    BReactorIOCPOverlapped_Init(&o->recv_olap, o->reactor, o, (BReactorIOCPOverlapped_handler)recv_olap_handler);
    
    goto success;
    
fail1:
    ASSERT_FORCE(CloseHandle(o->device))
fail0:
    return 0;
    
    #endif
    
    #if defined(BADVPN_LINUX) || defined(BADVPN_FREEBSD)
    
    #ifdef BADVPN_LINUX
    
    // open device
    
    if ((o->fd = open("/dev/net/tun", O_RDWR)) < 0) {
        BLog(BLOG_ERROR, "error opening device");
        goto fail0;
    }
    
    // configure device
    
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags |= IFF_NO_PI;
    if (tun) {
        ifr.ifr_flags |= IFF_TUN;
    } else {
        ifr.ifr_flags |= IFF_TAP;
    }
    if (devname) {
        snprintf(ifr.ifr_name, IFNAMSIZ, "%s", devname);
    }
    
    if (ioctl(o->fd, TUNSETIFF, (void *)&ifr) < 0) {
        BLog(BLOG_ERROR, "error configuring device");
        goto fail1;
    }
    
    strcpy(o->devname, ifr.ifr_name);
    
    #endif
    
    #ifdef BADVPN_FREEBSD
    
    if (tun) {
        BLog(BLOG_ERROR, "TUN not supported on FreeBSD");
        goto fail0;
    }
    
    if (!devname) {
        BLog(BLOG_ERROR, "no device specified");
        goto fail0;
    }
    
    // open device
    
    char devnode[10 + IFNAMSIZ];
    snprintf(devnode, sizeof(devnode), "/dev/%s", devname);
    
    if ((o->fd = open(devnode, O_RDWR)) < 0) {
        BLog(BLOG_ERROR, "error opening device");
        goto fail0;
    }
    
    // get name
    
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    if (ioctl(o->fd, TAPGIFNAME, (void *)&ifr) < 0) {
        BLog(BLOG_ERROR, "error configuring device");
        goto fail1;
    }
    
    strcpy(o->devname, ifr.ifr_name);
    
    #endif
    
    // get MTU
    
    // open dummy socket for ioctls
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        BLog(BLOG_ERROR, "socket failed");
        goto fail1;
    }
    
    memset(&ifr, 0, sizeof(ifr));
    strcpy(ifr.ifr_name, o->devname);
    
    if (ioctl(sock, SIOCGIFMTU, (void *)&ifr) < 0) {
        BLog(BLOG_ERROR, "error getting MTU");
        close(sock);
        goto fail1;
    }
    
    if (tun) {
        o->frame_mtu = ifr.ifr_mtu;
    } else {
        o->frame_mtu = ifr.ifr_mtu + BTAP_ETHERNET_HEADER_LENGTH;
    }
    
    close(sock);
    
    // set non-blocking
    if (fcntl(o->fd, F_SETFL, O_NONBLOCK) < 0) {
        BLog(BLOG_ERROR, "cannot set non-blocking");
        goto fail1;
    }
    
    // init file descriptor object
    BFileDescriptor_Init(&o->bfd, o->fd, (BFileDescriptor_handler)fd_handler, o);
    if (!BReactor_AddFileDescriptor(o->reactor, &o->bfd)) {
        BLog(BLOG_ERROR, "BReactor_AddFileDescriptor failed");
        goto fail1;
    }
    o->poll_events = 0;
    
    goto success;
    
fail1:
    ASSERT_FORCE(close(o->fd) == 0)
fail0:
    return 0;
    
    #endif
    
success:
    // init input
    PacketPassInterface_Init(&o->input, o->frame_mtu, (PacketPassInterface_handler_send)input_handler_send, o, BReactor_PendingGroup(o->reactor));
    
    // init output
    PacketRecvInterface_Init(&o->output, o->frame_mtu, (PacketRecvInterface_handler_recv)output_handler_recv, o, BReactor_PendingGroup(o->reactor));
    
    // set no input packet
    o->input_packet_len = -1;
    
    // set no output packet
    o->output_packet = NULL;
    
    DebugObject_Init(&o->d_obj);
    DebugError_Init(&o->d_err, BReactor_PendingGroup(o->reactor));
    
    return 1;
}

void BTap_Free (BTap *o)
{
    DebugError_Free(&o->d_err);
    DebugObject_Free(&o->d_obj);
    
    // free output
    PacketRecvInterface_Free(&o->output);
    
    // free input
    PacketPassInterface_Free(&o->input);
    
#ifdef BADVPN_USE_WINAPI
    
    // cancel I/O
    ASSERT_FORCE(CancelIo(o->device))
    
    // wait receiving to finish
    if (o->output_packet) {
        BLog(BLOG_DEBUG, "waiting for sending to finish");
        BReactorIOCPOverlapped_Wait(&o->recv_olap, NULL, NULL);
    }
    
    // wait sending to finish
    if (o->input_packet_len >= 0) {
        BLog(BLOG_DEBUG, "waiting for sending to finish");
        BReactorIOCPOverlapped_Wait(&o->send_olap, NULL, NULL);
    }
    
    // free recv olap
    BReactorIOCPOverlapped_Free(&o->recv_olap);
    
    // free send olap
    BReactorIOCPOverlapped_Free(&o->send_olap);
    
    // close device
    ASSERT_FORCE(CloseHandle(o->device))
    
#else
    
    // free BFileDescriptor
    BReactor_RemoveFileDescriptor(o->reactor, &o->bfd);
    
    // close file descriptor
    ASSERT_FORCE(close(o->fd) == 0)
    
#endif
}

int BTap_GetMTU (BTap *o)
{
    DebugObject_Access(&o->d_obj);
    
    return o->frame_mtu;
}

PacketPassInterface * BTap_GetInput (BTap *o)
{
    DebugObject_Access(&o->d_obj);
    
    return &o->input;
}

PacketRecvInterface * BTap_GetOutput (BTap *o)
{
    DebugObject_Access(&o->d_obj);
    
    return &o->output;
}
