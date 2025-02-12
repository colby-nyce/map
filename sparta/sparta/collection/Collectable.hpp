// <Collectable.hpp>  -*- C++ -*-

/**
 * \file Collectable.hpp
 *
 * \brief Implementation of the Collectable class that allows
 *        a user to collect an object into an pipeViewer pipeline file
 */

#pragma once

#include <algorithm>
#include <sstream>
#include <functional>
#include <type_traits>
#include <iomanip>

#include "sparta/collection/PipelineCollector.hpp"
#include "sparta/events/PayloadEvent.hpp"
#include "sparta/events/EventSet.hpp"
#include "sparta/events/SchedulingPhases.hpp"
#include "sparta/utils/Utils.hpp"
#include "sparta/utils/MetaStructs.hpp"

namespace sparta{
    namespace collection
    {

        /**
         * \class Collectable
         * \brief Class used to either manually or auto-collect an Annotation String
         *        object in a pipeline database
         * \tparam DataT The DataT of the collectable being collected
         * \tparam collection_phase The phase collection will occur.  If
         *                          sparta::SchedulingPhase::Collection,
         *                          collection is done prior to Tick.
         *
         *  Auto-collection will occur only if a Collectable is
         *  constructed with a collected_object.  If no object is
         *  provided, it assumes a manual collection and the
         *  scheduling phase is ignored.
         */

        template<typename DataT, SchedulingPhase collection_phase = SchedulingPhase::Collection, typename = void>
        class Collectable : public CollectableTreeNode
        {
        public:
            /**
             * \brief Construct the Collectable, no data object associated, part of a group
             * \param parent A pointer to a parent treenode.  Must not be null
             * \param name The name for which to create this object as a child sparta::TreeNode
             * \param group the name of the group for this treenode
             * \param index the index within the group
             * \param desc A description for the interface
             */
            Collectable(sparta::TreeNode* parent,
                        const std::string& name,
                        const std::string& group,
                        uint32_t index,
                        const std::string & desc = "Collectable <manual, no desc>") :
                CollectableTreeNode(sparta::notNull(parent), name, group, index, desc),
                event_set_(this),
                ev_close_record_(&event_set_, name + "_pipeline_collectable_close_event",
                                 CREATE_SPARTA_HANDLER_WITH_DATA(Collectable, closeRecord, bool))
            {
            }

            /**
             * \brief Construct the Collectable
             * \param parent A pointer to a parent treenode.  Must not be null
             * \param name The name for which to create this object as a child sparta::TreeNode
             * \param collected_object Pointer to the object to collect during the "COLLECT" phase
             * \param desc A description for the interface
             */
            Collectable(sparta::TreeNode* parent,
                        const std::string& name,
                        const DataT * collected_object,
                        const std::string & desc = "Collectable <no desc>") :
                Collectable(parent, name,
                            TreeNode::GROUP_NAME_NONE,
                            TreeNode::GROUP_IDX_NONE,
                            desc)
            {
                collected_object_ = collected_object;

                //TODO cnyce: prev_annot_ / initialize()
            }

            /**
             * \brief Construct the Collectable, no data object associated
             * \param parent A pointer to a parent treenode.  Must not be null
             * \param name The name for which to create this object as a child sparta::TreeNode
             * \param desc A description for the interface
             */
            Collectable(sparta::TreeNode* parent,
                        const std::string& name,
                        const std::string & desc = "Collectable <manual, no desc>") :
                Collectable(parent, name, nullptr, desc)
            {
                // Can't auto collect without setting collected_object_
                setManualCollection();
            }

            //! Virtual destructor -- does nothing
            virtual ~Collectable() {}

            //! Explicitly/manually collect a value for this collectable, ignoring
            //! what the Collectable is currently pointing to.
            //! Here we pass the actual object of the collectable type we are collecting.
            template<typename T>
            MetaStruct::enable_if_t<!MetaStruct::is_any_pointer<T>::value, void>
            collect(const T & val)
            {
                //TODO cnyce
                (void)val;

                if(SPARTA_EXPECT_TRUE(isCollected()))
                {
                    //std::ostringstream ss;
                    //ss << val;
                    //if((ss.str() != prev_annot_) && !record_closed_)
                    //{
                    //    // Close the old record (if there is one)
                    //    closeRecord();
                    //}

                    // Remember the new string for a new record and start
                    // a new record if not empty.
                    //prev_annot_ = ss.str();
                    //if(!prev_annot_.empty() && record_closed_) {
                    //    startNewRecord_();
                    //    record_closed_ = false;
                    //}
                }
            }

