// <DatabaseBackingStore> -*- C++ -*-

#pragma once

#include "sparta/serialization/checkpoint/Checkpointer.hpp"
#include "sparta/serialization/checkpoint/DeltaCheckpoint.hpp"

namespace simdb
{
    class AppManager;
}

namespace sparta::serialization::checkpoint
{
    class DatabaseBackingStoreImpl;

    /*!
     * \brief Checkpoint backing store which holds all checkpoints in SimDB (sqlite).
     */
    class DatabaseBackingStore : public CheckpointAccessor
    {
    public:
        DatabaseBackingStore(simdb::AppManager& app_mgr);

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

        checkpoint_type* findCheckpoint(chkpt_id_t id) noexcept override;

        bool hasCheckpoint(chkpt_id_t id) const noexcept override;

        uint64_t getTotalMemoryUse() const;

        uint64_t getContentMemoryUse() const;

        void dumpList(std::ostream& o) const;

        void dumpData(std::ostream& o) const;

        void dumpAnnotatedData(std::ostream& o) const;

        std::vector<chkpt_id_t> getCheckpointsAt(tick_t t) const override;

        std::vector<chkpt_id_t> getCheckpoints() const override;

        void erase(chkpt_id_t id);

        void insert(std::unique_ptr<Checkpoint> chkpt);

        void flagAllDeleted();

        void setArchDatas(std::vector<ArchData*>& adatas);

        void traceValue(std::ostream& o, chkpt_id_t id, const ArchData* container, uint32_t offset, uint32_t size);

        ////////////////////////////////////////////////////////////////////////
        //! @}

    private:
        class Impl
        {
        public:
            Impl(simdb::AppManager& app_mgr);
            DatabaseBackingStoreImpl* operator->();
            const DatabaseBackingStoreImpl* operator->() const;

        private:
            void cacheImpl_() const;
            simdb::AppManager& app_mgr_;
            mutable DatabaseBackingStoreImpl* impl_ = nullptr;
        };

        Impl impl_;
    };

} // namespace sparta::serialization::checkpoint
