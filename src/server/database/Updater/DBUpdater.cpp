/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "DBUpdater.h"
#include "BuiltInConfig.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "DatabaseLoader.h"
#include "GitRevision.h"
#include "Log.h"
#include "QueryResult.h"
#include "StartProcess.h"
#include "UpdateFetcher.h"
#include "MySQLWorkaround.h"
#include "StringConvert.h"
#include <iterator>
#include "StringFormat.h"
#include <boost/filesystem/operations.hpp>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <random>
#include <string_view>

std::string DBUpdaterUtil::GetCorrectedMySQLExecutable()
{
    if (!corrected_path().empty())
        return corrected_path();
    else
        return BuiltInConfig::GetMySQLExecutable();
}

bool DBUpdaterUtil::CheckExecutable()
{
    boost::filesystem::path exe(GetCorrectedMySQLExecutable());
    if (!is_regular_file(exe))
    {
        exe = Trinity::SearchExecutableInPath("mysql");
        if (!exe.empty() && is_regular_file(exe))
        {
            // Correct the path to the cli
            corrected_path() = absolute(exe).generic_string();
            return true;
        }

        TC_LOG_FATAL("sql.updates", "Didn't find any executable MySQL binary at \'{}\' or in path, correct the path in the *.conf (\"MySQLExecutable\").",
            absolute(exe).generic_string());

        return false;
    }
    return true;
}

std::string& DBUpdaterUtil::corrected_path()
{
    static std::string path;
    return path;
}

namespace
{
// Classic 1.60 fork: some sql/custom files name the default schema (UPDATE world.creature_template ..., DELETE FROM world.smart_scripts ...).
// For a database with another name (forever_world on a shared server) that schema prefix is rewritten to the configured name in the
// temporary copy that is applied; the file and its hash stay the same, so no update is applied again on servers using the default names.
std::string QualifyDefaultSchema(std::string const& sql, std::string const& defaultName, std::string const& database)
{
    static constexpr std::string_view keywords[] = { "UPDATE", "FROM", "INTO", "JOIN", "TABLE" };
    auto const isSpace = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };

    std::string out;
    out.reserve(sql.size());
    std::size_t copied = 0;
    for (std::size_t pos = sql.find(defaultName); pos != std::string::npos; pos = sql.find(defaultName, pos + defaultName.size()))
    {
        // `world`.table or world.table
        std::size_t begin = pos, end = pos + defaultName.size();
        if (begin > 0 && sql[begin - 1] == '`' && end < sql.size() && sql[end] == '`')
            --begin, ++end;
        if (end >= sql.size() || sql[end] != '.' || begin == 0 || !isSpace(sql[begin - 1]))
            continue;

        // preceded by one of the keywords
        std::size_t wordEnd = begin;
        while (wordEnd > 0 && isSpace(sql[wordEnd - 1]))
            --wordEnd;
        std::size_t wordBegin = wordEnd;
        while (wordBegin > 0 && std::isalpha(static_cast<unsigned char>(sql[wordBegin - 1])))
            --wordBegin;
        std::string_view const word(sql.data() + wordBegin, wordEnd - wordBegin);
        bool const isKeyword = std::any_of(std::begin(keywords), std::end(keywords), [&](std::string_view keyword)
        {
            return word.size() == keyword.size() && std::equal(word.begin(), word.end(), keyword.begin(),
                [](char a, char b) { return std::toupper(static_cast<unsigned char>(a)) == b; });
        });
        if (!isKeyword)
            continue;

        out.append(sql, copied, begin - copied);
        out.append("`").append(database).append("`");
        copied = end;
    }
    out.append(sql, copied, std::string::npos);
    return out;
}

std::string GetDefaultDatabaseName(std::string const& tableName)
{
    if (tableName == "Auth")
        return "auth";
    if (tableName == "Character")
        return "characters";
    if (tableName == "Hotfixes")
        return "hotfixes";
    return "world";
}
}

// Auth Database
template<>
std::string DBUpdater<LoginDatabaseConnection>::GetConfigEntry()
{
    return "Updates.Auth";
}

template<>
std::string DBUpdater<LoginDatabaseConnection>::GetTableName()
{
    return "Auth";
}

template<>
std::string DBUpdater<LoginDatabaseConnection>::GetBaseFile()
{
    return BuiltInConfig::GetSourceDirectory() +
        "/sql/base/auth_database.sql";
}

template<>
bool DBUpdater<LoginDatabaseConnection>::IsEnabled(uint32 const updateMask)
{
    // This way silences warnings under msvc
    return (updateMask & DatabaseLoader::DATABASE_LOGIN) ? true : false;
}

