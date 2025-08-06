// <DatabaseFastCheckpointer> -*- C++ -*-

#include "sparta/serialization/checkpoint/DatabaseFastCheckpointer.hpp"
#include "simdb/pipeline/Pipeline.hpp"
#include "simdb/pipeline/elements/Function.hpp"
#include "simdb/pipeline/elements/Buffer.hpp"
#include "simdb/utils/Compress.hpp"

#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/stream.hpp>

namespace sparta::serialization::checkpoint
{

using chkpt_id_t = typename FastCheckpointer::chkpt_id_t;
using checkpoint_type = typename FastCheckpointer::checkpoint_type;
using checkpoint_uptr = std::unique_ptr<checkpoint_type>;

DatabaseFastCheckpointer::DatabaseFastCheckpointer(simdb::DatabaseManager* db_mgr, TreeNode& root, Scheduler* sched)
    : FastCheckpointer(root, sched)
    , db_mgr_(db_mgr)
{}

void DatabaseFastCheckpointer::defineSchema(simdb::Schema& schema)
{
    using dt = simdb::SqlDataType;

    auto& window_bytes = schema.addTable("ChkptWindowBytes");
    window_bytes.addColumn("WindowBytes", dt::blob_t);

    auto& window_ids = schema.addTable("ChkptWindowIDs");
    window_ids.addColumn("ChkptWindowBytesID", dt::int32_t);
    window_ids.addColumn("ChkptID", dt::int32_t);
    window_ids.createIndexOn("ChkptID");
    window_ids.disableAutoIncPrimaryKey();
}

std::unique_ptr<simdb::pipeline::Pipeline> DatabaseFastCheckpointer::createPipeline(
    simdb::pipeline::AsyncDatabaseAccessor* db_accessor)
{
    auto pipeline = std::make_unique<simdb::pipeline::Pipeline>(db_mgr_, NAME);

    // Task 1: Buffer snapshots and their deltas into checkpoint windows
    using ChkptCloneUPtr = std::unique_ptr<checkpoint_clone>;
    const auto window_len = getSnapshotThreshold();
    auto create_window = simdb::pipeline::createTask<simdb::pipeline::Buffer<ChkptCloneUPtr>>(window_len);

    // Task 2: Add the IDs of all checkpoints in this window
    using ChkptCloneUPtrs = std::vector<ChkptCloneUPtr>;

    struct ChkptWindow {
        std::vector<chkpt_id_t> chkpt_ids;
        ChkptCloneUPtrs chkpts;
    };

    auto add_chkpt_ids = simdb::pipeline::createTask<simdb::pipeline::Function<ChkptCloneUPtrs, ChkptWindow>>(
        [](ChkptCloneUPtrs&& chkpts,
           simdb::ConcurrentQueue<ChkptWindow>& windows,
           bool)
        {
            ChkptWindow window;
            window.chkpts = std::move(chkpts);
            for (auto& chkpt : window.chkpts) {
                window.chkpt_ids.push_back(chkpt->getID());
            }
            windows.emplace(std::move(window));
        }
    );

    // Task 3: Serialize a checkpoint window into a char buffer
    struct ChkptWindowBytes {
        std::vector<chkpt_id_t> chkpt_ids;
        std::vector<char> chkpt_bytes;
    };

    auto window_to_bytes = simdb::pipeline::createTask<simdb::pipeline::Function<ChkptWindow, ChkptWindowBytes>>(
        [](ChkptWindow&& window,
           simdb::ConcurrentQueue<ChkptWindowBytes>& window_bytes,
           bool)
        {
            ChkptWindowBytes bytes;
            boost::iostreams::back_insert_device<std::vector<char>> inserter(bytes.chkpt_bytes);
            boost::iostreams::stream<boost::iostreams::back_insert_device<std::vector<char>>> os(inserter);
            boost::archive::binary_oarchive oa(os);
            (void)window; //oa << window;
            os.flush();

            for (const auto& chkpt : window.chkpts) {
                bytes.chkpt_ids.push_back(chkpt->getID());
            }

            window_bytes.emplace(std::move(bytes));
        }
    );

    // Task 4: Perform zlib compression on the checkpoint window bytes
    auto zlib_bytes = simdb::pipeline::createTask<simdb::pipeline::Function<ChkptWindowBytes, ChkptWindowBytes>>(
        [](ChkptWindowBytes&& bytes_in,
           simdb::ConcurrentQueue<ChkptWindowBytes>& bytes_out,
           bool)
        {
            ChkptWindowBytes compressed;
            compressed.chkpt_ids = std::move(bytes_in.chkpt_ids);
            simdb::compressData(bytes_in.chkpt_bytes, compressed.chkpt_bytes);
            bytes_out.emplace(std::move(compressed));
        }
    );

    // Task 5: Write to the database
    using EvictedChkptIDs = std::vector<chkpt_id_t>;
    auto write_to_db = db_accessor->createAsyncWriter<DatabaseFastCheckpointer, ChkptWindowBytes, EvictedChkptIDs>(
        [](ChkptWindowBytes&& bytes_in,
           simdb::ConcurrentQueue<EvictedChkptIDs>& evicted_ids,
           simdb::pipeline::AppPreparedINSERTs* tables,
           bool)
        {
            auto bytes_inserter = tables->getPreparedINSERT("ChkptWindowBytes");
            bytes_inserter->setColumnValue(0, bytes_in.chkpt_bytes);
            auto bytes_id = bytes_inserter->createRecord();

            auto chkpt_ids_inserter = tables->getPreparedINSERT("ChkptWindowIDs");
            chkpt_ids_inserter->setColumnValue(0, bytes_id);
            for (auto id : bytes_in.chkpt_ids) {
                chkpt_ids_inserter->setColumnValue(1, id);
                chkpt_ids_inserter->createRecord();
            }

            evicted_ids.emplace(std::move(bytes_in.chkpt_ids));
        }
    );

    // Task 6: Perform cache eviction after a window of checkpoints has been written to SimDB
    auto evict_from_cache = simdb::pipeline::createTask<simdb::pipeline::Function<EvictedChkptIDs, void>>(
        [this](EvictedChkptIDs&& evicted_ids, bool) mutable
        {
            (void)evicted_ids;
        }
    );

    *create_window >> *add_chkpt_ids >> *window_to_bytes >> *zlib_bytes >> *write_to_db >> *evict_from_cache;

    pipeline_head_ = create_window->getTypedInputQueue<ChkptCloneUPtr>();

    pipeline->createTaskGroup("CheckpointPipeline")
        ->addTask(std::move(create_window))
        ->addTask(std::move(window_to_bytes))
        ->addTask(std::move(zlib_bytes))
        ->addTask(std::move(evict_from_cache));

    return pipeline;
}

uint64_t DatabaseFastCheckpointer::getTotalMemoryUse() const noexcept
{
    return 0;
}

uint64_t DatabaseFastCheckpointer::getContentMemoryUse() const noexcept
{
    return 0;
}

bool DatabaseFastCheckpointer::hasCheckpoint(chkpt_id_t id) const noexcept
{
    (void)id;
    return false;
}

std::vector<chkpt_id_t> DatabaseFastCheckpointer::getCheckpointsAt(tick_t t) const
{
    (void)t;
    return {};
}

std::vector<chkpt_id_t> DatabaseFastCheckpointer::getCheckpoints() const
{
    return {};
}

void DatabaseFastCheckpointer::loadCheckpoint(chkpt_id_t id)
{
    FastCheckpointer::loadCheckpoint(id);
}

void DatabaseFastCheckpointer::dumpList(std::ostream& o) const
{
    (void)o;
}

void DatabaseFastCheckpointer::dumpData(std::ostream& o) const
{
    (void)o;
}

void DatabaseFastCheckpointer::dumpAnnotatedData(std::ostream& o) const
{
    (void)o;
}

checkpoint_type* DatabaseFastCheckpointer::findCheckpoint_(chkpt_id_t id) noexcept
{
    (void)id;
    return nullptr;
}

const checkpoint_type* DatabaseFastCheckpointer::findCheckpoint_(chkpt_id_t id) const noexcept
{
    (void)id;
    return nullptr;
}

void DatabaseFastCheckpointer::store_(std::unique_ptr<checkpoint_type> chkpt)
{
    std::unique_ptr<checkpoint_clone> chkpt_clone(chkpt->clone());

    auto id = chkpt->getID();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        chkpts_cache_[id] = std::move(chkpt);
    }

    pipeline_head_->emplace(std::move(chkpt_clone));
}

void DatabaseFastCheckpointer::deleteCheckpoint_(chkpt_id_t id)
{
    (void)id;
}

REGISTER_SIMDB_APPLICATION(DatabaseFastCheckpointer);

} // namespace sparta::serialization::checkpoint
