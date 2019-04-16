#include "utility.hpp"

const TransactionInfo& TransactionInfo::get_transaction_info(TransactionLog const& transaction_log)
{
    this->fee = transaction_log.fee;

    switch (transaction_log.action.type())
    {
    case Transfer::rtt:
    {
        Transfer transfer;
        transaction_log.action.get(transfer);

        this->from = transfer.from;
        this->to = transfer.to;
        this->amount = transfer.amount;

        break;
    }
    case File::rtt:
    {
        File file;
        transaction_log.action.get(file);

        this->from = file.author_addresses[0];

        break;
    }
    case ContentUnit::rtt:
    {
        ContentUnit content_unit;
        transaction_log.action.get(content_unit);

        this->from = content_unit.author_addresses[0];

        break;
    }
    case Content::rtt:
    {
        Content content;
        transaction_log.action.get(content);

        this->from = content.channel_address;

        break;
    }
    case Role::rtt:
    {
        Role role;
        transaction_log.action.get(role);

        this->from = role.node_address;

        break;
    }
    case StorageUpdate::rtt:
    {
        StorageUpdate storage_update;
        transaction_log.action.get(storage_update);

        this->from = storage_update.storage_address;

        break;
    }
    case ServiceStatistics::rtt:
    {
        ServiceStatistics service_statistics;
        transaction_log.action.get(service_statistics);

        this->from = service_statistics.server_address;

        break;
    }
    default:
    {
        assert(false);
        throw std::logic_error("unknown transaction log item - " +
                               std::to_string(transaction_log.action.type()));
    }
    }
    return *this;
}
