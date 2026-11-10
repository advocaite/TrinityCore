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

#include "LoginRESTService.h"
#include "Base32.h"
#include "Base64.h"
#include "Common.h"
#include "Configuration/Config.h"
#include "CryptoHash.h"
#include "CryptoRandom.h"
#include "DatabaseEnv.h"
#include "IpNetwork.h"
#include "ProtobufJSON.h"
#include "Resolver.h"
#include "SslContext.h"
#include "StringConvert.h"
#include "Timer.h"
#include "TOTP.h"
#include "Util.h"

namespace Battlenet
{
namespace
{
// the login screen sends what the player typed; accounts are stored as name@domain, so a plain name (no @) gets
// "@" + LoginREST.DefaultAccountDomain added, which lets players type "name" instead of "name@local".
// Names with an @ are left as they are, and an empty setting turns this off.
void NormalizeLoginName(std::string& login)
{
    Utf8ToUpperOnlyLatin(login);
    if (login.empty() || login.find('@') != std::string::npos)
        return;

    std::string domain = sConfigMgr->GetStringDefault("LoginREST.DefaultAccountDomain"sv, "local");
    if (domain.empty())
        return;

    Utf8ToUpperOnlyLatin(domain);
    login += '@';
    login += domain;
}
}

LoginRESTService& LoginRESTService::Instance()
{
    static LoginRESTService instance;
    return instance;
}

bool LoginRESTService::StartNetwork(Trinity::Asio::IoContext& ioContext, std::string const& bindIp, uint16 port, int32 threadCount)
{
    Trinity::Net::Resolver resolver(ioContext);

    _externalHostname = sConfigMgr->GetStringDefault("LoginREST.ExternalAddress"sv, "127.0.0.1");

    std::ranges::transform(resolver.ResolveAll(_externalHostname, ""),
        std::back_inserter(_addresses),
        [](boost::asio::ip::tcp::endpoint const& endpoint) { return endpoint.address(); });

    if (_addresses.empty())
    {
        TC_LOG_ERROR("server.http.login", "Could not resolve LoginREST.ExternalAddress {}", _externalHostname);
        return false;
    }

    _localHostname = sConfigMgr->GetStringDefault("LoginREST.LocalAddress"sv, "127.0.0.1");
    _firstLocalAddressIndex = _addresses.size();

    std::ranges::transform(resolver.ResolveAll(_localHostname, ""),
        std::back_inserter(_addresses),
        [](boost::asio::ip::tcp::endpoint const& endpoint) { return endpoint.address(); });

    if (_addresses.size() == _firstLocalAddressIndex)
    {
        TC_LOG_ERROR("server.http.login", "Could not resolve LoginREST.LocalAddress {}", _localHostname);
        return false;
    }

    if (!HttpService::StartNetwork(ioContext, bindIp, port, threadCount))
        return false;

    using Trinity::Net::Http::RequestHandlerFlag;

    RegisterHandler(boost::beast::http::verb::get, "/bnetserver/login/"sv, [this](std::shared_ptr<LoginHttpSession> session, HttpRequestContext& context)
    {
        return HandleGetForm(std::move(session), context);
    });

    RegisterHandler(boost::beast::http::verb::get, "/bnetserver/gameAccounts/"sv, [](std::shared_ptr<LoginHttpSession> session, HttpRequestContext& context)
    {
        return HandleGetGameAccounts(std::move(session), context);
    });

    RegisterHandler(boost::beast::http::verb::get, "/bnetserver/portal/"sv, [this](std::shared_ptr<LoginHttpSession> session, HttpRequestContext& context)
    {
        return HandleGetPortal(std::move(session), context);
    });

    RegisterHandler(boost::beast::http::verb::post, "/bnetserver/login/"sv, [this](std::shared_ptr<LoginHttpSession> session, HttpRequestContext& context)
    {
        return HandlePostLogin(std::move(session), context);
    }, RequestHandlerFlag::DoNotLogRequestContent);

    RegisterHandler(boost::beast::http::verb::post, "/bnetserver/login/authenticator/"sv, [this](std::shared_ptr<LoginHttpSession> session, HttpRequestContext& context)
    {
        return HandlePostLogin(std::move(session), context);
    }, RequestHandlerFlag::DoNotLogRequestContent);

    RegisterHandler(boost::beast::http::verb::post, "/bnetserver/login/srp/"sv, [](std::shared_ptr<LoginHttpSession> session, HttpRequestContext& context)
    {
        return HandlePostLoginSrpChallenge(std::move(session), context);
    });

    // Forever launcher auto-login ("remember me"): remember = login ticket -> long-lived token; login = token -> fresh login ticket and the
    // account ids the game's launcher login needs; forget = drop the token (sign out)
    RegisterHandler(boost::beast::http::verb::post, "/bnetserver/launcher/remember/"sv, [this](std::shared_ptr<LoginHttpSession> session, HttpRequestContext& context)
    {
        return HandlePostLauncherRemember(std::move(session), context);
    }, RequestHandlerFlag::DoNotLogRequestContent);

    RegisterHandler(boost::beast::http::verb::post, "/bnetserver/launcher/login/"sv, [this](std::shared_ptr<LoginHttpSession> session, HttpRequestContext& context)
    {
        return HandlePostLauncherLogin(std::move(session), context);
    }, RequestHandlerFlag::DoNotLogRequestContent);

    RegisterHandler(boost::beast::http::verb::post, "/bnetserver/launcher/forget/"sv, [this](std::shared_ptr<LoginHttpSession> session, HttpRequestContext& context)
    {
        return HandlePostLauncherForget(std::move(session), context);
    }, RequestHandlerFlag::DoNotLogRequestContent);

    RegisterHandler(boost::beast::http::verb::post, "/bnetserver/refreshLoginTicket/"sv, [this](std::shared_ptr<LoginHttpSession> session, HttpRequestContext& context)
    {
        return HandlePostRefreshLoginTicket(std::move(session), context);
    });

    _port = port;

    // set up form inputs
    JSON::Login::FormInput* input;
    _formInputs.set_type(JSON::Login::LOGIN_FORM);
    input = _formInputs.add_inputs();
    input->set_input_id("account_name");
    input->set_type("text");
    input->set_label("E-mail");
    input->set_max_length(320);

    input = _formInputs.add_inputs();
    input->set_input_id("password");
    input->set_type("password");
    input->set_label("Password");
    input->set_max_length(128);

    input = _formInputs.add_inputs();
    input->set_input_id("log_in_submit");
    input->set_type("submit");
    input->set_label("Log In");

    _loginTicketDuration = sConfigMgr->GetIntDefault("LoginREST.TicketDuration"sv, 3600);

    MigrateLegacyPasswordHashes();

    return true;
}

std::string const& LoginRESTService::GetHostnameForClient(boost::asio::ip::address const& address) const
{
    if (Optional<std::size_t> addressIndex = Trinity::Net::SelectAddressForClient(address, _addresses))
        return *addressIndex >= _firstLocalAddressIndex ? _localHostname : _externalHostname;

    if (address.is_loopback())
        return _localHostname;

    return _externalHostname;
}

std::string LoginRESTService::ExtractAuthorization(HttpRequest const& request)
{
    std::string ticket;
    auto itr = request.find(boost::beast::http::field::authorization);
    if (itr == request.end())
        return ticket;

    std::string_view authorization = Trinity::Net::Http::ToStdStringView(itr->value());
    constexpr std::string_view BASIC_PREFIX = "Basic "sv;

    if (authorization.starts_with(BASIC_PREFIX))
        authorization.remove_prefix(BASIC_PREFIX.length());

    Optional<std::vector<uint8>> decoded = Trinity::Encoding::Base64::Decode(authorization);
    if (!decoded)
        return ticket;

    std::string_view decodedHeader(reinterpret_cast<char const*>(decoded->data()), decoded->size());

    if (std::size_t ticketEnd = decodedHeader.find(':'); ticketEnd != std::string_view::npos)
        decodedHeader.remove_suffix(decodedHeader.length() - ticketEnd);

    ticket = decodedHeader;
    return ticket;
}

LoginRESTService::RequestHandlerResult LoginRESTService::HandleGetForm(std::shared_ptr<LoginHttpSession> session, HttpRequestContext& context) const
{
    JSON::Login::FormInputs form = _formInputs;
    form.set_srp_url(Trinity::StringFormat("http{}://{}:{}/bnetserver/login/srp/", !SslContext::UsesDevWildcardCertificate() ? "s" : "",
        GetHostnameForClient(session->GetRemoteIpAddress()), _port));

    context.response.set(boost::beast::http::field::content_type, "application/json;charset=utf-8");
    context.response.body() = ::JSON::Serialize(form);
    return RequestHandlerResult::Handled;
}

LoginRESTService::RequestHandlerResult LoginRESTService::HandleGetGameAccounts(std::shared_ptr<LoginHttpSession> session, HttpRequestContext& context)
{
    std::string ticket = ExtractAuthorization(context.request);
    if (ticket.empty())
        return HandleUnauthorized(std::move(session), context);

    LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_BNET_GAME_ACCOUNT_LIST);
    stmt->setString(0, ticket);
    session->QueueQuery(LoginDatabase.AsyncQuery(stmt)
        .WithPreparedCallback([session, context = std::move(context)](PreparedQueryResult result) mutable
    {
        JSON::Login::GameAccountList gameAccounts;
        if (result)
        {
            auto formatDisplayName = [](char const* name) -> std::string
            {
                if (char const* hashPos = strchr(name, '#'))
                    return std::string("WoW") + ++hashPos;
                else
                    return name;
            };

            time_t now = time(nullptr);
            do
            {
                Field* fields = result->Fetch();
                JSON::Login::GameAccountInfo* gameAccount = gameAccounts.add_game_accounts();
                gameAccount->set_display_name(formatDisplayName(fields[0].GetCString()));
                gameAccount->set_expansion(fields[1].GetUInt8());
                if (!fields[2].IsNull())
                {
                    uint32 banDate = fields[2].GetUInt32();
                    uint32 unbanDate = fields[3].GetUInt32();
                    gameAccount->set_is_suspended(unbanDate > now);
                    gameAccount->set_is_banned(banDate == unbanDate);
                    gameAccount->set_suspension_reason(fields[4].GetString());
                    gameAccount->set_suspension_expires(unbanDate);
                }
            } while (result->NextRow());
        }

        context.response.set(boost::beast::http::field::content_type, "application/json;charset=utf-8");
        context.response.body() = ::JSON::Serialize(gameAccounts);
        session->SendResponse(context);
    }));

