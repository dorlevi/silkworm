/*
   Copyright 2023 The Silkworm Authors

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#include "log.hpp"

#include <cstring>
#include <span>
#include <utility>

#include <silkworm/core/common/util.hpp>
#include <silkworm/silkrpc/common/util.hpp>

#include "types.hpp"

namespace silkworm::rpc {

void to_json(nlohmann::json& json, const Log& log) {
    json["address"] = log.address;
    json["topics"] = log.topics;
    json["data"] = "0x" + silkworm::to_hex(log.data);
    json["blockNumber"] = to_quantity(log.block_number);
    json["blockHash"] = log.block_hash;
    json["transactionHash"] = log.tx_hash;
    json["transactionIndex"] = to_quantity(log.tx_index);
    json["logIndex"] = to_quantity(log.index);
    json["removed"] = log.removed;
    if (log.timestamp) {
        json["timestamp"] = to_quantity(*(log.timestamp));
    }
}

void from_json(const nlohmann::json& json, Log& log) {
    if (json.is_array()) {
        if (json.size() < 3) {
            throw std::system_error{std::make_error_code(std::errc::invalid_argument), "Log CBOR: missing entries"};
        }
        if (!json[0].is_binary()) {
            throw std::system_error{std::make_error_code(std::errc::invalid_argument), "Log CBOR: binary expected in [0]"};
        }
        auto address_bytes = json[0].get_binary();
        log.address = silkworm::to_evmc_address(silkworm::Bytes{address_bytes.begin(), address_bytes.end()});
        if (!json[1].is_array()) {
            throw std::system_error{std::make_error_code(std::errc::invalid_argument), "Log CBOR: array expected in [1]"};
        }
        std::vector<evmc::bytes32> topics{};
        topics.reserve(json[1].size());
        for (auto topic : json[1]) {
            auto topic_bytes = topic.get_binary();
            topics.push_back(silkworm::to_bytes32(silkworm::Bytes{topic_bytes.begin(), topic_bytes.end()}));
        }
        log.topics = topics;
        if (json[2].is_binary()) {
            auto data_bytes = json[2].get_binary();
            log.data = silkworm::Bytes{data_bytes.begin(), data_bytes.end()};
        } else if (json[2].is_null()) {
            log.data = silkworm::Bytes{};
        } else {
            throw std::system_error{std::make_error_code(std::errc::invalid_argument), "Log CBOR: binary or null expected in [2]"};
        }
    } else {
        log.address = json.at("address").get<evmc::address>();
        log.topics = json.at("topics").get<std::vector<evmc::bytes32>>();
        log.data = json.at("data").get<silkworm::Bytes>();
    }
}

struct GlazeJsonLogItem {
    char address[addressSize];
    char tx_hash[hashSize];
    char block_hash[hashSize];
    char block_number[int64Size];
    char tx_index[int64Size];
    char index[int64Size];
    char data[dataSize];
    bool removed;
    std::optional<std::string> timestamp;
    std::vector<std::string> topics;

    struct glaze {
        using T = GlazeJsonLogItem;
        static constexpr auto value = glz::object(
            "address", &T::address,
            "transactionHash", &T::tx_hash,
            "blockHash", &T::block_hash,
            "blockNumber", &T::block_number,
            "transactionIndex", &T::tx_index,
            "logIndex", &T::index,
            "data", &T::data,
            "removed", &T::removed,
            "topics", &T::topics,
            "timestamp", &T::timestamp);
    };
};

struct GlazeJsonLog {
    char jsonrpc[jsonVersionSize] = "2.0";
    uint32_t id;
    std::vector<GlazeJsonLogItem> log_json_list;
    struct glaze {
        using T = GlazeJsonLog;
        static constexpr auto value = glz::object(
            "jsonrpc", &T::jsonrpc,
            "id", &T::id,
            "result", &T::log_json_list);
    };
};

void make_glaze_json_content(std::string& reply, uint32_t id, const Logs& logs) {
    GlazeJsonLog log_json_data{};
    log_json_data.log_json_list.reserve(logs.size());

    log_json_data.id = id;

    for (const auto& l : logs) {
        GlazeJsonLogItem item{};
        to_hex(std::span(item.address), l.address);
        to_hex(std::span(item.tx_hash), l.tx_hash);
        to_hex(std::span(item.block_hash), l.block_hash);
        to_quantity(std::span(item.block_number), l.block_number);
        to_quantity(std::span(item.tx_index), l.tx_index);
        to_quantity(std::span(item.index), l.index);
        item.removed = l.removed;
        to_hex(item.data, l.data);
        if (l.timestamp) {
            item.timestamp = to_quantity(*(l.timestamp));
        }
        for (const auto& t : l.topics) {
            item.topics.push_back("0x" + silkworm::to_hex(t));
        }
        log_json_data.log_json_list.push_back(item);
    }

    glz::write_json(log_json_data, reply);
}

}  // namespace silkworm::rpc
