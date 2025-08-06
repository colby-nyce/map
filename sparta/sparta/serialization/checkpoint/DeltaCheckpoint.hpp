// <DeltaCheckpoint.hpp> -*- C++ -*-

#pragma once

#include <iostream>
#include <sstream>
#include <stack>

#include "sparta/simulation/TreeNode.hpp"
#include "sparta/functional/ArchData.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/kernel/Scheduler.hpp"

#include "sparta/serialization/checkpoint/Checkpointer.hpp"
#include "sparta/serialization/checkpoint/CheckpointExceptions.hpp"


namespace sparta::serialization::checkpoint
{
    namespace storage
    {
        /*!
         * \brief Vector of buffers storage implementation
         */
        class VectorStorage
        {
            class Segment{
                ArchData::line_idx_type idx_;
                std::unique_ptr<char []> data_;
                uint32_t bytes_;
            public:

                /*!
                 * \brief This helper class is used for serialization purposes.
                 */
                class DetachedClone
                {
                public:
                    DetachedClone(ArchData::line_idx_type idx, std::vector<char>&& segment_data)
                        : idx_(idx)
                        , segment_data_(std::move(segment_data))
                    {}

                    ArchData::line_idx_type getLineIdx() const {
                        return idx_;
                    }

                    const std::vector<char>& getSegmentData() const {
                        return segment_data_;
                    }

                private:
                    ArchData::line_idx_type idx_;
                    std::vector<char> segment_data_;
                };

                /*!
                 * \brief Copy this segment for serialization purposes
                 */
                DetachedClone* clone() const {
                    std::vector<char> data_clone;
                    data_clone.resize(bytes_);
                    copyTo(data_clone.data(), data_clone.size());
                    return new DetachedClone(idx_, std::move(data_clone));
                }

                /*!
                 * \brief Copying disabled (avoid memcpy)
                 */
                Segment(const Segment&) = delete;

                /*!
                 * \brief Move constructor
                 */
                Segment(Segment&& rhp) :
                    idx_(rhp.idx_),
                    data_(std::move(rhp.data_)),
                    bytes_(rhp.bytes_)
                {
                    rhp.idx_ = ArchData::INVALID_LINE_IDX;
                    rhp.bytes_ = 0;
                }

                /*!
                 * \brief Dummy constructor. Represents null entry (end of ArchData)
                 */
                Segment() :
                    idx_(ArchData::INVALID_LINE_IDX),
                    bytes_(0)
                {;}

                /*!
                 * \brief Deleted assignment operator
                 */
                Segment& operator=(const Segment& rhp) = delete;

                /*!
                 * \brief Data constructor. Allocates data and copies results over
                 */
                Segment(ArchData::line_idx_type idx, const char* data, size_t bytes) :
                    idx_(idx), bytes_(bytes)
                {
                    sparta_assert(idx != ArchData::INVALID_LINE_IDX,
                                  "Attempted to create segment of " << bytes << " bytes with invalid line index");
                    data_.reset(new char[bytes]);
                    ::memcpy(data_.get(), data, bytes);
                }

                ArchData::line_idx_type getLineIdx() const {
                    return idx_;
                }

                uint32_t getSize() const {
                    return sizeof(decltype(*this)) + bytes_;
                }

                void copyTo(char* buf, uint32_t size) const {
                    sparta_assert(size == bytes_, \
                                  "Attempted to restore checkpoint data for a line where the "
                                  "data was " << bytes_ << " bytes but the loader requested "
                                  << size << " bytes. The sizes must match up or something is "
                                  "wrong");
                    memcpy(buf, data_.get(), bytes_);
                }

                void dump(std::ostream& o) const {
                    if(idx_ == ArchData::INVALID_LINE_IDX){
                        std::cout << "\nEnd of ArchData";
                        return;
                    }

                    std::cout << "\nLine: " << std::dec << idx_ << " (" << bytes_ << ") bytes";
                    for(uint32_t off = 0; off < bytes_;){
                        char chr = data_[off];
                        if(off % 32 == 0){
                            o << std::endl << std::setw(7) << std::hex << off;
                        }
                        if(chr == 0){
                            o << ' ' << "..";
                        }else{
                            o << ' ' << std::setfill('0') << std::setw(2) << std::hex << (0xff & (uint16_t)chr);
                        }
                        off++;
                    }
                }
            };

