# db init sql (creates all four TC databases + user)
CREATE DATABASE IF NOT EXISTS auth;
CREATE DATABASE IF NOT EXISTS characters;
CREATE DATABASE IF NOT EXISTS world;
CREATE DATABASE IF NOT EXISTS hotfixes;
CREATE USER IF NOT EXISTS 'trinity'@'%' IDENTIFIED BY 'trinity';
GRANT ALL PRIVILEGES ON auth.* TO 'trinity'@'%';
GRANT ALL PRIVILEGES ON characters.* TO 'trinity'@'%';
GRANT ALL PRIVILEGES ON world.* TO 'trinity'@'%';
GRANT ALL PRIVILEGES ON hotfixes.* TO 'trinity'@'%';
FLUSH PRIVILEGES;
