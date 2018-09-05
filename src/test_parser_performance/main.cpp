#include "../libblockchain/message.hpp"
//  download nlohmann json library to test against
//#include "json.hpp"

#include <belt.pp/global.hpp>
#include <belt.pp/utility.hpp>

#include <mesh.pp/fileutility.hpp>

#include <iostream>
#include <chrono>
#include <string>
#include <fstream>

using namespace BlockchainMessage;

using std::cout;
using std::endl;
namespace chrono = std::chrono;
using std::chrono::steady_clock;
using std::string;

int main(int argc, char** argv)
{
    std::ifstream fl("/path/to/blockchain.9771");
    std::istreambuf_iterator<char> begin(fl), end;
    string strr(begin, end);

    auto const count = 1000;
    steady_clock::time_point start = steady_clock::now();

    for (size_t index = 0; index < count; ++index)
    {
        auto str2 = strr;
        /*nlohmann::json res = nlohmann::json::parse(str2, nullptr, false);
        SignedBlock sb;
        sb.authority = res["authority"].get<std::string>();
        sb.signature = res["signature"].get<std::string>();

        auto& block_details = res["block_details"];
        auto& header = block_details["header"];

        Block bd;
        bd.header.block_number = header["block_number"].get<uint64_t>();
        bd.header.c_const = header["c_const"].get<uint64_t>();
        bd.header.c_sum = header["c_sum"].get<uint64_t>();
        bd.header.delta = header["delta"].get<uint64_t>();
        bd.header.prev_hash = header["prev_hash"].get<std::string>();
        beltpp::gm_string_to_gm_time_t(header["sign_time"].get<std::string>(),
                bd.header.sign_time.tm);

        auto& signed_transactions = block_details["signed_transactions"];
        bd.signed_transactions.reserve(signed_transactions.size());
        for (auto const& item : signed_transactions)
        {
            SignedTransaction str;
            str.authority = item["authority"];
            str.signature = item["signature"];

            auto& tr_details = item["transaction_details"];
            beltpp::gm_string_to_gm_time_t(tr_details["creation"].get<std::string>(),
                    str.transaction_details.creation.tm);
            beltpp::gm_string_to_gm_time_t(tr_details["expiry"].get<std::string>(),
                    str.transaction_details.expiry.tm);
            str.transaction_details.fee.whole = tr_details["fee"]["whole"].get<uint64_t>();
            str.transaction_details.fee.fraction = tr_details["fee"]["fraction"].get<uint64_t>();

            auto& action = tr_details["action"];
            Transfer trf;
            trf.to = action["to"].get<std::string>();
            trf.from = action["from"].get<std::string>();
            trf.amount.whole = action["amount"]["whole"];
            trf.amount.fraction = action["amount"]["fraction"];
            str.transaction_details.action.set(std::move(trf));

            bd.signed_transactions.push_back(str);
        }

        auto& rewards = block_details["rewards"];
        bd.rewards.reserve(rewards.size());
        for (auto const& item : rewards)
        {
            Reward rd;
            rd.to = item["to"].get<std::string>();
            rd.amount.fraction = item["amount"]["fraction"];
            rd.amount.whole = item["amount"]["whole"];
            bd.rewards.push_back(rd);
        }
        sb.block_details.set(std::move(bd));*/

        SignedBlock sb2;
        sb2.from_string(str2);
    }
    chrono::milliseconds duration =
            chrono::duration_cast<chrono::milliseconds>(steady_clock::now() - start);

    cout << duration.count() << " milliseconds" << endl;

    return 0;
}
