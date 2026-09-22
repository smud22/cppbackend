#include "audio.h"

#include <boost/asio.hpp>
#include <charconv>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace net = boost::asio;
using net::ip::udp;

using namespace std::literals;


void StartServer(std::uint16_t port);
void StartClient(std::uint16_t port);

int main(int argc, char** argv) {
    if (argc != 3 || !(strcmp(argv[1], "server") || strcmp(argv[1], "client"))) {
        std::cout << "Usage: radio [client/server] [port]";
        return 0;
    }
    
    u_int16_t port = std::atoi(argv[2]);
    if (port == 0 || port > 65535) {
        std::cout << "Wrong port format";
        return 1;
    }

    if (!strcmp(argv[1], "server"))
        StartServer(port);
    else
        StartClient(port);
    return 0;
}

void StartClient(std::uint16_t port) {
    Recorder recorder(ma_format_u8, 1);
    
    net::io_context io_context;
    udp::socket socket(io_context, udp::v4());
    socket.set_option(net::socket_base::send_buffer_size(65507));

    constexpr std::size_t max_message_size = 65000;
    const std::size_t frame_size = static_cast<std::size_t>(recorder.GetFrameSize());
    const std::size_t max_frames = max_message_size / frame_size;
    std::string ip;

    while (true) {
        std::cout << "Enter IPv4 address: " << std::flush;
        std::getline(std::cin, ip);

        boost::system::error_code e;
        const auto ip_address = net::ip::make_address_v4(ip, e);
        if (e) {
            std::cout << "Invalid IPv4 address" << std::endl;
            continue;
        }
        
        const udp::endpoint endpoint(ip_address, port);

        std::cout << "Press Enter to record message..." << std::endl;
        std::getline(std::cin, ip);
        auto rec_result = recorder.Record(max_frames, 1.5s);
        const auto bytes = rec_result.frames * frame_size;
        if (bytes == 0 ){
            std::cout << "Recording error" << std::endl;
            continue;
        }
        std::cout << "Recording done" << std::endl;

        socket.send_to(net::buffer(rec_result.data, bytes), endpoint);
        std::cout << "Message sent!" << std::endl;
    }
}

void StartServer(std::uint16_t port) {
    net::io_context io_context;
    udp::socket socket(io_context, udp::endpoint(udp::v4(), port));

    Player player(ma_format_u8, 1);
    constexpr std::size_t max_message_size = 65000;
    const std::size_t frame_size = static_cast<std::size_t>(player.GetFrameSize());
    const std::size_t max_frames = max_message_size / frame_size;

    std::array<char, 65507> buffer;

    std::cout << "Listening on port: " << port << std::endl;

    while (true) {
        udp::endpoint sender_endpoint;
        const auto size = socket.receive_from(net::buffer(buffer), sender_endpoint);
        if (size == 0 || size % frame_size != 0) {
            std::cerr << "Invalid audio message size" << std::endl;
            continue;
        }

        const auto frames = size / frame_size;
        std::cout << "PLaying audio message" << std::endl;
        player.PlayBuffer(buffer.data(), frames, 1.5s);
        std::cout << "Plying done" << std::endl;
    }
}