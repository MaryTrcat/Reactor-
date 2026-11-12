CREATE DATABASE IF NOT EXISTS reactor_user_service DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

USE reactor_user_service;

CREATE TABLE IF NOT EXISTS users (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  username VARCHAR(64) NOT NULL UNIQUE,
  password_hash VARCHAR(255) NOT NULL,
  role VARCHAR(32) NOT NULL DEFAULT 'user',
  frozen TINYINT(1) NOT NULL DEFAULT 0,
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  KEY idx_users_role_frozen (role, frozen)
);

CREATE TABLE IF NOT EXISTS audit_logs (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  actor VARCHAR(64) NOT NULL,
  action VARCHAR(64) NOT NULL,
  target VARCHAR(128) NOT NULL DEFAULT '',
  result VARCHAR(32) NOT NULL DEFAULT '',
  detail VARCHAR(512) NOT NULL DEFAULT '',
  actor_ip VARCHAR(64) NOT NULL DEFAULT '',
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  KEY idx_audit_created_at (created_at),
  KEY idx_audit_action_created (action, created_at),
  KEY idx_audit_actor_created (actor, created_at)
) ENGINE = InnoDB DEFAULT CHARSET = utf8mb4;

CREATE USER IF NOT EXISTS 'reactor_user'@'localhost' IDENTIFIED BY 'reactor_pass';
CREATE USER IF NOT EXISTS 'reactor_user'@'127.0.0.1' IDENTIFIED BY 'reactor_pass';
GRANT ALL PRIVILEGES ON reactor_user_service.* TO 'reactor_user'@'localhost';
GRANT ALL PRIVILEGES ON reactor_user_service.* TO 'reactor_user'@'127.0.0.1';
FLUSH PRIVILEGES;

-- admin / 123456 的密码哈希是改造前的 std::hash 结果：
-- 服务端首次用该账号登录时会自动识别为老格式并原地升级成 PBKDF2。
INSERT INTO users(username, password_hash, role, frozen)
VALUES ('admin', '6995202307657208293', 'admin', 0)
ON DUPLICATE KEY UPDATE role = 'admin', frozen = 0;
