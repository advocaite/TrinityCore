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

#include "GameUtilitiesService.h"
#include "BattlenetRpcErrorCodes.h"
#include "DatabaseEnv.h"
#include "Hash.h"
#include "Log.h"
#include "MapUtils.h"
#include "ProtobufJSON.h"
#include "RealmList.h"
#include "RealmList.pb.h"
#include "Session.h"
#include "Types.h"
#include <zlib.h>

namespace Battlenet::Services
{
namespace Shared
{
std::string_view GameUtilities::ParseParamName(std::string_view command)
{
    if (command.starts_with("Command_"sv))
    {
        size_t pos = command.rfind('_');
        if (pos != std::string_view::npos)
            command.remove_suffix(command.length() - pos);
    }

    return command;
}

Variant* GameUtilities::FindParamValue(std::vector<std::pair<std::string_view, Variant>>& params, std::string_view paramName)
{
    auto itr = std::ranges::find(params, paramName, Trinity::Containers::MapKey);
    return itr != params.end() ? &itr->second : nullptr;
}

std::vector<uint8> GameUtilities::CompressJson(std::string const& json)
{
    uLong uncompressedLength = uLong(json.length() + 1);
    uLong compressedLength = compressBound(uLong(json.length()));
    std::vector<uint8> compressed(compressedLength + 4);
    memcpy(compressed.data(), &uncompressedLength, sizeof(uncompressedLength));

    if (compress(compressed.data() + 4, &compressedLength, reinterpret_cast<uint8 const*>(json.data()), uncompressedLength) != Z_OK)
    {
        compressed.clear();
        return compressed;
    }

    compressed.resize(compressedLength + 4);   // trim excess bytes
    return compressed;
}

static void LogClientRequestParams(Session const* session, std::vector<std::pair<std::string_view, Variant>> const& params)
{
    if (!sLog->ShouldLog("session.rpc", LOG_LEVEL_DEBUG))
        return;

    for (auto const& [name, value] : params)
    {
        std::string valueText = std::visit([]<typename T>(T const& v) -> std::string
        {
            if constexpr (std::is_same_v<T, std::string>)
                return v;
            else if constexpr (std::is_same_v<T, std::vector<uint8>>)
                return ByteArrayToHexStr(v);
            else if constexpr (std::is_arithmetic_v<T>)
                return std::to_string(v);
            else
                return "<unknown>";
        }, value);

        TC_LOG_DEBUG("session.rpc", "{} ClientRequest param {} = {}", session->GetClientInfo(), name, valueText);
    }
}

uint32 GameUtilities::HandleClientRequest(Session* session,
    std::vector<std::pair<std::string_view, Variant>>& params,
    std::vector<std::pair<std::string_view, Variant>>& responseValues)
{
    LogClientRequestParams(session, params);

    auto command = std::ranges::find_if(params,
        [](std::string_view paramName) { return paramName.starts_with("Command_"sv); },
        Trinity::Containers::MapKey);

    if (command == params.end())
    {
        TC_LOG_ERROR("session.rpc", "{} sent ClientRequest with no command.", session->GetClientInfo());
        return ERROR_RPC_MALFORMED_REQUEST;
    }

    switch (Trinity::HashFnv1a<>::GetHash(command->first))
    {
        case Trinity::HashFnv1a<>::GetHash("Command_LastCharPlayedRequest_v1"sv):
            return GetLastCharPlayed(session, params, responseValues);
        case Trinity::HashFnv1a<>::GetHash("Command_RealmListTicketRequest_v1"sv):
            return GetRealmListTicket(session, params, responseValues);
        case Trinity::HashFnv1a<>::GetHash("Command_RealmListRequest_v1"sv):
            return GetRealmList(session, params, responseValues);
        case Trinity::HashFnv1a<>::GetHash("Command_RealmJoinRequest_v1"sv):
            return JoinRealm(session, params, responseValues);
        case Trinity::HashFnv1a<>::GetHash("Command_FetchBleepProxiesRequest_v1"sv):
            return GetBleepProxies(session, params, responseValues);
        case Trinity::HashFnv1a<>::GetHash("Command_SuperDistrictListRequest_v1"sv):
            return GetSuperDistrictList(session, params, responseValues);
        default:
            break;
    }

    TC_LOG_ERROR("session.rpc", "{} sent ClientRequest with unknown command {}.", session->GetClientInfo(), command->first);
    return ERROR_RPC_NOT_IMPLEMENTED;
}

uint32 GameUtilities::HandleGetAllValuesForAttribute(Session const* session, std::string_view command, std::vector<Variant>& responseValues)
{
    if (!session->IsAuthed())
        return ERROR_DENIED;

    TC_LOG_DEBUG("session.rpc", "{} GetAllValuesForAttribute {}", session->GetClientInfo(), command);

    switch (Trinity::HashFnv1a<>::GetHash(command))
    {
        case Trinity::HashFnv1a<>::GetHash("Command_RealmListRequest_v1"sv):
            for (std::string& subRegion : sRealmList->GetSubRegions())
                responseValues.emplace_back(std::move(subRegion));
            return ERROR_OK;
        default:
            break;
    }

    TC_LOG_ERROR("session.rpc", "{} sent GetAllValuesForAttributeRequest with unknown command {}.", session->GetClientInfo(), command);
    return ERROR_RPC_NOT_IMPLEMENTED;
}

uint32 GameUtilities::GetLastCharPlayed(Session const* session,
    std::vector<std::pair<std::string_view, Variant>>& params,
    std::vector<std::pair<std::string_view, Variant>>& responseValues)
{
    Variant const* subRegion = FindParamValue(params, "Command_LastCharPlayedRequest_v1");
    if (!subRegion || !std::holds_alternative<std::string>(*subRegion))
        return ERROR_UTIL_SERVER_UNKNOWN_REALM;

    // Classic (1.60+): with a ruleset picked (Param_ContentSetIDFilter = its season) the answer must be that ruleset's realm, and the last
    // character played there. The client always asks for the super realm sub-region (70-1-70), so look the character up by that realm.
    LastPlayedCharacterInfo const* lastPlayerChar = session->GetLastPlayedCharacter(std::get<std::string>(*subRegion));
    if (Variant const* filter = FindParamValue(params, "Param_ContentSetIDFilter"))
    {
        int64 filterContentSet = std::visit([]<typename T>(T const& v) -> int64
        {
            if constexpr (std::is_arithmetic_v<T>)
                return int64(v);
            else
                return -1;
        }, *filter);

        if (filterContentSet >= 0)
            if (Optional<Battlenet::RealmHandle> rulesetRealm = sRealmList->GetRealmIdForContentSet(uint32(filterContentSet)))
                lastPlayerChar = session->GetLastPlayedCharacter(rulesetRealm->GetSubRegionAddress());
    }

    // GetRealmEntryJSON is empty for an offline realm (or a build / security level mismatch). Do not answer with the last played realm then:
    // drop it so the client is handed an online realm of the selected ruleset below.
    std::string lastPlayedRealmEntry;
    if (lastPlayerChar)
    {
        lastPlayedRealmEntry = sRealmList->GetRealmEntryJSON(lastPlayerChar->RealmId, session->GetBuild(), session->GetGameAccountInfo()->SecurityLevel);
        if (lastPlayedRealmEntry.empty())
            lastPlayerChar = nullptr;
    }

    if (lastPlayerChar)
    {
        std::vector<uint8> guidData(sizeof(lastPlayerChar->CharacterGUID));
        std::memcpy(guidData.data(), &lastPlayerChar->CharacterGUID, sizeof(lastPlayerChar->CharacterGUID));

        ::JSON::RealmList::UtilityInfo utilityInfo;
        utilityInfo.set_realmpermissions(0x200);

        std::vector<uint8> utilityInfoJson = CompressJson("JSONUtilityInfo:" + ::JSON::Serialize(utilityInfo));

        responseValues.emplace_back("Param_RealmEntry"sv, CompressJson("JamJSONRealmEntry:" + lastPlayedRealmEntry));
        responseValues.emplace_back("Param_CharacterName"sv, lastPlayerChar->CharacterName);
        responseValues.emplace_back("Param_CharacterGUID"sv, std::move(guidData));
        responseValues.emplace_back("Param_LastPlayedTime"sv, int64(lastPlayerChar->LastPlayedTime));
        responseValues.emplace_back("Param_UtilityInfo"sv, std::move(utilityInfoJson));
    }
    else if (Variant const* contentSetFilter = FindParamValue(params, "Param_ContentSetIDFilter"))
    {
        // Classic (1.60+) clients pick a realm for the selected super district (gameplay ruleset) this way:
        // Command_LastCharPlayedRequest_v1 + Param_ContentSetIDFilter = Cfg_SuperDistrict.ContentSetID. Hand them our realm.
        int64 contentSetId = std::visit([]<typename T>(T const& v) -> int64
        {
            if constexpr (std::is_arithmetic_v<T>)
                return int64(v);
            else
                return -1;
        }, *contentSetFilter);

        Optional<Battlenet::RealmHandle> realmId = contentSetId >= 0 ? sRealmList->GetRealmIdForContentSet(uint32(contentSetId)) : Optional<Battlenet::RealmHandle>();
        if (!realmId)
            realmId = sRealmList->GetFirstRealmId();   // no realm for that ruleset: fall back to the first one
        if (contentSetId >= 0 && realmId)
        {
            std::string realmEntryJson = sRealmList->GetRealmEntryJSON(*realmId, session->GetBuild(), session->GetGameAccountInfo()->SecurityLevel);
            TC_LOG_DEBUG("session.rpc", "{} LastCharPlayed for content set {} -> realm entry {}", session->GetClientInfo(), contentSetId, realmEntryJson);
            if (!realmEntryJson.empty())
            {
                ::JSON::RealmList::UtilityInfo utilityInfo;
                utilityInfo.set_realmpermissions(0x200);

                // the client requires Param_LastPlayedTime (int) right after Param_RealmEntry, otherwise it discards the realm
                responseValues.emplace_back("Param_RealmEntry"sv, CompressJson("JamJSONRealmEntry:" + realmEntryJson));
                responseValues.emplace_back("Param_LastPlayedTime"sv, int64(time(nullptr)));
                responseValues.emplace_back("Param_UtilityInfo"sv, CompressJson("JSONUtilityInfo:" + ::JSON::Serialize(utilityInfo)));
            }
        }
    }

    return ERROR_OK;
}

uint32 GameUtilities::GetRealmListTicket(Session* session,
    std::vector<std::pair<std::string_view, Variant>>& params,
    std::vector<std::pair<std::string_view, Variant>>& responseValues)
{
    GameAccountInfo const* gameAccountInfo = nullptr;

    if (Variant const* identity = FindParamValue(params, "Param_Identity"); identity && std::holds_alternative<std::vector<uint8>>(*identity))
    {
        std::string_view json = { reinterpret_cast<char const*>(std::get<std::vector<uint8>>(*identity).data()), std::get<std::vector<uint8>>(*identity).size() };
        ::JSON::RealmList::RealmListTicketIdentity data;
        std::size_t jsonStart = json.find(':');
        if (jsonStart != std::string_view::npos && ::JSON::Deserialize(json.substr(jsonStart + 1), &data))
            gameAccountInfo = session->GetGameAccountInfo(data.gameaccountid());
    }

    if (!gameAccountInfo)
        return ERROR_UTIL_SERVER_INVALID_IDENTITY_ARGS;

    if (gameAccountInfo->IsPermanenetlyBanned)
        return ERROR_GAME_ACCOUNT_BANNED;
    if (gameAccountInfo->IsBanned)
        return ERROR_GAME_ACCOUNT_SUSPENDED;

    ClientBuild::VariantId clientBuildVariant = { };
    Optional<std::array<uint8, 32>> clientSecret;
    if (Variant const* clientInfo = FindParamValue(params, "Param_ClientInfo"))
    {
        std::string_view json = { reinterpret_cast<char const*>(std::get<std::vector<uint8>>(*clientInfo).data()), std::get<std::vector<uint8>>(*clientInfo).size() };
        ::JSON::RealmList::RealmListTicketClientInformation data;
        std::size_t jsonStart = json.find(':');
        if (jsonStart != std::string_view::npos && ::JSON::Deserialize(json.substr(jsonStart + 1), &data))
        {
            if (data.info().secret().size() == 32)
                memcpy(clientSecret.emplace().data(), data.info().secret().data(), data.info().secret().size());

            clientBuildVariant =
            {
                .Platform = ClientBuild::Platform::Id(data.info().platformtype()),
                .Arch = ClientBuild::Arch::Id(data.info().clientarch()),
                .Type = ClientBuild::Type::Id(data.info().type())
            };
        }
    }

    if (!clientSecret)
        return ERROR_WOW_SERVICES_DENIED_REALM_LIST_TICKET;

    session->SetClientInfo(gameAccountInfo->Id, clientBuildVariant, *clientSecret);

    LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_UPD_BNET_LAST_LOGIN_INFO);
    stmt->setString(0, session->GetRemoteIpAddress().to_string());
    stmt->setUInt8(1, GetLocaleByName(session->GetLocale()));
    stmt->setString(2, session->GetOS());
    stmt->setUInt32(3, session->GetAccountId());

