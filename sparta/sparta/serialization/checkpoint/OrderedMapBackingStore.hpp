// <OrderedMapBackingStore> -*- C++ -*-

#pragma once

#include "sparta/serialization/checkpoint/Checkpointer.hpp"
#include "sparta/serialization/checkpoint/DeltaCheckpoint.hpp"

namespace sparta::serialization::checkpoint
{
    /*!
     * \brief Checkpoint backing store which holds all checkpoints in a std::map.
     */
    class OrderedMapBackingStore : public CheckpointAccessor
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
            auto itr = chkpts_.find(id);
            if (itr != chkpts_.end()) {
                return static_cast<checkpoint_type*>(itr->second.get());
            }
            return nullptr;
        }

        const checkpoint_type* findCheckpoint(chkpt_id_t id) const noexcept override {
            auto itr = chkpts_.find(id);
            if (itr != chkpts_.end()) {
                return static_cast<const checkpoint_type*>(itr->second.get());
            }
            return nullptr;
        }

        uint64_t getTotalMemoryUse() const {
            uint64_t mem = 0;
            for (auto& cp : chkpts_) {
                mem += cp.second->getTotalMemoryUse();
            }
            return mem;
        }

        uint64_t getContentMemoryUse() const {
            uint64_t mem = 0;
            for (auto& cp : chkpts_) {
                mem += cp.second->getContentMemoryUse();
            }
            return mem;
        }

        void dumpList(std::ostream& o) const {
            for (auto& cp : chkpts_) {
                o << cp.second->stringize() << std::endl;
            }
        }

        void dumpData(std::ostream& o) const {
            for (auto& cp : chkpts_) {
                cp.second->dumpData(o);
                o << std::endl;
            }
        }

        void dumpAnnotatedData(std::ostream& o) const {
            for (auto& cp : chkpts_) {
                o << cp.second->stringize() << std::endl;
                cp.second->dumpData(o);
                o << std::endl;
            }
        }

        std::vector<chkpt_id_t> getCheckpointsAt(tick_t t) const override {
            std::vector<chkpt_id_t> results;
            for (auto& p : chkpts_) {
                const Checkpoint* cp = p.second.get();
                const checkpoint_type* dcp = static_cast<const checkpoint_type*>(cp);
                if (cp->getTick() == t && !dcp->isFlaggedDeleted()) {
                    results.push_back(cp->getID());
                }
            }
            return results;
        }

        std::vector<chkpt_id_t> getCheckpoints() const override {
            std::vector<chkpt_id_t> results;
            for (auto& p : chkpts_) {
                const Checkpoint* cp = p.second.get();
                const checkpoint_type* dcp = static_cast<const checkpoint_type*>(cp);
                if (!dcp->isFlaggedDeleted()) {
                    results.push_back(cp->getID());
                }
            }
            return results;
        }

        void erase(chkpt_id_t id) {
            if (auto it = chkpts_.find(id); it != chkpts_.end()) {
                chkpts_.erase(it);
            }
        }

        void insert(std::unique_ptr<Checkpoint> chkpt) {
            auto id = chkpt->getID();
            chkpts_[id] = std::move(chkpt);
        }

        void flagAllDeleted() {
            // Reverse iterate and flag all as free
            for (auto itr = chkpts_.rbegin(); itr != chkpts_.rend(); ++itr) {
                checkpoint_type* d = static_cast<checkpoint_type*>(itr->second.get());
                if (!d->isFlaggedDeleted()) {
                    d->flagDeleted();
                }
            }
        }

        void setArchDatas(std::vector<ArchData*>& adatas) {
            adatas_ = adatas;
        }

        void traceValue(std::ostream& o, chkpt_id_t id, const ArchData* container, uint32_t offset, uint32_t size) {
            checkpoint_type* dcp = findCheckpoint(id);
            o << "trace: Searching for 0x" << std::hex << offset << " (" << std::dec << size
              << " bytes) in ArchData " << (const void*)container << " when loading checkpoint "
              << std::dec << id << std::endl;
            if (!dcp) {
                o << "trace: Checkpoint " << id << " not found" << std::endl;
            } else {
                if (adatas_.empty()) {
                    throw SpartaException("ArchData never set!");
                }
                dcp->traceValue(o, adatas_, container, offset, size);
            }
        }

        ////////////////////////////////////////////////////////////////////////
        //! @}

    private:
        /*!
         * \brief All checkpoints sorted by ascending tick number (or
         * equivalently ascending checkpoint ID since both are monotonically
         * increasing)
         *
         * This map must still be explicitly torn down in reverse order by a
         * subclass of Checkpointer
         */
        std::map<chkpt_id_t, std::unique_ptr<Checkpoint>> chkpts_;

        /*!
         * \brief ArchDatas required to checkpoint for this checkpointiner based
         * on the root TreeNode
         */
        std::vector<ArchData*> adatas_;
    };

} // namespace sparta::serialization::checkpoint