    return RequestHandlerResult::Async;
}

LoginRESTService::RequestHandlerResult LoginRESTService::HandleGetPortal(std::shared_ptr<LoginHttpSession> session, HttpRequestContext& context) const
{
    context.response.set(boost::beast::http::field::content_type, "text/plain");
    context.response.body() = Trinity::StringFormat("{}:{}", GetHostnameForClient(session->GetRemoteIpAddress()), sConfigMgr->GetIntDefault("BattlenetPort", 1119));
    return RequestHandlerResult::Handled;
}

LoginRESTService::RequestHandlerResult LoginRESTService::HandlePostLogin(std::shared_ptr<LoginHttpSession> session, HttpRequestContext& context) const
{
    std::shared_ptr<JSON::Login::LoginForm> loginForm = std::make_shared<JSON::Login::LoginForm>();
    if (!::JSON::Deserialize(context.request.body(), loginForm.get()))
    {
        JSON::Login::LoginResult loginResult;
        loginResult.set_authentication_state(JSON::Login::LOGIN);
        loginResult.set_error_code("UNABLE_TO_DECODE");
        loginResult.set_error_message("There was an internal error while connecting to Battle.net. Please try again later.");

        context.response.result(boost::beast::http::status::bad_request);
        context.response.set(boost::beast::http::field::content_type, "application/json;charset=utf-8");
        context.response.body() = ::JSON::Serialize(loginResult);

        return RequestHandlerResult::Handled;
    }

    auto getInputValue = [](JSON::Login::LoginForm const* loginForm, std::string_view inputId) -> std::string
    {
        for (int32 i = 0; i < loginForm->inputs_size(); ++i)
            if (loginForm->inputs(i).input_id() == inputId)
                return loginForm->inputs(i).value();
        return "";
    };

    // second step of an authenticator login: the code typed into the client's authenticator prompt
    for (int32 i = 0; i < loginForm->inputs_size(); ++i)
        if (loginForm->inputs(i).input_id() == "authenticator_input")
            return HandleAuthenticatorCode(std::move(session), context, loginForm->inputs(i).value());

    std::string login(getInputValue(loginForm.get(), "account_name"));
    NormalizeLoginName(login);

    LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_BNET_AUTHENTICATION);
    stmt->setString(0, login);

    session->QueueQuery(LoginDatabase.AsyncQuery(stmt)
        .WithChainingPreparedCallback([this, session, context = std::move(context), loginForm = std::move(loginForm), getInputValue](QueryCallback& callback, PreparedQueryResult result) mutable
    {
        if (!result)
        {
            JSON::Login::LoginResult loginResult;
            loginResult.set_authentication_state(JSON::Login::DONE);
            context.response.set(boost::beast::http::field::content_type, "application/json;charset=utf-8");
            context.response.body() = ::JSON::Serialize(loginResult);
            session->SendResponse(context);
            return;
        }

        std::string login(getInputValue(loginForm.get(), "account_name"));
        NormalizeLoginName(login);
        bool passwordCorrect = false;
        Optional<std::string> serverM2;

        Field* fields = result->Fetch();
        uint32 accountId = fields[0].GetUInt32();
        // an SRP challenge answers one login only: take it out of the session so a repeated post can't verify it twice
        // (assert in SRP6::VerifyClientEvidence), and only accept it for the account it was made for
        std::unique_ptr<Trinity::Crypto::SRP::BnetSRP6Base> srp = std::move(session->GetSessionState()->Srp);
        std::string srpLogin = std::move(session->GetSessionState()->SrpLogin);
        if (!srp)
        {
            SrpVersion version = SrpVersion(fields[1].GetInt8());
            std::string srpUsername = ByteArrayToHexStr(Trinity::Crypto::SHA256::GetDigestOf(login));
            Trinity::Crypto::SRP::Salt s = fields[2].GetBinary<Trinity::Crypto::SRP::SALT_LENGTH>();
            Trinity::Crypto::SRP::Verifier v = fields[3].GetBinary();
            srp = CreateSrpImplementation(version, SrpHashFunction::Sha256, srpUsername, s, v);

            std::string password(getInputValue(loginForm.get(), "password"));
            if (version == SrpVersion::v1)
                Utf8ToUpperOnlyLatin(password);

            passwordCorrect = srp && srp->CheckCredentials(srpUsername, password);
        }
        else if (srpLogin == login)
        {
            BigNumber A(getInputValue(loginForm.get(), "public_A"));
            BigNumber M1(getInputValue(loginForm.get(), "client_evidence_M1"));
            if (Optional<BigNumber> sessionKey = srp->VerifyClientEvidence(A, M1))
            {
                passwordCorrect = true;
                serverM2 = srp->CalculateServerEvidence(A, M1, *sessionKey).AsHexStr();
            }
        }

        uint32 failedLogins = fields[4].GetUInt32();
        std::string loginTicket = fields[5].GetString();
        uint32 loginTicketExpiry = fields[6].GetUInt32();
        bool isBanned = fields[7].GetUInt64() != 0;

        if (!passwordCorrect)
        {
            if (!isBanned)
            {
                std::string ip_address = session->GetRemoteIpAddress().to_string();
                uint32 maxWrongPassword = uint32(sConfigMgr->GetIntDefault("WrongPass.MaxCount", 0));

                if (sConfigMgr->GetBoolDefault("WrongPass.Logging", false))
                    TC_LOG_DEBUG("server.http.login", "[{}, Account {}, Id {}] Attempted to connect with wrong password!", ip_address, login, accountId);

                if (maxWrongPassword)
                {
                    LoginDatabaseTransaction trans = LoginDatabase.BeginTransaction();
                    LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_UPD_BNET_FAILED_LOGINS);
                    stmt->setUInt32(0, accountId);
                    trans->Append(stmt);

                    ++failedLogins;

                    TC_LOG_DEBUG("server.http.login", "MaxWrongPass : {}, failed_login : {}", maxWrongPassword, accountId);

                    if (failedLogins >= maxWrongPassword)
                    {
                        BanMode banType = BanMode(sConfigMgr->GetIntDefault("WrongPass.BanType", uint16(BanMode::BAN_IP)));
                        int32 banTime = sConfigMgr->GetIntDefault("WrongPass.BanTime", 600);

                        if (banType == BanMode::BAN_ACCOUNT)
                        {
                            stmt = LoginDatabase.GetPreparedStatement(LOGIN_INS_BNET_ACCOUNT_AUTO_BANNED);
                            stmt->setUInt32(0, accountId);
                        }
                        else
                        {
                            stmt = LoginDatabase.GetPreparedStatement(LOGIN_INS_IP_AUTO_BANNED);
                            stmt->setString(0, ip_address);
                        }

                        stmt->setUInt32(1, banTime);
                        trans->Append(stmt);

                        stmt = LoginDatabase.GetPreparedStatement(LOGIN_UPD_BNET_RESET_FAILED_LOGINS);
                        stmt->setUInt32(0, accountId);
                        trans->Append(stmt);
                    }

                    LoginDatabase.CommitTransaction(trans);
                }
            }

            JSON::Login::LoginResult loginResult;
            loginResult.set_authentication_state(JSON::Login::DONE);

            context.response.set(boost::beast::http::field::content_type, "application/json;charset=utf-8");
            context.response.body() = ::JSON::Serialize(loginResult);
            session->SendResponse(context);
            return;
        }

        // account with an authenticator: ask for the code first (the client shows its authenticator prompt for this state and posts
        // authenticator_input to next_url with the same JSESSIONID); the ticket is issued in HandleAuthenticatorCode
        if (!fields[8].IsNull() && !fields[8].GetStringView().empty())
        {
            LoginSessionState* state = session->GetSessionState();
            state->AuthenticatorAccountId = accountId;
            state->AuthenticatorSecret = fields[8].GetString();
            state->AuthenticatorServerM2 = serverM2.value_or("");
            state->AuthenticatorTries = 0;

            JSON::Login::LoginResult loginResult;
            loginResult.set_authentication_state(JSON::Login::AUTHENTICATOR);
            loginResult.set_next_url(GetAuthenticatorUrl(*session));
            if (serverM2)
                loginResult.set_server_evidence_m2(*serverM2);

            context.response.set(boost::beast::http::field::content_type, "application/json;charset=utf-8");
            context.response.body() = ::JSON::Serialize(loginResult);
            session->SendResponse(context);
            return;
        }

        if (loginTicket.empty() || loginTicketExpiry < time(nullptr))
            loginTicket = "TC-" + ByteArrayToHexStr(Trinity::Crypto::GetRandomBytes<20>());

        LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_UPD_BNET_AUTHENTICATION);
        stmt->setString(0, loginTicket);
        stmt->setUInt32(1, time(nullptr) + _loginTicketDuration);
        stmt->setUInt32(2, accountId);
        callback.WithPreparedCallback([session, context = std::move(context), loginTicket = std::move(loginTicket), serverM2 = std::move(serverM2)](PreparedQueryResult) mutable
        {
            JSON::Login::LoginResult loginResult;
            loginResult.set_authentication_state(JSON::Login::DONE);
            loginResult.set_login_ticket(loginTicket);
            if (serverM2)
                loginResult.set_server_evidence_m2(*serverM2);

            context.response.set(boost::beast::http::field::content_type, "application/json;charset=utf-8");
            context.response.body() = ::JSON::Serialize(loginResult);
            session->SendResponse(context);
        }).SetNextQuery(LoginDatabase.AsyncQuery(stmt));
    }));

    return RequestHandlerResult::Async;
}