    LoginDatabase.Execute(stmt);

    std::vector<uint8> realmListTicket;
    std::ranges::copy("AuthRealmListTicket\0"sv, std::back_inserter(realmListTicket));
    responseValues.emplace_back("Param_RealmListTicket"sv, std::move(realmListTicket));

    return ERROR_OK;
}

uint32 GameUtilities::GetRealmList(Session const* session,
    std::vector<std::pair<std::string_view, Variant>>& params,
    std::vector<std::pair<std::string_view, Variant>>& responseValues)
{
    std::string subRegionId;
    if (Variant const* subRegion = FindParamValue(params, "Command_RealmListRequest_v1"); subRegion && std::holds_alternative<std::string>(*subRegion))
        subRegionId = std::get<std::string>(*subRegion);

    std::vector<uint8> realmListJson = sRealmList->GetRealmList(session->GetBuild(), session->GetGameAccountInfo()->SecurityLevel, subRegionId);

    if (realmListJson.empty())
        return ERROR_UTIL_SERVER_FAILED_TO_SERIALIZE_RESPONSE;

    ::JSON::RealmList::RealmCharacterCountList realmCharacterCounts;
    for (auto const& [realmAddress, count] : session->GetGameAccountInfo()->CharacterCounts)
    {
        ::JSON::RealmList::RealmCharacterCountEntry* countEntry = realmCharacterCounts.add_counts();
        countEntry->set_wowrealmaddress(realmAddress);
        countEntry->set_count(count);
    }

    std::vector<uint8> characterCountsJson = CompressJson("JSONRealmCharacterCountList:" + ::JSON::Serialize(realmCharacterCounts));

    if (characterCountsJson.empty())
        return ERROR_UTIL_SERVER_FAILED_TO_SERIALIZE_RESPONSE;

    responseValues.emplace_back("Param_RealmList"sv, std::move(realmListJson));
    responseValues.emplace_back("Param_CharacterCountList"sv, std::move(characterCountsJson));

    return ERROR_OK;
}

uint32 GameUtilities::JoinRealm(Session const* session,
    std::vector<std::pair<std::string_view, Variant>>& params,
    std::vector<std::pair<std::string_view, Variant>>& responseValues)
{
    Variant const* realmAddress = FindParamValue(params, "Param_RealmAddress");
    if (!realmAddress || !std::holds_alternative<uint64>(*realmAddress))
        return ERROR_UTIL_SERVER_UNKNOWN_REALM;

    RealmJoinResult result = sRealmList->JoinRealm(std::get<uint64>(*realmAddress), session->GetBuild(), session->GetBuildVariant(),
        session->GetRemoteIpAddress(), session->GetClientSecret(), GetLocaleByName(session->GetLocale()), session->GetOS(),
        session->GetTimezoneOffset(), session->GetGameAccountInfo()->Name, session->GetGameAccountInfo()->SecurityLevel);

    if (result.Result == ERROR_OK)
    {
        responseValues.emplace_back("Param_RealmJoinTicket"sv, std::move(result.JoinTicket));
        responseValues.emplace_back("Param_ServerAddresses"sv, std::move(result.ServerAddresses));
        responseValues.emplace_back("Param_JoinSecret"sv, std::move(result.JoinSecret));
    }

    return result.Result;
}

uint32 GameUtilities::GetBleepProxies(Session const* /*session*/,
    std::vector<std::pair<std::string_view, Variant>>& /*params*/,
    std::vector<std::pair<std::string_view, Variant>>& responseValues)
{
    std::vector<uint8> proxyListJson = CompressJson("JSONBleepProxyList:" + ::JSON::Serialize(::JSON::RealmList::BleepProxyList()));

    if (proxyListJson.empty())
        return ERROR_UTIL_SERVER_FAILED_TO_SERIALIZE_RESPONSE;

    responseValues.emplace_back("Param_BleepProxyList"sv, std::move(proxyListJson));

    return ERROR_OK;
}

// Classic (1.60+) clients ask for their "super districts" (gameplay rulesets: Normal, PvP, Roleplay, Hardcore...) before requesting the realm list.
// JSONSuperDistrictList { superDistricts: [JamJSONSuperDistrictEntry { superDistrictID, disallowLogin, holdDownUntilTime }] }
// superDistrictID refers to Cfg_SuperDistrict.db2 (IDs 1-5 in 1.60.1.70009).
uint32 GameUtilities::GetSuperDistrictList(Session const* session,
    std::vector<std::pair<std::string_view, Variant>>& /*params*/,
    std::vector<std::pair<std::string_view, Variant>>& responseValues)
{
    std::string superDistricts = sRealmList->GetClassicSuperDistrictListEntries();

    std::string json = Trinity::StringFormat(R"(JSONSuperDistrictList:{{"superDistricts":[{}]}})", superDistricts);
    TC_LOG_DEBUG("session.rpc", "{} Param_SuperDistrictList = {}", session->GetClientInfo(), json);

    std::vector<uint8> superDistrictListJson = CompressJson(json);
    if (superDistrictListJson.empty())
        return ERROR_UTIL_SERVER_FAILED_TO_SERIALIZE_RESPONSE;

    responseValues.emplace_back("Param_SuperDistrictList"sv, std::move(superDistrictListJson));

    return ERROR_OK;
}
}