// World Database
template<>
std::string DBUpdater<WorldDatabaseConnection>::GetConfigEntry()
{
    return "Updates.World";
}

template<>
std::string DBUpdater<WorldDatabaseConnection>::GetTableName()
{
    return "World";
}

template<>
std::string DBUpdater<WorldDatabaseConnection>::GetBaseFile()
{
    return GitRevision::GetFullDatabase();
}

template<>
bool DBUpdater<WorldDatabaseConnection>::IsEnabled(uint32 const updateMask)
{
    // This way silences warnings under msvc
    return (updateMask & DatabaseLoader::DATABASE_WORLD) ? true : false;
}

template<>
BaseLocation DBUpdater<WorldDatabaseConnection>::GetBaseLocationType()
{
    return LOCATION_DOWNLOAD;
}

// Character Database
template<>
std::string DBUpdater<CharacterDatabaseConnection>::GetConfigEntry()
{
    return "Updates.Character";
}

template<>
std::string DBUpdater<CharacterDatabaseConnection>::GetTableName()
{
    return "Character";
}

template<>
std::string DBUpdater<CharacterDatabaseConnection>::GetBaseFile()
{
    return BuiltInConfig::GetSourceDirectory() +
        "/sql/base/characters_database.sql";
}

template<>
bool DBUpdater<CharacterDatabaseConnection>::IsEnabled(uint32 const updateMask)
{
    // This way silences warnings under msvc
    return (updateMask & DatabaseLoader::DATABASE_CHARACTER) ? true : false;
}

// Hotfix Database
template<>
std::string DBUpdater<HotfixDatabaseConnection>::GetConfigEntry()
{
    return "Updates.Hotfix";
}

template<>
std::string DBUpdater<HotfixDatabaseConnection>::GetTableName()
{
    return "Hotfixes";
}

template<>
std::string DBUpdater<HotfixDatabaseConnection>::GetBaseFile()
{
    return GitRevision::GetHotfixesDatabase();
}

template<>
bool DBUpdater<HotfixDatabaseConnection>::IsEnabled(uint32 const updateMask)
{
    // This way silences warnings under msvc
    return (updateMask & DatabaseLoader::DATABASE_HOTFIX) ? true : false;
}

template<>
BaseLocation DBUpdater<HotfixDatabaseConnection>::GetBaseLocationType()
{
    return LOCATION_DOWNLOAD;
}

// All
template<class T>
BaseLocation DBUpdater<T>::GetBaseLocationType()
{
    return LOCATION_REPOSITORY;
}

template<class T>
bool DBUpdater<T>::Create(DatabaseWorkerPool<T>& pool)
{
    TC_LOG_INFO("sql.updates", "Database \"{}\" does not exist, do you want to create it? [yes (default) / no]: ",
        pool.GetConnectionInfo()->database);

    std::string answer;
    std::getline(std::cin, answer);
    if (!answer.empty() && !(answer.substr(0, 1) == "y"))
        return false;

    TC_LOG_INFO("sql.updates", "Creating database \"{}\"...", pool.GetConnectionInfo()->database);

    // Path of temp file
    static Path const temp("create_table.sql");

    // Create temporary query to use external MySQL CLi
    std::ofstream file(temp.generic_string());
    if (!file.is_open())
    {
        TC_LOG_FATAL("sql.updates", "Failed to create temporary query file \"{}\"!", temp.generic_string());
        return false;
    }

    file << "CREATE DATABASE `" << pool.GetConnectionInfo()->database << "` DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci\n\n";

    file.close();

    try
    {
        DBUpdater<T>::ApplyFile(pool, pool.GetConnectionInfo()->host, pool.GetConnectionInfo()->user, pool.GetConnectionInfo()->password,
            pool.GetConnectionInfo()->port_or_socket, "", pool.GetConnectionInfo()->ssl, temp);
    }
    catch (UpdateException&)
    {
        TC_LOG_FATAL("sql.updates", "Failed to create database {}! Does the user (named in *.conf) have `CREATE`, `ALTER`, `DROP`, `INSERT` and `DELETE` privileges on the MySQL server?", pool.GetConnectionInfo()->database);
        boost::filesystem::remove(temp);
        return false;
    }

    TC_LOG_INFO("sql.updates", "Done.");
    boost::filesystem::remove(temp);
    return true;
}

