#ifdef WIN32
#include <sdkddkver.h>
#endif

#include "seabattle.h"

#include <atomic>
#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <string_view>

namespace net = boost::asio;
using net::ip::tcp;
using namespace std::literals;

void PrintFieldPair(const SeabattleField& left, const SeabattleField& right) {
    auto left_pad = "  "s;
    auto delimeter = "    "s;
    std::cout << left_pad;
    SeabattleField::PrintDigitLine(std::cout);
    std::cout << delimeter;
    SeabattleField::PrintDigitLine(std::cout);
    std::cout << std::endl;
    for (size_t i = 0; i < SeabattleField::field_size; ++i) {
        std::cout << left_pad;
        left.PrintLine(std::cout, i);
        std::cout << delimeter;
        right.PrintLine(std::cout, i);
        std::cout << std::endl;
    }
    std::cout << left_pad;
    SeabattleField::PrintDigitLine(std::cout);
    std::cout << delimeter;
    SeabattleField::PrintDigitLine(std::cout);
    std::cout << std::endl;
}

template <size_t sz>
static std::optional<std::string> ReadExact(tcp::socket& socket) {
    boost::array<char, sz> buf;
    boost::system::error_code ec;

    net::read(socket, net::buffer(buf), net::transfer_exactly(sz), ec);

    if (ec) {
        return std::nullopt;
    }

    return {{buf.data(), sz}};
}

static bool WriteExact(tcp::socket& socket, std::string_view data) {
    boost::system::error_code ec;

    net::write(socket, net::buffer(data), net::transfer_exactly(data.size()), ec);

    return !ec;
}

class SeabattleAgent {
public:
    SeabattleAgent(const SeabattleField& field)
        : my_field_(field) {
    }

    void StartGame(tcp::socket& socket, bool my_initiative) {
        while (!IsGameEnded()) {
            PrintFields();
            if (my_initiative) {
                std::cout << "Your turn: ";
                std::string input;
                if (!(std::cin >> input)) {
                    std::cout << "Input ended" << std::endl;
                    return;
                }
                const auto move = ParseMove(input);
                if (!move) {
                    std::cout << "Invalid move coordiantes" << std::endl;
                    continue;
                }

                auto [row, column] = *move;

                if (other_field_(column, row) != SeabattleField::State::UNKNOWN) {
                    std::cout << "This cell is already known" << std::endl;
                    continue;
                }

                if (!SendMove(socket, *move)) {
                    std::cout << "Failed to send move" << std::endl;
                    return;
                }

                auto result = ReadResult(socket);

                if (!result) {
                    std::cout << "Failed to read result" << std::endl;
                    return;
                }

                switch (*result) {
                    case SeabattleField::ShotResult::HIT:
                        other_field_.MarkHit(column, row);
                        std::cout << "Hit!" << std::endl;
                        break;
                    case SeabattleField::ShotResult::KILL:
                        other_field_.MarkKill(column, row);
                        std::cout << "Kill!" << std::endl;
                        break;
                    case SeabattleField::ShotResult::MISS:
                        other_field_.MarkMiss(column, row);
                        std::cout << "Miss!" << std::endl;
                        break;
                    default:
                        break;
                }

                my_initiative = *result != SeabattleField::ShotResult::MISS;
            } else {
                std::cout << "Waiting for turn: " << std::endl;
                auto move = ReadMove(socket);
                if (!move) {
                    std::cout << "Failed to read move" << std::endl;
                    return;
                }
                auto [row, column] = *move;
                auto result = my_field_.Shoot(column, row);
                if (!SendResult(socket, result)) {
                    std::cout << "Failed to to send result" << std::endl;
                    return;
                }

                std::cout << "Shot to " << MoveToString(*move) << std::endl;
                my_initiative = result == SeabattleField::ShotResult::MISS;
            }
        }
        PrintFields();
        if (my_field_.IsLoser())
            std::cout << "You lost..." << std::endl;
        else
            std::cout << "You won!" << std::endl;
    }

private:
    static std::optional<std::pair<int, int>> ParseMove(const std::string_view& sv) {
        if (sv.size() != 2) return std::nullopt;

        int p1 = sv[0] - 'A', p2 = sv[1] - '1';

        if (p1 < 0 || p1 >= 8) return std::nullopt;
        if (p2 < 0 || p2 >= 8) return std::nullopt;

        return {{p1, p2}};
    }

    static std::string MoveToString(std::pair<int, int> move) {
        char buff[] = {static_cast<char>(move.first + 'A'), static_cast<char>(move.second + '1')};
        return {buff, 2};
    }

    void PrintFields() const {
        PrintFieldPair(my_field_, other_field_);
    }

    bool IsGameEnded() const {
        return my_field_.IsLoser() || other_field_.IsLoser();
    }

    static bool SendMove(tcp::socket& socket, std::pair<int, int> move) {
        return WriteExact(socket, MoveToString(move));
    }

    static std::optional<std::pair<int, int>> ReadMove(tcp::socket& socket) {
        auto data = ReadExact<2>(socket);
        if (!data)
            return std::nullopt;
        return ParseMove(*data);
    }

    static bool SendResult(tcp::socket& socket, SeabattleField::ShotResult result) {
        const char val = static_cast<char>(result);
        return WriteExact(socket, std::string_view(&val, 1));
    }

    static std::optional<SeabattleField::ShotResult> ReadResult(tcp::socket& socket) {
        auto data = ReadExact<1>(socket);
        if (!data || static_cast<SeabattleField::ShotResult>((*data)[0]) > SeabattleField::ShotResult::KILL)
            return std::nullopt;
        return static_cast<SeabattleField::ShotResult>((*data)[0]);
    }

    // TODO: добавьте методы по вашему желанию

private:
    SeabattleField my_field_;
    SeabattleField other_field_;
};

void StartServer(const SeabattleField& field, unsigned short port) {
    SeabattleAgent agent(field);

    net::io_context io_context;
    tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), port));

    boost::system::error_code ec;
    tcp::socket socket{io_context};

    std::cout << "Waiting for connection..." << std::endl;
    acceptor.accept(socket, ec);

    if (ec) {
        std::cout << "Failed to accept connection" << std::endl;
        return;
    }

    agent.StartGame(socket, false);
};

void StartClient(const SeabattleField& field, const std::string& ip_str, unsigned short port) {
    SeabattleAgent agent(field);

    net::io_context io_context;
    tcp::socket socket{io_context};
    boost::system::error_code ec;

    const auto address = net::ip::make_address(ip_str, ec);

    if (ec) {
        std::cout << "Error: invalid ip address format" << std::endl;
        return;
    }

    tcp::endpoint endpoint(address, port);
    socket.connect(endpoint, ec);

    if (ec) {
        std::cout << "Error: failed to connect server" << std::endl;
        return;
    }

    agent.StartGame(socket, true);
};

int main(int argc, const char** argv) {
    if (argc != 3 && argc != 4) {
        std::cout << "Usage: program <seed> [<ip>] <port>" << std::endl;
        return 1;
    }

    std::mt19937 engine(std::stoi(argv[1]));
    SeabattleField fieldL = SeabattleField::GetRandomField(engine);

    if (argc == 3) {
        StartServer(fieldL, std::stoi(argv[2]));
    } else if (argc == 4) {
        StartClient(fieldL, argv[2], std::stoi(argv[3]));
    }
}
