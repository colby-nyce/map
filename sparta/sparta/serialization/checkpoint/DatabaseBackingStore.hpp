// <DatabaseBackingStore> -*- C++ -*-

#pragma once

#include "sparta/serialization/checkpoint/Checkpointer.hpp"
#include "sparta/serialization/checkpoint/DeltaCheckpoint.hpp"

#include "simdb/apps/App.hpp"
#include "simdb/pipeline/Pipeline.hpp"

namespace sparta::serialization::checkpoint
{

    /*!
     * \brief Checkpoint backing store which holds all checkpoints in SimDB (sqlite).
     */
    class DatabaseBackingStore : public CheckpointAccessor, public simdb::App
    {
    public:
        //! \name Local Types
        //! @{
        ////////////////////////////////////////////////////////////////////////

        //! \brief checkpoint_type Checkpoint subclass used by this store
        typedef DeltaCheckpoint<storage::VectorStorage> checkpoint_type;

        ////////////////////////////////////////////////////////////////////////
        //! @}

        //! \name Required methods used by Checkpointer and its subclasses.
        //! \note See FastCheckpointer for all API doxygen.
        //! @{
        ////////////////////////////////////////////////////////////////////////

        checkpoint_type* findCheckpoint(chkpt_id_t id) noexcept override {
            (void)id;
            return nullptr;
        }

        const checkpoint_type* findCheckpoint(chkpt_id_t id) const noexcept override {
            (void)id;
            return nullptr;
        }

        uint64_t getTotalMemoryUse() const {
            return 0;
        }

        uint64_t getContentMemoryUse() const {
            return 0;
        }

        void dumpList(std::ostream& o) const {
            (void)o;
        }

        void dumpData(std::ostream& o) const {
            (void)o;
        }

        void dumpAnnotatedData(std::ostream& o) const {
            (void)o;
        }

        std::vector<chkpt_id_t> getCheckpointsAt(tick_t t) const override {
            (void)t;
            return {};
        }

        std::vector<chkpt_id_t> getCheckpoints() const override {
            return {};
        }

        void erase(chkpt_id_t id) {
            (void)id;
        }

        void insert(std::unique_ptr<Checkpoint> chkpt) {
            (void)chkpt;
        }

        void flagAllDeleted() {
        }

        void setArchDatas(std::vector<ArchData*>& adatas) {
            (void)adatas;
        }

        void traceValue(std::ostream& o, chkpt_id_t id, const ArchData* container, uint32_t offset, uint32_t size) {
            (void)o;
            (void)id;
            (void)container;
            (void)offset;
            (void)size;
        }

        ////////////////////////////////////////////////////////////////////////
        //! @}

        //! \name simdb::App method overrides
        //! @{
        ////////////////////////////////////////////////////////////////////////

        void postInit(int argc, char** argv) override {
            (void)argc;
            (void)argv;
        }

        std::unique_ptr<simdb::pipeline::Pipeline> createPipeline(
            simdb::pipeline::AsyncDatabaseAccessor* db_accessor)
        {
            return nullptr;
        }

        void preTeardown() {}

        void postTeardown() {}

        ////////////////////////////////////////////////////////////////////////
        //! @}

    private:
    };

} // namespace sparta::serialization::checkpoint
