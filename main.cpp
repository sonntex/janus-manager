#include "json.hpp"
#include "hash.hpp"
#include "http.hpp"
#include "stream.hpp"
#include "uri.hpp"

#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/process.hpp>

static boost::log::trivial::severity_level severity = boost::log::trivial::info;
static std::string client_conf = "/etc/janus";
static std::string client_host = "127.0.0.1";
static std::uint16_t client_port = 8088;
static std::uint16_t client_admin_port = 8089;
static std::uint16_t client_rtp_port_min = 20000;
static std::uint16_t client_rtp_port_max = 20999;
static std::uint16_t client_rtp_port = client_rtp_port_min;
static std::string server_host = "127.0.0.1";
static std::uint16_t server_port = 8087;
static stream_info_map streams;

static boost::asio::io_context ioc;
static boost::asio::system_timer deadline(ioc);
static boost::process::child process;

static std::string application(char* argv0)
{ return boost::filesystem::path(argv0).filename().string(); }

static std::string make_target()
{ return "/janus"; }
static std::string make_target(std::uint64_t session_id)
{ return make_target() + "/" + std::to_string(session_id); }
static std::string make_target(std::uint64_t session_id, std::uint64_t session_plugin_id)
{ return make_target() + "/" + std::to_string(session_id) + "/" + std::to_string(session_plugin_id); }

static std::chrono::system_clock::time_point expires_at(const std::string& query)
{
    if (query == "expires_at=min")
        return std::chrono::system_clock::time_point::min();
    if (query == "expires_at=max")
        return std::chrono::system_clock::time_point::max();
    return std::chrono::system_clock::now() + std::chrono::seconds(60);
}

static bool send(
    http_client& c, const std::string& target, nlohmann::json& req_json, nlohmann::json& res_json)
{
    http_req req(boost::beast::http::verb::post, target, 11);
    http_res res;
    req.set(boost::beast::http::field::content_type, "application/json");
    req.keep_alive(true);
    req.body() = req_json.dump();
    req.prepare_payload();
    c(req, res);
    if (res.result() == boost::beast::http::status::ok) {
        try {
            res_json = nlohmann::json::parse(res.body());
        } catch (const std::exception& e) {
            BOOST_LOG_TRIVIAL(error) << "parse error: " << e.what();
            return false;
        }
        BOOST_LOG_TRIVIAL(trace) << "client send: " << req_json;
        BOOST_LOG_TRIVIAL(trace) << "client recv: " << res_json;
        return true;
    }
    return false;
}

static bool send_session_create(
    http_client& c, std::uint64_t& session_id)
{
    std::string target = make_target();
    nlohmann::json req_json;
    nlohmann::json res_json;
    req_json["janus"] = "create";
    req_json["transaction"] = md5();
    if (!send(c, target, req_json, res_json))
        return false;
    try {
        if (res_json["janus"] != "success" ||
            res_json["transaction"] != req_json["transaction"])
            return false;
        session_id = res_json["data"]["id"];
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "parse error: " << e.what();
        return false;
    }
    return true;
}

static bool send_session_plugin_attach(
    http_client& c, std::uint64_t session_id, std::uint64_t& session_plugin_id)
{
    session_plugin_id = 0;
    std::string target = make_target(session_id);
    nlohmann::json req_json;
    nlohmann::json res_json;
    req_json["janus"] = "attach";
    req_json["transaction"] = md5();
    req_json["plugin"] = "janus.plugin.streaming";
    if (!send(c, target, req_json, res_json))
        return false;
    try {
        if (res_json["janus"] != "success" ||
            res_json["transaction"] != req_json["transaction"])
            return false;
        session_plugin_id = res_json["data"]["id"];
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "parse error: " << e.what();
        return false;
    }
    return true;
}

