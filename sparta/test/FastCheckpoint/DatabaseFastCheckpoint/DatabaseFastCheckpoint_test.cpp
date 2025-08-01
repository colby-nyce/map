
#include <inttypes.h>
#include <cstdio>
#include <iostream>
#include <stack>
#include <ctime>
#include <array>

#include "sparta/sparta.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/log/Tap.hpp"
#include "sparta/log/Destination.hpp"
#include "sparta/functional/Register.hpp"
#include "sparta/functional/RegisterSet.hpp"
#include "sparta/memory/MemoryObject.hpp"
#include "sparta/serialization/checkpoint/FastCheckpointer.hpp"
#include "sparta/serialization/checkpoint/DatabaseBackingStore.hpp"
#include "simdb/sqlite/DatabaseManager.hpp"
#include "simdb/apps/AppManager.hpp"

#include "sparta/utils/SpartaTester.hpp"

/*!
 * \file DatabaseFastCheckpoint_test.cpp
 * \brief Test for FastCheckpoints backed by SimDB
 *
 * This is modified from FastCheckpoint_test.cpp.
 *
 * Register is built on DataView and RegisterSet is built on ArchData.
 * The DataView test performs extensive testing so some test-cases related
 * to register sizes and layouts may be omitted from this test.
 */

TEST_INIT

using sparta::Register;
using sparta::RegisterSet;
using sparta::RootTreeNode;
using sparta::memory::MemoryObject;
using sparta::memory::BlockingMemoryObjectIFNode;
using sparta::serialization::checkpoint::FastCheckpointer;
using sparta::serialization::checkpoint::DatabaseBackingStore;

static const uint16_t HINT_NONE=0;

//
// Some register and field definition tables
//

Register::Definition reg_defs[] = {
    { 0, "reg0", Register::GROUP_NUM_NONE, "", Register::GROUP_IDX_NONE, "reg desc", 1,  {}, {}, nullptr, Register::INVALID_ID, 0, nullptr, HINT_NONE, 0 },
    { 1, "reg1", Register::GROUP_NUM_NONE, "", Register::GROUP_IDX_NONE, "reg desc", 2,  {}, {}, nullptr, Register::INVALID_ID, 0, nullptr, HINT_NONE, 0 },
    { 2, "reg2", Register::GROUP_NUM_NONE, "", Register::GROUP_IDX_NONE, "reg desc", 4,  {}, {}, nullptr, Register::INVALID_ID, 0, nullptr, HINT_NONE, 0 },
    { 3, "reg3", Register::GROUP_NUM_NONE, "", Register::GROUP_IDX_NONE, "reg desc", 8,  {}, {}, nullptr, Register::INVALID_ID, 0, nullptr, HINT_NONE, 0 },
    { 4, "reg4", Register::GROUP_NUM_NONE, "", Register::GROUP_IDX_NONE, "reg desc", 16, {}, {}, nullptr, Register::INVALID_ID, 0, nullptr, HINT_NONE, 0 },
    Register::DEFINITION_END
};


//! Dummy device
class DummyDevice : public sparta::TreeNode
{
public:
    DummyDevice(sparta::TreeNode* parent) :
        sparta::TreeNode(parent, "dummy", "", sparta::TreeNode::GROUP_IDX_NONE, "dummy node for checkpoint test")
    {}
};

//! \brief General test for checkpointing behavior. Creates/deletes/loads, etc.
void generalTest()
{
    sparta::Scheduler sched;
    RootTreeNode clocks("clocks");
    sparta::Clock clk(&clocks, "clock", &sched);

    // Create a tree with some register sets and a memory
    RootTreeNode root;
    DummyDevice dummy(&root);
    std::unique_ptr<RegisterSet> rset(RegisterSet::create(&dummy, reg_defs));
    auto r1 = rset->getRegister("reg2");
    DummyDevice dummy2(&dummy);
    std::unique_ptr<RegisterSet> rset2(RegisterSet::create(&dummy2, reg_defs));
    auto r2 = rset2->getRegister("reg2");
    EXPECT_NOTEQUAL(r1, r2);
    MemoryObject mem_obj(&dummy2, // underlying ArchData is associated and checkpointed through with this node.
                         64, // 64B blocks
                         4096, // 4k size
                         0xcc, // fill with conspicuous bytes
                         1 // 1 byte of fill
                         );
    BlockingMemoryObjectIFNode mem_if(&dummy2, // Parent node
                                      "mem", // Name
                                      "Memory interface",
                                      nullptr, // associated translation interface
                                      mem_obj);

    // Print current register set by the ostream insertion operator
    std::cout << *rset << std::endl;


    // Create a checkpointer
    simdb::DatabaseManager db_mgr("test.db", true);
    simdb::AppManager app_mgr(&db_mgr);
    FastCheckpointer<DatabaseBackingStore> fcp(root, &sched, app_mgr);
    fcp.setSnapshotThreshold(5);

    app_mgr.createEnabledApps();
    app_mgr.createSchemas();
    app_mgr.postInit(0, nullptr);
    app_mgr.openPipelines();

    root.enterConfiguring();
    root.enterFinalized();

    // Set up checkpointing (after tree finalization)

    EXPECT_EQUAL(sched.getCurrentTick(), 0); //unfinalized sched at tick 0

    // Teardown
    app_mgr.postSimLoopTeardown();
}

int main() {
    std::unique_ptr<sparta::log::Tap> warn_cerr(new sparta::log::Tap(
        sparta::TreeNode::getVirtualGlobalNode(),
        sparta::log::categories::WARN,
        std::cerr));

    std::unique_ptr<sparta::log::Tap> warn_file(new sparta::log::Tap(
        sparta::TreeNode::getVirtualGlobalNode(),
        sparta::log::categories::WARN,
        "warnings.log"));

    generalTest();

    REPORT_ERROR;
    return ERROR_CODE;
}
