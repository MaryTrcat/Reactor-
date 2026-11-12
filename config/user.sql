CREATE DATABASE IF NOT EXISTS reactor_user_service DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

USE reactor_user_service;

CREATE TABLE IF NOT EXISTS users (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  username VARCHAR(64) NOT NULL UNIQUE,
  password_hash VARCHAR(255) NOT NULL,
  role VARCHAR(32) NOT NULL DEFAULT 'user',
  frozen TINYINT(1) NOT NULL DEFAULT 0,
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  -- 列表页默认按 id 排序，状态过滤也常用，组合索引能覆盖大部分查询
  KEY idx_users_role_frozen (role, frozen)
);

-- 老版本 password_hash 只有 VARCHAR(128)，PBKDF2 的
-- "pbkdf2-sha256$12000$<24字节盐>$<44字节摘要>" 约 100 字符，
-- 但为了给未来提升迭代次数/换算法留余量，这里统一放宽到 255。
SET @hash_column_length = (
  SELECT CHARACTER_MAXIMUM_LENGTH
  FROM information_schema.columns
  WHERE table_schema = DATABASE() AND table_name = 'users' AND column_name = 'password_hash'
);
SET @alter_hash_sql = IF(
  @hash_column_length IS NOT NULL AND @hash_column_length < 255,
  'ALTER TABLE users MODIFY COLUMN password_hash VARCHAR(255) NOT NULL',
  'SELECT 1'
);
PREPARE alter_hash_statement FROM @alter_hash_sql;
EXECUTE alter_hash_statement;
DEALLOCATE PREPARE alter_hash_statement;

-- 审计日志：只追加不更新，查询按 id 倒序分页
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

-- 兼容旧表：补 frozen 列
SET @has_frozen_column = (
  SELECT COUNT(*)
  FROM information_schema.columns
  WHERE table_schema = DATABASE()
    AND table_name = 'users'
    AND column_name = 'frozen'
);
SET @migration_sql = IF(
  @has_frozen_column = 0,
  'ALTER TABLE users ADD COLUMN frozen TINYINT(1) NOT NULL DEFAULT 0 AFTER role',
  'SELECT 1'
);
PREPARE migration_statement FROM @migration_sql;
EXECUTE migration_statement;
DEALLOCATE PREPARE migration_statement;