static bool send_session_stream_create(
    http_client& c, std::uint64_t session_id, std::uint64_t session_plugin_id,
    const stream_info& stream)
{
    std::string target = make_target(session_id, session_plugin_id);
    nlohmann::json req_json;
    nlohmann::json res_json;
    req_json["janus"] = "message";
    req_json["transaction"] = md5();
    req_json["body"]["request"] = "create";
    req_json["body"]["id"] = stream.id;
    req_json["body"]["type"] = "rtp";
    req_json["body"]["video"] = true;
    req_json["body"]["videoport"] = stream.port;
    req_json["body"]["videopt"] = 96;
    req_json["body"]["videortpmap"] = "H264/90000";
    req_json["body"]["videofmtp"] = "profile-level-id=42e01f;packetization-mode=1";
    req_json["body"]["audio"] = true;
    req_json["body"]["audioport"] = stream.port + 2;
    req_json["body"]["audiopt"] = 8;
    req_json["body"]["audiortpmap"] = "PCMA/8000/1";
    req_json["body"]["is_private"] = true;
    if (!send(c, target, req_json, res_json))
        return false;
    try {
        if (res_json["janus"] != "success" ||
            res_json["transaction"] != req_json["transaction"] ||
            res_json["plugindata"]["data"].count("created") == 0)
            return false;
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "parse error: " << e.what();
        return false;
    }
    return true;
}

static bool send_session_stream_remove(
    http_client& c, std::uint64_t session_id, std::uint64_t session_plugin_id,
    const stream_info& stream)
{
    std::string target = make_target(session_id, session_plugin_id);
    nlohmann::json req_json;
    nlohmann::json res_json;
    req_json["janus"] = "message";
    req_json["transaction"] = md5();
    req_json["body"]["request"] = "destroy";
    req_json["body"]["id"] = stream.id;
    if (!send(c, target, req_json, res_json))
        return false;
    try {
        if (res_json["janus"] != "success" ||
            res_json["transaction"] != req_json["transaction"])
            return false;
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "parse error: " << e.what();
        return false;
    }
    return true;
}

static nlohmann::json stream_to_json(const stream_info& stream)
{
    nlohmann::json res_json;
    res_json["id"] = stream.id;
    res_json["video_port"] = stream.port;
    res_json["audio_port"] = stream.port + 2;
    res_json["expires_at"] =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            stream.expires_at.time_since_epoch()).count();
    return res_json;
}

static void handle_ok(http_req& req, http_res& res)
{ res = http_res(boost::beast::http::status::ok, req.version()); }
static void handle_internal_server_error(http_req& req, http_res& res)
{ res = http_res(boost::beast::http::status::internal_server_error, req.version()); }
static void handle_bad_request(http_req& req, http_res& res)
{ res = http_res(boost::beast::http::status::bad_request, req.version()); }
static void handle_found(http_req& req, http_res& res)
{ res = http_res(boost::beast::http::status::found, req.version()); }
static void handle_not_found(http_req& req, http_res& res)
{ res = http_res(boost::beast::http::status::not_found, req.version()); }
static void handle_method_not_allowed(http_req& req, http_res& res)
{ res = http_res(boost::beast::http::status::method_not_allowed, req.version()); }

static void handle_streams_post(http_req& req, http_res& res,
    std::chrono::system_clock::time_point expires_at)
{
    auto stream = make_stream(streams,
        client_rtp_port_min, client_rtp_port_max, client_rtp_port);
    boost::asio::io_context ioc;
    http_client c(ioc, make_endpoint(client_host, client_port));
    std::uint64_t session_id;
    std::uint64_t session_plugin_id;
    if (!send_session_create(c, session_id) ||
        !send_session_plugin_attach(c, session_id, session_plugin_id) ||
        !send_session_stream_create(c, session_id, session_plugin_id, stream)) {
        BOOST_LOG_TRIVIAL(error) << "client error";
        handle_internal_server_error(req, res);
        return;
    }
    keep_alive(stream, expires_at);
    streams[stream.id] = std::move(stream);
    handle_ok(req, res);
    nlohmann::json res_json;
    res_json["stream"] = stream_to_json(stream);
    res.body() = res_json.dump();
    res.prepare_payload();
}

static void handle_streams_get(http_req& req, http_res& res)
{
    handle_ok(req, res);
    nlohmann::json res_json;
    for (auto it = streams.begin(); it != streams.end(); ++it) {
        auto& stream = it->second;
        res_json["streams"].push_back(stream_to_json(stream));
    }
    res.body() = res_json.dump();
    res.prepare_payload();
}

static void handle_streams_put(http_req& req, http_res& res,
    std::chrono::system_clock::time_point expires_at)
{
    handle_ok(req, res);
    nlohmann::json res_json;
    for (auto it = streams.begin(); it != streams.end(); ++it) {
        auto& stream = it->second;
        keep_alive(stream, expires_at);
        res_json["streams"].push_back(stream_to_json(stream));
    }
    res.body() = res_json.dump();
    res.prepare_payload();
}

