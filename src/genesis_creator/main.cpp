#include "../libblockchain/message.hpp"

#include <mesh.pp/cryptoutility.hpp>

#include <iostream>
#include <string>
#include <chrono>

using namespace BlockchainMessage;

using std::cout;
using std::cin;
using std::endl;
using std::string;

int main(int /*argc*/, char** /*argv*/)
{
    try
    {
        size_t count = 0;
        cout << "enter the count of rewards, then all the rewards to be used in genesis" << endl;
        cin >> count;

        string public_key_prefix;
        cout << "enter the public key prefix" << endl;
        cin >> public_key_prefix;

        string genesis_reference;
        cout << "enter the string, which will be hashed and referenced in genesis" << endl;
        cin >> genesis_reference;

        meshpp::config::set_public_key_prefix(public_key_prefix);
        SignedBlock signed_block;

        Block& block = signed_block.block_details;
        BlockHeader& block_header = block.header;

        block_header.block_number = 0;
        block_header.c_sum = 0;
        block_header.delta = 0;
        block_header.c_const = 1;
        block_header.prev_hash = meshpp::hash(genesis_reference);
        auto now = std::chrono::system_clock::now();
        block_header.time_signed.tm = std::chrono::system_clock::to_time_t(now);

        Reward reward;
        reward.amount.whole = 100;
        reward.amount.fraction = 0;
        reward.reward_type = RewardType::initial;

        meshpp::random_seed node_rs("NODE");
        meshpp::private_key node_pv = node_rs.get_private_key(0);
        reward.to = node_pv.get_public_key().to_string();
        block.rewards.push_back(std::move(reward));

        meshpp::random_seed armen_rs("ARMEN");
        meshpp::private_key armen_pv = armen_rs.get_private_key(0);
        reward.to = armen_pv.get_public_key().to_string();
        block.rewards.push_back(std::move(reward));

        meshpp::random_seed tigran_rs("TIGRAN");
        meshpp::private_key tigran_pv = tigran_rs.get_private_key(0);
        reward.to = tigran_pv.get_public_key().to_string();
        block.rewards.push_back(std::move(reward));

        meshpp::random_seed gagik_rs("GAGIK");
        meshpp::private_key gagik_pv = gagik_rs.get_private_key(0);
        reward.to = gagik_pv.get_public_key().to_string();
        block.rewards.push_back(std::move(reward));

        meshpp::random_seed sona_rs("SONA");
        meshpp::private_key sona_pv = sona_rs.get_private_key(0);
        reward.to = sona_pv.get_public_key().to_string();
        block.rewards.push_back(std::move(reward));

        for (size_t index = 0; index < count; ++index)
        {
            size_t id = 0;
            cin >> id;

            string address;
            cin >> address;

            uint64_t amount;
            cin >> amount;

            Reward item;
            item.amount.whole = amount / 100000000;
            item.amount.fraction = amount % 100000000;

            item.to = address;

            block.rewards.push_back(item);
        }

        meshpp::random_seed rs("GENESIS");
        meshpp::private_key pv_key = rs.get_private_key(0);
        meshpp::signature sgn = pv_key.sign(block.to_string());

        signed_block.authorization.address = sgn.base58;
        signed_block.authorization.signature = sgn.pb_key.to_string();

        cout << signed_block.to_string() << endl;
    }
    catch(std::exception const& e)
    {
        cout << "exception: " << e.what() << endl;
    }

    return 0;
}