// ---------------------------------------------------------------- Forever launcher auto-login

namespace
{
    constexpr uint32 LauncherTokenDays = 30;

    bool IsHex(std::string_view s, std::size_t length)
    {
        return s.length() == length && std::ranges::all_of(s, [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); });
    }

    // login tickets are "TC-" + 40 hex
    bool IsLoginTicket(std::string_view s)
    {
        return s.length() == 43 && s.starts_with("TC-") && IsHex(s.substr(3), 40);
    }

    std::string JsonEscape(std::string_view s)
    {
        std::string out;
        for (char c : s)
        {
            switch (c)
            {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                default:
                    if (uint8(c) < 0x20)
                        out += Trinity::StringFormat("\\u{:04x}", uint8(c));
                    else
                        out += c;
                    break;
            }
        }
        return out;
    }

    void SendJson(LoginHttpSession& session, Trinity::Net::Http::RequestContext& context, std::string body, boost::beast::http::status status = boost::beast::http::status::ok)
    {
        context.response.result(status);
        context.response.set(boost::beast::http::field::content_type, "application/json;charset=utf-8");
        context.response.body() = std::move(body);
    }
}

LoginRESTService::RequestHandlerResult LoginRESTService::HandlePostLauncherRemember(std::shared_ptr<LoginHttpSession> session, HttpRequestContext& context) const
{
    std::string ticket = ExtractAuthorization(context.request);
    if (!IsLoginTicket(ticket))
        return HandleUnauthorized(std::move(session), context);

    QueryResult account = LoginDatabase.PQuery("SELECT id FROM battlenet_accounts WHERE LoginTicket = '{}' AND LoginTicketExpiry > UNIX_TIMESTAMP()", ticket);
    if (!account)
        return HandleUnauthorized(std::move(session), context);

    uint32 const accountId = (*account)[0].GetUInt32();
    std::string token = ByteArrayToHexStr(Trinity::Crypto::GetRandomBytes<32>());
    std::string hash = ByteArrayToHexStr(Trinity::Crypto::SHA256::GetDigestOf(token));
    uint32 const now = uint32(time(nullptr));
    uint32 const expires = now + LauncherTokenDays * DAY;
    LoginDatabase.DirectPExecute("INSERT INTO battlenet_launcher_tokens (account_id, token_hash, created, last_used, expires, ip) VALUES ({}, '{}', {}, {}, {}, '{}')",
        accountId, hash, now, now, expires, session->GetRemoteIpAddress().to_string());

    TC_LOG_INFO("server.http.login", "[{}, Id {}] launcher auto-login token created", session->GetClientInfo(), accountId);
    SendJson(*session, context, Trinity::StringFormat(R"({{"token":"{}","expires":{}}})", token, expires));
    return RequestHandlerResult::Handled;
}