            /*!
             * \brief Data segments to restore
             */
            std::vector<Segment> data_;

            /*!
             * \brief Next line index to store when writing lines
             */
            ArchData::line_idx_type next_idx_ = ArchData::INVALID_LINE_IDX;

            /*!
             * \brief Index in data_ of next line to restore in nextRestoreLine
             */
            uint32_t next_restore_idx_ = 0;

            /*!
             * \brief iterator in data_ of line being read by call to readLineData.
             * Is always next_restore_idx_ or one less.
             */
            decltype(data_)::const_iterator cur_restore_itr_;

        public:
            VectorStorage() {
            }

            ~VectorStorage() {
            }

            void dump(std::ostream& o) const {
                for(auto const &seg : data_){
                    seg.dump(o);
                }
            }

            uint32_t getSize() const {
                uint32_t bytes = sizeof(decltype(*this));
                for(Segment const & seg : data_){
                    bytes += seg.getSize();
                }
                return bytes;
            }

            void prepareForLoad() {
                next_restore_idx_ = 0;
                cur_restore_itr_ = data_.begin();
            }

            void beginLine(ArchData::line_idx_type idx) {
                sparta_assert(idx != ArchData::INVALID_LINE_IDX,
                              "Cannot begin line with INVALID_LINE_IDX index");
                next_idx_ = idx;
            }

            void writeLineBytes(const char* data, size_t size) {
                sparta_assert(data_.size() == 0 || data_.back().getLineIdx() != next_idx_,
                              "Cannot store the same line idx twice in a checkpoint. Line "
                              << next_idx_ << " detected twice in a row");
                sparta_assert(next_idx_ != ArchData::INVALID_LINE_IDX,
                              "Cannot write line bytes with INVALID_LINE_IDX index");
                data_.emplace_back(next_idx_, data, size);
            }

            /*!
             * \brief Signals end of this checkpoint's data for one ArchData
             */
            void endArchData() {
                data_.emplace_back();
            }

            /*!
             * \brief Is the reading state of this storage good? (i.e. haven't tried
             * to read past the end of the data)
             */
            bool good() const {
                return next_restore_idx_ <= data_.size(); // Not past end of stream
            }

            /*!
             * \brief Restore next line. Return ArchData::INVALID_LINE_IDX on
             * end of data.
             */
            ArchData::line_idx_type getNextRestoreLine() {
                if(next_restore_idx_ == data_.size()){
                    next_restore_idx_++; // Increment to detect errors
                    return ArchData::INVALID_LINE_IDX; // Done with restore
                }else if(next_restore_idx_ > data_.size()){ // Past the end
                    throw SpartaException("Failed to restore a checkpoint because ")
                        << "caller tried to keep getting next line even after "
                        "reaching the end of the restore data";
                }
                if(next_restore_idx_ != 0){
                    cur_restore_itr_++;
                }
                next_restore_idx_++;

                const auto next_line_idx = cur_restore_itr_->getLineIdx(); // May be invalid to indicate end of ArchData
                return next_line_idx;
            };

            /*!
             * \brief Read bytes for the current line
             */
            void copyLineBytes(char* buf, uint32_t size) {
                sparta_assert(cur_restore_itr_ != data_.end(),
                              "Attempted to copy line bytes from an invalid line iterator");
                sparta_assert(cur_restore_itr_->getLineIdx() != ArchData::INVALID_LINE_IDX,
                              "About to return line from checkpoint data segment with INVALID_LINE_IDX index");
                cur_restore_itr_->copyTo(buf, size);
            }

            /*!
             * \brief Helper class used for serialization purposes.
             */
            class DetachedClone
            {
            public:
                DetachedClone(std::vector<std::unique_ptr<Segment::DetachedClone>>&& segment_clones)
                    : segment_clones_(std::move(segment_clones))
                {}

                const std::vector<std::unique_ptr<Segment::DetachedClone>>& getSegmentClones() const {
                    return segment_clones_;
                }

            private:
                std::vector<std::unique_ptr<Segment::DetachedClone>> segment_clones_;
            };

