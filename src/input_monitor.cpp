// SPDX-License-Identifier: MIT
#include "input_monitor.h"
#include "idle-client.h"
#include <wayland-client.h>
#include <QSocketNotifier>
#include <QByteArray>
#include <QGuiApplication>
#include <vector>
#include <algorithm>
#include <cstring>
#include <poll.h>

namespace Studio {
struct InputMonitor::Impl {
    InputMonitor *owner;
    wl_display *display=nullptr;
    wl_registry *registry=nullptr;
    ext_idle_notifier_v1 *manager=nullptr;
    struct Seat {uint32_t id; wl_seat *object;};
    std::vector<Seat> seats;
    std::vector<ext_idle_notification_v1*> notifications;
    QSocketNotifier *socket=nullptr;
    uint32_t managerId=0;
    bool listening=false;
    static void global(void *data,wl_registry *registry,uint32_t name,const char *interface,uint32_t version) {
        auto self=static_cast<Impl*>(data);
        if(std::strcmp(interface,"ext_idle_notifier_v1")==0&&version>=2&&!self->manager) {
            self->manager=static_cast<ext_idle_notifier_v1*>(wl_registry_bind(registry,name,&ext_idle_notifier_v1_interface,2));
            self->managerId=name;
        } else if(std::strcmp(interface,"wl_seat")==0) {
            auto seat=static_cast<wl_seat*>(wl_registry_bind(registry,name,&wl_seat_interface,1));
            static const wl_seat_listener seatListener{
                [](void *,wl_seat *,uint32_t){},
                [](void *,wl_seat *,const char *){}
            };
            wl_seat_add_listener(seat,&seatListener,self);
            self->seats.push_back({name,seat});
            if(self->listening)self->subscribe(seat);
        }
    }
    static void removed(void *data,wl_registry *,uint32_t name) {
        auto self=static_cast<Impl*>(data);
        if(name==self->managerId||std::any_of(self->seats.begin(),self->seats.end(),[name](const Seat &s){return s.id==name;}))
            emit self->owner->unavailable();
    }
    static void idled(void *,ext_idle_notification_v1 *) {}
    static void resumed(void *data,ext_idle_notification_v1 *) {
        auto self=static_cast<Impl*>(data);
        if(self->listening)emit self->owner->activity();
    }
    void subscribe(wl_seat *seat) {
        auto n=ext_idle_notifier_v1_get_input_idle_notification(manager,0,seat);
        static const ext_idle_notification_v1_listener listener{&Impl::idled,&Impl::resumed};
        ext_idle_notification_v1_add_listener(n,&listener,this);
        notifications.push_back(n);
    }
};
InputMonitor::InputMonitor(QObject *parent):QObject(parent),impl(std::make_unique<Impl>()) {impl->owner=this;}
InputMonitor::~InputMonitor() {
    stop();
    if(impl->socket){impl->socket->setEnabled(false);delete impl->socket;}
    for(const auto &s:impl->seats)wl_seat_destroy(s.object);
    if(impl->manager)ext_idle_notifier_v1_destroy(impl->manager);
    if(impl->registry)wl_registry_destroy(impl->registry);
    if(impl->display)wl_display_disconnect(impl->display);
}
bool InputMonitor::prepare() {
    if(impl->display)return impl->manager&&!impl->seats.empty();
    // A separate connection keeps monitoring independent of GUI focus and window visibility.
    impl->display=wl_display_connect(nullptr);if(!impl->display)return false;
    impl->registry=wl_display_get_registry(impl->display);
    static const wl_registry_listener listener{&Impl::global,&Impl::removed};
    wl_registry_add_listener(impl->registry,&listener,impl.get());
    if(wl_display_roundtrip(impl->display)<0||!impl->manager||impl->seats.empty())return false;
    impl->socket=new QSocketNotifier(wl_display_get_fd(impl->display),QSocketNotifier::Read,this);
    connect(impl->socket,&QSocketNotifier::activated,this,[this]{
        if(!impl->listening)return;
        // Drain queued events before preparing a read; never wait for another event.
        while(wl_display_prepare_read(impl->display)!=0){
            if(wl_display_dispatch_pending(impl->display)<0){impl->socket->setEnabled(false);emit unavailable();return;}
            if(!impl->listening)return;
        }
        pollfd fd{wl_display_get_fd(impl->display),POLLIN,0};
        if(::poll(&fd,1,0)>0&&(fd.revents&POLLIN)){
            if(wl_display_read_events(impl->display)<0){impl->socket->setEnabled(false);emit unavailable();return;}
        }else wl_display_cancel_read(impl->display);
        if(wl_display_dispatch_pending(impl->display)<0){impl->socket->setEnabled(false);emit unavailable();return;}
        wl_display_flush(impl->display);
    });
    impl->socket->setEnabled(false);
    return true;
}
bool InputMonitor::arm() {
    if(!impl->manager||impl->seats.empty())return false;
    stop();impl->listening=true;impl->socket->setEnabled(true);
    for(const auto &s:impl->seats)impl->subscribe(s.object);
    return wl_display_flush(impl->display)>=0;
}
void InputMonitor::stop() {
    impl->listening=false;
    if(impl->socket)impl->socket->setEnabled(false);
    for(auto n:impl->notifications)ext_idle_notification_v1_destroy(n);
    impl->notifications.clear();
    if(impl->display)wl_display_flush(impl->display);
}
}
