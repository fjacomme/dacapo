#include "soundgen/soundgen.hpp"
#define WIN32_LEAN_AND_MEAN
#pragma warning(push)
#pragma warning(disable : 4061)
#pragma warning(disable : 4242)
#pragma warning(disable : 4266)
#pragma warning(disable : 4355)
#pragma warning(disable : 4365)
#pragma warning(disable : 4548)
#pragma warning(disable : 4619)
#pragma warning(disable : 4625)
#pragma warning(disable : 4626)
#pragma warning(disable : 4668)
#pragma warning(disable : 5026)
#pragma warning(disable : 5027)
#pragma warning(disable : 5031)
#pragma warning(disable : 5039)
#pragma warning(disable : 5204)

#define OSCPKT_OSTREAM_OUTPUT
#include "soundgen/sc/oscpkt.hh"

#include <boost/asio.hpp>
#pragma warning(pop)

#include <array>
#include <filesystem>
#include <iostream>
#include <thread>

namespace fs = std::filesystem;
using oscpkt::Message;

namespace ba     = boost::asio;
using basocket   = ba::ip::udp::socket;
using baendpoint = ba::ip::udp::endpoint;

struct soundgen::pimpl {
    ba::io_context         ioc;
    basocket               sock;
    baendpoint             server_addr;
    std::array<char, 1024> recv_buffer;
    bool                   debug = true;
    pimpl()
        : sock(ioc)
    {
    }

    void connect(const char* addr, int port)
    {
        uint16_t const uport = uint16_t(port);
        server_addr          = baendpoint(ba::ip::make_address_v4(addr), uport);
        std::cout << "Connecting to " << addr << ":" << uport << std::endl;
        sock.open(ba::ip::udp::v4());
        sock.bind(baendpoint(ba::ip::udp::v4(), uint16_t(uport + 1)));
    }

    bool send(Message const& msg)
    {
        oscpkt::PacketWriter pw;
        pw.addMessage(msg);
        std::cout << "Msg:" << msg << std::endl;
        return sock.send_to(ba::buffer(pw.packetData(), pw.packetSize()), server_addr) > 0;
    }

    void wait_for_response()
    {
        auto const           recvlen = sock.receive(ba::buffer(recv_buffer));
        oscpkt::PacketReader pr(recv_buffer.data(), recvlen);
        auto                 msg = pr.popMessage();
        if (!msg) {
            std::cerr << "No response" << std::endl;
        }
        else {
            while (msg) {
                std::cout << "Resp:" << *msg << std::endl;
                msg = pr.popMessage();
            }
        }
    }
    void consume_responses()
    {
        while (sock.available() > 0) {
            sock.receive(ba::buffer(recv_buffer));
        }
    }

    void init()
    {
        send(Message("/notify").pushInt32(1));
        wait_for_response();
        if (debug) {
            send(Message("/dumpOSC").pushInt32(1));
        }
        // send(Message("/d_loadDir").pushStr("etc/synthdefs/compiled"));
        // wait_for_response();
        consume_responses();
    }
    void load_synth(fs::path const& path)
    {
        send(Message("/d_load").pushStr(path.generic_string()));
        consume_responses();
    }
    void load_sound(fs::path const& path, int id)
    {
        send(Message("/b_allocRead").pushInt32(id).pushStr(path.generic_string()));
        consume_responses();
    }
};

soundgen::soundgen()
    : _p(std::make_unique<pimpl>())
{
    _p->connect("127.0.0.1", 1988);

    _p->init();

    _p->load_synth("etc/synthdefs/utils/sonic-pi-stereo_player.scsyndef");

    std::cout << "synth: " << std::endl;
    for (auto it = fs::directory_iterator("etc/synthdefs/synth"); it != fs::directory_iterator();
         ++it) {
        std::cout << it->path() << std::endl;
        _p->load_synth(it->path());
        auto filename = it->path().filename().stem().string();
        filename      = filename.substr(9); // todo, better handling of prefixes
        defs.synths.push_back(filename);
    }

    std::cout << "sounds: " << std::endl;
    for (auto it = fs::directory_iterator("etc/samples"); it != fs::directory_iterator(); ++it) {
        std::cout << it->path() << std::endl;
        _p->load_sound(it->path(), (int)defs.samples.size());
        defs.samples.push_back(it->path().filename().stem().string());
    }
}

soundgen::~soundgen()
{
}

static int cpt = 1;

void soundgen::play(synth const& s)
{
    Message msg("/s_new");
    // todo, better handling of prefixes
    msg.pushStr("sonic-pi-" + s.name).pushInt32(cpt++).pushInt32(0).pushInt32(0);
    for (auto& p : s.params) {
        msg.pushInt32(int(p.first)).pushFloat(p.second);
    }
    _p->send(msg);
}

void soundgen::play(sample const& s)
{
    int id = -1;
    for (size_t i = 0; i < defs.samples.size(); i++) {
        if (defs.samples[i] == s.name) {
            id = (int)i;
            break;
        }
    }
    if (id < 0) {
        std::cerr << "sample not found: " << s.name << std::endl;
        return;
    }
    Message msg("/s_new");
    msg.pushStr("sonic-pi-stereo_player").pushInt32(cpt++).pushInt32(0).pushInt32(0);
    msg.pushInt32(0).pushInt32(id);
    for (auto& p : s.params) {
        msg.pushInt32(int(p.first) + 1) // because 0 is buffer id
            .pushFloat(p.second);
    }
    _p->send(msg);
}