            /*!
             * \brief Clone this vector storage for serialization purposes.
             */
            DetachedClone* clone() const {
                std::vector<std::unique_ptr<Segment::DetachedClone>> cloned_segments;
                cloned_segments.reserve(data_.size());
                for (auto& seg : data_) {
                    cloned_segments.emplace_back(seg.clone());
                }
                return new DetachedClone(std::move(cloned_segments));
            }
        };

    } // namespace storage

    class FastCheckpointer;

    /*!
     * \brief Single delta checkpoint object containing all simulator state
     * which changed since some previous DeltaCheckpoint. Can contain all
     * simulator state if it has no previous DeltaCheckpoint. The previous
     * delta can be referenced by getPrev().
     *
     * Allows timeline branching by having one DeltaCheckpoint be the previous
     * checkpoint of multiple other checkpoints.
     *
     * Once this checkpoint becomes another's previous checkpoint, that
     * checkpoint can be referenced (among the rest) through getNextDeltas().
     *
     * Intended to be constructed and manipulated only by a FastCheckpointer
     * instance.
     *
     * \todo Store reverse deltas additional (or maybe instead) so that rewind
     * is quicker
     */
    template<typename StorageT=storage::VectorStorage>
    class DeltaCheckpoint : public Checkpoint
    {
    public:

        //! \name Construction & Initialization
        //! @{
        ////////////////////////////////////////////////////////////////////////

        //! \brief Not default constructable
        DeltaCheckpoint() = delete;

        //! \brief Not copy constructable
        DeltaCheckpoint(const DeltaCheckpoint&) = delete;

        //! \brief Non-assignable
        const DeltaCheckpoint& operator=(const DeltaCheckpoint&) = delete;

        //! \brief This helper class is used for serialization purposes,
        //! notably removing the checkpoint prev/next pointers in favor
        //! of their checkpoint IDs.
        class DetachedClone : public Checkpoint::DetachedClone
        {
        public:
            using StorageClone = typename StorageT::DetachedClone;

            DetachedClone(chkpt_id_t id,
                          chkpt_id_t prev_id,
                          const std::vector<chkpt_id_t>& next_ids,
                          tick_t tick,
                          chkpt_id_t deleted_id,
                          bool is_snapshot,
                          StorageClone* storage_clone)
                : Checkpoint::DetachedClone(id, prev_id, next_ids, tick)
                , deleted_id_(deleted_id)
                , is_snapshot_(is_snapshot)
                , storage_clone_(storage_clone)
            {}

            chkpt_id_t getDeletedID() const {
                return deleted_id_;
            }

            bool isSnapshot() const {
                return is_snapshot_;
            }

            const StorageClone& getStorage() const {
                return *storage_clone_;
            }

        private:
            chkpt_id_t deleted_id_;
            bool is_snapshot_;
            std::unique_ptr<StorageClone> storage_clone_;
        };

        //! \brief Clonable
        DetachedClone* clone() const override {
            auto tick = getTick();
            auto id = getID();
            auto prev_id = getPrev() ? getPrev()->getID() : Checkpoint::UNIDENTIFIED_CHECKPOINT;

            std::vector<chkpt_id_t> next_ids;
            for (auto c : getNexts()) {
                next_ids.push_back(c->getID());
            }

            return new DetachedClone(id, prev_id, next_ids, tick, deleted_id_, is_snapshot_, data_.clone());
        }

    private:

