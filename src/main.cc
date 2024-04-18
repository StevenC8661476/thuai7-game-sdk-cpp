#include <fmt/format.h>
#include <hv/Event.h>
#include <hv/EventLoop.h>
#include <spdlog/common.h>
#include <spdlog/spdlog.h>

#include <cxxopts.hpp>
#include <optional>
#include <string>

#include "agent/agent.h"

void Setup(thuai7_agent::Agent& agent);
void Loop(thuai7_agent::Agent& agent);

struct Options {
  std::string server;
  std::string token;
};

constexpr auto kDefaultServer = "ws://localhost:14514";
constexpr auto kDefaultToken = "1919810";
constexpr auto kLoopInterval = 1000;  // In milliseconds.

auto ParseOptions(int argc, char** argv) -> std::optional<Options> {
  cxxopts::Options options("agent");
  options.add_options()(
      "server", "Server address",
      cxxopts::value<std::string>()->default_value(kDefaultServer))(
      "token", "Agent token",
      cxxopts::value<std::string>()->default_value(kDefaultToken))(
      "h,help", "Print usage");
  auto result = options.parse(argc, argv);

  if (result.count("help") > 0) {
    std::cout << options.help() << std::endl;
    return std::nullopt;
  }

  return Options{
      .server = result["server"].as<std::string>(),
      .token = result["token"].as<std::string>(),
  };
}

auto main(int argc, char* argv[]) -> int {
#ifdef NDEBUG
  spdlog::set_level(spdlog::level::info);
#else
  spdlog::set_level(spdlog::level::debug);
#endif

  auto options = ParseOptions(argc, argv);
  if (!options.has_value()) {
    return 0;
  }

  hv::EventLoopPtr event_loop = std::make_shared<hv::EventLoop>();

  thuai7_agent::Agent agent(options->token, event_loop, kLoopInterval);

  spdlog::info("{} is starting with server {}", agent, options->server);

  event_loop->runInLoop([&] { agent.Connect(options->server); });

  bool is_previous_connected = false;
  bool is_previous_game_ready = false;
  bool is_setup = false;

  event_loop->setInterval(kLoopInterval, [&](hv::TimerID) {
    // Check connection status.
    if (!agent.IsConnected()) {
      if (is_previous_connected) {
        spdlog::error("{} is disconnected", agent);
        is_previous_connected = false;
      }
      spdlog::debug("{} is waiting for connection", agent);
      return;
    }

    if (!is_previous_connected) {
      spdlog::info("{} is connected", agent);
      is_previous_connected = true;
    }

    // Check game ready status.
    if (!agent.IsGameReady()) {
      if (is_previous_game_ready) {
        spdlog::error("{} is no longer in a ready game", agent);
        is_previous_game_ready = false;
      }
      spdlog::debug("{} is waiting for the game to be ready", agent);
      return;
    }

    if (!is_previous_game_ready) {
      spdlog::info("{} is in a ready game", agent);
      is_previous_game_ready = true;
    }

    // Setup agent if not done yet.
    if (!is_setup) {
      Setup(agent);
      spdlog::info("{} is setup", agent);
      is_setup = true;
    }

    Loop(agent);
  });

  event_loop->run();

  return 0;
}
