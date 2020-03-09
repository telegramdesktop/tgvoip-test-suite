#include <utility>
#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <algorithm>
#include <fstream>
#include <sstream>

#include <tgvoip/VoIPController.h>
#include <tgvoip/json11.hpp>

extern "C" {
#include <getopt.h>
}

#include <cstring>
#include <cstdio>

#include "io.hpp"
#include "call_common.hpp"



static tgvoip::Semaphore barrier{1, 0};

std::string readfile(const std::string& filename) {
    std::FILE* file = std::fopen(filename.c_str(), "rt");
    if (!file || std::ferror(file))
        throw std::runtime_error{"Cannot read file"};

    std::ostringstream buffer;
    constexpr size_t bufsz = 1024;
    char buf[bufsz];

    while (!std::feof(file)) {
        size_t got = std::fread(buf, 1, bufsz, file);
        if (std::ferror(file)) {
            throw std::runtime_error{"Cannot read file"};
        }
        buffer << std::string_view(buf, got);
    }

    return buffer.str();
}

static void finalize(tgvoip::VoIPController*) {
    std::cerr << "Transmission done" << std::endl;
    barrier.Release();
}

std::shared_ptr<tgvoip::VoIPController> init_controller(
    tgvoipcontest::CallMode mode, std::vector<tgvoip::Endpoint> endpoints,
    std::vector<char> encryption_key,
    const char* input_file, const char* output_file
) {
    auto controller = std::make_shared<tgvoip::VoIPController>();
    controller->SetRemoteEndpoints(std::move(endpoints), false,
                                   controller->GetConnectionMaxLayer() /* assuming equal versions */);
    controller->SetEncryptionKey(encryption_key.data(), mode == tgvoipcontest::CallMode::CALLER);

    const auto audio_io = new tgvoipcontest::OpusAudioIO(
        input_file, output_file,
        [controller]() {
            finalize(controller.get());
        }
    );
    controller->SetPredefinedAudioIO(audio_io);
    controller->SetConfig(tgvoip::VoIPController::Config{
        5.0
    });

    tgvoip::VoIPController::Callbacks callbacks{};
    callbacks.connectionStateChanged = [](tgvoip::VoIPController* controller, int state) {
        if (state == tgvoip::STATE_FAILED) {
            finalize(controller);
        }
    };
    controller->SetCallbacks(callbacks);
    controller->SetNetworkType(tgvoip::NET_TYPE_LTE);

    return controller;
}

struct Options {
    std::string relay_address;
    std::string tag;

    std::string input_file;
    std::string output_file;
    std::string config_file;
    std::string token;
    std::vector<char> encryption_key;
    tgvoipcontest::CallMode mode = tgvoipcontest::CallMode::CALLEE;
};

static void usage() {
    std::cout << "Usage: \n"
                 "tgvoipcall refl:port tag_hex -k encryption_key_hex -i sound_A.opus -o sound_out_B.opus "
                 "-c config.json -r (caller | callee)"
              << std::endl;
    std::abort();
}

static Options parse_options(int argc, char* argv[]) {
    Options result;
    std::string processed;

    for (int c; (c = getopt(argc, argv, "k:i:o:c:r:t:d")) != -1;) {
        processed.push_back(static_cast<char>(c));

        switch (c) {
            case 'i':
                result.input_file = optarg;
                break;
            case 'o':
                result.output_file = optarg;
                break;
            case 'k':
                result.encryption_key = tgvoipcontest::from_hex<char>(optarg);
                break;
            case 'c':
                result.config_file = optarg;
                break;
            case 'r':
                if (!std::strcmp(optarg, "callee"))
                    result.mode = tgvoipcontest::CallMode::CALLEE;
                else if (!std::strcmp(optarg, "caller"))
                    result.mode = tgvoipcontest::CallMode::CALLER;
                else
                    throw std::runtime_error{"Invalid argument (-r)"};
                break;

            case '?':
            default:
                throw std::runtime_error{"Invalid argument"};
        }
    }
    std::sort(processed.begin(), processed.end());

    if (processed != "cikor" || optind != argc - 2)
        throw std::runtime_error{"Invalid arguments"};

    result.relay_address = argv[optind];
    result.tag = argv[optind + 1];

    return result;
}

int main(int argc, char* argv[]) {
    Options opts;
    try {
        opts = parse_options(argc, argv);
    } catch (std::runtime_error&) {
        usage();
        return 1; // unreachable
    }

    std::vector<tgvoip::Endpoint> endpoints;
    size_t colon_pos = opts.relay_address.find(':');
    auto addr = tgvoip::IPv4Address{opts.relay_address.substr(0, colon_pos)};
    uint16_t port = std::stoul(opts.relay_address.substr(colon_pos + 1));

    endpoints.push_back(tgvoip::Endpoint{
        123, port, addr, tgvoip::IPv6Address{},
        tgvoip::Endpoint::UDP_RELAY, tgvoipcontest::from_hex<uint8_t>(opts.tag).data()
    });

    std::string err;
    auto cfg = json11::Json::parse(readfile(opts.config_file), err);
    tgvoipcontest::set_voip_config(cfg);

    auto c = init_controller(
        opts.mode, endpoints, opts.encryption_key, opts.input_file.c_str(), opts.output_file.c_str()
    );

    c->Connect();
    c->Start();

    barrier.Acquire();

    if (c->GetConnectionState() != tgvoip::STATE_FAILED)
        tgvoip::Thread::Sleep(3.0);
    else
        std::cerr << "Connection failed" << std::endl;

    c->Stop();
    std::cout << c->GetDebugLog() << std::endl;

    return 0;
}