        /*!
         * \brief Construct a checkpoint
         * \param root TreeNode at which the checkpoint will be taken. Includes
         * this node and all children of any depth.
         * \param dats ArchDatas relevant to checkpointing this simulation
         * starting from root as determined by the checkpointer that owns this
         * checkpoint
         * \param id ID of this checkpoint which distinguishes it from all other
         * DeltaCheckpoints having the same owning FastCheckpointer. If
         * UNIDENTIFIED_CHECKPOINT, this checkpoint cannot be referenced
         * directly and serves only as an anonymous, intermediate delta which
         * will be removed after it receives one or more next deltas and then
         * loses its last next_delta.
         * \param tick Simulator tick number at which this checkpoint was taken
         * \param prev_delta Points to a checkpoint having a lower or equal tick
         * number. If nullptr, then \a snapshot arg must be true. Note that a
         * prev_delta \b must be specified unless this is the very first
         * checkpoint in the simulation (head). Multiple heads are not allowed
         * \param is_snapshot Store as a full snapshot (all simulation state).
         * Otherwise, this checkpoint will store only the changes in any
         * ArchData object where lines are flagged as changed. Note that this
         * requires that ArchData line states reflect status since \a prev_delta
         * was created or longer. It is the caller's responsibility to ensure
         * this. If not ensured, a loaded checkpoint could produce incorrect
         * state
         *
         * Snapshot checkpoint can be restored without walking any checkpoint
         * chains
         */
        DeltaCheckpoint(TreeNode& root,
                        const std::vector<ArchData*>& dats,
                        chkpt_id_t id,
                        tick_t tick,
                        DeltaCheckpoint* prev_delta,
                        bool is_snapshot) :
            Checkpoint(id, tick, prev_delta),
            deleted_id_(UNIDENTIFIED_CHECKPOINT),
            is_snapshot_(is_snapshot)
        {
            (void) root;
            if(nullptr == prev_delta){
                if(is_snapshot == false){
                    throw CheckpointError("Cannot create a DeltaCheckpoint id=")
                        << id << " at tick=" << tick << " which has no prev_delta and is not a snapshot";
                }
            }else{
                prev_delta->addNext(this);
            }

            // Store the checkpoint from root
            if(is_snapshot){
                storeSnapshot_(dats);
            }else{
                storeDelta_(dats);
            }
        }

        //! DeltaCheckpoints can only be constructed by the FastCheckpointer
        friend class FastCheckpointer;

    public:

        /*!
         * \brief Destructor
         * \note Checkpoint destructor removes this Checkpoint from the chain.
         *
         * Prints a warning if checkpoint was not allowed to be deleted
         * \see canDelete
         */
        virtual ~DeltaCheckpoint() {
            if(!canDelete()){
                std::cerr << "WARNING: DeltaCheckpoint " << getID()
                          << " being destructed without being allowed to delete" << std::endl;
            }
        }

        ////////////////////////////////////////////////////////////////////////
        //! @}

        /*!
         * \brief Returns a string describing this object
         */
        virtual std::string stringize() const override {
            std::stringstream ss;
            ss << "<DeltaCheckpoint id=";
            if(isFlaggedDeleted()){
                ss << "DELETED";
            }else{
                ss << getID();
            }
            ss << " at t=" << getTick();
            if(isSnapshot()){
                ss << "(snapshot)";
            }
            ss << ' ' << getTotalMemoryUse()/1000.0f << "kB (" << getContentMemoryUse()/1000.0f << "kB Data)";
            ss << '>';
            return ss.str();
        }

        /*!
         * \brief Writes all checkpoint raw data to an ostream
         * \param o ostream to which raw data will be written
         * \note No newlines or other extra characters will be appended
         */
        virtual void dumpData(std::ostream& o) const override {
            data_.dump(o);
        }

        /*!
         * \brief Dumps the restore chain for this checkpoint.
         * \see getRestoreChain()
         * \param o ostream to which chain data will be dumped
         */
        void dumpRestoreChain(std::ostream& o) const {
            auto rc = getRestoreChain();
            while(1){
                const DeltaCheckpoint* cp = rc.top();
                rc.pop();
                if(cp->isSnapshot()){
                    o << '(';
                }
                if(cp->getID() == UNIDENTIFIED_CHECKPOINT){
                    o << "*" << getDeletedID() << "";
                }else{
                    o << cp->getID();
                }
                if(cp->isSnapshot()){
                    o << ')';
                }
                if(rc.empty()){
                    break;
                }else{
                    o << " --> ";
                }
            }
        }

        /*!
         * \brief Returns memory usage by this checkpoint
         */
        virtual uint64_t getTotalMemoryUse() const noexcept override {
            return getContentMemoryUse() \
                + sizeof(decltype(*this)) \
                + (getNexts().size() * sizeof(typename std::remove_reference<decltype(*this)>::type*));
        }

        /*!
         * \brief Returns memory usage by the content of this checkpoint
         */
        virtual uint64_t getContentMemoryUse() const noexcept override {
            return data_.getSize();
        }

