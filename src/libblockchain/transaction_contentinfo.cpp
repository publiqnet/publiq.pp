#include "transaction_contentinfo.hpp"
#include "common.hpp"
#include "node_internals.hpp"
#include "exception.hpp"

#include <mesh.pp/cryptoutility.hpp>

using namespace BlockchainMessage;

namespace publiqpp
{
void action_validate(SignedTransaction const& signed_transaction,
                     ContentInfo const& content_info)
{
    if (signed_transaction.authority != content_info.channel_address)
        throw authority_exception(signed_transaction.authority, content_info.channel_address);
}

bool action_can_apply(std::unique_ptr<publiqpp::detail::node_internals> const& pimpl,
                      ContentInfo const& content_info)
{
    NodeType node_type;
    if (false == pimpl->m_state.get_role(content_info.channel_address, node_type) ||
        node_type != NodeType::storage)
        return false;
    return true;
}

void action_apply(std::unique_ptr<publiqpp::detail::node_internals>& pimpl,
                  ContentInfo const& content_info)
{
    NodeType node_type;
    if (false == pimpl->m_state.get_role(content_info.channel_address, node_type) ||
        node_type != NodeType::storage)
        throw wrong_data_exception("process_content_info -> wrong authority type : " + content_info.channel_address);
}

void action_revert(std::unique_ptr<publiqpp::detail::node_internals>& /*pimpl*/,
                   ContentInfo const& /*content_info*/)
{
}
}
