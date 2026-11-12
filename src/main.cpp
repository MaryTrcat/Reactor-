#include "base/Config.h"
#include "base/Logger.h"
#include "business/AuditService.h"
#include "business/FileAuditLogRepository.h"
#include "business/InMemoryAuditLogRepository.h"
#include "business/InMemoryUserRepository.h"
#include "business/LoginAttemptTracker.h"
#include "business/SessionRegistry.h"
#include "business/UserHandler.h"
#include "business/UserService.h"
#include "net/TcpServer.h"

#ifdef HAVE_MYSQL
#include "db/MySqlAuditLogRepository.h"
#include "db/MySqlConnectionPool.h"
#include "db/MySqlUserRepository.h"
#endif

#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <string>

int main(int argc, char** argv) {
    const std::string config_path = argc > 1 ? argv[1] : "config/server.conf";

    base::Config config;
    config.load(config_path);

    base::Logger::instance().setFile(config.getString("log_file", "server.log"));

    const auto host = config.getString("host", "0.0.0.0");
    const auto port = static_cast<std::uint16_t>(config.getInt("port", 9000));

    net::TcpServerOptions options;
    options.worker_threads = static_cast<std::size_t>(config.getInt("worker_threads", 4));
    options.io_threads = static_cast<std::size_t>(config.getInt("io_threads", 4));
    options.idle_timeout_seconds = config.getInt("idle_timeout_seconds", 300);
    options.shutdown_poll_ms = config.getInt("shutdown_poll_ms", 200);

    std::shared_ptr<business::UserRepository> repository;
    std::shared_ptr<business::AuditLogRepository> audit_repository;

#ifdef HAVE_MYSQL
    std::shared_ptr<db::MySqlConnectionPool> pool;
    if (config.getString("storage", "mysql") == "mysql") {
        try {
            db::MySqlConfig mysql;
            mysql.host = config.getString("mysql_host", "127.0.0.1");
            mysql.port = static_cast<unsigned int>(config.getInt("mysql_port", 3306));
            mysql.user = config.getString("mysql_user", "root");
            mysql.password = config.getString("mysql_password", "");
            mysql.database = config.getString("mysql_database", "reactor_user_service");

            pool = std::make_shared<db::MySqlConnectionPool>(
                mysql, static_cast<std::size_t>(config.getInt("mysql_pool_size", 4)));
            repository = std::make_shared<db::MySqlUserRepository>(pool);
            audit_repository = std::make_shared<db::MySqlAuditLogRepository>(pool);
        } catch (const std::exception& ex) {
            base::logWarning(std::string("MySQL unavailable, falling back to local storage: ") + ex.what());
        }
    }
#endif

    if (!repository) {
        base::logWarning("using in-memory repository; install MySQL client libraries and set storage=mysql for persistent data");
        repository = std::make_shared<business::InMemoryUserRepository>();
    }
    if (!audit_repository) {
        // 没有 MySQL 时把审计日志落到文件，保证控制台 Demo 也能看到操作留痕。
        audit_repository = std::make_shared<business::FileAuditLogRepository>(
            config.getString("audit_log_file", "data/audit.log"));
    }
    const bool using_mysql =
#ifdef HAVE_MYSQL
        dynamic_cast<const db::MySqlUserRepository*>(repository.get()) != nullptr;
#else
        false;
#endif

    auto service = std::make_shared<business::UserService>(repository);
    // 未登录用户自助注册普通账号的开关（注册出的账号角色固定为 user）。
    service->setAllowSelfRegister(config.getInt("allow_self_register", 1) != 0);
    auto sessions = std::make_shared<business::SessionRegistry>();
    auto audit = std::make_shared<business::AuditService>(audit_repository);

    business::LoginAttemptTracker::Config limiter_config;
    limiter_config.max_failures = config.getInt("login_max_failures", 5);
    limiter_config.window_seconds = config.getInt("login_window_seconds", 300);
    limiter_config.lock_seconds = config.getInt("login_lock_seconds", 300);
    auto login_limiter = std::make_shared<business::LoginAttemptTracker>(limiter_config);

    auto handler = std::make_shared<business::UserHandler>(service, sessions, audit, login_limiter);

    try {
        net::TcpServer server(host, port, handler, options, sessions);
        // STATS_REQUEST 的数据源：TcpServer 线程安全地收集运行指标。
        const std::string storage_backend = using_mysql ? "mysql" : "memory";
        handler->setStatsProvider([&server, &options, storage_backend]() {
            auto stats = server.statsSnapshot();
            stats.storage = storage_backend;
            return stats;
        });
        std::cout << "reactor_user_server listening on " << host << ":" << port << " io_threads=" << options.io_threads
                  << " workers=" << options.worker_threads << " idle_timeout=" << options.idle_timeout_seconds << "s\n";
        server.run();
    } catch (const std::exception& ex) {
        base::logError(ex.what());
        std::cerr << "server failed: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