LoginRESTService::RequestHandlerResult LoginRESTService::HandlePostLauncherLogin(std::shared_ptr<LoginHttpSession> session, HttpRequestContext& context) const
{
    std::string token = ExtractAuthorization(context.request);
    if (!IsHex(token, 64))
        return HandleUnauthorized(std::move(session), context);

    std::string hash = ByteArrayToHexStr(Trinity::Crypto::SHA256::GetDigestOf(token));
    // every column needs its own name (the result set asserts on duplicates: t.id / a.id crashed bnetserver 2026-10-08)
    QueryResult result = LoginDatabase.PQuery("SELECT t.id AS token_id, a.id AS account_id, a.email AS email, a.LoginTicket AS ticket, "
        "a.LoginTicketExpiry AS ticket_expiry, bab.unbandate > UNIX_TIMESTAMP() OR bab.unbandate = bab.bandate AS banned FROM battlenet_launcher_tokens t "
        "JOIN battlenet_accounts a ON a.id = t.account_id LEFT JOIN battlenet_account_bans bab ON bab.id = a.id "
        "WHERE t.token_hash = '{}' AND t.expires > UNIX_TIMESTAMP()", hash);
    if (!result)
    {
        SendJson(*session, context, R"({"error":"expired"})", boost::beast::http::status::unauthorized);
        return RequestHandlerResult::Handled;
    }

    Field* fields = result->Fetch();
    uint32 const tokenId = fields[0].GetUInt32();
    uint32 const accountId = fields[1].GetUInt32();
    std::string email = fields[2].GetString();
    std::string loginTicket = fields[3].IsNull() ? "" : fields[3].GetString();
    uint32 const ticketExpiry = fields[4].GetUInt32();
    if (!fields[5].IsNull() && fields[5].GetUInt64() != 0)
    {
        SendJson(*session, context, R"({"error":"banned"})", boost::beast::http::status::forbidden);
        return RequestHandlerResult::Handled;
    }

    uint32 const now = uint32(time(nullptr));
    if (loginTicket.empty() || ticketExpiry < now)
        loginTicket = "TC-" + ByteArrayToHexStr(Trinity::Crypto::GetRandomBytes<20>());
    LoginDatabase.DirectPExecute("UPDATE battlenet_accounts SET LoginTicket = '{}', LoginTicketExpiry = {} WHERE id = {}", loginTicket, now + _loginTicketDuration, accountId);
    LoginDatabase.DirectPExecute("UPDATE battlenet_launcher_tokens SET last_used = {}, expires = {}, ip = '{}' WHERE id = {}",
        now, now + LauncherTokenDays * DAY, session->GetRemoteIpAddress().to_string(), tokenId);

    std::string gameAccounts;
    if (QueryResult accounts = LoginDatabase.PQuery("SELECT id, username FROM account WHERE battlenet_account = {} ORDER BY battlenet_index", accountId))
    {
        do
        {
            std::string name = (*accounts)[1].GetString();
            if (std::size_t hashPos = name.find('#'); hashPos != std::string::npos)
                name = "WoW" + name.substr(hashPos + 1);
            gameAccounts += Trinity::StringFormat(R"({}{{"id":{},"name":"{}"}})", gameAccounts.empty() ? "" : ",", (*accounts)[0].GetUInt32(), JsonEscape(name));
        } while (accounts->NextRow());
    }

    TC_LOG_INFO("server.http.login", "[{}, Id {}] launcher auto-login", session->GetClientInfo(), accountId);
    SendJson(*session, context, Trinity::StringFormat(R"({{"login_ticket":"{}","account_id":{},"email":"{}","game_accounts":[{}]}})",
        loginTicket, accountId, JsonEscape(email), gameAccounts));
    return RequestHandlerResult::Handled;
}