template<class T>
bool DBUpdater<T>::Update(DatabaseWorkerPool<T>& pool)
{
    if (!DBUpdaterUtil::CheckExecutable())
        return false;

    TC_LOG_INFO("sql.updates", "Updating {} database...", DBUpdater<T>::GetTableName());

    Path const sourceDirectory(BuiltInConfig::GetSourceDirectory());

    if (!is_directory(sourceDirectory))
    {
        TC_LOG_ERROR("sql.updates", "DBUpdater: The given source directory {} does not exist, change the path to the directory where your sql directory exists (for example c:\\source\\trinitycore). Shutting down.", sourceDirectory.generic_string());
        return false;
    }

    UpdateFetcher updateFetcher(sourceDirectory, [&](std::string const& query) { DBUpdater<T>::Apply(pool, query); },
        [&](Path const& file) { DBUpdater<T>::ApplyFile(pool, file); },
            [&](std::string const& query) -> QueryResult { return DBUpdater<T>::Retrieve(pool, query); });

    UpdateResult result;
    try
    {
        result = updateFetcher.Update(
            sConfigMgr->GetBoolDefault("Updates.Redundancy", true),
            sConfigMgr->GetBoolDefault("Updates.AllowRehash", true),
            sConfigMgr->GetBoolDefault("Updates.ArchivedRedundancy", false),
            sConfigMgr->GetIntDefault("Updates.CleanDeadRefMaxCount", 3));
    }
    catch (UpdateException&)
    {
        return false;
    }

    std::string const info = Trinity::StringFormat("Containing {} new and {} archived updates.",
        result.recent, result.archived);

    if (!result.updated)
        TC_LOG_INFO("sql.updates", ">> {} database is up-to-date! {}", DBUpdater<T>::GetTableName(), info);
    else
        TC_LOG_INFO("sql.updates", ">> Applied {} {}. {}", result.updated, result.updated == 1 ? "query" : "queries", info);

    return true;
}

template<class T>
bool DBUpdater<T>::Populate(DatabaseWorkerPool<T>& pool)
{
    {
        QueryResult const result = Retrieve(pool, "SHOW TABLES");
        if (result && (result->GetRowCount() > 0))
            return true;
    }

    if (!DBUpdaterUtil::CheckExecutable())
        return false;

    TC_LOG_INFO("sql.updates", "Database {} is empty, auto populating it...", DBUpdater<T>::GetTableName());

    std::string const p = DBUpdater<T>::GetBaseFile();
    if (p.empty())
    {
        TC_LOG_INFO("sql.updates", ">> No base file provided, skipped!");
        return true;
    }

    Path const base(p);
    if (!exists(base))
    {
        switch (DBUpdater<T>::GetBaseLocationType())
        {
            case LOCATION_REPOSITORY:
            {
                TC_LOG_ERROR("sql.updates", ">> Base file \"{}\" is missing. Try fixing it by cloning the source again.",
                    base.generic_string());

                break;
            }
            case LOCATION_DOWNLOAD:
            {
                std::string const filename = base.filename().generic_string();
                std::string const workdir = boost::filesystem::current_path().generic_string();
                TC_LOG_ERROR("sql.updates", ">> File \"{}\" is missing, download it from \"https://github.com/advocaite/TrinityCore/releases\"" \
                    " uncompress it and place the file \"{}\" in the directory \"{}\".", filename, filename, workdir);
                break;
            }
        }
        return false;
    }

    // Update database
    TC_LOG_INFO("sql.updates", ">> Applying \'{}\'...", base.generic_string());
    try
    {
        ApplyFile(pool, base);
    }
    catch (UpdateException&)
    {
        return false;
    }

    TC_LOG_INFO("sql.updates", ">> Done!");
    return true;
}

template<class T>
QueryResult DBUpdater<T>::Retrieve(DatabaseWorkerPool<T>& pool, std::string const& query)
{
    return pool.Query(query.c_str());
}

template<class T>
void DBUpdater<T>::Apply(DatabaseWorkerPool<T>& pool, std::string const& query)
{
    pool.DirectExecute(query.c_str());
}

