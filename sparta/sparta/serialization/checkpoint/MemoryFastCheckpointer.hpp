// <MemoryFastCheckpointer> -*- C++ -*-

#pragma once

#include "sparta/serialization/checkpoint/FastCheckpointer.hpp"

namespace sparta::serialization::checkpoint
{
    /*!
     * \brief Implementation of the FastCheckpointer which holds all
     * checkpoints in memory at all times unless explicitly told to
     * delete them.
     */
    class MemoryFastCheckpointer : public FastCheckpointer
    {
    public:
        MemoryFastCheckpointer(TreeNode& root, Scheduler* sched=nullptr)
            : FastCheckpointer(root, sched)
        {}

        /*!
         * \brief Destructor
         *
         * Frees all checkpoint data
         */
        ~MemoryFastCheckpointer() {
            for(auto itr = chkpts_.rbegin(); itr != chkpts_.rend(); ++itr){
                checkpoint_type* d = static_cast<checkpoint_type*>(itr->second.get());
                if(!d->isFlaggedDeleted()){
                    d->flagDeleted();
                }
            }
        }

        /*!
         * \brief Computes and returns the memory usage by this checkpointer at
         * this moment including any framework overhead
         * \note This is an approxiation and does not include some of
         * minimal dynamic overhead from stl containers.
         */
        uint64_t getTotalMemoryUse() const noexcept override {
            uint64_t mem = 0;
            for(auto& cp : chkpts_){
                mem += cp.second->getTotalMemoryUse();
            }
            return mem;
        }

        /*!
         * \brief Computes and returns the memory usage by this checkpointer at
         * this moment purely for the checkpoint state being held
         */
        uint64_t getContentMemoryUse() const noexcept override {
            uint64_t mem = 0;
            for(auto& cp : chkpts_){
                mem += cp.second->getContentMemoryUse();
            }
            return mem;
        }

        /*!
         * \brief Tests whether this checkpoint manager has a checkpoint with
         * the given id.
         * \return True if id refers to a checkpoint held by this checkpointer
         * and false if not. If id == Checkpoint::UNIDENTIFIED_CHECKPOINT,
         * always returns false
         */
        bool hasCheckpoint(chkpt_id_t id) const noexcept override {
            return chkpts_.find(id) != chkpts_.end();
        }

        /*!
         * \brief Gets all checkpoints taken at tick t on any timeline.
         * \param t Tick number at which checkpoints should found.
         * \return vector of valid checkpoint IDs (never
         * checkpoint_type::UNIDENTIFIED_CHECKPOINT)
         * \note Makes a new vector of results. This should not be called in the
         * critical path.
         */
        std::vector<chkpt_id_t> getCheckpointsAt(tick_t t) const override {
            std::vector<chkpt_id_t> results;
            for(auto& p : chkpts_){
                const Checkpoint* cp = p.second.get();
                const checkpoint_type* dcp = static_cast<const checkpoint_type*>(cp);
                if(cp->getTick() == t && !dcp->isFlaggedDeleted()){
                    results.push_back(cp->getID());
                }
            }
            return results;
        }

        /*!
         * \brief Gets all checkpoint IDs available on any timeline sorted by
         * tick (or equivalently checkpoint ID).
         * \return vector of valid checkpoint IDs (never
         * checkpoint_type::UNIDENTIFIED_CHECKPOINT)
         * \note Makes a new vector of results. This should not be called in the
         * critical path.
         */
        std::vector<chkpt_id_t> getCheckpoints() const override {
            std::vector<chkpt_id_t> results;
            for(auto& p : chkpts_){
                const Checkpoint* cp = p.second.get();
                const checkpoint_type* dcp = static_cast<const checkpoint_type*>(cp);
                if(!dcp->isFlaggedDeleted()){
                    results.push_back(cp->getID());
                }
            }
            return results;
        }

        /*!
         * \brief Dumps this checkpointer's flat list of checkpoints to an
         * ostream with a newline following each checkpoint
         * \param o ostream to dump to
         */
        void dumpList(std::ostream& o) const override {
            for(auto& cp : chkpts_){
                o << cp.second->stringize() << std::endl;
            }
        }

        /*!
         * \brief Dumps this checkpointer's data to an ostream with a newline
         * following each checkpoint
         * \param o ostream to dump to
         */
        void dumpData(std::ostream& o) const override {
            for(auto& cp : chkpts_){
                cp.second->dumpData(o);
                o << std::endl;
            }
        }

        /*!
         * \brief Dumps this checkpointer's data to an
         * ostream with annotations between each ArchData and a newline
         * following each checkpoint description and each checkpoint data dump
         * \param o ostream to dump to
         */
        void dumpAnnotatedData(std::ostream& o) const override {
            for(auto& cp : chkpts_){
                o << cp.second->stringize() << std::endl;
                cp.second->dumpData(o);
                o << std::endl;
            }
        }

    private:
        /*!
         * \brief Attempts to find a checkpoint within this checkpointer by ID.
         * \param id Checkpoint ID to search for
         * \return Pointer to found checkpoint with matchind ID. If not found,
         * returns nullptr.
         * \todo Faster lookup?
         */
        checkpoint_type* findCheckpoint_(chkpt_id_t id) noexcept override {
            auto itr = chkpts_.find(id);
            if (itr != chkpts_.end()) {
                return static_cast<checkpoint_type*>(itr->second.get());
            }
            return nullptr;
        }

        /*!
         * \brief Attempts to find a checkpoint within this checkpointer by ID.
         * \param id Checkpoint ID to search for
         * \return Pointer to found checkpoint with matchind ID. If not found,
         * returns nullptr.
         * \todo Faster lookup?
         */
        const checkpoint_type* findCheckpoint_(chkpt_id_t id) const noexcept override {
            auto itr = chkpts_.find(id);
            if (itr != chkpts_.end()) {
                return static_cast<const checkpoint_type*>(itr->second.get());
            }
            return nullptr;
        }

        /*!
         * \brief Store a newly created checkpoint
         */
        void store_(std::unique_ptr<checkpoint_type> chkpt) override {
            auto id = chkpt->getID();
            chkpts_[id] = std::move(chkpt);
        }

        /*!
         * \brief Remove the checkpoint from the backing store
         */
        void deleteCheckpoint_(chkpt_id_t id) override {
            auto itr = chkpts_.find(id);
            sparta_assert(itr != chkpts_.end());
            chkpts_.erase(itr);
        }

        /*!
         * \brief All checkpoints sorted by ascending tick number (or
         * equivalently ascending checkpoint ID since both are monotonically
         * increasing)
         *
         * This map must still be explicitly torn down in reverse order by a
         * subclass of Checkpointer
         */
        std::map<chkpt_id_t, std::unique_ptr<Checkpoint>> chkpts_;
    };

} // namespace sparta::serialization::checkpoint