        //! \name Checkpoint Actions
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Implement trace of a value across the restore chain as described in Checkpointer::traceValue
         */
        void traceValue(std::ostream& o, const std::vector<ArchData*>& dats,
                        const ArchData* container, uint32_t offset, uint32_t size)
        {
            std::stack<DeltaCheckpoint*> dcps = getHistoryChain();

            std::vector<std::pair<uint8_t,bool>> bytes; // pair<value,valid>
            bytes.resize(size, decltype(bytes)::value_type(0,false));

            constexpr uint32_t BUF_SIZE = 8192*2;
            std::unique_ptr<char[]> buf(new char[BUF_SIZE]); // Line-reading buffer

            while(!dcps.empty()){
                DeltaCheckpoint* d = dcps.top();
                o << "trace: Checkpoint " << d->getDeletedRepr() << (d->isSnapshot()?" (snapshot)":"") << std::endl;
                dcps.pop();
                d->data_.prepareForLoad();
                bool found_ad = false;
                bool changed = false;
                for(ArchData* ad : dats){
                    if(ad == container){
                        found_ad = true;
                        if(d->isSnapshot()){
                            for(auto &x : bytes){
                                x.second = false; // Invalidate result due to snapshot load
                            }
                        }
                    }
                    while(1){
                        auto ln_idx = d->data_.getNextRestoreLine();
                        if(ln_idx == ArchData::INVALID_LINE_IDX){
                            if(ad == container){
                                //if(changed == false){
                                //    o << "trace: No data for this value" << std::endl;
                                //}
                            }
                            break; // Done with this ArchData
                        }
                        auto ln_off = ln_idx * ad->getLineSize();
                        sparta_assert(BUF_SIZE > ad->getLineSize(),
                                      "Cannot trace value on ArchDatas with line sizes > " << BUF_SIZE << " (" << ad->getLineSize() << ")");
                        d->data_.copyLineBytes((char*)buf.get(), ad->getLineSize()); // Need to read regardless of usefulness of data
                        if(ad == container){
                            //o << "trace: Contains data for line idx " << std::dec << ln_idx
                            //  << " offsets [" << ln_off << "," << ln_off+ad->getLineSize() << ")" << std::endl;
                            if(offset >= ln_off && offset < ln_off + ad->getLineSize()){
                                sparta_assert(offset+size < ln_off + ad->getLineSize(),
                                              "Cannot trace value which spans multiple lines!");
                                sparta_assert(changed == false,
                                              "Value being traced changed twice in the same checkpoint");
                                changed = true;
                                auto off_in_line = offset - ln_off;
                                o << "trace: Value changed (line " << std::dec << ln_idx << ")" << std::endl;
                                for(uint32_t i=0; i<bytes.size(); i++){
                                    bytes[i].first = buf.get()[i+off_in_line];
                                    bytes[i].second = true; // Valid
                                }
                            }
                        }
                    }
                }
                if(!found_ad){
                    o << "trace: Could not find selected ArchData " << (const void*)container << " in this checkpoint!" << std::endl;
                }
                o << "trace: Value:";
                for(uint32_t i=0; i<bytes.size(); i++){
                    if(bytes[i].second){
                        o << ' ' << std::setfill('0') << std::setw(2) << std::hex << (uint16_t)bytes[i].first;
                    }else{
                        o << " xx"; // Invalid
                    }
                }
                o << std::endl;
            }
            o << std::endl;
        }

        /*!
         * \brief Returns a stack of checkpoints from this checkpoint as far
         * back as possible until no previous link is found. This is a superset
         * of getRestoreChain and contains checkpoints that do not actually need
         * to be inspected for restoring this checkpoint's data. This may reach
         * the head checkpoint if no gaps are encountered.
         */
        std::stack<DeltaCheckpoint*> getHistoryChain() {
            // Build stack up to last snapshot
            DeltaCheckpoint* n = this;
            std::stack<DeltaCheckpoint*> dcps;
            while(n){
                dcps.push(n);
                n = static_cast<DeltaCheckpoint*>(n->getPrev());
            }
            return dcps;
        }

        /*!
         * \brief Returns a stack of checkpoints that must be restored from
         * top-to-bottom to fully restore the state associated with this
         * checkpoint.
         */
        std::stack<DeltaCheckpoint*> getRestoreChain() {
            // Build stack up to last snapshot
            DeltaCheckpoint* n = this;
            std::stack<DeltaCheckpoint*> dcps;
            while(1){
                dcps.push(n);
                if(n->isSnapshot()){
                    break;
                }
                n = static_cast<DeltaCheckpoint*>(n->getPrev());
            }
            return dcps;
        }

