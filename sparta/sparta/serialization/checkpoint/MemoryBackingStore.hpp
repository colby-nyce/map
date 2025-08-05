#pragma once

#include "sparta/serialization/checkpoint/Checkpoint.hpp"
#include <map>
#include <memory>

namespace sparta::serialization::checkpoint
{

template <typename CheckpointType>
class MemoryBackingStore : public CheckpointBackingStore
{
public:
    using checkpoint_type = CheckpointType;

    void store(std::unique_ptr<checkpoint_type> chkpt) {
        auto id = chkpt->getID();
        chkpts_[id] = std::move(chkpt);
    }

    checkpoint_type* findCheckpoint(chkpt_id_t id) noexcept {
        auto itr = chkpts_.find(id);
        if (itr != chkpts_.end()) {
            return static_cast<checkpoint_type*>(itr->second.get());
        }
        return nullptr;
    }

    const checkpoint_type* findCheckpoint(chkpt_id_t id) const noexcept {
        auto itr = chkpts_.find(id);
        if (itr != chkpts_.end()) {
            return static_cast<checkpoint_type*>(itr->second.get());
        }
        return nullptr;
    }

    std::vector<chkpt_id_t> getCheckpointsAt(tick_t t) const {
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

    std::vector<chkpt_id_t> getCheckpoints() const {
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

    uint64_t getTotalMemoryUse() const noexcept {
        uint64_t mem = 0;
        for(auto& cp : chkpts_){
            mem += cp.second->getTotalMemoryUse();
        }
        return mem;
    }

    uint64_t getContentMemoryUse() const noexcept {
        uint64_t mem = 0;
        for(auto& cp : chkpts_){
            mem += cp.second->getContentMemoryUse();
        }
        return mem;
    }

    void dumpList(std::ostream& o) const {
        for(auto& cp : chkpts_){
            o << cp.second->stringize() << std::endl;
        }
    }

    void dumpData(std::ostream& o) const {
        for(auto& cp : chkpts_){
            cp.second->dumpData(o);
            o << std::endl;
        }
    }

    void dumpAnnotatedData(std::ostream& o) const {
        for(auto& cp : chkpts_){
            o << cp.second->stringize() << std::endl;
            cp.second->dumpData(o);
            o << std::endl;
        }
    }

    void deleteCheckpoint(chkpt_id_t id) {
        auto itr = chkpts_.find(id);
        sparta_assert(itr != chkpts_.end());
        itr->second->disconnect();
        chkpts_.erase(itr);
    }

    void deleteAll() {
        for(auto itr = chkpts_.rbegin(); itr != chkpts_.rend(); ++itr){
            checkpoint_type* d = static_cast<checkpoint_type*>(itr->second.get());
            if(!d->isFlaggedDeleted()){
                d->flagDeleted();
            }
        }
    }

    chkpt_id_t getPrev(chkpt_id_t id) const override
    {
        (void)id;
        return 0;
    }

    void setPrev(chkpt_id_t id, chkpt_id_t prev_id) override
    {
        (void)id;
        (void)prev_id;
    }

    void addNext(chkpt_id_t id, chkpt_id_t next_id) override
    {
        (void)id;
        (void)next_id;
    }

    void removeNext(chkpt_id_t id, chkpt_id_t next_id) override
    {
        (void)id;
        (void)next_id;
    }

    std::vector<chkpt_id_t> getNexts(chkpt_id_t id) const override
    {
        (void)id;
        return {};
    }

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
};

} // namespace sparta::serialization::checkpoint
