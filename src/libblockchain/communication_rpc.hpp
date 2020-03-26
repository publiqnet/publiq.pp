#pragma once

#include "node_internals.hpp"
#include "transaction_pool.hpp"

namespace publiqpp
{
void get_actions(LoggedTransactionsRequest const& msg_get_actions,
                 publiqpp::action_log& action_log,
                 beltpp::stream& sk,
                 beltpp::stream::peer_id const& peerid);

void get_hash(DigestRequest&& msg_get_hash,
              beltpp::stream& sk,
              beltpp::stream::peer_id const& peerid);

void get_random_seed(beltpp::stream& sk,
                     beltpp::stream::peer_id const& peerid);

void get_public_addresses(beltpp::stream& sk,
                          beltpp::stream::peer_id const& peerid,
                          publiqpp::detail::node_internals& impl);

void get_peers_addresses(beltpp::stream& sk,
                         beltpp::stream::peer_id const& peerid,
                         publiqpp::detail::node_internals& impl);

void get_key_pair(KeyPairRequest const& kpr_msg,
                  beltpp::stream& sk,
                  beltpp::stream::peer_id const& peerid);

void get_signature(SignRequest&& msg,
                   beltpp::stream& sk,
                   beltpp::stream::peer_id const& peerid);

void verify_signature(Signature const& msg,
                      beltpp::stream& sk,
                      beltpp::stream::peer_id const& peerid);

void broadcast_message(BlockchainMessage::Broadcast&& broadcast,
                       publiqpp::detail::node_internals& impl);

}// end of namespace publiqpp
