-- Runs once, when the db volume is created. The trinity user (MYSQL_USER) already exists at this point.
-- The worldserver fills these databases on its first start.
CREATE DATABASE IF NOT EXISTS `auth` DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
CREATE DATABASE IF NOT EXISTS `characters` DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
CREATE DATABASE IF NOT EXISTS `world` DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
CREATE DATABASE IF NOT EXISTS `hotfixes` DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

GRANT ALL PRIVILEGES ON `auth`.* TO 'trinity'@'%';
GRANT ALL PRIVILEGES ON `characters`.* TO 'trinity'@'%';
GRANT ALL PRIVILEGES ON `world`.* TO 'trinity'@'%';
GRANT ALL PRIVILEGES ON `hotfixes`.* TO 'trinity'@'%';