LoginRESTService::RequestHandlerResult LoginRESTService::HandlePostLauncherForget(std::shared_ptr<LoginHttpSession> session, HttpRequestContext& context) const
{
    std::string token = ExtractAuthorization(context.request);
    if (!IsHex(token, 64))
        return HandleUnauthorized(std::move(session), context);

    LoginDatabase.DirectPExecute("DELETE FROM battlenet_launcher_tokens WHERE token_hash = '{}'", ByteArrayToHexStr(Trinity::Crypto::SHA256::GetDigestOf(token)));
    SendJson(*session, context, R"({"ok":true})");
    return RequestHandlerResult::Handled;
}

std::string LoginRESTService::GetAuthenticatorUrl(LoginHttpSession const& session) const
{
    return Trinity::StringFormat("http{}://{}:{}/bnetserver/login/authenticator/", !SslContext::UsesDevWildcardCertificate() ? "s" : "",
        GetHostnameForClient(session.GetRemoteIpAddress()), _port);
}

LoginRESTService::RequestHandlerResult LoginRESTService::HandleAuthenticatorCode(std::shared_ptr<LoginHttpSession> session, HttpRequestContext& context,
    std::string const& code) const
{
    constexpr uint32 MaxAuthenticatorTries = 5;

    auto respond = [](LoginHttpSession& session, HttpRequestContext& context, JSON::Login::LoginResult const& loginResult)
    {
        context.response.set(boost::beast::http::field::content_type, "application/json;charset=utf-8");
        context.response.body() = ::JSON::Serialize(loginResult);
        session.SendResponse(context);
    };

    LoginSessionState* state = session->GetSessionState();
    if (!state || !state->AuthenticatorAccountId)
    {
        // no password step before this (or it expired): fail the login
        JSON::Login::LoginResult loginResult;
        loginResult.set_authentication_state(JSON::Login::DONE);
        respond(*session, context, loginResult);
        return RequestHandlerResult::Handled;
    }

    std::string digits;
    for (char c : code)
        if (c >= '0' && c <= '9')
            digits += c;

    Optional<std::vector<uint8>> secret = Trinity::Encoding::Base32::Decode(state->AuthenticatorSecret);
    Optional<uint32> token = digits.length() == 6 ? Trinity::StringTo<uint32>(digits) : Optional<uint32>();
    if (!secret || !token || !Trinity::Crypto::TOTP::ValidateToken(*secret, *token))
    {
        JSON::Login::LoginResult loginResult;
        if (++state->AuthenticatorTries >= MaxAuthenticatorTries)
        {
            TC_LOG_DEBUG("server.http.login", "[{}, Id {}] Too many wrong authenticator codes", session->GetClientInfo(), state->AuthenticatorAccountId);
            state->AuthenticatorAccountId = 0;
            state->AuthenticatorSecret.clear();
            loginResult.set_authentication_state(JSON::Login::DONE);
        }
        else
        {
            loginResult.set_authentication_state(JSON::Login::AUTHENTICATOR);
            loginResult.set_next_url(GetAuthenticatorUrl(*session));
        }
        respond(*session, context, loginResult);
        return RequestHandlerResult::Handled;
    }

    uint32 const accountId = state->AuthenticatorAccountId;
    std::string serverM2 = std::move(state->AuthenticatorServerM2);
    state->AuthenticatorAccountId = 0;
    state->AuthenticatorSecret.clear();

    std::string loginTicket = "TC-" + ByteArrayToHexStr(Trinity::Crypto::GetRandomBytes<20>());
    LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_UPD_BNET_AUTHENTICATION);
    stmt->setString(0, loginTicket);
    stmt->setUInt32(1, time(nullptr) + _loginTicketDuration);
    stmt->setUInt32(2, accountId);
    session->QueueQuery(LoginDatabase.AsyncQuery(stmt)
        .WithPreparedCallback([session, context = std::move(context), loginTicket = std::move(loginTicket), serverM2 = std::move(serverM2), respond](PreparedQueryResult) mutable
    {
        JSON::Login::LoginResult loginResult;
        loginResult.set_authentication_state(JSON::Login::DONE);
        loginResult.set_login_ticket(loginTicket);
        if (!serverM2.empty())
            loginResult.set_server_evidence_m2(serverM2);
        respond(*session, context, loginResult);
    }));

    return RequestHandlerResult::Async;
}