namespace V1
{
Battlenet::Services::Variant FromProto(bgs::protocol::Variant const& from)
{
    if (from.has_bool_value())
        return from.bool_value();
    if (from.has_int_value())
        return from.int_value();
    if (from.has_float_value())
        return from.float_value();
    if (from.has_string_value())
        return from.string_value();
    if (from.has_blob_value())
        return Variant{ std::in_place_type<std::vector<uint8>>,
            reinterpret_cast<uint8 const*>(from.blob_value().data()),
            reinterpret_cast<uint8 const*>(from.blob_value().data()) + from.blob_value().size() };
    if (from.has_uint_value())
        return from.uint_value();

    return { };
}

void ToProto(Battlenet::Services::Variant const& from, bgs::protocol::Variant* to)
{
    std::visit([to]<typename T>(T const& value)
    {
        if constexpr (std::is_same_v<T, bool>)
            to->set_bool_value(value);
        else if constexpr (std::is_same_v<T, int64>)
            to->set_int_value(value);
        else if constexpr (std::is_same_v<T, double>)
            to->set_float_value(value);
        else if constexpr (std::is_same_v<T, std::string>)
            to->set_string_value(value);
        else if constexpr (std::is_same_v<T, std::vector<uint8>>)
            to->set_blob_value(value.data(), value.size());
        else if constexpr (std::is_same_v<T, uint64>)
            to->set_uint_value(value);
        else
            static_assert(Trinity::dependant_false_v<T>);
    }, from);
}

GameUtilities::GameUtilities(Session* session) : GameUtilitiesService(session)
{
}

uint32 GameUtilities::HandleProcessClientRequest(game_utilities::v1::ClientRequest const* request, game_utilities::v1::ClientResponse* response, std::function<void(ServiceBase*, uint32, ::google::protobuf::Message const*)>& /*continuation*/)
{
    if (!_session->IsAuthed())
        return ERROR_DENIED;

    std::vector<std::pair<std::string_view, Variant>> params;
    std::vector<std::pair<std::string_view, Variant>> responseValues;

    for (Attribute const& attribute : request->attribute())
    {
        if (!attribute.has_name() || !attribute.has_value())
            continue;

        params.emplace_back(Shared::GameUtilities::ParseParamName(attribute.name()), FromProto(attribute.value()));
    }

    uint32 result = Shared::GameUtilities::HandleClientRequest(_session, params, responseValues);

    for (auto&& [name, value] : responseValues)
    {
        Attribute* attr = response->add_attribute();
        attr->set_name(name.data(), name.length());
        ToProto(value, attr->mutable_value());
    }

    return result;
}

uint32 GameUtilities::HandleGetAllValuesForAttribute(game_utilities::v1::GetAllValuesForAttributeRequest const* request, game_utilities::v1::GetAllValuesForAttributeResponse* response, std::function<void(ServiceBase*, uint32, ::google::protobuf::Message const*)>& /*continuation*/)
{
    if (!_session->IsAuthed())
        return ERROR_DENIED;

    std::vector<Variant> responseValues;

    uint32 result = Shared::GameUtilities::HandleGetAllValuesForAttribute(_session, Shared::GameUtilities::ParseParamName(request->attribute_key()), responseValues);

    for (Variant& value : responseValues)
        ToProto(value, response->add_attribute_value());

    return result;
}
}

