// <DatabaseFastCheckpointer> -*- C++ -*-

#pragma once

#include "sparta/serialization/checkpoint/FastCheckpointer.hpp"
#include "simdb/apps/AppRegistration.hpp"
#include "simdb/apps/App.hpp"
#include "simdb/utils/ConcurrentQueue.hpp"

namespace sparta::serialization::checkpoint
{

/*!
 * \brief Implementation of the FastCheckpointer which only holds
 * a "window" of checkpoints in memory at any given time, and sends
 * checkpoints outside this window to/from SimDB.
 */
class DatabaseFastCheckpointer : public simdb::App, public FastCheckpointer
{
public:
    static constexpr auto NAME = "db-fast-checkpointer";

    DatabaseFastCheckpointer(
        simdb::DatabaseManager* db_mgr,
        TreeNode& root,
        Scheduler* sched = nullptr);

    /*!
     * \brief Define the SimDB schema for this checkpointer.
     */
    static void defineSchema(simdb::Schema& schema);

    /*!
     * \brief Instantiate the async processing pipeline to save/load checkpoints.
     */
    std::unique_ptr<simdb::pipeline::Pipeline> createPipeline(
        simdb::pipeline::AsyncDatabaseAccessor* db_accessor) override;

    /*!
     * \brief Computes and returns the memory usage by this checkpointer at
     * this moment including any framework overhead
     * \note This is an approxiation and does not include some of
     * minimal dynamic overhead from stl containers.
     */
    uint64_t getTotalMemoryUse() const noexcept override;

    /*!
     * \brief Computes and returns the memory usage by this checkpointer at
     * this moment purely for the checkpoint state being held
     */
    uint64_t getContentMemoryUse() const noexcept override;

    /*!
     * \brief Tests whether this checkpoint manager has a checkpoint with
     * the given id.
     * \return True if id refers to a checkpoint held by this checkpointer
     * and false if not. If id == Checkpoint::UNIDENTIFIED_CHECKPOINT,
     * always returns false
     */
    bool hasCheckpoint(chkpt_id_t id) const noexcept override;

    /*!
     * \brief Gets all checkpoints taken at tick t on any timeline.
     * \param t Tick number at which checkpoints should found.
     * \return vector of valid checkpoint IDs (never
     * checkpoint_type::UNIDENTIFIED_CHECKPOINT)
     * \note Makes a new vector of results. This should not be called in the
     * critical path.
     */
    std::vector<chkpt_id_t> getCheckpointsAt(tick_t t) const override;

    /*!
     * \brief Gets all checkpoint IDs available on any timeline sorted by
     * tick (or equivalently checkpoint ID).
     * \return vector of valid checkpoint IDs (never
     * checkpoint_type::UNIDENTIFIED_CHECKPOINT)
     * \note Makes a new vector of results. This should not be called in the
     * critical path.
     */
    std::vector<chkpt_id_t> getCheckpoints() const override;

    /*!
     * \brief Overridden to we can load many surrounding checkpoints into
     * memory around the given checkpoint.
     */
    void loadCheckpoint(chkpt_id_t id) override;

    /*!
     * \brief Dumps this checkpointer's flat list of checkpoints to an
     * ostream with a newline following each checkpoint
     * \param o ostream to dump to
     */
    void dumpList(std::ostream& o) const override;

    /*!
     * \brief Dumps this checkpointer's data to an ostream with a newline
     * following each checkpoint
     * \param o ostream to dump to
     */
    void dumpData(std::ostream& o) const override;

    /*!
     * \brief Dumps this checkpointer's data to an
     * ostream with annotations between each ArchData and a newline
     * following each checkpoint description and each checkpoint data dump
     * \param o ostream to dump to
     */
    void dumpAnnotatedData(std::ostream& o) const override;

private:
    /*!
     * \brief Attempts to find a checkpoint within this checkpointer by ID.
     * \param id Checkpoint ID to search for
     * \return Pointer to found checkpoint with matchind ID. If not found,
     * returns nullptr.
     * \todo Faster lookup?
     */
    checkpoint_type* findCheckpoint_(chkpt_id_t id) noexcept override;

    /*!
     * \brief Attempts to find a checkpoint within this checkpointer by ID.
     * \param id Checkpoint ID to search for
     * \return Pointer to found checkpoint with matchind ID. If not found,
     * returns nullptr.
     * \todo Faster lookup?
     */
    const checkpoint_type* findCheckpoint_(chkpt_id_t id) const noexcept override;

    /*!
     * \brief Store a newly created checkpoint
     */
    void store_(std::unique_ptr<checkpoint_type> chkpt) override;

    /*!
     * \brief Remove the checkpoint from the backing store
     */
    void deleteCheckpoint_(chkpt_id_t id) override;

    //! \brief SimDB instance
    simdb::DatabaseManager* db_mgr_ = nullptr;

    //! \brief Cloned checkpoints for pipeline. Original checkpoints held in cache.
    using checkpoint_clone = checkpoint_type::DetachedClone;
    simdb::ConcurrentQueue<std::unique_ptr<checkpoint_clone>>* pipeline_head_ = nullptr;

    //! \brief Subset (or all of) our checkpoints that we currently are holding in memory.
    std::unordered_map<chkpt_id_t, std::unique_ptr<checkpoint_type>> chkpts_cache_;

    //! \brief Mutex to protect our checkpoints cache.
    mutable std::mutex mutex_;
};

} // namespace sparta::serialization::checkpoint

namespace simdb
{

/*!
 * \brief This AppFactory specialization is provided since we have an app that inherits
 * from FastCheckpointer, and thus cannot have the default app subclass ctor signature
 * that only takes the DatabaseManager like most other apps.
 */
template <>
class AppFactory<sparta::serialization::checkpoint::DatabaseFastCheckpointer> : public AppFactoryBase
{
public:
    using AppT = sparta::serialization::checkpoint::DatabaseFastCheckpointer;

    void setSpartaElems(sparta::TreeNode& root, sparta::Scheduler* sched = nullptr)
    {
        root_ = &root;
        sched_ = sched;
    }

    AppT* createApp(DatabaseManager* db_mgr) override
    {
        if (!root_) {
            throw sparta::SpartaException("Must set root (and maybe scheduler) before instantiating apps!");
        }

        // Make the ctor call that the default AppFactory cannot make.
        return new AppT(db_mgr, *root_, sched_);
    }

    void defineSchema(Schema& schema) const override
    {
        AppT::defineSchema(schema);
    }

private:
    sparta::TreeNode* root_ = nullptr;
    sparta::Scheduler* sched_ = nullptr;
};

} // namespace simdb