LoginRESTService::RequestHandlerResult LoginRESTService::HandlePostLoginSrpChallenge(std::shared_ptr<LoginHttpSession> session, HttpRequestContext& context)
{
    JSON::Login::LoginForm loginForm;
    if (!::JSON::Deserialize(context.request.body(), &loginForm))
    {
        JSON::Login::LoginResult loginResult;
        loginResult.set_authentication_state(JSON::Login::LOGIN);
        loginResult.set_error_code("UNABLE_TO_DECODE");
        loginResult.set_error_message("There was an internal error while connecting to Battle.net. Please try again later.");

        context.response.result(boost::beast::http::status::bad_request);
        context.response.set(boost::beast::http::field::content_type, "application/json;charset=utf-8");
        context.response.body() = ::JSON::Serialize(loginResult);

        return RequestHandlerResult::Handled;
    }

    std::string login;

    for (int32 i = 0; i < loginForm.inputs_size(); ++i)
        if (loginForm.inputs(i).input_id() == "account_name")
            login = loginForm.inputs(i).value();

    NormalizeLoginName(login);

    LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_BNET_CHECK_PASSWORD_BY_EMAIL);
    stmt->setString(0, login);

    session->QueueQuery(LoginDatabase.AsyncQuery(stmt)
        .WithPreparedCallback([session, context = std::move(context), login = std::move(login)](PreparedQueryResult result) mutable
    {
        if (!result)
        {
            JSON::Login::LoginResult loginResult;
            loginResult.set_authentication_state(JSON::Login::DONE);
            context.response.set(boost::beast::http::field::content_type, "application/json;charset=utf-8");
            context.response.body() = ::JSON::Serialize(loginResult);
            session->SendResponse(context);
            return;
        }

        Field* fields = result->Fetch();
        SrpVersion version = SrpVersion(fields[0].GetInt8());
        SrpHashFunction hashFunction = SrpHashFunction::Sha256;
        std::string srpUsername = ByteArrayToHexStr(Trinity::Crypto::SHA256::GetDigestOf(login));
        Trinity::Crypto::SRP::Salt s = fields[1].GetBinary<Trinity::Crypto::SRP::SALT_LENGTH>();
        Trinity::Crypto::SRP::Verifier v = fields[2].GetBinary();

        session->GetSessionState()->Srp = CreateSrpImplementation(version, hashFunction, srpUsername, s, v);
        session->GetSessionState()->SrpLogin = login;
        if (!session->GetSessionState()->Srp)
        {
            context.response.result(boost::beast::http::status::internal_server_error);
            session->SendResponse(context);
            return;
        }

        JSON::Login::SrpLoginChallenge challenge;
        challenge.set_version(session->GetSessionState()->Srp->GetVersion());
        challenge.set_iterations(session->GetSessionState()->Srp->GetXIterations());
        challenge.set_modulus(session->GetSessionState()->Srp->GetN().AsHexStr());
        challenge.set_generator(session->GetSessionState()->Srp->Getg().AsHexStr());
        challenge.set_hash_function([=]
        {
            switch (hashFunction)
            {
                case SrpHashFunction::Sha256:
                    return "SHA-256";
                case SrpHashFunction::Sha512:
                    return "SHA-512";
                default:
                    break;
            }
            return "";
        }());
        challenge.set_username(srpUsername);
        challenge.set_salt(ByteArrayToHexStr(session->GetSessionState()->Srp->s));
        challenge.set_public_b(session->GetSessionState()->Srp->B.AsHexStr());

        context.response.set(boost::beast::http::field::content_type, "application/json;charset=utf-8");
        context.response.body() = ::JSON::Serialize(challenge);
        session->SendResponse(context);
    }));

    return RequestHandlerResult::Async;
}