template<class T>
void DBUpdater<T>::ApplyFile(DatabaseWorkerPool<T>& pool, Path const& path)
{
    // Classic 1.60 fork: the mysql client sometimes exits with success without running a single statement of the file (binlog of
    // 2026-10-05: 30 files "reapplied", only the `updates` rows written), so the database silently missed them. The file is applied
    // with a marker statement at its end and the marker is read back over our own connection: a run that did nothing is retried and
    // then stops the server instead of being recorded as applied.
    pool.DirectExecute("CREATE TABLE IF NOT EXISTS `updates_apply_check` (`name` VARCHAR(255) NOT NULL PRIMARY KEY, "
        "`token` BIGINT UNSIGNED NOT NULL) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    std::string name = path.filename().generic_string();
    pool.EscapeString(name);
    uint64 const token = (uint64(time(nullptr)) << 24) ^ uint64(std::random_device{}());
    Path const temp = boost::filesystem::temp_directory_path() / ("tc_update_" + std::to_string(token) + ".sql");
    {
        std::ifstream in(path.generic_string(), std::ios::binary);
        std::ofstream out(temp.generic_string(), std::ios::binary | std::ios::trunc);
        if (!in.is_open() || !out.is_open())
        {
            TC_LOG_FATAL("sql.updates", "Failed to prepare the update file \"{}\" (temporary copy \"{}\")!", path.generic_string(), temp.generic_string());
            throw UpdateException("update failed");
        }
        std::string const& database = pool.GetConnectionInfo()->database;
        std::string const defaultName = GetDefaultDatabaseName(DBUpdater<T>::GetTableName());
        if (database == defaultName)
            out << in.rdbuf();
        else
            out << QualifyDefaultSchema(std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()), defaultName, database);
        out << "\n\nREPLACE INTO `updates_apply_check` (`name`, `token`) VALUES ('" << name << "', " << token << ");\n";
    }

    auto const applied = [&]() -> bool
    {
        QueryResult result = pool.Query(Trinity::StringFormat("SELECT `token` FROM `updates_apply_check` WHERE `name` = '{}'", name).c_str());
        return result && (*result)[0].GetUInt64() == token;
    };

    try
    {
        for (uint32 attempt = 1; ; ++attempt)
        {
            DBUpdater<T>::ApplyFile(pool, pool.GetConnectionInfo()->host, pool.GetConnectionInfo()->user, pool.GetConnectionInfo()->password,
                pool.GetConnectionInfo()->port_or_socket, pool.GetConnectionInfo()->database, pool.GetConnectionInfo()->ssl, temp);
            if (applied())
                break;

            if (attempt >= 3)
            {
                TC_LOG_FATAL("sql.updates", "The mysql client reported success for \"{}\" but did not run it ({} attempts)! The file was not applied.",
                    path.generic_string(), attempt);
                throw UpdateException("update failed");
            }

            TC_LOG_ERROR("sql.updates", "The mysql client reported success for \"{}\" but did not run it, applying it again...", path.generic_string());
        }
    }
    catch (...)
    {
        boost::system::error_code ec;
        boost::filesystem::remove(temp, ec);
        throw;
    }

    boost::system::error_code ec;
    boost::filesystem::remove(temp, ec);
}

// Classic 1.60 fork: the files are run over our own MySQL connection (multi statements), not through the mysql command line client.
// The client silently stopped reading some files (quotes in `--` comments) and, started from the worldserver, sometimes ran nothing
// at all while exiting with success (2026-10-05). The file is cut into chunks at statement ends (outside strings and comments).
namespace
{
    // splits SQL text at the `;` that end statements, skipping strings, quoted names and comments; whole statements only
    std::vector<std::string> SplitSqlStatements(std::string const& text)
    {
        std::vector<std::string> statements;
        std::string current;
        size_t i = 0;
        size_t const n = text.size();
        auto flush = [&]()
        {
            if (current.find_first_not_of(" \t\r\n") != std::string::npos)
                statements.push_back(current);
            current.clear();
        };
        auto isSpace = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
        while (i < n)
        {
            char const c = text[i];
            // comments: `-- ` (or `--` at the end of the text), `#`, `/* */` (`/*!` version comments are kept as code)
            if (c == '-' && i + 1 < n && text[i + 1] == '-' && (i + 2 >= n || isSpace(text[i + 2])))
            {
                while (i < n && text[i] != '\n')
                    ++i;
                continue;
            }
            if (c == '#')
            {
                while (i < n && text[i] != '\n')
                    ++i;
                continue;
            }
            if (c == '/' && i + 1 < n && text[i + 1] == '*' && !(i + 2 < n && text[i + 2] == '!'))
            {
                i += 2;
                while (i + 1 < n && !(text[i] == '*' && text[i + 1] == '/'))
                    ++i;
                i += 2;
                continue;
            }
            if (c == '\'' || c == '"' || c == '`')
            {
                char const quote = c;
                current += c;
                ++i;
                while (i < n)
                {
                    char const d = text[i];
                    current += d;
                    ++i;
                    if (d == '\\' && quote != '`' && i < n)
                    {
                        current += text[i];
                        ++i;
                        continue;
                    }
                    if (d == quote)
                    {
                        if (i < n && text[i] == quote)      // doubled quote inside the string
                        {
                            current += text[i];
                            ++i;
                            continue;
                        }
                        break;
                    }
                }
                continue;
            }
            current += c;
            ++i;
            if (c == ';')
                flush();
        }
        flush();
        return statements;
    }
}

