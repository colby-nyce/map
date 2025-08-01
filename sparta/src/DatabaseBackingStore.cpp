#include "sparta/serialization/checkpoint/DatabaseBackingStore.hpp"
#include "simdb/apps/App.hpp"
#include "simdb/apps/AppRegistration.hpp"
#include "simdb/pipeline/Pipeline.hpp"

namespace sparta::serialization::checkpoint {

    using checkpoint_type = typename DatabaseBackingStore::checkpoint_type;

    class DatabaseBackingStoreImpl : public simdb::App
    {
    public:
        static constexpr auto NAME = "database-backing-store";

        DatabaseBackingStoreImpl(simdb::DatabaseManager*)
        {}

        static void defineSchema(simdb::Schema& schema)
        {
            (void)schema;
        }

        checkpoint_type* findCheckpoint(chkpt_id_t id) noexcept {
            (void)id;
            return nullptr;
        }

        bool hasCheckpoint(chkpt_id_t id) const noexcept {
            (void)id;
            return false;
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

        std::vector<chkpt_id_t> getCheckpointsAt(tick_t t) const {
            (void)t;
            return {};
        }

        std::vector<chkpt_id_t> getCheckpoints() const {
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

        void postInit(int argc, char** argv) override {
            (void)argc;
            (void)argv;
        }

        std::unique_ptr<simdb::pipeline::Pipeline> createPipeline(
            simdb::pipeline::AsyncDatabaseAccessor* db_accessor) override
        {
            (void)db_accessor;
            return nullptr;
        }

        void preTeardown() override {}

        void postTeardown() override {}

    private:
    };

    REGISTER_SIMDB_APPLICATION(DatabaseBackingStoreImpl);

    DatabaseBackingStore::Impl::Impl(simdb::AppManager& app_mgr)
        : app_mgr_(app_mgr)
    {
        app_mgr.enableApp(DatabaseBackingStoreImpl::NAME);
    }

    DatabaseBackingStoreImpl* DatabaseBackingStore::Impl::operator->()
    {
        cacheImpl_();
        return impl_;
    }

    const DatabaseBackingStoreImpl* DatabaseBackingStore::Impl::operator->() const
    {
        cacheImpl_();
        return impl_;
    }

    void DatabaseBackingStore::Impl::cacheImpl_() const
    {
        if (!impl_)
        {
            impl_ = app_mgr_.getApp<DatabaseBackingStoreImpl>();
            if (!impl_)
            {
                throw SpartaException("Unable to get the DatabaseBackingStore app");
            }
        }
    }

    DatabaseBackingStore::DatabaseBackingStore(simdb::AppManager& app_mgr)
        : impl_(app_mgr)
    {
    }

    checkpoint_type* DatabaseBackingStore::findCheckpoint(chkpt_id_t id) noexcept
    {
        return impl_->findCheckpoint(id);
    }

    bool DatabaseBackingStore::hasCheckpoint(chkpt_id_t id) const noexcept
    {
        return impl_->hasCheckpoint(id);
    }

    uint64_t DatabaseBackingStore::getTotalMemoryUse() const
    {
        return impl_->getTotalMemoryUse();
    }

    uint64_t DatabaseBackingStore::getContentMemoryUse() const
    {
        return impl_->getContentMemoryUse();
    }

    void DatabaseBackingStore::dumpList(std::ostream& o) const
    {
        impl_->dumpList(o);
    }

    void DatabaseBackingStore::dumpData(std::ostream& o) const
    {
        impl_->dumpData(o);
    }

    void DatabaseBackingStore::dumpAnnotatedData(std::ostream& o) const
    {
        impl_->dumpAnnotatedData(o);
    }

    std::vector<chkpt_id_t> DatabaseBackingStore::getCheckpointsAt(tick_t t) const
    {
        return impl_->getCheckpointsAt(t);
    }

    std::vector<chkpt_id_t> DatabaseBackingStore::getCheckpoints() const
    {
        return impl_->getCheckpoints();
    }

    void DatabaseBackingStore::erase(chkpt_id_t id)
    {
        impl_->erase(id);
    }

    void DatabaseBackingStore::insert(std::unique_ptr<Checkpoint> chkpt)
    {
        impl_->insert(std::move(chkpt));
    }

    void DatabaseBackingStore::flagAllDeleted()
    {
        impl_->flagAllDeleted();
    }

    void DatabaseBackingStore::setArchDatas(std::vector<ArchData*>& adatas)
    {
        impl_->setArchDatas(adatas);
    }

    void DatabaseBackingStore::traceValue(std::ostream& o, chkpt_id_t id, const ArchData* container, uint32_t offset, uint32_t size)
    {
        impl_->traceValue(o, id, container, offset, size);
    }
}