namespace V2
{
Battlenet::Services::Variant FromProto(bgs::protocol::v2::Variant const& from)
{
    switch (from.type_case())
    {
        case v2::Variant::kBoolValue:
            return from.bool_value();
        case v2::Variant::kIntValue:
            return from.int_value();
        case v2::Variant::kFloatValue:
            return from.float_value();
        case v2::Variant::kStringValue:
            return from.string_value();
        case v2::Variant::kBlobValue:
            return Variant{ std::in_place_type<std::vector<uint8>>,
                reinterpret_cast<uint8 const*>(from.blob_value().data()),
                reinterpret_cast<uint8 const*>(from.blob_value().data()) + from.blob_value().size() };
        case v2::Variant::kUintValue:
            return from.uint_value();
        default:
            break;
    }

    return { };
}

void ToProto(Battlenet::Services::Variant const& from, bgs::protocol::v2::Variant* to)
{
    std::visit([to]<typename T>(T const& value)
    {
        if constexpr (std::is_same_v<T, bool>)
            to->set_bool_value(value);
        else if constexpr (std::is_same_v<T, int64>)
            to->set_int_value(value);
        else if constexpr (std::is_same_v<T, double>)
            to->set_float_value(value);
        else if constexpr (std::is_same_v<T, std::string>)
            to->set_string_value(value);
        else if constexpr (std::is_same_v<T, std::vector<uint8>>)
            to->set_blob_value(value.data(), value.size());
        else if constexpr (std::is_same_v<T, uint64>)
            to->set_uint_value(value);
        else
            static_assert(Trinity::dependant_false_v<T>);
    }, from);
}

GameUtilities::GameUtilities(Session* session) : GameUtilitiesService(session)
{
}

uint32 GameUtilities::HandleProcessTask(game_utilities::v2::client::ProcessTaskRequest const* request, game_utilities::v2::client::ProcessTaskResponse* response, std::function<void(ServiceBase*, uint32, ::google::protobuf::Message const*)>& /*continuation*/)
{
    if (!_session->IsAuthed())
        return ERROR_DENIED;

    std::vector<std::pair<std::string_view, Variant>> params;
    std::vector<std::pair<std::string_view, Variant>> responseValues;

    for (v2::Attribute const& attribute : request->attribute())
    {
        if (!attribute.has_name() || !attribute.has_value())
            continue;

        params.emplace_back(Shared::GameUtilities::ParseParamName(attribute.name()), FromProto(attribute.value()));
    }

    uint32 result = Shared::GameUtilities::HandleClientRequest(_session, params, responseValues);

    for (auto&& [name, value] : responseValues)
    {
        v2::Attribute* attr = response->add_result();
        attr->set_name(name.data(), name.length());
        ToProto(value, attr->mutable_value());
    }

    return result;
}

uint32 GameUtilities::HandleGetAllValuesForAttribute(game_utilities::v2::client::GetAllValuesForAttributeRequest const* request, game_utilities::v2::client::GetAllValuesForAttributeResponse* response, std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    if (!_session->IsAuthed())
        return ERROR_DENIED;

    std::vector<Variant> responseValues;

    uint32 result = Shared::GameUtilities::HandleGetAllValuesForAttribute(_session, Shared::GameUtilities::ParseParamName(request->attribute_key()), responseValues);

    for (Variant& value : responseValues)
        ToProto(value, response->add_attribute_value());

    return result;
}
}
}