static void handle_streams_id_get(http_req& req, http_res& res, const stream_info& stream)
{
    handle_ok(req, res);
    nlohmann::json res_json;
    res_json["stream"] = stream_to_json(stream);
    res.body() = res_json.dump();
    res.prepare_payload();
}

static void handle_streams_id_put(http_req& req, http_res& res, stream_info& stream,
    std::chrono::system_clock::time_point expires_at)
{
    keep_alive(stream, expires_at);
    handle_ok(req, res);
    nlohmann::json res_json;
    res_json["stream"] = stream_to_json(stream);
    res.body() = res_json.dump();
    res.prepare_payload();
}

static void handle(http_req& req, http_res& res)
{
    auto uri = make_uri(req.target().begin(), req.target().end());
    if (std::distance(uri.path.begin(), uri.path.end()) == 2 &&
        uri.path.is_absolute() && std::next(uri.path.begin(), 1)->string() == "streams") {
        switch (req.method()) {
        case boost::beast::http::verb::post: handle_streams_post(req, res, expires_at(uri.query)); break;
        case boost::beast::http::verb::get: handle_streams_get(req, res); break;
        case boost::beast::http::verb::put: handle_streams_put(req, res, expires_at(uri.query)); break;
        default: handle_method_not_allowed(req, res); break;
        }
    } else
    if (std::distance(uri.path.begin(), uri.path.end()) == 3 &&
        uri.path.is_absolute() && std::next(uri.path.begin(), 1)->string() == "streams") {
        auto it = streams.find(std::stoul(std::next(uri.path.begin(), 2)->string()));
        if (it == streams.end()) {
            handle_not_found(req, res);
            return;
        }
        auto& stream = it->second;
        switch (req.method()) {
        case boost::beast::http::verb::get: handle_streams_id_get(req, res, stream); break;
        case boost::beast::http::verb::put: handle_streams_id_put(req, res, stream, expires_at(uri.query)); break;
        default: handle_method_not_allowed(req, res); break;
        }
    } else
        handle_not_found(req, res);
    BOOST_LOG_TRIVIAL(trace) << "handle:"
        << " method=" << boost::beast::http::to_string(req.method())
        << " target=" << req.target()
        << " status=" << res.result_int();
}

static void handle_safe(http_req& req, http_res& res)
{
    try {
        handle(req, res);
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "client error: " << e.what();
        handle_internal_server_error(req, res);
    }
}

static void create(const stream_info_map& streams)
{
    if (streams.empty())
        return;
    boost::asio::io_context ioc;
    http_client c(ioc, make_endpoint(client_host, client_port));
    std::uint64_t session_id;
    std::uint64_t session_plugin_id;
    if (!send_session_create(c, session_id) ||
        !send_session_plugin_attach(c, session_id, session_plugin_id)) {
        BOOST_LOG_TRIVIAL(error) << "client error";
        return;
    }
   for (auto it = streams.begin(); it != streams.end(); ++it) {
        auto& stream = it->second;
        if (!send_session_stream_create(c, session_id, session_plugin_id, stream)) {
            BOOST_LOG_TRIVIAL(error) << "client error";
            continue;
        }
    }
}

static void remove(const stream_info_map& streams)
{
    if (streams.empty())
        return;
    boost::asio::io_context ioc;
    http_client c(ioc, make_endpoint(client_host, client_port));
    std::uint64_t session_id;
    std::uint64_t session_plugin_id;
    if (!send_session_create(c, session_id) ||
        !send_session_plugin_attach(c, session_id, session_plugin_id)) {
        BOOST_LOG_TRIVIAL(error) << "client error";
        return;
    }
    for (auto it = streams.begin(); it != streams.end(); ++it) {
        auto& stream = it->second;
        if (!send_session_stream_remove(c, session_id, session_plugin_id, stream)) {
            BOOST_LOG_TRIVIAL(error) << "client error";
            continue;
        }
    }
}