        /*!
         * \brief Const-qualified version of getRestoreChain
         */
        std::stack<const DeltaCheckpoint*> getRestoreChain() const {
            // Build stack up to last snapshot
            const DeltaCheckpoint* n = this;
            std::stack<const DeltaCheckpoint*> dcps;
            while(1){
                dcps.push(n);
                if(n->isSnapshot()){
                    break;
                }
                n = static_cast<const DeltaCheckpoint*>(n->getPrev());
            }
            return dcps;
        }

        /*!
         * \brief Attempts to restore this checkpoint including any previous
         * deltas (dependencies).
         *
         * Uses loadState to restore state from each checkpoint in the
         * restore chain.
         */
        virtual void load(const std::vector<ArchData*>& dats) override {
            // Build stack up to last snapshot
            std::stack<DeltaCheckpoint*> dcps = getRestoreChain();

            // Load in proper order
            while(!dcps.empty()){
                DeltaCheckpoint* d = dcps.top();
                dcps.pop();
                d->loadState(dats);
            }
        }

        /*!
         * \brief Can this checkpoint be deleted
         * Cannot be deleted if:
         * \li This checkpoint has any ancestors which are not deletable and not snapshots
         * \li This checkpoint was not flagged for deletion with flagDeleted
         * \warning This is a recursive search of a checkpoint tree which has potentially many
         * branches and could have high time cost
         */
        bool canDelete() const noexcept {
            if(!isFlaggedDeleted()){
                return false;
            }
            for(auto d : getNexts()){
                const DeltaCheckpoint* dcp = static_cast<const DeltaCheckpoint*>(d);
                if(!dcp->canDelete() && !dcp->isSnapshot()){
                    return false;
                }
            }
            return true; // This checkpoint was flagged deleted
        }

        /*!
         * \brief Allows this checkpoint to be deleted if it is no longer a
         * previous delta of some other delta (i.e. getNexts() returns an
         * empty vector). Sets the checkpoint ID to invalid. Calling multiple
         * times has no effect
         * \pre Must not already be flagged deleted
         * \post isFlaggedDeleted() will return true
         * \post getDeletedID() will return the current ID (if any)
         * \see canDelete
         * \see isFlaggedDeleted
         */
        void flagDeleted() {
            sparta_assert(not isFlaggedDeleted(),
                          "Cannot delete a checkpoint when it is already deleted: " << this);
            deleted_id_ = getID();
            setID_(UNIDENTIFIED_CHECKPOINT);
        }

        /*!
         * \brief Indicates whether this checkpoint has been flagged deleted.
         * \note Does not imply that the checkpoint can safely be deleted;
         * only that it was flagged for deletion.
         * \note If false, Checkpoint ID will also be UNIDENTIFIED_CHECKPOINT
         * \see flagDeleted()
         */
        bool isFlaggedDeleted() const noexcept {
            return getID() == UNIDENTIFIED_CHECKPOINT;
        }

        /*!
         * \brief Return the ID had by this checkpoint before it was deleted
         * If this checkpoint has not been flagged for deletion, this will be
         * UNIDENTIFIED_CHECKPOINT
         */
        chkpt_id_t getDeletedID() const noexcept {
            return deleted_id_;
        }

        /*!
         * \brief Gets the representation of this deleted checkpoint as part of
         * a checkpoint chain (if that checkpointer supports deletion)
         * \return "D-" concatenate with ID copied when being deleted. Returns
         * the ID if not yet deleted
         */
        virtual std::string getDeletedRepr() const override {
            std::stringstream ss;
            if(isFlaggedDeleted()){
                ss << "*" << getDeletedID();
            }else{
                ss << getID();
            }
            return ss.str();
        }

        /*!
         * \brief Is this checkpoint a snapshot (contains ALL simulator state)
         */
        bool isSnapshot() const noexcept { return is_snapshot_; }