            //! Explicitly/manually collect a value for this collectable, ignoring
            //! what the Collectable is currently pointing to.
            //! Here we pass the shared pointer to the actual object of the collectable type we are collecting.
            template <typename T>
            MetaStruct::enable_if_t<MetaStruct::is_any_pointer<T>::value, void>
            collect(const T & val){
                // If pointer has become nullified, close the record
                if(nullptr == val) {
                    closeRecord();
                    return;
                }
                collect(*val);
            }

            /*!
             * \brief Explicitly collect a value for the given duration
             * \param duration The amount of time in cycles the value is available
             *
             * Explicitly collect a value for this collectable for the
             * given amount of time.
             *
             * \warn No checks are performed if a new value is collected
             *       within the previous duration!
             */
            template<typename T>
            MetaStruct::enable_if_t<!MetaStruct::is_any_pointer<T>::value, void>
            collectWithDuration(const T & val, sparta::Clock::Cycle duration){
                if(SPARTA_EXPECT_TRUE(isCollected()))
                {
                    if(duration != 0) {
                        ev_close_record_.preparePayload(false)->schedule(duration);
                    }
                    collect(val);
                }
            }

            /*!
             * \brief Explicitly collect a value from a shared pointer for the given duration
             * \param duration The amount of time in cycles the value is available
             *
             * Explicitly collect a value for this collectable passed as a shared pointer for the
             * given amount of time.
             *
             * \warn No checks are performed if a new value is collected
             *       within the previous duration!
             */
            template<typename T>
            MetaStruct::enable_if_t<MetaStruct::is_any_pointer<T>::value, void>
            collectWithDuration(const T & val, sparta::Clock::Cycle duration){
                // If pointer has become nullified, close the record
                if(nullptr == val) {
                    closeRecord();
                    return;
                }
                collectWithDuration(*val, duration);
            }

            //! Virtual method called by
            //! CollectableTreeNode/PipelineCollector when a user of the
            //! TreeNode requests this object to be collected.
            void collect() override final {
                // If pointer has become nullified, close the record
                if(nullptr == collected_object_) {
                    closeRecord();
                    return;
                }
                collect(*collected_object_);
            }

            /*!
             * \brief Calls collectWithDuration using the internal collected_object_
             * specified at construction.
             * \pre Must have constructed wit ha non-null collected object
             */
            void collectWithDuration(sparta::Clock::Cycle duration) {
                // If pointer has become nullified, close the record
                if(nullptr == collected_object_) {
                    closeRecord();
                    return;
                }
                collectWithDuration(*collected_object_, duration);
            }

            //! Force close a record.
            void closeRecord(const bool & simulation_ending = false) override final
            {
                if(SPARTA_EXPECT_TRUE(isCollected()))
                {
                    if(!record_closed_) {
                        writeRecord_(simulation_ending);
                        record_closed_ = true;
                    }
                }
            }

            //! \brief Do not perform any automatic collection
            //! The SchedulingPhase is ignored
            void setManualCollection()
            {
                auto_collect_ = false;
            }

        protected:

            //! \brief Get a reference to the internal event set
            //! \return Reference to event set -- used by DelayedCollectable
            inline EventSet & getEventSet_() {
                return event_set_;
            }

        private:
            //! Return true if the record was written; false otherwise
            bool writeRecord_(bool simulation_ending = false)
            {
                //TODO cnyce
                (void)simulation_ending;
                return true;
            }

            //! Start a new record
            void startNewRecord_()
            {
                //TODO cnyce
            }

            void setCollecting_(bool collect, PipelineCollector * collector, simdb::DatabaseManager*) override final
            {
                // If the collected object is null, this Collectable
                // object is to be explicitly collected
                if(collected_object_ && auto_collect_) {
                    if(collect) {
                        // Add this Collectable to the PipelineCollector's
                        // list of objects requiring collection
                        collector->addToAutoCollection(this, collection_phase);
                    }
                    else {
                        // Remove this Collectable from the
                        // PipelineCollector's list of objects requiring
                        // collection
                        collector->removeFromAutoCollection(this);
                    }
                }

                if(!collect && !record_closed_) {
                    // Force the record to be written
                    closeRecord();
                }
            }

            // The annotation object to be collected
            const DataT * collected_object_ = nullptr;

            // For those folks that want a value to automatically
            // disappear in the future
            sparta::EventSet event_set_;
            sparta::PayloadEvent<bool, sparta::SchedulingPhase::Trigger> ev_close_record_;

            // Is this collectable currently closed?
            bool record_closed_ = true;

            // Should we auto-collect?
            bool auto_collect_ = true;
        };

}//namespace collection
}//namespace sparta
