#include "../libblockchain/message.hpp"

#include <belt.pp/log.hpp>
#include <belt.pp/packet.hpp>
#include <belt.pp/socket.hpp>
#include <belt.pp/event.hpp>

#include <boost/program_options.hpp>

#include <memory>
#include <iostream>
#include <vector>
#include <sstream>
#include <chrono>
#include <memory>
#include <exception>

using namespace BlockchainMessage;
using pr_id = beltpp::socket::peer_id;
//namespace program_options = boost::program_options;

using std::unique_ptr;
using std::string;
using std::cout;
using std::endl;
using std::vector;
namespace chrono = std::chrono;
using chrono::steady_clock;
using std::runtime_error;

beltpp::socket openChannel(char** argv, pr_id& channel_id, beltpp::event_handler& evH);
template<typename T>
void SendReceive(T &evType, beltpp::socket& sk, pr_id channel_id,
                                                beltpp::event_handler& evH);

//  MSVS does not instansiate template function only because its address
//  is needed, so let's force it
template beltpp::void_unique_ptr beltpp::new_void_unique_ptr<Error>();
template beltpp::void_unique_ptr beltpp::new_void_unique_ptr<Join>();
template beltpp::void_unique_ptr beltpp::new_void_unique_ptr<Drop>();

using sf = beltpp::socket_family_t<
    Error::rtt,
    Join::rtt,
    Drop::rtt,
    &beltpp::new_void_unique_ptr<Error>,
    &beltpp::new_void_unique_ptr<Join>,
    &beltpp::new_void_unique_ptr<Drop>,
    &Error::pvoid_saver,
    &Join::pvoid_saver,
    &Drop::pvoid_saver,
    &message_list_load
>;

int main(int argc, char** argv)
{
    pr_id channel_id;
    beltpp::event_handler eh;
    beltpp::socket sk = openChannel(argv, channel_id, eh);

    cout << channel_id << endl;

    //
    LoggedTransactionsRequest logTransR;
    logTransR.start_index = 5;
    //
    ChainInfoRequest chainIR;
    //
    KeyPairRequest keyPR;
    keyPR.index = 2;
    keyPR.master_key = "master_key = 000001";
    //
    SignRequest signR;
    signR.private_key = "5JNncPzzupdnuCtXT1y91WJoUhevSF6GSinPUmdb8L2fNVdrkig";
    //
    DigestRequest digestR;
    //
    /*
    MasterKeyRequest masterKeyR;
    //
    SyncRequest syncReq;
    syncReq.block_number = 5;
    syncReq.consensus_sum = 5;
    //
    BlockChainRequest blockChainReq;
    blockChainReq.blocks_from = 0;
    blockChainReq.blocks_to = 5;
    */

    SendReceive(logTransR, sk, channel_id, eh);
    SendReceive(logTransR, sk, channel_id, eh);
    SendReceive(logTransR, sk, channel_id, eh);
    SendReceive(chainIR, sk, channel_id, eh);
    SendReceive(keyPR, sk, channel_id, eh);
    SendReceive(keyPR, sk, channel_id, eh);
    SendReceive(keyPR, sk, channel_id, eh);
    SendReceive(digestR, sk, channel_id, eh);
    SendReceive(digestR, sk, channel_id, eh);
    SendReceive(signR, sk, channel_id, eh);
    Shutdown shutDown;
    SendReceive(shutDown, sk, channel_id, eh);

    return 0;
}

beltpp::socket openChannel(char** argv, pr_id& channel_id, beltpp::event_handler& eh)
{
    beltpp::socket sk = beltpp::getsocket<sf>(eh);
    eh.add(sk);

    beltpp::ip_address address_item;
    address_item.from_string(argv[1]);
    beltpp::ip_address open_address("", 0, address_item.local.address,
                                                       address_item.local.port,
                                                       beltpp::ip_address::e_type::ipv4);
    sk.open(open_address);

    beltpp::isocket::packets pcs;
    std::unordered_set<beltpp::ievent_item const*> set_items;
    while(true)
    {
        if (beltpp::ievent_handler::wait_result::event == eh.wait(set_items))
            pcs = sk.receive(channel_id);
        if (channel_id.empty() && pcs.empty())
            continue;
        else
            break;
    }

    return sk;
}

template<typename T>
void SendReceive(T &evType, beltpp::socket& sk, pr_id channel_id,
                                                    beltpp::event_handler& eh)
{
    sk.send(channel_id, evType);

    bool isContinue{true};
    while(isContinue)
    {
        beltpp::isocket::packets pcs;
        std::unordered_set<beltpp::ievent_item const*> set_items;
        if (beltpp::ievent_handler::wait_result::event == eh.wait(set_items))
            pcs = sk.receive(channel_id);
        if (channel_id.empty() || pcs.empty())
            continue;
        for(auto &pc:pcs)
        {
            switch(pc.type())
            {
            case Join::rtt:
                std::cout << "The event type is Join: " << endl << endl;
                isContinue = false;
                break;
            case LoggedTransactions::rtt:
            {
                LoggedTransactions logTrans;
                pc.get(logTrans);

                uint64_t index = 0;
                if(logTrans.actions.size() > 0)
                   index = logTrans.actions[0].index;
                cout << "The start_index is = " << index << endl << endl;
                isContinue = false;
                break;
            }
            case ChainInfo::rtt:
            {
                ChainInfo chainInfo;
                pc.get(chainInfo);
                cout << "The ChainInfo length = " << chainInfo.length << endl << endl;
                isContinue = false;
                break;
            }
            case KeyPair::rtt:
            {
                KeyPair keyP;
                pc.get(keyP);
                cout << "The KeyPair index = " << keyP.index << endl;
                cout << "The KeyPair public_key = " << keyP.public_key << endl;
                cout << "The KeyPair private_key = " << keyP.private_key << endl;
                cout << "The KeyPair master_key = " << keyP.master_key << endl << endl;
                isContinue = false;
                break;
            }
            case Digest::rtt:
            {
                Digest digest;
                pc.get(digest);
                cout << "The Digest base58_hash = " << digest.base58_hash << endl << endl;
                isContinue = false;
                break;
            }
            case Signature::rtt:
            {
                Signature sign;
                pc.get(sign);
                cout << "The Signature public_key = " << sign.public_key << endl;
                cout << "The Signature signature = " << sign.signature << endl << endl;
                isContinue = false;
                break;
            }
            case RemoteError::rtt:
                //RemoteError error;
                //pc.get(error);
                //cout << "RemoteError: " << rError.message <<endl;
                cout << "Received a RemoteError!!!" << endl << endl;
                isContinue = false;
                break;
            case Drop::rtt:
                cout << "The process was Dropped!" << endl << endl;
                isContinue = false;
                break;
            case Done::rtt:
                cout << "The Send-Receive process was Shutted Down!" << endl << endl;
                isContinue = false;
                break;
            default:
                cout << "Nothing for receive.!" << endl;
                isContinue = false;
                break;
            }
        }
    }
}