static void spawn()
{
    auto path = boost::process::search_path("janus");
    BOOST_LOG_TRIVIAL(debug) << "spawn " << path.string();
    process = boost::process::child(path, std::string("--configs-folder=") + client_conf,
        boost::process::std_out > boost::process::null,
        boost::process::std_err > boost::process::null);
    std::size_t retry = 0;
    for ( ; retry < retries; ++retry) {
        try {
            create(streams);
            break;
        } catch (const std::exception& e) {
            if (retry == retries)
                throw e;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

static void respawn()
{
    bool dead = false;
    try {
        dead = process.wait_for(std::chrono::seconds(0));
    } catch (const std::exception&) {
        dead = true;
    }
    if (dead)
        spawn();
}

static void start_deadline()
{
    deadline.expires_from_now(std::chrono::seconds(1));
    deadline.async_wait(
        [](boost::system::error_code ec) {
            if (ec == boost::asio::error::operation_aborted)
                return;
            try {
                respawn();
            } catch (const std::exception& e) {
                BOOST_LOG_TRIVIAL(error) << "system error: " << e.what();
            }
            try {
                remove(expired(streams));
            } catch (const std::exception& e) {
                BOOST_LOG_TRIVIAL(error) << "client error: " << e.what();
            }
            start_deadline();
        });
}

static void work()
{
    BOOST_LOG_TRIVIAL(info) << "init";
    BOOST_LOG_TRIVIAL(info) << "client conf: " << client_conf;
    BOOST_LOG_TRIVIAL(info) << "client host: " << client_host;
    BOOST_LOG_TRIVIAL(info) << "client port: " << client_port;
    BOOST_LOG_TRIVIAL(info) << "client admin port: " << client_admin_port;
    BOOST_LOG_TRIVIAL(info) << "client rtp port min: " << client_rtp_port_min;
    BOOST_LOG_TRIVIAL(info) << "client rtp port max: " << client_rtp_port_max;
    BOOST_LOG_TRIVIAL(info) << "server host: " << server_host;
    BOOST_LOG_TRIVIAL(info) << "server port: " << server_port;
    BOOST_LOG_TRIVIAL(info) << "work";
    spawn();
    start_deadline();
    http_server s(ioc, make_endpoint(server_host, server_port), handle_safe);
    try {
        ioc.run();
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(fatal) << "server error: " << e.what();
    }
    BOOST_LOG_TRIVIAL(info) << "done";
}

static void init(int argc, char* argv[])
{
    std::srand(std::time(nullptr));
    boost::log::register_simple_formatter_factory<boost::log::trivial::severity_level, char>("Severity");
    auto sink0 = boost::log::add_console_log(std::cerr,
        boost::log::keywords::format = "[%TimeStamp%][%Severity%]: %Message%");
    sink0->set_filter(boost::log::trivial::severity >= severity);
    auto sink1 = boost::log::add_file_log(
        boost::log::keywords::format = "[%TimeStamp%][%Severity%]: %Message%",
        boost::log::keywords::file_name = std::string("/var/log/") + application(argv[0]) + ".log.%N",
        boost::log::keywords::rotation_size = 1024 * 1024 * 10,
        boost::log::keywords::auto_flush = true,
        boost::log::keywords::open_mode = std::ios_base::app);
    sink1->set_filter(boost::log::trivial::severity >= severity);
    boost::log::add_common_attributes();
}

static void usage(int argc, char* argv[])
{
    std::printf("Usage: %s [OPTIONS]", application(argv[0]).c_str());
    std::printf("\n  -h help");
    std::printf("\n  -v verbose");
    std::printf("\n  -d arg (%s) client conf", client_conf.c_str());
    std::printf("\n  -q arg (%u) client port", client_port);
    std::printf("\n  -y arg (%u) client min rtp port", client_rtp_port_min);
    std::printf("\n  -z arg (%u) client max rtp port", client_rtp_port_max);
    std::printf("\n  -l arg (%s) server host", server_host.c_str());
    std::printf("\n  -p arg (%u) server port", server_port);
    std::printf("\n");
    std::printf("\n");
    std::exit(0);
}

int main(int argc, char* argv[])
{
    int ret;
    while ((ret = getopt(argc, argv, "vq:d:y:z:l:p:h")) != -1) {
        switch (ret) {
        case 'v': severity = boost::log::trivial::trace; break;
        case 'd': client_conf = optarg; break;
        case 'q': client_port = std::stoul(optarg); break;
        case 'y': client_rtp_port_min = std::stoul(optarg); break;
        case 'z': client_rtp_port_max = std::stoul(optarg); break;
        case 'l': server_host = optarg; break;
        case 'p': server_port = std::stoul(optarg); break;
        case 'h':
        default:
            usage(argc, argv);
            break;
        }
    }
    if (optind != argc)
        usage(argc, argv);
    init(argc, argv);
    work();
    return 0;
}
