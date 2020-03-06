#pragma once

#include "node_internals.hpp"
#include "transaction_pool.hpp"

namespace publiqpp
{
void get_actions(LoggedTransactionsRequest const& msg_get_actions,
                 publiqpp::action_log& action_log,
                 beltpp::isocket& sk,
                 beltpp::isocket::peer_id const& peerid);

void get_hash(DigestRequest&& msg_get_hash,
              beltpp::isocket& sk,
              beltpp::isocket::peer_id const& peerid);

void get_random_seed(beltpp::isocket& sk,
                     beltpp::isocket::peer_id const& peerid);

void get_public_addresses(beltpp::isocket& sk,
                          beltpp::isocket::peer_id const& peerid,
                          publiqpp::detail::node_internals& impl);

void get_peers_addresses(beltpp::isocket& sk,
                         beltpp::isocket::peer_id const& peerid,
                         publiqpp::detail::node_internals& impl);

void get_key_pair(KeyPairRequest const& kpr_msg,
                  beltpp::isocket& sk,
                  beltpp::isocket::peer_id const& peerid);

void get_signature(SignRequest&& msg,
                   beltpp::isocket& sk,
                   beltpp::isocket::peer_id const& peerid);

void verify_signature(Signature const& msg,
                      beltpp::isocket& sk,
                      beltpp::isocket::peer_id const& peerid);

void broadcast_message(BlockchainMessage::Broadcast&& broadcast,
                       beltpp::isocket::peer_id const& self,
                       bool full_broadcast,
                       beltpp::ilog* plog,
                       std::unordered_set<beltpp::isocket::peer_id> const& all_peers,
                       beltpp::isocket* psk);

}// end of namespace publiqpp