        /*!
         * \brief Determines how many checkpoints away the closest, earlier
         * snapshot is.
         * \return distance to closest snapshot. If this node is a snapshot,
         * returns 0; if immediate getPrev() is a snapshot, returns 1; and
         * so on.
         *
         * \note This is a noexcept function, which means that the exception if
         * no snapshot is encountered is uncatchable. This is intentional.
         */
        uint32_t getDistanceToPrevSnapshot() const noexcept {
            const DeltaCheckpoint* d = this;
            uint32_t dist = 0;
            while(d){
                if(d->isSnapshot()){
                    return dist;
                }
                d = static_cast<const DeltaCheckpoint*>(d->getPrev());
                ++dist;
            }

            // This will compile just fine....
#ifdef __clang__
            // This is known to be needed with Clang 8.0.0; not sure about other versions.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wexceptions"
#elif defined __GNUC__  // Note that clang defines both __clang__ and __GNUC__
#pragma GCC diagnostic push
#if __GNUC__ > 5
            // gcc 4.7 doesn't like this.
#pragma GCC diagnostic ignored "-Wterminate"
#endif  // __GNUC__ > 5
#endif  // ifdef __clang__ ... elif defined __GNUC__ ...


            throw CheckpointError() << "In getDistanceToPrevious, somehow reached null "
                                    << "previous-checkpoint without encountering a snapshot. This should "
                                    << "never occur and is a critical error";
#ifdef __clang__
#pragma clang diagnostic pop
#elif defined __GNUC__
#pragma GCC diagnostic pop
#endif


            // But using my macro that injects the pragmas doesn't work... ugh
            /*
              TERMINATING_THROW(CheckpointError, "In getDistanceToPrevious, somehow reached null "
              << "previous-checkpoint without encountering a snapshot. This should "
              << "never occur and is a critical error");
            */
        }

        ////////////////////////////////////////////////////////////////////////
        //! @}

    protected:

        /*!
         * \brief Loads delta state of this checkpoint to root.
         * Does not look at any other checkpoints checkpoints.
         * \see load
         */
        void loadState(const std::vector<ArchData*>& dats) {
            data_.prepareForLoad();
            sparta_assert(data_.good(),
                          "Attempted to loadState from a DeltaCheckpoint with a bad data buffer");
            if(isSnapshot()){
                for(ArchData* ad : dats){
                    //std::cout << "Restoring for ArchData: " << (void*)ad << " " << ad->getOwnerNode()->getLocation() << std::endl;
                    ad->restoreAll(data_);
                }
            }else{
                for(ArchData* ad : dats){
                    //std::cout << "Restoring for ArchData: " << (void*)ad << " " << ad->getOwnerNode()->getLocation() << std::endl;
                    ad->restore(data_);
                }
            }
        }

    private:

        //! \name Internal storage mechanisms
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Writes checkpoint data starting from current root to
         * checkpoint storage
         * \pre Must not have already stored data for this checkpoint
         * This should only be called at construction
         */
        void storeSnapshot_(const std::vector<ArchData*>& dats) {
            sparta_assert(data_.good(),
                          "Attempted to storeSnapshot_ from a DeltaCheckpoint with a bad data buffer");
            // Cannot have stored already
            for(ArchData* ad : dats){
                //std::cout << "SaveAll for ArchData: " << ad->getOwnerNode()->getLocation() << std::endl;
                ad->saveAll(data_);
            }
        }

        /*!
         * \brief Writes checkpoint data starting from current root to
         * checkpoint storage
         * \pre Must not have already stored data for this checkpoint
         * This should only be called at construction
         */
        void storeDelta_(const std::vector<ArchData*>& dats) {
            sparta_assert(data_.good(),
                          "Attempted to storeDelta_ from a DeltaCheckpoint with a bad data buffer");
            // Cannot have stored already
            for(ArchData* ad : dats){
                //std::cout << "Save for ArchData: " << ad->getOwnerNode()->getLocation() << std::endl;
                ad->save(data_);
            }
        }

        ////////////////////////////////////////////////////////////////////////
        //! @}


        /*!
         * \brief ID of the checkpoint before it was deleted. This is invalid
         * until deletion. Prevents misuse of checkpoint ID or any confusion
         * about whether it is deleted or not.
         */
        chkpt_id_t deleted_id_;
        bool const is_snapshot_; //!< Is this node a snapshot
        StorageT data_; //!< Storage implementation
    };

} // namespace sparta::serialization::checkpoint
