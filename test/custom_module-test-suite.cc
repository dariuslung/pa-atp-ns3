
// Include a header file from your module to test.
#include "ns3/custom_module.h"

// An essential include is test.h
#include "ns3/test.h"

// Do not put your test classes in namespace ns3.  You may find it useful
// to use the using directive to access the ns3 namespace directly
using namespace ns3;

// Add a doxygen group for tests.
// If you have more than one test, this should be in only one of them.
/**
 * \defgroup custom_module-tests Tests for custom_module
 * \ingroup custom_module
 * \ingroup tests
 */

// This is an example TestCase.
/**
 * \ingroup custom_module-tests
 * Test case for feature 1
 */
class Custom_moduleTestCase1 : public TestCase
{
  public:
    Custom_moduleTestCase1();
    virtual ~Custom_moduleTestCase1();

  private:
    void DoRun() override;
};

// Add some help text to this case to describe what it is intended to test
Custom_moduleTestCase1::Custom_moduleTestCase1()
    : TestCase("Custom_module test case (does nothing)")
{
}

// This destructor does nothing but we include it as a reminder that
// the test case should clean up after itself
Custom_moduleTestCase1::~Custom_moduleTestCase1()
{
}

//
// This method is the pure virtual method from class TestCase that every
// TestCase must implement
//
void
Custom_moduleTestCase1::DoRun()
{
    // A wide variety of test macros are available in src/core/test.h
    NS_TEST_ASSERT_MSG_EQ(true, true, "true doesn't equal true for some reason");
    // Use this one for floating point comparisons
    NS_TEST_ASSERT_MSG_EQ_TOL(0.01, 0.01, 0.001, "Numbers are not equal within tolerance");
}

// The TestSuite class names the TestSuite, identifies what type of TestSuite,
// and enables the TestCases to be run.  Typically, only the constructor for
// this class must be defined

/**
 * \ingroup custom_module-tests
 * TestSuite for module custom_module
 */
class Custom_moduleTestSuite : public TestSuite
{
  public:
    Custom_moduleTestSuite();
};

Custom_moduleTestSuite::Custom_moduleTestSuite()
    : TestSuite("custom_module", Type::UNIT)
{
    // Duration for TestCase can be QUICK, EXTENSIVE or TAKES_FOREVER
    AddTestCase(new Custom_moduleTestCase1, TestCase::Duration::QUICK);
}

// Do not forget to allocate an instance of this TestSuite
/**
 * \ingroup custom_module-tests
 * Static variable for test initialization
 */
static Custom_moduleTestSuite scustom_moduleTestSuite;
