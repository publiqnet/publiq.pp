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

void get_key_pair(KeyPairRequest const& kpr_msg,
                  beltpp::isocket& sk,
                  beltpp::isocket::peer_id const& peerid);

void get_signature(SignRequest&& msg,
                   beltpp::isocket& sk,
                   beltpp::isocket::peer_id const& peerid);

void verify_signature(Signature const& msg,
                      beltpp::isocket& sk,
                      beltpp::isocket::peer_id const& peerid);

bool process_transfer(BlockchainMessage::SignedTransaction const& signed_transaction,
                      BlockchainMessage::Transfer const& transfer,
                      std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl);

bool process_file(BlockchainMessage::SignedTransaction const& signed_transaction,
                  BlockchainMessage::File const& file,
                  std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl);

bool process_content_unit(BlockchainMessage::SignedTransaction const& signed_transaction,
                          BlockchainMessage::ContentUnit const& content_unit,
                          std::unique_ptr<publiqpp::detail::node_internals>& pimpl);

bool process_content(BlockchainMessage::SignedTransaction const& signed_transaction,
                     BlockchainMessage::Content const& content,
                     std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl);

void broadcast_message(BlockchainMessage::Broadcast&& broadcast,
                       beltpp::isocket::peer_id const& self,
                       beltpp::isocket::peer_id const& from,
                       bool from_rpc,
                       beltpp::ilog* plog,
                       std::unordered_set<beltpp::isocket::peer_id> const& all_peers,
                       beltpp::isocket* psk);

//bool do_i_need_it(BlockchainMessage::ArticleInfo article_info,
//                  std::unique_ptr<publiqpp::detail::node_internals>& m_pimpl);

}// end of namespace publiqpp