LoginRESTService::RequestHandlerResult LoginRESTService::HandlePostRefreshLoginTicket(std::shared_ptr<LoginHttpSession> session, HttpRequestContext& context) const
{
    std::string ticket = ExtractAuthorization(context.request);
    if (ticket.empty())
        return HandleUnauthorized(std::move(session), context);

    LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_BNET_EXISTING_AUTHENTICATION);
    stmt->setString(0, ticket);
    session->QueueQuery(LoginDatabase.AsyncQuery(stmt)
        .WithPreparedCallback([this, session, context = std::move(context), ticket = std::move(ticket)](PreparedQueryResult result) mutable
    {
        JSON::Login::LoginRefreshResult loginRefreshResult;
        if (result)
        {
            uint32 loginTicketExpiry = (*result)[0].GetUInt32();
            time_t now = time(nullptr);
            if (loginTicketExpiry > now)
            {
                loginRefreshResult.set_login_ticket_expiry(now + _loginTicketDuration);

                LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_UPD_BNET_EXISTING_AUTHENTICATION);
                stmt->setUInt32(0, uint32(now + _loginTicketDuration));
                stmt->setString(1, ticket);
                LoginDatabase.Execute(stmt);
            }
            else
                loginRefreshResult.set_is_expired(true);
        }
        else
            loginRefreshResult.set_is_expired(true);

        context.response.set(boost::beast::http::field::content_type, "application/json;charset=utf-8");
        context.response.body() = ::JSON::Serialize(loginRefreshResult);
        session->SendResponse(context);
    }));

    return RequestHandlerResult::Async;
}

