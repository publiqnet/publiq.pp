#include "utility.hpp"

using namespace BlockchainMessage;

TransactionInfo::TransactionInfo(TransactionLog const& transaction_log)
{
    fee = transaction_log.fee;

    switch (transaction_log.action.type())
    {
    case Transfer::rtt:
    {
        Transfer transfer;
        transaction_log.action.get(transfer);

        from = transfer.from;
        to = transfer.to;
        amount = transfer.amount;
        message = transfer.message;

        break;
    }
    case File::rtt:
    {
        File file;
        transaction_log.action.get(file);

        from = file.author_addresses[0];

        break;
    }
    case ContentUnit::rtt:
    {
        ContentUnit content_unit;
        transaction_log.action.get(content_unit);

        from = content_unit.author_addresses[0];
        message = "content_unit";

        break;
    }
    case Content::rtt:
    {
        Content content;
        transaction_log.action.get(content);

        from = content.channel_address;
        message = "content";

        break;
    }
    case Role::rtt:
    {
        Role role;
        transaction_log.action.get(role);

        from = role.node_address;
        message = "role";

        break;
    }
    case StorageUpdate::rtt:
    {
        StorageUpdate storage_update;
        transaction_log.action.get(storage_update);

        from = storage_update.storage_address;
        message = "storage_update";

        break;
    }
    case ServiceStatistics::rtt:
    {
        ServiceStatistics service_statistics;
        transaction_log.action.get(service_statistics);

        from = service_statistics.server_address;
        message = "service_statistics";

        break;
    }
    case SponsorContentUnit::rtt:
    {
        SponsorContentUnit sponsor_content_unit;
        transaction_log.action.get(sponsor_content_unit);

        from = sponsor_content_unit.sponsor_address;
        message = "sponsor_content_unit";

        break;
    }
    case CancelSponsorContentUnit::rtt:
    {
        break;
    }
    default:
    {
        assert(false);
        throw std::logic_error("unknown transaction log item - " +
                               std::to_string(transaction_log.action.type()));
    }
    }
}