template<class T>
void DBUpdater<T>::ApplyFile(DatabaseWorkerPool<T>& pool, std::string const& host, std::string const& user,
    std::string const& password, std::string const& port_or_socket, std::string const& database, std::string const& ssl,
    Path const& path)
{
    MYSQL* mysql = nullptr;
    auto fail = [&](std::string const& reason)
    {
        if (mysql)
            mysql_close(mysql);
        TC_LOG_FATAL("sql.updates", "Applying of file \'{}\' to database \'{}\' failed: {}", path.generic_string(), pool.GetConnectionInfo()->database, reason);
        throw UpdateException("update failed");
    };

    std::ifstream in(path.generic_string(), std::ios::binary);
    if (!in.is_open())
        fail("the file can not be read");
    std::string const text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::vector<std::string> const statements = SplitSqlStatements(text);

    mysql = mysql_init(nullptr);
    if (!mysql)
        fail("mysql_init");

    mysql_options(mysql, MYSQL_SET_CHARSET_NAME, "utf8mb4");
    unsigned long maxPacket = 1024ul * 1024ul * 1024ul;
    mysql_options(mysql, MYSQL_OPT_MAX_ALLOWED_PACKET, &maxPacket);

    int port = 0;
    char const* socket = nullptr;
    std::string connectHost = host;
    if (host != ".")
        port = Trinity::StringTo<int32>(port_or_socket).value_or(0);
    else
    {
#ifdef _WIN32
        unsigned int protocol = MYSQL_PROTOCOL_PIPE;
#else
        connectHost = "localhost";
        socket = port_or_socket.c_str();
        unsigned int protocol = MYSQL_PROTOCOL_SOCKET;
#endif
        mysql_options(mysql, MYSQL_OPT_PROTOCOL, (char const*)&protocol);
    }

#if !defined(MARIADB_VERSION_ID) && MYSQL_VERSION_ID >= 80000
    if (!ssl.empty())
    {
        mysql_ssl_mode sslMode = ssl == "ssl" ? SSL_MODE_REQUIRED : SSL_MODE_DISABLED;
        mysql_options(mysql, MYSQL_OPT_SSL_MODE, (char const*)&sslMode);
    }
#endif

    if (!mysql_real_connect(mysql, connectHost.c_str(), user.c_str(), password.c_str(), database.empty() ? nullptr : database.c_str(), port, socket,
        CLIENT_MULTI_STATEMENTS))
        fail(std::string("connect: ") + mysql_error(mysql));

    // run the statements in chunks of about 4 MB (multi statements), reading every result
    std::string chunk;
    size_t sent = 0;
    auto runChunk = [&]()
    {
        if (chunk.empty())
            return;
        if (mysql_real_query(mysql, chunk.c_str(), static_cast<unsigned long>(chunk.size())) != 0)
            fail(Trinity::StringFormat("error {} in the statements before #{}: {}", mysql_errno(mysql), sent + 1, mysql_error(mysql)));
        int status = 0;
        do
        {
            if (MYSQL_RES* result = mysql_store_result(mysql))
                mysql_free_result(result);
            else if (mysql_field_count(mysql) != 0)
                fail(Trinity::StringFormat("result error {}: {}", mysql_errno(mysql), mysql_error(mysql)));
            status = mysql_next_result(mysql);
            if (status > 0)
                fail(Trinity::StringFormat("error {} in the statements before #{}: {}", mysql_errno(mysql), sent + 1, mysql_error(mysql)));
        } while (status == 0);
        chunk.clear();
    };

    for (std::string const& statement : statements)
    {
        if (!chunk.empty() && chunk.size() + statement.size() > 4 * 1024 * 1024)
            runChunk();
        chunk += statement;
        chunk += '\n';
        ++sent;
    }
    runChunk();
    mysql_close(mysql);
}

template class TC_DATABASE_API DBUpdater<LoginDatabaseConnection>;
template class TC_DATABASE_API DBUpdater<WorldDatabaseConnection>;
template class TC_DATABASE_API DBUpdater<CharacterDatabaseConnection>;
template class TC_DATABASE_API DBUpdater<HotfixDatabaseConnection>;