std::unique_ptr<Trinity::Crypto::SRP::BnetSRP6Base> LoginRESTService::CreateSrpImplementation(SrpVersion version, SrpHashFunction hashFunction,
    std::string const& username, Trinity::Crypto::SRP::Salt const& salt, Trinity::Crypto::SRP::Verifier const& verifier)
{
    if (version == SrpVersion::v2)
    {
        if (hashFunction == SrpHashFunction::Sha256)
            return std::make_unique<Trinity::Crypto::SRP::BnetSRP6v2<Trinity::Crypto::SHA256>>(username, salt, verifier);
        if (hashFunction == SrpHashFunction::Sha512)
            return std::make_unique<Trinity::Crypto::SRP::BnetSRP6v2<Trinity::Crypto::SHA512>>(username, salt, verifier);
    }

    if (version == SrpVersion::v1)
    {
        if (hashFunction == SrpHashFunction::Sha256)
            return std::make_unique<Trinity::Crypto::SRP::BnetSRP6v1<Trinity::Crypto::SHA256>>(username, salt, verifier);
        if (hashFunction == SrpHashFunction::Sha512)
            return std::make_unique<Trinity::Crypto::SRP::BnetSRP6v1<Trinity::Crypto::SHA512>>(username, salt, verifier);
    }

    return nullptr;
}

std::shared_ptr<Trinity::Net::Http::SessionState> LoginRESTService::CreateNewSessionState(boost::asio::ip::address const& address)
{
    std::shared_ptr<LoginSessionState> state = std::make_shared<LoginSessionState>();
    InitAndStoreSessionState(state, address);
    return state;
}

void LoginRESTService::MigrateLegacyPasswordHashes() const
{
    if (!LoginDatabase.Query("SELECT 1 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = SCHEMA() AND TABLE_NAME = 'battlenet_accounts' AND COLUMN_NAME = 'sha_pass_hash'"))
        return;

    TC_LOG_INFO(_logger, "Updating password hashes...");
    uint32 const start = getMSTime();
    // the auth update query nulls salt/verifier if they cannot be converted
    // if they are non-null but s/v have been cleared, that means a legacy tool touched our auth DB (otherwise, the core might've done it itself, it used to use those hacks too)
    QueryResult result = LoginDatabase.Query("SELECT id, sha_pass_hash, IF((salt IS null) OR (verifier IS null), 0, 1) AS shouldWarn FROM battlenet_accounts WHERE sha_pass_hash != DEFAULT(sha_pass_hash) OR salt IS NULL OR verifier IS NULL");
    if (!result)
    {
        TC_LOG_INFO(_logger, ">> No password hashes to update - this took us {} ms to realize", GetMSTimeDiffToNow(start));
        return;
    }

    bool hadWarning = false;
    uint32 c = 0;
    LoginDatabaseTransaction tx = LoginDatabase.BeginTransaction();
    do
    {
        uint32 const id = (*result)[0].GetUInt32();

        Trinity::Crypto::SRP::Salt salt = Trinity::Crypto::GetRandomBytes<Trinity::Crypto::SRP::SALT_LENGTH>();
        BigNumber x = Trinity::Crypto::SHA256::GetDigestOf(salt, HexStrToByteArray<Trinity::Crypto::SHA256::DIGEST_LENGTH>((*result)[1].GetString(), true));
        Trinity::Crypto::SRP::Verifier verifier = Trinity::Crypto::SRP::BnetSRP6v1Base::g.ModExp(x, Trinity::Crypto::SRP::BnetSRP6v1Base::N).ToByteVector();

        if ((*result)[2].GetInt64())
        {
            if (!hadWarning)
            {
                hadWarning = true;
                TC_LOG_WARN(_logger,
                    "       ========\n"
                    "(!) You appear to be using an outdated external account management tool.\n"
                    "(!) Update your external tool.\n"
                    "(!!) If no update is available, refer your tool's developer to https://github.com/TrinityCore/TrinityCore/issues/25157.\n"
                    "       ========");
            }
        }

        LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_UPD_BNET_LOGON);
        stmt->setInt8(0, AsUnderlyingType(SrpVersion::v1));
        stmt->setBinary(1, salt);
        stmt->setBinary(2, std::move(verifier));
        stmt->setUInt32(3, id);
        tx->Append(stmt);

        tx->Append(Trinity::StringFormat("UPDATE battlenet_accounts SET sha_pass_hash = DEFAULT(sha_pass_hash) WHERE id = {}", id).c_str());

        if (tx->GetSize() >= 10000)
        {
            LoginDatabase.CommitTransaction(tx);
            tx = LoginDatabase.BeginTransaction();
        }

        ++c;
    } while (result->NextRow());
    LoginDatabase.CommitTransaction(tx);

    TC_LOG_INFO(_logger, ">> {} password hashes updated in {} ms", c, GetMSTimeDiffToNow(start));
}
}
